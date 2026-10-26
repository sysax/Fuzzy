#include "fuzzy/core/SyncOutbox.h"

#include "fuzzy/core/SyncEntities.h"
#include "fuzzy/core/Uuid.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>

#include <utility>

namespace fuzzy::core {

namespace {

qint64 enteroDeJson(const QJsonObject& payload, const QString& clave)
{
    return static_cast<qint64>(payload.value(clave).toDouble());
}

} // namespace

SyncOutbox::SyncOutbox(IQueryExecutor& local, QString nodo_uuid)
    : local_(local), nodo_uuid_(std::move(nodo_uuid))
{
}

bool SyncOutbox::registrar(const QString& entidad_nombre, const QString& uuid)
{
    const EntidadSync* entidad = entidadSync(entidad_nombre);
    if (entidad == nullptr || uuid.isEmpty() || nodo_uuid_.isEmpty()) {
        return false;
    }

    QJsonObject payload;
    QString error;
    if (!serializarEntidad(local_, *entidad, uuid, payload, &error)) {
        return false;
    }

    const qint64 updated_at = enteroDeJson(payload, QStringLiteral("updated_at"));
    const bool borrada = !payload.value(QStringLiteral("deleted_at")).isNull()
        && !payload.value(QStringLiteral("deleted_at")).isUndefined();
    const QString operacion = borrada ? QStringLiteral("delete")
                                      : QStringLiteral("upsert");

    // Determinista: el mismo estado local produce el mismo evento. Un
    // reintento tras un corte inserta el mismo uuid y el change log lo ignora.
    const QString event_uuid = uuidDeterminista(
        QStringLiteral("%1|%2|%3|%4|%5")
            .arg(nodo_uuid_, entidad_nombre, uuid)
            .arg(updated_at)
            .arg(operacion));

    const QString payload_json =
        QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact));
    const qint64 ahora = QDateTime::currentMSecsSinceEpoch();

    const ExecResult registrado = local_.execute(
        QStringLiteral(
            "INSERT INTO sync_outbox "
            "(event_uuid, entidad, entidad_uuid, operacion, payload, updated_at, creado_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(entidad, entidad_uuid) DO UPDATE SET "
            "event_uuid = excluded.event_uuid, "
            "operacion = excluded.operacion, "
            "payload = excluded.payload, "
            "updated_at = excluded.updated_at, "
            "enviado_at = NULL, intentos = 0, ultimo_error = NULL"),
        {QVariant(event_uuid), QVariant(entidad_nombre), QVariant(uuid),
         QVariant(operacion), QVariant(payload_json),
         QVariant::fromValue(updated_at), QVariant::fromValue(ahora)});
    return registrado.ok;
}

int SyncOutbox::detectarPendientes(int limite)
{
    int encolados = 0;
    for (const EntidadSync& entidad : entidadesSync()) {
        const auto filas = local_.query(
            QStringLiteral("SELECT uuid FROM %1 WHERE sync_state = 'pendiente' LIMIT ?")
                .arg(entidad.tabla),
            {QVariant::fromValue(limite)});
        for (const QueryRow& fila : filas) {
            if (fila.values.empty()) {
                continue;
            }
            if (registrar(entidad.nombre, fila.values.front().toString())) {
                ++encolados;
            }
        }
    }
    return encolados;
}

std::vector<EventoSalida> SyncOutbox::pendientes(int limite) const
{
    std::vector<EventoSalida> eventos;
    const auto filas = local_.query(
        QStringLiteral("SELECT id, event_uuid, entidad, entidad_uuid, operacion, "
                       "payload, updated_at FROM sync_outbox "
                       "WHERE enviado_at IS NULL ORDER BY id LIMIT ?"),
        {QVariant::fromValue(limite)});
    eventos.reserve(filas.size());
    for (const QueryRow& fila : filas) {
        if (fila.values.size() != 7) {
            continue;
        }
        EventoSalida evento;
        evento.id = fila.values.at(0).toLongLong();
        evento.event_uuid = fila.values.at(1).toString();
        evento.entidad = fila.values.at(2).toString();
        evento.entidad_uuid = fila.values.at(3).toString();
        evento.operacion = fila.values.at(4).toString();
        evento.payload = fila.values.at(5).toString();
        evento.updated_at = fila.values.at(6).toLongLong();
        eventos.push_back(evento);
    }
    return eventos;
}

bool SyncOutbox::marcarEnviado(const EventoSalida& evento)
{
    const qint64 ahora = QDateTime::currentMSecsSinceEpoch();
    const ExecResult marcado = local_.execute(
        QStringLiteral("UPDATE sync_outbox SET enviado_at = ?, intentos = intentos + 1, "
                       "ultimo_error = NULL WHERE id = ?"),
        {QVariant::fromValue(ahora), QVariant::fromValue(evento.id)});
    if (!marcado.ok) {
        return false;
    }

    // Solo se marca la fila si sigue en la versión publicada. Si el usuario
    // editó mientras el lote viajaba, queda 'pendiente' y se reencola.
    const EntidadSync* entidad = entidadSync(evento.entidad);
    if (entidad != nullptr) {
        local_.execute(
            QStringLiteral("UPDATE %1 SET sync_state = 'sincronizado' "
                           "WHERE uuid = ? AND updated_at = ?")
                .arg(entidad->tabla),
            {QVariant(evento.entidad_uuid), QVariant::fromValue(evento.updated_at)});
    }
    return true;
}

bool SyncOutbox::marcarError(const EventoSalida& evento, const QString& error)
{
    return local_.execute(
               QStringLiteral("UPDATE sync_outbox SET intentos = intentos + 1, "
                              "ultimo_error = ? WHERE id = ?"),
               {QVariant(error), QVariant::fromValue(evento.id)})
        .ok;
}

} // namespace fuzzy::core
