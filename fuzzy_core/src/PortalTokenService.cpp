#include "fuzzy/core/PortalTokenService.h"

#include "fuzzy/core/PortalBaseStore.h"

#include <QCryptographicHash>
#include <QUuid>
#include <QVariant>

namespace fuzzy::core {

namespace {

const QString kAhora = QStringLiteral("CAST(strftime('%s', 'now') AS INTEGER) * 1000");

// La base de la clínica solo custodia la huella SHA-256 del token (D27).
QString hashToken(const QString& token)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(token.toUtf8(), QCryptographicHash::Sha256).toHex());
}

} // namespace

PortalTokenService::PortalTokenService(IQueryExecutor& executor, PortalBaseStore* bases)
    : executor_(executor), bases_(bases)
{
}

QString PortalTokenService::emitir(const QString& clinica_uuid, qint64 cuenta_id,
                                   qint64 cliente_id, const QString& url, const QString& token)
{
    if (clinica_uuid.isEmpty() || url.isEmpty() || token.isEmpty()) {
        return {};
    }

    // El ejecutor debe ser el de esta clínica: un portal nunca se registra en
    // la base de otra sede.
    const auto clinica = executor_.query(
        QStringLiteral("SELECT 1 FROM clinicas WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(clinica_uuid)});
    if (clinica.size() != 1) {
        return {};
    }

    // La cuenta y la ficha deben ser la misma persona dentro de esta clínica.
    const auto vinculo = executor_.query(
        QStringLiteral("SELECT 1 FROM cuentas c "
                       "JOIN clientes cl ON cl.id = c.cliente_id "
                       "WHERE c.id = ? AND cl.id = ? "
                       "AND c.deleted_at IS NULL AND cl.deleted_at IS NULL"),
        {QVariant::fromValue(cuenta_id), QVariant::fromValue(cliente_id)});
    if (vinculo.size() != 1) {
        return {};
    }

    const QString portal_uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const ExecResult insertado = executor_.execute(
        QStringLiteral("INSERT INTO portales "
                       "(uuid, token_hash, cuenta_id, cliente_id, url, estado, pareado_at) "
                       "VALUES (?, ?, ?, ?, ?, 'activo', ") + kAhora + QStringLiteral(")"),
        {QVariant(portal_uuid), QVariant(hashToken(token)),
         QVariant::fromValue(cuenta_id), QVariant::fromValue(cliente_id), QVariant(url)});
    if (insertado.affected != 1) {
        return {};
    }
    return portal_uuid;
}

bool PortalTokenService::verificar(const QString& token, const QString& clinica_uuid) const
{
    if (token.isEmpty() || clinica_uuid.isEmpty()) {
        return false;
    }

    // `clinicas` tiene una sola fila por base; el EXISTS es lo que impide que
    // el token de un portal se valide contra la base de otra clínica.
    const auto filas = executor_.query(
        QStringLiteral("SELECT p.estado FROM portales p "
                       "WHERE p.token_hash = ? AND p.deleted_at IS NULL "
                       "AND EXISTS (SELECT 1 FROM clinicas c "
                       "            WHERE c.uuid = ? AND c.deleted_at IS NULL)"),
        {QVariant(hashToken(token)), QVariant(clinica_uuid)});

    if (filas.size() != 1 || filas.front().values.size() != 1) {
        return false;
    }
    return filas.front().values.front().toString() == QStringLiteral("activo");
}

bool PortalTokenService::rotar(const QString& token_actual, const QString& token_nuevo)
{
    if (token_actual.isEmpty() || token_nuevo.isEmpty() || token_actual == token_nuevo) {
        return false;
    }

    const ExecResult rotado = executor_.execute(
        QStringLiteral("UPDATE portales SET token_hash = ?, updated_at = ") + kAhora
            + QStringLiteral(" WHERE token_hash = ? AND estado = 'activo' "
                             "AND deleted_at IS NULL"),
        {QVariant(hashToken(token_nuevo)), QVariant(hashToken(token_actual))});
    return rotado.affected == 1;
}

bool PortalTokenService::revocar(const QString& token)
{
    if (token.isEmpty()) {
        return false;
    }

    const ExecResult revocado = executor_.execute(
        QStringLiteral("UPDATE portales SET estado = 'revocado', updated_at = ") + kAhora
            + QStringLiteral(" WHERE token_hash = ? AND estado = 'activo' "
                             "AND deleted_at IS NULL"),
        {QVariant(hashToken(token))});
    return revocado.affected == 1;
}

bool PortalTokenService::borrar(const QString& token)
{
    if (token.isEmpty()) {
        return false;
    }

    // El uuid del portal es necesario para destruir su base: no se deriva del
    // token porque el token rota y el uuid no.
    const auto filas = executor_.query(
        QStringLiteral("SELECT uuid FROM portales WHERE token_hash = ?"),
        {QVariant(hashToken(token))});
    if (filas.size() != 1 || filas.front().values.size() != 1) {
        return false;
    }
    const QString portal_uuid = filas.front().values.front().toString();
    if (portal_uuid.isEmpty()) {
        return false;
    }

    // Primero se cierra el acceso (el token deja de verificar); después se
    // destruye la base. Si la base no se puede borrar, se devuelve false para
    // que el operador lo vea, pero el portal ya está denegado.
    const ExecResult borrado = executor_.execute(
        QStringLiteral("UPDATE portales SET estado = 'borrado', "
                       "deleted_at = COALESCE(deleted_at, ") + kAhora + QStringLiteral("), "
                       "updated_at = ") + kAhora
            + QStringLiteral(" WHERE token_hash = ?"),
        {QVariant(hashToken(token))});
    if (borrado.affected != 1) {
        return false;
    }

    return bases_ == nullptr || bases_->eliminar(portal_uuid);
}

QString PortalTokenService::huella(const QString& token)
{
    return hashToken(token).left(6).toUpper();
}

} // namespace fuzzy::core
