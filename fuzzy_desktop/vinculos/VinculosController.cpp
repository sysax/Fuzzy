#include "vinculos/VinculosController.h"

#include "common/Consulta.h"
#include "fuzzy/core/PasswordHasher.h"
#include "fuzzy/core/Uuid.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QVariant>

namespace fuzzy::desktop {

VinculosController::VinculosController(fuzzy::core::IQueryExecutor& db,
                                       fuzzy::core::PortalProvisioner& provisioner,
                                       fuzzy::core::PortalBaseStore& bases, QObject* parent)
    : QObject(parent), db_(db), provisioner_(provisioner), bases_(bases)
{
    recargar();
}

QString VinculosController::clinicaUuid() const
{
    const auto filas = db_.query(QStringLiteral("SELECT uuid FROM clinicas LIMIT 1"));
    return filas.empty() ? QString() : textoDeFila(filas.front(), 0);
}

void VinculosController::fallo(const QString& texto)
{
    error_ = texto;
    mensaje_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

void VinculosController::exito(const QString& texto)
{
    mensaje_ = texto;
    error_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

void VinculosController::recargar()
{
    const auto filas = db_.query(
        QStringLiteral("SELECT cl.uuid, cl.nombre_completo, cu.uuid, cu.correo, cu.estado, "
                       "p.uuid, p.estado, p.url, p.pareado_at, p.ultima_sync_at, "
                       "coalesce(p.token_hash, '') "
                       "FROM clientes cl "
                       "LEFT JOIN cuentas cu ON cu.cliente_id = cl.id AND cu.deleted_at IS NULL "
                       "LEFT JOIN portales p ON p.cuenta_id = cu.id AND p.deleted_at IS NULL "
                       "WHERE cl.deleted_at IS NULL ORDER BY cl.nombre_completo"));
    vinculos_.clear();
    for (const auto& fila : filas) {
        QVariantMap item;
        item.insert(QStringLiteral("clienteUuid"), textoDeFila(fila, 0));
        item.insert(QStringLiteral("cliente"), textoDeFila(fila, 1));
        item.insert(QStringLiteral("cuentaUuid"), textoDeFila(fila, 2));
        item.insert(QStringLiteral("correo"), textoDeFila(fila, 3));
        item.insert(QStringLiteral("estadoCuenta"), textoDeFila(fila, 4));
        item.insert(QStringLiteral("portalUuid"), textoDeFila(fila, 5));
        item.insert(QStringLiteral("estadoPortal"), textoDeFila(fila, 6));
        item.insert(QStringLiteral("url"), textoDeFila(fila, 7));
        item.insert(QStringLiteral("pareado"), fechaHoraTexto(enteroDeFila(fila, 8)));
        item.insert(QStringLiteral("ultimaSync"), fechaHoraTexto(enteroDeFila(fila, 9)));
        item.insert(QStringLiteral("emparejado"), !textoDeFila(fila, 8).isEmpty());
        item.insert(QStringLiteral("conToken"), !textoDeFila(fila, 10).isEmpty());
        vinculos_.append(item);
    }

    sinCuenta_ = listaDeFilas(
        db_.query(QStringLiteral("SELECT cl.uuid, cl.nombre_completo, "
                                 "coalesce(cl.correo, ''), coalesce(cl.telefono, '') "
                                 "FROM clientes cl WHERE cl.deleted_at IS NULL "
                                 "AND NOT EXISTS (SELECT 1 FROM cuentas cu "
                                 " WHERE cu.cliente_id = cl.id AND cu.deleted_at IS NULL) "
                                 "ORDER BY cl.nombre_completo")),
        {QStringLiteral("uuid"), QStringLiteral("nombre"), QStringLiteral("correo"),
         QStringLiteral("telefono")});
    emit datosCambiados();
}

void VinculosController::cargar()
{
    recargar();
}

void VinculosController::limpiarMensajes()
{
    error_.clear();
    mensaje_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

void VinculosController::limpiarEmitido()
{
    emitido_.clear();
    qr_.clear();
    emit emitidoCambiado();
}

void VinculosController::aprobar(const QString& clienteUuid, const QString& correo,
                                 const QString& password)
{
    const QString correoLimpio = correo.trimmed().toLower();
    if (clienteUuid.isEmpty() || correoLimpio.isEmpty()) {
        fallo(QStringLiteral("elige la ficha y escribe el correo del dueño"));
        return;
    }
    if (password.size() < 8) {
        fallo(QStringLiteral("la contraseña inicial debe tener al menos 8 caracteres"));
        return;
    }
    const auto cliente = db_.query(
        QStringLiteral("SELECT id FROM clientes WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(clienteUuid)});
    if (cliente.size() != 1) {
        fallo(QStringLiteral("la ficha del propietario no existe"));
        return;
    }
    const qint64 clienteId = enteroDeFila(cliente.front(), 0);

    const QString clinica = clinicaUuid();
    if (clinica.isEmpty()) {
        fallo(QStringLiteral("la instalación no tiene clínica"));
        return;
    }

    const qint64 ahora = QDateTime::currentMSecsSinceEpoch();
    const QString cuentaUuid = fuzzy::core::uuidNuevo();
    const auto alta = db_.execute(
        QStringLiteral("INSERT INTO cuentas "
                       "(uuid, cliente_id, correo, password_hash, estado, activo, "
                       " debe_cambiar_password, verificado_at, created_at, updated_at, "
                       " sync_state) "
                       "VALUES (?, ?, ?, ?, 'Activa', 1, 0, ?, ?, ?, 'pendiente')"),
        {QVariant(cuentaUuid), QVariant::fromValue(clienteId), QVariant(correoLimpio),
         QVariant(fuzzy::core::PasswordHasher::generar(password)),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!alta.ok) {
        fallo(QStringLiteral("no se pudo crear la cuenta del dueño"));
        return;
    }

    const auto creado = provisioner_.crear(clinica, alta.last_insert_id.toLongLong(),
                                           clienteId, QString(), QString());
    if (!creado.has_value()) {
        db_.execute(QStringLiteral("DELETE FROM cuentas WHERE uuid = ?"), {QVariant(cuentaUuid)});
        fallo(QStringLiteral("no se pudo crear el portal"));
        return;
    }

    emitido_.clear();
    emitido_.insert(QStringLiteral("portalUuid"), creado->portal_uuid);
    emitido_.insert(QStringLiteral("token"), creado->token);
    emitido_.insert(QStringLiteral("url"), creado->url);
    emitido_.insert(QStringLiteral("huella"), creado->huella);
    emitido_.insert(QStringLiteral("cliente"), correoLimpio);
    emit emitidoCambiado();

    recargar();
    exito(QStringLiteral("Vínculo aprobado. Entrega el QR al dueño."));
}

void VinculosController::revocar(const QString& portalUuid)
{
    const auto cambio = db_.execute(
        QStringLiteral("UPDATE portales SET estado = 'revocado', updated_at = ? "
                       "WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant::fromValue(QDateTime::currentMSecsSinceEpoch()), QVariant(portalUuid)});
    if (!cambio.ok || cambio.affected == 0) {
        fallo(QStringLiteral("el portal no existe o ya está revocado"));
        return;
    }
    recargar();
    exito(QStringLiteral("Portal revocado; el token deja de abrir la base."));
}

void VinculosController::eliminar(const QString& portalUuid)
{
    if (portalUuid.isEmpty()) {
        fallo(QStringLiteral("falta el portal"));
        return;
    }
    const auto cambio = db_.execute(
        QStringLiteral("UPDATE portales SET deleted_at = ?, updated_at = ?, estado = 'revocado' "
                       "WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant::fromValue(QDateTime::currentMSecsSinceEpoch()),
         QVariant::fromValue(QDateTime::currentMSecsSinceEpoch()), QVariant(portalUuid)});
    if (!cambio.ok || cambio.affected == 0) {
        fallo(QStringLiteral("el portal no existe"));
        return;
    }
    bases_.eliminar(portalUuid);
    recargar();
    exito(QStringLiteral("Portal eliminado y base borrada."));
}

} // namespace fuzzy::desktop
