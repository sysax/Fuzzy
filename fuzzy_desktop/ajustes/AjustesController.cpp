#include "ajustes/AjustesController.h"

#include "common/Consulta.h"

#include <QVariant>

namespace fuzzy::desktop {

AjustesController::AjustesController(fuzzy::core::AuthService& auth, QObject* parent)
    : QObject(parent), auth_(auth)
{
    recargarClinica();
    recargarEquipo();
}

void AjustesController::setAdminUuid(const QString& uuid)
{
    if (adminUuid_ == uuid) {
        return;
    }
    adminUuid_ = uuid;
    emit adminCambiado();
}

QVariantList AjustesController::roles() const
{
    QVariantList lista;
    for (const QString& nombre : {QStringLiteral("Admin"), QStringLiteral("Veterinario"),
                                  QStringLiteral("Recepcion"), QStringLiteral("Estilista")}) {
        QVariantMap rol;
        rol.insert(QStringLiteral("nombre"), nombre);
        lista.append(rol);
    }
    return lista;
}

void AjustesController::recargarClinica()
{
    const auto datos = auth_.clinica();
    clinica_.clear();
    clinica_.insert(QStringLiteral("uuid"), datos.uuid);
    clinica_.insert(QStringLiteral("nombre"), datos.nombre);
    clinica_.insert(QStringLiteral("ciudad"), datos.ciudad);
    clinica_.insert(QStringLiteral("direccion"), datos.direccion);
    clinica_.insert(QStringLiteral("telefono"), datos.telefono);
    emit clinicaCambiada();
}

void AjustesController::recargarEquipo()
{
    equipo_.clear();
    for (const auto& usuario : auth_.equipo()) {
        QVariantMap item;
        item.insert(QStringLiteral("uuid"), usuario.uuid);
        item.insert(QStringLiteral("nombre"), usuario.nombre);
        item.insert(QStringLiteral("correo"), usuario.correo);
        item.insert(QStringLiteral("rol"), usuario.rol);
        item.insert(QStringLiteral("activo"), usuario.activo);
        item.insert(QStringLiteral("debeCambiarPassword"), usuario.debeCambiarPassword);
        item.insert(QStringLiteral("ultimoAcceso"), fechaHoraTexto(usuario.ultimoAcceso));
        equipo_.append(item);
    }
    emit equipoCambiado();
}

void AjustesController::fallo(const QString& texto)
{
    error_ = texto;
    mensaje_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

void AjustesController::exito(const QString& texto)
{
    mensaje_ = texto;
    error_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

void AjustesController::cargar()
{
    recargarClinica();
    recargarEquipo();
}

void AjustesController::limpiarMensajes()
{
    error_.clear();
    mensaje_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

void AjustesController::limpiarPasswordTemporal()
{
    passwordTemporal_.clear();
    emit equipoCambiado();
}

void AjustesController::guardarClinica(const QString& nombre, const QString& ciudad,
                                       const QString& direccion, const QString& telefono)
{
    const auto resultado =
        auth_.actualizarClinica(adminUuid_, nombre, ciudad, direccion, telefono);
    if (!resultado.ok) {
        fallo(resultado.error);
        return;
    }
    recargarClinica();
    exito(QStringLiteral("Datos de la clínica guardados."));
}

void AjustesController::crearUsuario(const QString& nombre, const QString& correo,
                                     const QString& rol)
{
    const auto resultado = auth_.crearUsuario(adminUuid_, nombre, correo, rol);
    if (!resultado.ok) {
        fallo(resultado.error);
        return;
    }
    passwordTemporal_ = resultado.passwordTemporal;
    recargarEquipo();
    exito(QStringLiteral("Cuenta creada. Entrega la contraseña temporal antes de 72 horas."));
}

void AjustesController::actualizarUsuario(const QString& usuarioUuid, const QString& nombre,
                                          const QString& correo, const QString& rol,
                                          bool activo)
{
    const auto resultado =
        auth_.actualizarUsuario(adminUuid_, usuarioUuid, nombre, correo, rol, activo);
    if (!resultado.ok) {
        fallo(resultado.error);
        return;
    }
    recargarEquipo();
    exito(QStringLiteral("Cuenta actualizada."));
}

void AjustesController::restablecerPassword(const QString& usuarioUuid)
{
    const auto resultado = auth_.restablecerPassword(adminUuid_, usuarioUuid);
    if (!resultado.ok) {
        fallo(resultado.error);
        return;
    }
    passwordTemporal_ = resultado.passwordTemporal;
    recargarEquipo();
    exito(QStringLiteral("Contraseña temporal emitida; vence en 72 horas."));
}

} // namespace fuzzy::desktop
