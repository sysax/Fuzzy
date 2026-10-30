#include "agenda/AgendaController.h"

#include "common/Consulta.h"
#include "fuzzy/core/Uuid.h"

#include <QDateTime>
#include <QLocale>
#include <QVariant>

namespace fuzzy::desktop {

namespace {

QString diaSemanaCorto(const QDate& fecha)
{
    return QLocale(QLocale::Spanish, QLocale::Colombia)
        .toString(fecha, QStringLiteral("ddd"));
}

} // namespace

AgendaController::AgendaController(fuzzy::core::IQueryExecutor& db, QObject* parent)
    : QObject(parent), db_(db)
{
    fecha_ = fechaHoy();
    recargar();
}

QString AgendaController::etiquetaPeriodo() const
{
    const QLocale locale(QLocale::Spanish, QLocale::Colombia);
    if (modo_ == QStringLiteral("dia")) {
        return locale.toString(fecha_, QStringLiteral("dddd d 'de' MMMM 'de' yyyy"));
    }
    const QDate inicio = fecha_.addDays(1 - fecha_.dayOfWeek());
    const QDate fin = inicio.addDays(6);
    return QStringLiteral("%1 – %2")
        .arg(locale.toString(inicio, QStringLiteral("d MMM")),
             locale.toString(fin, QStringLiteral("d MMM yyyy")));
}

qint64 AgendaController::inicioPeriodo() const
{
    if (modo_ == QStringLiteral("dia")) {
        return inicioDiaMs(fecha_);
    }
    return inicioDiaMs(fecha_.addDays(1 - fecha_.dayOfWeek()));
}

qint64 AgendaController::finPeriodo() const
{
    if (modo_ == QStringLiteral("dia")) {
        return finDiaMs(fecha_);
    }
    return finDiaMs(fecha_.addDays(7 - fecha_.dayOfWeek()));
}

void AgendaController::cargar()
{
    recargar();
}

void AgendaController::irHoy()
{
    fecha_ = fechaHoy();
    recargar();
}

void AgendaController::anterior()
{
    fecha_ = fecha_.addDays(modo_ == QStringLiteral("dia") ? -1 : -7);
    recargar();
}

void AgendaController::siguiente()
{
    fecha_ = fecha_.addDays(modo_ == QStringLiteral("dia") ? 1 : 7);
    recargar();
}

void AgendaController::cambiarModo(const QString& modo)
{
    if (modo != QStringLiteral("dia") && modo != QStringLiteral("semana")) {
        return;
    }
    modo_ = modo;
    recargar();
}

void AgendaController::recargar()
{
    const auto filas = db_.query(
        QStringLiteral("SELECT c.uuid, c.fecha_hora_programada, m.nombre, "
                       "cl.nombre_completo, ts.nombre_servicio, ec.nombre_estado, "
                       "c.prioridad, c.es_emergencia, m.uuid, ts.uuid, ec.uuid, "
                       "coalesce(u.nombre_completo, '') "
                       "FROM citas c "
                       "JOIN mascotas m ON m.id = c.mascota_id "
                       "JOIN clientes cl ON cl.id = m.cliente_id "
                       "JOIN tipos_servicio_cita ts ON ts.id = c.tipo_servicio_id "
                       "JOIN estados_cita ec ON ec.id = c.estado_cita_id "
                       "LEFT JOIN usuarios u ON u.id = c.empleado_asignado_id "
                       "WHERE c.deleted_at IS NULL "
                       "AND c.fecha_hora_programada BETWEEN ? AND ? "
                       "ORDER BY c.fecha_hora_programada"),
        {QVariant::fromValue(inicioPeriodo()), QVariant::fromValue(finPeriodo())});

    citas_.clear();
    for (const auto& fila : filas) {
        const qint64 ms = enteroDeFila(fila, 1);
        const QDateTime fechaHora = QDateTime::fromMSecsSinceEpoch(ms);
        QVariantMap cita;
        cita.insert(QStringLiteral("uuid"), textoDeFila(fila, 0));
        cita.insert(QStringLiteral("fechaHora"), fechaHoraTexto(ms));
        cita.insert(QStringLiteral("fecha"), fechaHora.toString(QStringLiteral("yyyy-MM-dd")));
        cita.insert(QStringLiteral("hora"), horaTexto(ms));
        cita.insert(QStringLiteral("dia"), diaSemanaCorto(fechaHora.date()));
        cita.insert(QStringLiteral("diaNumero"), fechaHora.date().day());
        cita.insert(QStringLiteral("mascota"), textoDeFila(fila, 2));
        cita.insert(QStringLiteral("propietario"), textoDeFila(fila, 3));
        cita.insert(QStringLiteral("tipo"), textoDeFila(fila, 4));
        cita.insert(QStringLiteral("estado"), textoDeFila(fila, 5));
        cita.insert(QStringLiteral("prioridad"), textoDeFila(fila, 6));
        cita.insert(QStringLiteral("emergencia"), enteroDeFila(fila, 7) == 1);
        cita.insert(QStringLiteral("mascotaUuid"), textoDeFila(fila, 8));
        cita.insert(QStringLiteral("tipoUuid"), textoDeFila(fila, 9));
        cita.insert(QStringLiteral("estadoUuid"), textoDeFila(fila, 10));
        cita.insert(QStringLiteral("empleado"), textoDeFila(fila, 11));
        citas_.append(cita);
    }

    tipos_ = listaDeFilas(
        db_.query(QStringLiteral("SELECT uuid, nombre_servicio FROM tipos_servicio_cita "
                                 "WHERE deleted_at IS NULL ORDER BY id")),
        {QStringLiteral("uuid"), QStringLiteral("nombre")});
    estados_ = listaDeFilas(
        db_.query(QStringLiteral("SELECT uuid, nombre_estado FROM estados_cita "
                                 "WHERE deleted_at IS NULL ORDER BY id")),
        {QStringLiteral("uuid"), QStringLiteral("nombre")});
    mascotas_ = listaDeFilas(
        db_.query(QStringLiteral("SELECT m.uuid, m.nombre, cl.nombre_completo, m.especie "
                                 "FROM mascotas m JOIN clientes cl ON cl.id = m.cliente_id "
                                 "WHERE m.deleted_at IS NULL AND m.activo = 1 "
                                 "ORDER BY m.nombre")),
        {QStringLiteral("uuid"), QStringLiteral("nombre"), QStringLiteral("propietario"),
         QStringLiteral("especie")});

    dias_.clear();
    const QDate inicioSemana = fecha_.addDays(1 - fecha_.dayOfWeek());
    for (int i = 0; i < 7; ++i) {
        const QDate dia = inicioSemana.addDays(i);
        QVariantMap entrada;
        entrada.insert(QStringLiteral("fecha"), dia.toString(QStringLiteral("yyyy-MM-dd")));
        entrada.insert(QStringLiteral("nombre"), diaSemanaCorto(dia));
        entrada.insert(QStringLiteral("numero"), dia.day());
        entrada.insert(QStringLiteral("mes"),
                       QLocale(QLocale::Spanish, QLocale::Colombia)
                           .toString(dia, QStringLiteral("MMM")));
        entrada.insert(QStringLiteral("hoy"), dia == fechaHoy());
        dias_.append(entrada);
    }

    emit datosCambiados();
}

void AgendaController::fallo(const QString& texto)
{
    error_ = texto;
    mensaje_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

void AgendaController::exito(const QString& texto)
{
    mensaje_ = texto;
    error_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

bool AgendaController::resolverCita(const QString& citaUuid, qint64* id,
                                    qint64* estadoActual)
{
    const auto filas = db_.query(
        QStringLiteral("SELECT id, estado_cita_id FROM citas "
                       "WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(citaUuid)});
    if (filas.size() != 1) {
        return false;
    }
    *id = enteroDeFila(filas.front(), 0);
    *estadoActual = enteroDeFila(filas.front(), 1);
    return true;
}

void AgendaController::agendar(const QString& mascotaUuid, const QString& tipoUuid,
                               const QString& fecha, const QString& hora,
                               const QString& notas, bool emergencia)
{
    const QDate dia = QDate::fromString(fecha, QStringLiteral("yyyy-MM-dd"));
    const QTime tiempo = QTime::fromString(hora, QStringLiteral("HH:mm"));
    if (!dia.isValid() || !tiempo.isValid()) {
        fallo(QStringLiteral("la fecha y la hora de la cita no son válidas"));
        return;
    }
    if (mascotaUuid.isEmpty()) {
        fallo(QStringLiteral("elige una mascota"));
        return;
    }

    const auto mascota = db_.query(
        QStringLiteral("SELECT id FROM mascotas WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(mascotaUuid)});
    const auto tipo = db_.query(
        QStringLiteral("SELECT id FROM tipos_servicio_cita WHERE uuid = ?"),
        {QVariant(tipoUuid)});
    const auto estado = db_.query(
        QStringLiteral("SELECT id FROM estados_cita WHERE nombre_estado = 'Agendada'"));
    if (mascota.size() != 1 || tipo.size() != 1 || estado.size() != 1) {
        fallo(QStringLiteral("la mascota o el tipo de servicio no existe"));
        return;
    }

    const qint64 ms = msDesdeFechaHora(QDateTime(dia, tiempo));
    const qint64 conflicto = [&]() {
        const auto filas = db_.query(
            QStringLiteral("SELECT count(*) FROM citas WHERE mascota_id = ? "
                           "AND deleted_at IS NULL AND fecha_hora_programada BETWEEN ? AND ?"),
            {mascota.front().values.front(), QVariant::fromValue(ms - 30 * 60 * 1000),
             QVariant::fromValue(ms + 30 * 60 * 1000)});
        return filas.empty() ? 0LL : filas.front().values.front().toLongLong();
    }();
    if (conflicto > 0) {
        fallo(QStringLiteral("la mascota ya tiene una cita en esa franja"));
        return;
    }

    const qint64 ahora = QDateTime::currentMSecsSinceEpoch();
    const auto alta = db_.execute(
        QStringLiteral("INSERT INTO citas "
                       "(uuid, mascota_id, tipo_servicio_id, estado_cita_id, "
                       " fecha_hora_programada, prioridad, es_emergencia, origen, "
                       " notas_cliente, fecha_creacion, fecha_actualizacion, "
                       " created_at, updated_at, sync_state) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, 'mostrador', ?, ?, ?, ?, ?, 'pendiente')"),
        {QVariant(fuzzy::core::uuidNuevo()), mascota.front().values.front(),
         tipo.front().values.front(), estado.front().values.front(),
         QVariant::fromValue(ms),
         QVariant(emergencia ? QStringLiteral("Emergencia") : QStringLiteral("Normal")),
         QVariant::fromValue(emergencia ? 1 : 0),
         notas.trimmed().isEmpty() ? QVariant() : QVariant(notas.trimmed()),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!alta.ok) {
        fallo(QStringLiteral("no se pudo agendar la cita"));
        return;
    }

    fecha_ = dia;
    recargar();
    exito(QStringLiteral("Cita agendada para el %1 a las %2.")
              .arg(fechaTexto(ms), horaTexto(ms)));
}

void AgendaController::cambiarEstado(const QString& citaUuid, const QString& estadoUuid)
{
    qint64 id = 0;
    qint64 estadoAnterior = 0;
    if (!resolverCita(citaUuid, &id, &estadoAnterior)) {
        fallo(QStringLiteral("la cita no existe"));
        return;
    }
    const auto estado = db_.query(
        QStringLiteral("SELECT id, nombre_estado FROM estados_cita WHERE uuid = ?"),
        {QVariant(estadoUuid)});
    if (estado.size() != 1) {
        fallo(QStringLiteral("el estado no existe"));
        return;
    }

    const qint64 nuevo = enteroDeFila(estado.front(), 0);
    const QString nombre = textoDeFila(estado.front(), 1);
    const qint64 ahora = QDateTime::currentMSecsSinceEpoch();
    const auto cambio = db_.execute(
        QStringLiteral("UPDATE citas SET estado_cita_id = ?, fecha_actualizacion = ?, "
                       "updated_at = ?, fecha_llegada = CASE WHEN ? = 'En espera' "
                       "THEN coalesce(fecha_llegada, ?) ELSE fecha_llegada END "
                       "WHERE id = ?"),
        {QVariant::fromValue(nuevo), QVariant::fromValue(ahora), QVariant::fromValue(ahora),
         QVariant(nombre), QVariant::fromValue(ahora), QVariant::fromValue(id)});
    if (!cambio.ok) {
        fallo(QStringLiteral("no se pudo cambiar el estado"));
        return;
    }

    db_.execute(
        QStringLiteral("INSERT INTO historial_cambios_cita "
                       "(uuid, cita_id, estado_anterior_id, estado_nuevo_id, "
                       " motivo_cambio, created_at, sync_state) "
                       "VALUES (?, ?, ?, ?, ?, ?, 'pendiente')"),
        {QVariant(fuzzy::core::uuidNuevo()), QVariant::fromValue(id),
         QVariant::fromValue(estadoAnterior), QVariant::fromValue(nuevo),
         QVariant(QStringLiteral("Cambio desde la agenda")), QVariant::fromValue(ahora)});
    recargar();
    exito(QStringLiteral("Cita marcada como %1.").arg(nombre));
}

void AgendaController::mover(const QString& citaUuid, const QString& fecha,
                             const QString& hora)
{
    const QDate dia = QDate::fromString(fecha, QStringLiteral("yyyy-MM-dd"));
    const QTime tiempo = QTime::fromString(hora, QStringLiteral("HH:mm"));
    if (!dia.isValid() || !tiempo.isValid()) {
        fallo(QStringLiteral("la nueva fecha no es válida"));
        return;
    }
    qint64 id = 0;
    qint64 estadoAnterior = 0;
    if (!resolverCita(citaUuid, &id, &estadoAnterior)) {
        fallo(QStringLiteral("la cita no existe"));
        return;
    }
    const auto reprogramada = db_.query(
        QStringLiteral("SELECT id FROM estados_cita WHERE nombre_estado = 'Reprogramada'"));
    if (reprogramada.size() != 1) {
        fallo(QStringLiteral("falta el estado Reprogramada"));
        return;
    }

    const qint64 ms = msDesdeFechaHora(QDateTime(dia, tiempo));
    const qint64 ahora = QDateTime::currentMSecsSinceEpoch();
    const auto cambio = db_.execute(
        QStringLiteral("UPDATE citas SET fecha_hora_programada = ?, estado_cita_id = ?, "
                       "fecha_actualizacion = ?, updated_at = ? WHERE id = ?"),
        {QVariant::fromValue(ms), reprogramada.front().values.front(),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora), QVariant::fromValue(id)});
    if (!cambio.ok) {
        fallo(QStringLiteral("no se pudo mover la cita"));
        return;
    }

    db_.execute(
        QStringLiteral("INSERT INTO historial_cambios_cita "
                       "(uuid, cita_id, estado_anterior_id, estado_nuevo_id, "
                       " fecha_hora_nueva, motivo_cambio, created_at, sync_state) "
                       "VALUES (?, ?, ?, ?, ?, 'Reprogramación desde la agenda', ?, 'pendiente')"),
        {QVariant(fuzzy::core::uuidNuevo()), QVariant::fromValue(id),
         QVariant::fromValue(estadoAnterior), reprogramada.front().values.front(),
         QVariant::fromValue(ms), QVariant::fromValue(ahora)});
    fecha_ = dia;
    recargar();
    exito(QStringLiteral("Cita movida al %1 a las %2.").arg(fechaTexto(ms), horaTexto(ms)));
}

void AgendaController::cancelar(const QString& citaUuid)
{
    const auto estado = db_.query(
        QStringLiteral("SELECT uuid FROM estados_cita WHERE nombre_estado = 'Cancelada'"));
    if (estado.size() != 1) {
        fallo(QStringLiteral("falta el estado Cancelada"));
        return;
    }
    cambiarEstado(citaUuid, textoDeFila(estado.front(), 0));
}

void AgendaController::limpiarMensajes()
{
    error_.clear();
    mensaje_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

} // namespace fuzzy::desktop
