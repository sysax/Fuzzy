#include "fuzzy/core/BuzonService.h"

#include "fuzzy/core/Uuid.h"

#include <QDateTime>
#include <QTime>
#include <QVariant>

namespace fuzzy::core {

namespace {

QString textoRecortado(const QVariant& valor)
{
    return valor.toString().trimmed();
}

QString validarAlta(const QString& nombre, const QString& telefono,
                    const QString& correo, const QString& mascota,
                    const QString& motivo, const QString& franja)
{
    if (nombre.size() < 3 || nombre.size() > 120) {
        return QStringLiteral("el nombre debe tener entre 3 y 120 caracteres");
    }
    if (telefono.size() < 7 || telefono.size() > 20) {
        return QStringLiteral("el teléfono debe tener entre 7 y 20 caracteres");
    }
    if (!correo.isEmpty() && (correo.size() < 5 || correo.size() > 254)) {
        return QStringLiteral("el correo no es válido");
    }
    if (mascota.size() > 120) {
        return QStringLiteral("el nombre de la mascota excede 120 caracteres");
    }
    if (motivo.size() > 500) {
        return QStringLiteral("el motivo excede 500 caracteres");
    }
    if (franja.size() > 40) {
        return QStringLiteral("la franja excede 40 caracteres");
    }
    return {};
}

qint64 fechaDeFranja(const QString& franja, qint64 creada_at, qint64 ahora)
{
    const QDateTime fecha = QDateTime::fromString(franja, Qt::ISODate);
    if (fecha.isValid()) {
        return fecha.toMSecsSinceEpoch();
    }

    QTime hora = QTime::fromString(franja, QStringLiteral("HH:mm"));
    if (!hora.isValid()) {
        hora = QTime::fromString(franja, QStringLiteral("HH:mm:ss"));
    }
    if (!hora.isValid()) {
        hora = QTime(9, 0);
    }

    const qint64 base = creada_at > 0 ? creada_at : ahora;
    const QDate dia = QDateTime::fromMSecsSinceEpoch(base).date().addDays(1);
    return QDateTime(dia, hora).toMSecsSinceEpoch();
}

qint64 idDeCatalogo(IQueryExecutor& clinica, const QString& tabla,
                    const QString& columna, const QString& valor)
{
    const auto filas = clinica.query(
        QStringLiteral("SELECT id FROM %1 WHERE %2 = ? ORDER BY id LIMIT 1")
            .arg(tabla, columna),
        {QVariant(valor)});
    if (!filas.empty() && !filas.front().values.empty()) {
        return filas.front().values.front().toLongLong();
    }
    const auto cualquiera = clinica.query(
        QStringLiteral("SELECT id FROM %1 ORDER BY id LIMIT 1").arg(tabla));
    if (!cualquiera.empty() && !cualquiera.front().values.empty()) {
        return cualquiera.front().values.front().toLongLong();
    }
    return 0;
}

QString uuidClienteExistente(IQueryExecutor& clinica, const QString& correo,
                             const QString& telefono)
{
    if (!correo.isEmpty()) {
        const auto filas = clinica.query(
            QStringLiteral("SELECT uuid FROM clientes WHERE deleted_at IS NULL "
                           "AND correo IS NOT NULL AND lower(correo) = lower(?) LIMIT 1"),
            {QVariant(correo)});
        if (!filas.empty() && !filas.front().values.empty()) {
            return filas.front().values.front().toString();
        }
    }
    if (!telefono.isEmpty()) {
        const auto filas = clinica.query(
            QStringLiteral("SELECT uuid FROM clientes WHERE deleted_at IS NULL "
                           "AND telefono = ? LIMIT 1"),
            {QVariant(telefono)});
        if (!filas.empty() && !filas.front().values.empty()) {
            return filas.front().values.front().toString();
        }
    }
    return {};
}

qint64 idDe(IQueryExecutor* executor, const QString& tabla, const QString& uuid)
{
    if (executor == nullptr || uuid.isEmpty()) {
        return 0;
    }
    const auto filas = executor->query(
        QStringLiteral("SELECT id FROM %1 WHERE uuid = ?").arg(tabla),
        {QVariant(uuid)});
    if (filas.empty() || filas.front().values.empty()) {
        return 0;
    }
    return filas.front().values.front().toLongLong();
}

} // namespace

BuzonService::BuzonService(IQueryExecutor& executor, IQueryExecutor* clinica)
    : executor_(executor), clinica_(clinica)
{
}

bool BuzonService::consumir(const QString& clinica_uuid, const QString& uuid)
{
    if (clinica_uuid.isEmpty() || uuid.isEmpty()) {
        return false;
    }

    // El DELETE acotado por clínica y uuid es la operación atómica: solo la
    // primera llamada afecta una fila; las siguientes devuelven 0.
    const ExecResult consumido = executor_.execute(
        QStringLiteral("DELETE FROM altas WHERE uuid = ? AND clinica_uuid = ?"),
        {QVariant(uuid), QVariant(clinica_uuid)});
    return consumido.ok && consumido.affected == 1;
}

ResultadoRecoleccion BuzonService::recolectar(const QString& clinica_uuid, qint64 ahora)
{
    ResultadoRecoleccion resultado;

    if (clinica_uuid.isEmpty()) {
        resultado.errores.append(QStringLiteral("falta el uuid de la clínica"));
        return resultado;
    }
    if (clinica_ == nullptr) {
        resultado.errores.append(QStringLiteral("no se indicó la base de la clínica"));
        return resultado;
    }

    if (ahora <= 0) {
        ahora = QDateTime::currentMSecsSinceEpoch();
    }

    const auto filas = executor_.query(
        QStringLiteral("SELECT uuid, nombre, correo, telefono, mascota, motivo, franja, "
                       "creada_at FROM altas WHERE clinica_uuid = ? "
                       "ORDER BY creada_at, id"),
        {QVariant(clinica_uuid)});

    for (const QueryRow& fila : filas) {
        if (fila.values.size() != 8) {
            resultado.errores.append(QStringLiteral("fila del buzón con columnas inesperadas"));
            continue;
        }

        const QString alta_uuid = fila.values.at(0).toString();
        const QString nombre = textoRecortado(fila.values.at(1));
        const QString correo = textoRecortado(fila.values.at(2));
        const QString telefono = textoRecortado(fila.values.at(3));
        const QString mascota = textoRecortado(fila.values.at(4));
        const QString motivo = fila.values.at(5).toString();
        const QString franja = textoRecortado(fila.values.at(6));
        const qint64 creada_at = fila.values.at(7).toLongLong();

        const QString invalido =
            validarAlta(nombre, telefono, correo, mascota, motivo, franja);
        if (!invalido.isEmpty()) {
            ++resultado.rechazadas;
            resultado.errores.append(QStringLiteral("%1: %2").arg(alta_uuid, invalido));
            continue;
        }

        // Tres uuid derivados del alta: reintentar el lote reutiliza las
        // mismas filas y `ON CONFLICT` las deja intactas.
        const QString cliente_uuid =
            uuidClienteExistente(*clinica_, correo, telefono);
        const QString cliente_propio =
            cliente_uuid.isEmpty()
            ? uuidDeterminista(QStringLiteral("alta:%1:cliente").arg(alta_uuid))
            : cliente_uuid;
        const QString mascota_uuid =
            uuidDeterminista(QStringLiteral("alta:%1:mascota").arg(alta_uuid));
        const QString cita_uuid =
            uuidDeterminista(QStringLiteral("alta:%1:cita").arg(alta_uuid));

        if (cliente_uuid.isEmpty()) {
            const ExecResult cliente = clinica_->execute(
                QStringLiteral("INSERT INTO clientes "
                               "(uuid, nombre_completo, telefono, correo, direccion, dni, "
                               " activo, created_at, updated_at, sync_state) "
                               "VALUES (?, ?, ?, ?, NULL, NULL, 1, ?, ?, 'pendiente') "
                               "ON CONFLICT(uuid) DO NOTHING"),
                {QVariant(cliente_propio), QVariant(nombre), QVariant(telefono),
                 correo.isEmpty() ? QVariant() : QVariant(correo),
                 QVariant::fromValue(creada_at), QVariant::fromValue(creada_at)});
            if (!cliente.ok) {
                resultado.errores.append(
                    QStringLiteral("%1: no se pudo crear la ficha provisional").arg(alta_uuid));
                continue;
            }
        }

        const qint64 cliente_id = idDe(clinica_, QStringLiteral("clientes"), cliente_propio);
        if (cliente_id <= 0) {
            resultado.errores.append(
                QStringLiteral("%1: la ficha provisional no quedó disponible").arg(alta_uuid));
            continue;
        }

        const QString nombre_mascota =
            mascota.isEmpty() ? QStringLiteral("Mascota de %1").arg(nombre) : mascota;
        const ExecResult mascota_creada = clinica_->execute(
            QStringLiteral("INSERT INTO mascotas "
                           "(uuid, cliente_id, nombre, especie, raza, sexo, peso_kg, "
                           " numero_chip, fecha_nacimiento, activo, castrado, alergias, "
                           " atencion_especial, created_at, updated_at, sync_state) "
                           "VALUES (?, ?, ?, 'No indicada', NULL, 'Desconocido', 0, "
                           " NULL, NULL, 1, 0, NULL, NULL, ?, ?, 'pendiente') "
                           "ON CONFLICT(uuid) DO NOTHING"),
            {QVariant(mascota_uuid), QVariant::fromValue(cliente_id),
             QVariant(nombre_mascota), QVariant::fromValue(creada_at),
             QVariant::fromValue(creada_at)});
        if (!mascota_creada.ok) {
            resultado.errores.append(
                QStringLiteral("%1: no se pudo crear la mascota provisional").arg(alta_uuid));
            continue;
        }

        const qint64 mascota_id = idDe(clinica_, QStringLiteral("mascotas"), mascota_uuid);
        if (mascota_id <= 0) {
            resultado.errores.append(
                QStringLiteral("%1: la mascota provisional no quedó disponible").arg(alta_uuid));
            continue;
        }

        const qint64 tipo_servicio =
            idDeCatalogo(*clinica_, QStringLiteral("tipos_servicio_cita"),
                         QStringLiteral("nombre_servicio"), QStringLiteral("Médica"));
        const qint64 estado = idDeCatalogo(*clinica_, QStringLiteral("estados_cita"),
                                           QStringLiteral("nombre_estado"),
                                           QStringLiteral("Agendada"));
        if (tipo_servicio <= 0 || estado <= 0) {
            resultado.errores.append(
                QStringLiteral("%1: faltan catálogos de agenda").arg(alta_uuid));
            continue;
        }

        const qint64 fecha_hora = fechaDeFranja(franja, creada_at, ahora);
        const ExecResult cita = clinica_->execute(
            QStringLiteral("INSERT INTO citas "
                           "(uuid, mascota_id, tipo_servicio_id, estado_cita_id, "
                           " fecha_hora_programada, origen, direccion_domicilio, notas_cliente, "
                           " fecha_creacion, fecha_actualizacion, created_at, updated_at, "
                           " sync_state) "
                           "VALUES (?, ?, ?, ?, ?, 'primera_visita', NULL, ?, ?, ?, ?, ?, "
                           " 'pendiente') "
                           "ON CONFLICT(uuid) DO NOTHING"),
            {QVariant(cita_uuid), QVariant::fromValue(mascota_id),
             QVariant::fromValue(tipo_servicio), QVariant::fromValue(estado),
             QVariant::fromValue(fecha_hora),
             motivo.isEmpty() ? QVariant() : QVariant(motivo),
             QVariant::fromValue(ahora), QVariant::fromValue(ahora),
             QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
        if (!cita.ok) {
            resultado.errores.append(
                QStringLiteral("%1: no se pudo crear la cita de primera visita").arg(alta_uuid));
            continue;
        }

        const ExecResult consumida = executor_.execute(
            QStringLiteral("DELETE FROM altas WHERE uuid = ? AND clinica_uuid = ?"),
            {QVariant(alta_uuid), QVariant(clinica_uuid)});
        if (!consumida.ok) {
            resultado.errores.append(
                QStringLiteral("%1: no se pudo borrar el alta del buzón").arg(alta_uuid));
            continue;
        }

        ++resultado.procesadas;
        resultado.altas.append(
            {alta_uuid, cliente_propio, mascota_uuid, cita_uuid});
    }

    resultado.ok = resultado.errores.isEmpty();
    return resultado;
}

} // namespace fuzzy::core
