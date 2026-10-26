#include "fuzzy/core/SyncEngine.h"

#include "fuzzy/core/SyncEntities.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>

#include <algorithm>

namespace fuzzy::core {

namespace {

bool tablaExiste(IQueryExecutor& executor, const QString& tabla)
{
    const auto filas = executor.query(
        QStringLiteral("SELECT name FROM sqlite_master WHERE type = 'table' AND name = ?"),
        {QVariant(tabla)});
    return filas.size() == 1;
}

bool eventoAplicado(IQueryExecutor& local, const QString& event_uuid)
{
    const auto filas = local.query(
        QStringLiteral("SELECT 1 FROM sync_aplicado WHERE event_uuid = ?"),
        {QVariant(event_uuid)});
    return !filas.empty();
}

struct EventoEntrante
{
    qint64 id = 0;
    QString event_uuid;
    QString entidad;
    QString entidad_uuid;
    QString operacion;
    QString payload;
    qint64 updated_at = 0;
};

} // namespace

SyncEngine::SyncEngine(IQueryExecutor& local, IQueryExecutor& remoto, QString nodo_uuid)
    : local_(local), remoto_(remoto), nodo_uuid_(std::move(nodo_uuid)),
      outbox_(local, nodo_uuid_)
{
}

bool SyncEngine::preparar(QString* error)
{
    if (nodo_uuid_.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("el nodo no tiene identidad");
        }
        return false;
    }

    for (const QString& tabla : {QStringLiteral("sync_outbox"),
                                 QStringLiteral("sync_change_log"),
                                 QStringLiteral("sync_aplicado"),
                                 QStringLiteral("sync_meta")}) {
        if (!tablaExiste(local_, tabla)) {
            if (error != nullptr) {
                *error = QStringLiteral("falta la tabla local %1; aplica la migración 0004")
                             .arg(tabla);
            }
            return false;
        }
    }
    if (!tablaExiste(remoto_, QStringLiteral("sync_change_log"))) {
        if (error != nullptr) {
            *error = QStringLiteral("la base remota no tiene sync_change_log");
        }
        return false;
    }

    const qint64 ahora = QDateTime::currentMSecsSinceEpoch();
    const ExecResult meta = local_.execute(
        QStringLiteral("INSERT INTO sync_meta (id, nodo_uuid, actualizado_at) VALUES (1, ?, ?) "
                       "ON CONFLICT(id) DO UPDATE SET "
                       "ultimo_pull_id = CASE WHEN sync_meta.nodo_uuid = excluded.nodo_uuid "
                       "THEN sync_meta.ultimo_pull_id ELSE 0 END, "
                       "nodo_uuid = excluded.nodo_uuid, "
                       "actualizado_at = excluded.actualizado_at"),
        {QVariant(nodo_uuid_), QVariant::fromValue(ahora)});
    if (!meta.ok) {
        if (error != nullptr) {
            *error = QStringLiteral("no se pudo registrar sync_meta");
        }
        return false;
    }
    return true;
}

ResultadoSync SyncEngine::sincronizar(int limite)
{
    ResultadoSync resultado;
    QString error;
    if (!preparar(&error)) {
        resultado.error = error;
        return resultado;
    }

    resultado.detectados = outbox_.detectarPendientes(limite);

    const ResultadoSync push = empujar(limite);
    const ResultadoSync pull = jalar(limite);
    resultado.empujados = push.empujados;
    resultado.aplicados = pull.aplicados;
    resultado.aplazados = pull.aplazados;
    resultado.ok = push.ok && pull.ok;
    if (!push.error.isEmpty()) {
        resultado.error = push.error;
    } else if (!pull.error.isEmpty()) {
        resultado.error = pull.error;
    }
    return resultado;
}

