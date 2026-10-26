#include "fuzzy/core/SolicitudCitaService.h"

#include "fuzzy/core/PortalBaseStore.h"
#include "fuzzy/core/Uuid.h"

#include <QDate>
#include <QDateTime>
#include <QTime>
#include <QVariant>

namespace fuzzy::core {

namespace {

qint64 idDeCatalogo(IQueryExecutor& executor, const QString& tabla,
                    const QString& columna, const QString& valor)
{
    const auto filas = executor.query(
        QStringLiteral("SELECT id FROM %1 WHERE %2 = ? ORDER BY id LIMIT 1")
            .arg(tabla, columna),
        {QVariant(valor)});
    if (!filas.empty() && !filas.front().values.empty()) {
        return filas.front().values.front().toLongLong();
    }
    const auto cualquiera =
        executor.query(QStringLiteral("SELECT id FROM %1 ORDER BY id LIMIT 1").arg(tabla));
    if (!cualquiera.empty() && !cualquiera.front().values.empty()) {
        return cualquiera.front().values.front().toLongLong();
    }
    return 0;
}

bool fechaHoraDe(const QString& fecha_texto, const QString& hora_texto, qint64& salida)
{
    QDate fecha = QDate::fromString(fecha_texto, QStringLiteral("yyyy-MM-dd"));
    if (!fecha.isValid()) {
        fecha = QDate::fromString(fecha_texto, Qt::ISODate);
    }

    QTime hora = QTime::fromString(hora_texto, QStringLiteral("HH:mm"));
    if (!hora.isValid()) {
        hora = QTime::fromString(hora_texto, QStringLiteral("HH:mm:ss"));
    }
    if (!hora.isValid()) {
        hora = QTime::fromString(hora_texto, Qt::ISODate);
    }

    if (!fecha.isValid() || !hora.isValid()) {
        return false;
    }
    salida = QDateTime(fecha, hora).toMSecsSinceEpoch();
    return true;
}

QString actualizarSolicitud(IQueryExecutor& portal, const QString& uuid,
                            const QString& estado, const QString& respuesta,
                            const QString& cita_uuid, qint64 ahora)
{
    const ExecResult actualizada = portal.execute(
        QStringLiteral("UPDATE solicitudes_cita SET estado = ?, respuesta = ?, "
                       "cita_uuid = ?, updated_at = ? "
                       "WHERE uuid = ? AND estado = 'Pendiente'"),
        {QVariant(estado), QVariant(respuesta),
         cita_uuid.isEmpty() ? QVariant() : QVariant(cita_uuid),
         QVariant::fromValue(ahora), QVariant(uuid)});
    if (!actualizada.ok) {
        return QStringLiteral("no se pudo actualizar la solicitud %1").arg(uuid);
    }
    return {};
}

bool proyectarCita(IQueryExecutor& portal, const QString& cita_uuid,
                   const QString& mascota_uuid, qint64 fecha_hora,
                   const QString& tipo_servicio, const QString& estado,
                   const QString& notas, qint64 ahora)
{
    return portal.execute(
        QStringLiteral("INSERT INTO citas "
                       "(uuid, mascota_uuid, fecha_hora, tipo_servicio, estado, "
                       " direccion, notas, origen, created_at, updated_at, deleted_at) "
                       "VALUES (?, ?, ?, ?, ?, NULL, ?, 'app', ?, ?, NULL) "
                       "ON CONFLICT(uuid) DO UPDATE SET "
                       "fecha_hora = excluded.fecha_hora, "
                       "tipo_servicio = excluded.tipo_servicio, "
                       "estado = excluded.estado, notas = excluded.notas, "
                       "origen = excluded.origen, updated_at = excluded.updated_at, "
                       "deleted_at = NULL"),
        {QVariant(cita_uuid), QVariant(mascota_uuid), QVariant::fromValue(fecha_hora),
         QVariant(tipo_servicio), QVariant(estado),
         notas.isEmpty() ? QVariant() : QVariant(notas),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)})
        .ok;
}

} // namespace

SolicitudCitaService::SolicitudCitaService(IQueryExecutor& clinica, PortalBaseStore& bases)
    : clinica_(clinica), bases_(bases)
{
}

