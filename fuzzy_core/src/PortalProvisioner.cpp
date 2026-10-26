#include "fuzzy/core/PortalProvisioner.h"

#include "fuzzy/core/MigrationRunner.h"
#include "fuzzy/core/PortalBaseStore.h"
#include "fuzzy/core/Uuid.h"

#include <QDateTime>
#include <QUuid>
#include <QVariant>

namespace fuzzy::core {

namespace {

QString urlDePortal(const QString& url, const QString& portal_uuid)
{
    return url.isEmpty() ? QStringLiteral("local://portal/%1").arg(portal_uuid) : url;
}

} // namespace

PortalProvisioner::PortalProvisioner(IQueryExecutor& clinica, PortalTokenService& tokens,
                                     PortalBaseStore& bases)
    : clinica_(clinica), tokens_(tokens), bases_(bases)
{
}

std::optional<PortalProvisioner::Creado> PortalProvisioner::crear(
    const QString& clinica_uuid, qint64 cuenta_id, qint64 cliente_id,
    const QString& url, const QString& token_recibido)
{
    if (clinica_uuid.isEmpty() || cuenta_id <= 0 || cliente_id <= 0) {
        return std::nullopt;
    }

    const auto contexto = clinica_.query(
        QStringLiteral("SELECT c.nombre, cu.uuid FROM clinicas c "
                       "JOIN cuentas cu ON cu.id = ? "
                       "JOIN clientes cl ON cl.id = cu.cliente_id "
                       "WHERE c.uuid = ? AND cl.id = ? "
                       "AND c.deleted_at IS NULL AND cu.deleted_at IS NULL "
                       "AND cl.deleted_at IS NULL"),
        {QVariant::fromValue(cuenta_id), QVariant(clinica_uuid),
         QVariant::fromValue(cliente_id)});
    if (contexto.size() != 1 || contexto.front().values.size() != 2) {
        return std::nullopt;
    }
    const QString clinica_nombre = contexto.front().values.at(0).toString();
    const QString cuenta_uuid = contexto.front().values.at(1).toString();
    if (cuenta_uuid.isEmpty()) {
        return std::nullopt;
    }

    const QString token = token_recibido.isEmpty()
        ? QUuid::createUuid().toString(QUuid::WithoutBraces)
        : token_recibido;
    const QString urlRegistro = url.isEmpty() ? QStringLiteral("local://portal") : url;
    const QString portal_uuid =
        tokens_.emitir(clinica_uuid, cuenta_id, cliente_id, urlRegistro, token);
    if (portal_uuid.isEmpty()) {
        return std::nullopt;
    }

    auto limpiar = [this, &token] {
        // Si la base no se pudo preparar, el registro tampoco debe sobrevivir.
        tokens_.borrar(token);
    };

    if (!bases_.crear(portal_uuid)) {
        limpiar();
        return std::nullopt;
    }

    std::unique_ptr<IQueryExecutor> portal = bases_.abrir(portal_uuid);
    if (portal == nullptr) {
        limpiar();
        return std::nullopt;
    }

    const MigrationRunner runner = MigrationRunner::portal();
    if (!runner.aplicar(*portal)) {
        limpiar();
        return std::nullopt;
    }

    const ExecResult meta = portal->execute(
        QStringLiteral("INSERT INTO portal_meta "
                       "(id, esquema_version, cuenta_uuid, clinica_uuid, generado_at, "
                       " clinica_nombre) "
                       "VALUES (1, ?, ?, ?, ?, ?)"),
        {QVariant(runner.versionActual().toInt()), QVariant(cuenta_uuid),
         QVariant(clinica_uuid),
         QVariant::fromValue(QDateTime::currentMSecsSinceEpoch()),
         QVariant(clinica_nombre)});
    if (!meta.ok) {
        limpiar();
        return std::nullopt;
    }

    Creado creado;
    creado.portal_uuid = portal_uuid;
    creado.token = token;
    creado.url = urlDePortal(url, portal_uuid);
    if (url.isEmpty()) {
        clinica_.execute(QStringLiteral("UPDATE portales SET url = ? WHERE uuid = ?"),
                         {QVariant(creado.url), QVariant(portal_uuid)});
    }
    creado.huella = PortalTokenService::huella(token);
    creado.qr = QJsonObject{
        {QStringLiteral("v"), 1},
        {QStringLiteral("url"), creado.url},
        {QStringLiteral("token"), token},
        {QStringLiteral("portal_uuid"), portal_uuid},
        {QStringLiteral("clinica"), clinica_uuid},
        {QStringLiteral("huella"), creado.huella},
    };
    return creado;
}

bool PortalProvisioner::migrar(const QString& portal_uuid)
{
    std::unique_ptr<IQueryExecutor> portal = bases_.abrir(portal_uuid);
    if (portal == nullptr) {
        return false;
    }

    const MigrationRunner runner = MigrationRunner::portal();
    if (!runner.aplicar(*portal)) {
        return false;
    }

    const ExecResult actualizada = portal->execute(
        QStringLiteral("UPDATE portal_meta SET esquema_version = ? WHERE id = 1"),
        {QVariant(runner.versionActual().toInt())});
    return actualizada.ok;
}

} // namespace fuzzy::core