ResultadoSync SyncEngine::empujar(int limite)
{
    ResultadoSync resultado;
    resultado.ok = true;

    const std::vector<EventoSalida> eventos = outbox_.pendientes(limite);
    if (eventos.empty()) {
        return resultado;
    }

    const qint64 ahora = QDateTime::currentMSecsSinceEpoch();
    for (const EventoSalida& evento : eventos) {
        // Sin transacción explícita: el id determinista del evento hace que
        // reintentar tras un corte sea inocuo y funciona igual sobre HTTP.
        const ExecResult publicado = remoto_.execute(
            QStringLiteral("INSERT INTO sync_change_log "
                           "(event_uuid, entidad, entidad_uuid, operacion, payload, "
                           " updated_at, origen_nodo, aplicado_at) "
                           "VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
                           "ON CONFLICT(event_uuid) DO NOTHING"),
            {QVariant(evento.event_uuid), QVariant(evento.entidad),
             QVariant(evento.entidad_uuid), QVariant(evento.operacion),
             QVariant(evento.payload), QVariant::fromValue(evento.updated_at),
             QVariant(nodo_uuid_), QVariant::fromValue(ahora)});
        if (!publicado.ok) {
            outbox_.marcarError(evento, QStringLiteral("push rechazado"));
            resultado.ok = false;
            resultado.error = QStringLiteral("no se pudo publicar %1/%2")
                                  .arg(evento.entidad, evento.entidad_uuid);
            return resultado;
        }

        if (!outbox_.marcarEnviado(evento)) {
            resultado.error = QStringLiteral("evento publicado pero no marcado: %1")
                                  .arg(evento.event_uuid);
        }
        ++resultado.empujados;
    }

    local_.execute(
        QStringLiteral("UPDATE sync_meta SET ultimo_push_at = ?, actualizado_at = ? WHERE id = 1"),
        {QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    return resultado;
}

ResultadoSync SyncEngine::jalar(int limite)
{
    ResultadoSync resultado;
    resultado.ok = true;

    const auto meta = local_.query(
        QStringLiteral("SELECT ultimo_pull_id FROM sync_meta WHERE id = 1"));
    qint64 alto = 0;
    if (!meta.empty() && !meta.front().values.empty()) {
        alto = meta.front().values.front().toLongLong();
    }

    const auto filas = remoto_.query(
        QStringLiteral("SELECT id, event_uuid, entidad, entidad_uuid, operacion, payload, "
                       "updated_at FROM sync_change_log "
                       "WHERE id > ? AND origen_nodo <> ? ORDER BY id LIMIT ?"),
        {QVariant::fromValue(alto), QVariant(nodo_uuid_), QVariant::fromValue(limite)});

    qint64 primerAplazado = -1;
    qint64 ultimoVisto = alto;

    for (const QueryRow& fila : filas) {
        if (fila.values.size() != 7) {
            continue;
        }
        EventoEntrante evento;
        evento.id = fila.values.at(0).toLongLong();
        evento.event_uuid = fila.values.at(1).toString();
        evento.entidad = fila.values.at(2).toString();
        evento.entidad_uuid = fila.values.at(3).toString();
        evento.operacion = fila.values.at(4).toString();
        evento.payload = fila.values.at(5).toString();
        evento.updated_at = fila.values.at(6).toLongLong();
        ultimoVisto = std::max(ultimoVisto, evento.id);

        if (eventoAplicado(local_, evento.event_uuid)) {
            continue;
        }

        const EntidadSync* entidad = entidadSync(evento.entidad);
        if (entidad == nullptr) {
            if (primerAplazado < 0) {
                primerAplazado = evento.id;
            }
            ++resultado.aplazados;
            resultado.error = QStringLiteral("entidad desconocida: %1").arg(evento.entidad);
            continue;
        }

        QString error;
        bool aplicado = false;
        if (evento.operacion == QStringLiteral("delete")) {
            aplicado = aplicarDelete(local_, *entidad, evento.entidad_uuid,
                                     evento.updated_at, &error);
        } else if (evento.operacion == QStringLiteral("upsert")) {
            QJsonParseError parse_error;
            const QJsonDocument documento =
                QJsonDocument::fromJson(evento.payload.toUtf8(), &parse_error);
            if (parse_error.error == QJsonParseError::NoError && documento.isObject()) {
                aplicado = aplicarUpsert(local_, *entidad, documento.object(), &error);
            } else {
                error = QStringLiteral("payload inválido");
            }
        } else {
            error = QStringLiteral("operación desconocida: %1").arg(evento.operacion);
        }

        if (!aplicado) {
            if (primerAplazado < 0) {
                primerAplazado = evento.id;
            }
            ++resultado.aplazados;
            if (!error.isEmpty()) {
                resultado.error = error;
            }
            continue;
        }

        const qint64 ahora = QDateTime::currentMSecsSinceEpoch();
        local_.execute(
            QStringLiteral("INSERT INTO sync_aplicado (event_uuid, aplicado_at) VALUES (?, ?) "
                           "ON CONFLICT(event_uuid) DO NOTHING"),
            {QVariant(evento.event_uuid), QVariant::fromValue(ahora)});
        local_.execute(
            QStringLiteral("INSERT INTO sync_change_log "
                           "(event_uuid, entidad, entidad_uuid, operacion, payload, "
                           " updated_at, origen_nodo, aplicado_at) "
                           "VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
                           "ON CONFLICT(event_uuid) DO NOTHING"),
            {QVariant(evento.event_uuid), QVariant(evento.entidad),
             QVariant(evento.entidad_uuid), QVariant(evento.operacion),
             QVariant(evento.payload), QVariant::fromValue(evento.updated_at),
             QVariant(QStringLiteral("remoto")), QVariant::fromValue(ahora)});
        ++resultado.aplicados;
    }

    const qint64 nuevoAlto = (primerAplazado >= 0) ? primerAplazado - 1 : ultimoVisto;
    if (nuevoAlto > alto) {
        const qint64 ahora = QDateTime::currentMSecsSinceEpoch();
        local_.execute(
            QStringLiteral("UPDATE sync_meta SET ultimo_pull_id = ?, ultimo_pull_at = ?, "
                           "actualizado_at = ? WHERE id = 1"),
            {QVariant::fromValue(nuevoAlto), QVariant::fromValue(ahora),
             QVariant::fromValue(ahora)});
    }

    if (resultado.aplazados > 0) {
        resultado.ok = false;
    }
    return resultado;
}

bool SyncEngine::registrarCambio(const QString& entidad, const QString& uuid)
{
    return outbox_.registrar(entidad, uuid);
}

} // namespace fuzzy::core