ResultadoSolicitudes SolicitudCitaService::procesar(const QString& portal_uuid, qint64 ahora)
{
    ResultadoSolicitudes resultado;

    std::unique_ptr<IQueryExecutor> portal = bases_.abrir(portal_uuid);
    if (portal == nullptr) {
        resultado.error = QStringLiteral("no existe la base del portal");
        return resultado;
    }

    const auto meta = portal->query(
        QStringLiteral("SELECT cuenta_uuid FROM portal_meta WHERE id = 1"));
    if (meta.size() != 1 || meta.front().values.empty()) {
        resultado.error = QStringLiteral("portal_meta no tiene la fila única");
        return resultado;
    }
    const QString cuenta_uuid = meta.front().values.front().toString();

    const auto cliente = clinica_.query(
        QStringLiteral("SELECT cl.id FROM clientes cl "
                       "JOIN cuentas cu ON cu.cliente_id = cl.id "
                       "WHERE cu.uuid = ? AND cu.deleted_at IS NULL "
                       "AND cl.deleted_at IS NULL"),
        {QVariant(cuenta_uuid)});
    if (cliente.size() != 1 || cliente.front().values.empty()) {
        resultado.error = QStringLiteral("la cuenta del portal no tiene ficha en la clínica");
        return resultado;
    }
    const qint64 cliente_id = cliente.front().values.front().toLongLong();

    if (ahora <= 0) {
        ahora = QDateTime::currentMSecsSinceEpoch();
    }

    const auto pendientes = portal->query(
        QStringLiteral("SELECT uuid, mascota_uuid, tipo_servicio, fecha_preferida, "
                       "hora_preferida, motivo, created_at FROM solicitudes_cita "
                       "WHERE estado = 'Pendiente' AND deleted_at IS NULL "
                       "ORDER BY created_at, id"));

    for (const QueryRow& fila : pendientes) {
        if (fila.values.size() != 7) {
            continue;
        }

        const QString solicitud_uuid = fila.values.at(0).toString();
        const QString mascota_uuid = fila.values.at(1).toString();
        const QString tipo_servicio_solicitado = fila.values.at(2).toString();
        const QString fecha_preferida = fila.values.at(3).toString();
        const QString hora_preferida = fila.values.at(4).toString();
        const QString motivo = fila.values.at(5).toString();

        const auto mascota = clinica_.query(
            QStringLiteral("SELECT id FROM mascotas WHERE uuid = ? AND cliente_id = ? "
                           "AND deleted_at IS NULL"),
            {QVariant(mascota_uuid), QVariant::fromValue(cliente_id)});
        if (mascota.size() != 1 || mascota.front().values.empty()) {
            const QString error = actualizarSolicitud(
                *portal, solicitud_uuid, QStringLiteral("Rechazada"),
                QStringLiteral("La mascota no pertenece a esta ficha."), {}, ahora);
            if (!error.isEmpty()) {
                resultado.error = error;
                return resultado;
            }
            resultado.procesadas.append(
                {solicitud_uuid, QStringLiteral("Rechazada"), {},
                 QStringLiteral("La mascota no pertenece a esta ficha.")});
            continue;
        }
        const qint64 mascota_id = mascota.front().values.front().toLongLong();

        qint64 fecha_hora = 0;
        if (!fechaHoraDe(fecha_preferida, hora_preferida, fecha_hora)) {
            const QString respuesta = QStringLiteral("La fecha u hora solicitada no es válida.");
            const QString error = actualizarSolicitud(
                *portal, solicitud_uuid, QStringLiteral("Rechazada"), respuesta, {}, ahora);
            if (!error.isEmpty()) {
                resultado.error = error;
                return resultado;
            }
            resultado.procesadas.append(
                {solicitud_uuid, QStringLiteral("Rechazada"), {}, respuesta});
            continue;
        }
        if (fecha_hora <= ahora) {
            const QString respuesta = QStringLiteral("La franja solicitada ya pasó.");
            const QString error = actualizarSolicitud(
                *portal, solicitud_uuid, QStringLiteral("Rechazada"), respuesta, {}, ahora);
            if (!error.isEmpty()) {
                resultado.error = error;
                return resultado;
            }
            resultado.procesadas.append(
                {solicitud_uuid, QStringLiteral("Rechazada"), {}, respuesta});
            continue;
        }

        const auto ocupada = clinica_.query(
            QStringLiteral("SELECT 1 FROM citas WHERE fecha_hora_programada = ? "
                           "AND deleted_at IS NULL AND estado_cita_id NOT IN "
                           "(SELECT id FROM estados_cita WHERE nombre_estado = 'Cancelada') "
                           "LIMIT 1"),
            {QVariant::fromValue(fecha_hora)});
        if (!ocupada.empty()) {
            const QString respuesta = QStringLiteral("La franja ya no está disponible.");
            const QString error = actualizarSolicitud(
                *portal, solicitud_uuid, QStringLiteral("Rechazada"), respuesta, {}, ahora);
            if (!error.isEmpty()) {
                resultado.error = error;
                return resultado;
            }
            resultado.procesadas.append(
                {solicitud_uuid, QStringLiteral("Rechazada"), {}, respuesta});
            continue;
        }

        const qint64 tipo_servicio = idDeCatalogo(
            clinica_, QStringLiteral("tipos_servicio_cita"),
            QStringLiteral("nombre_servicio"), tipo_servicio_solicitado);
        const qint64 estado = idDeCatalogo(
            clinica_, QStringLiteral("estados_cita"), QStringLiteral("nombre_estado"),
            QStringLiteral("Agendada"));
        if (tipo_servicio <= 0 || estado <= 0) {
            resultado.error = QStringLiteral("faltan catálogos de agenda");
            return resultado;
        }

        const QString cita_uuid =
            uuidDeterminista(QStringLiteral("solicitud:%1:cita").arg(solicitud_uuid));
        const ExecResult cita = clinica_.execute(
            QStringLiteral("INSERT INTO citas "
                           "(uuid, mascota_id, tipo_servicio_id, estado_cita_id, "
                           " fecha_hora_programada, origen, direccion_domicilio, "
                           " notas_cliente, fecha_creacion, fecha_actualizacion, "
                           " created_at, updated_at, sync_state) "
                           "VALUES (?, ?, ?, ?, ?, 'app', NULL, ?, ?, ?, ?, ?, 'pendiente') "
                           "ON CONFLICT(uuid) DO NOTHING"),
            {QVariant(cita_uuid), QVariant::fromValue(mascota_id),
             QVariant::fromValue(tipo_servicio), QVariant::fromValue(estado),
             QVariant::fromValue(fecha_hora),
             motivo.isEmpty() ? QVariant() : QVariant(motivo),
             QVariant::fromValue(ahora), QVariant::fromValue(ahora),
             QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
        if (!cita.ok) {
            resultado.error = QStringLiteral("no se pudo crear la cita de la solicitud %1")
                                  .arg(solicitud_uuid);
            return resultado;
        }

        const auto nombre_servicio = clinica_.query(
            QStringLiteral("SELECT nombre_servicio FROM tipos_servicio_cita WHERE id = ?"),
            {QVariant::fromValue(tipo_servicio)});
        const QString nombre_tipo = !nombre_servicio.empty()
                && !nombre_servicio.front().values.empty()
            ? nombre_servicio.front().values.front().toString()
            : tipo_servicio_solicitado;
        const auto nombre_estado = clinica_.query(
            QStringLiteral("SELECT nombre_estado FROM estados_cita WHERE id = ?"),
            {QVariant::fromValue(estado)});
        const QString nombre_estado_texto = !nombre_estado.empty()
                && !nombre_estado.front().values.empty()
            ? nombre_estado.front().values.front().toString()
            : QStringLiteral("Agendada");

        if (!proyectarCita(*portal, cita_uuid, mascota_uuid, fecha_hora, nombre_tipo,
                           nombre_estado_texto, motivo, ahora)) {
            resultado.error = QStringLiteral("no se pudo proyectar la cita %1").arg(cita_uuid);
            return resultado;
        }

        const QString respuesta = QStringLiteral("Cita agendada para el %1 a las %2.")
                                      .arg(fecha_preferida, hora_preferida);
        const QString error = actualizarSolicitud(
            *portal, solicitud_uuid, QStringLiteral("Aceptada"), respuesta, cita_uuid, ahora);
        if (!error.isEmpty()) {
            resultado.error = error;
            return resultado;
        }

        resultado.procesadas.append(
            {solicitud_uuid, QStringLiteral("Aceptada"), cita_uuid, respuesta});
    }

    resultado.ok = true;
    return resultado;
}

} // namespace fuzzy::core
