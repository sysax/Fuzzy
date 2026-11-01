#pragma once

#include "fuzzy/core/AuthService.h"
#include "fuzzy/core/Permisos.h"
#include "fuzzy/core/Politicas.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace fuzzy::desktop {

// Estado de acceso del escritorio: bootstrap, login, cambio obligatorio de la
// contraseña temporal y cierre de sesión. Toda la lógica vive en AuthService;
// esta clase solo la adapta a QML.
class SessionController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool hayInstalacion READ hayInstalacion NOTIFY instalacionCambiada)
    Q_PROPERTY(bool autenticado READ autenticado NOTIFY sesionCambiada)
    Q_PROPERTY(bool debeCambiarPassword READ debeCambiarPassword NOTIFY sesionCambiada)
    Q_PROPERTY(QString nombre READ nombre NOTIFY sesionCambiada)
    Q_PROPERTY(QString correo READ correo NOTIFY sesionCambiada)
    Q_PROPERTY(QString rol READ rol NOTIFY sesionCambiada)
    Q_PROPERTY(QString usuarioUuid READ usuarioUuid NOTIFY sesionCambiada)
    Q_PROPERTY(QString clinicaUuid READ clinicaUuid NOTIFY sesionCambiada)
    Q_PROPERTY(QString nombreClinica READ nombreClinica NOTIFY sesionCambiada)
    Q_PROPERTY(QStringList permisos READ permisos NOTIFY sesionCambiada)
    Q_PROPERTY(QString error READ error NOTIFY errorCambiado)
    Q_PROPERTY(QString mensaje READ mensaje NOTIFY mensajeCambiado)

public:
    explicit SessionController(fuzzy::core::AuthService& auth, QObject* parent = nullptr);

    bool hayInstalacion() const { return hayInstalacion_; }
    bool autenticado() const { return sesion_.rol != fuzzy::core::Rol::Desconocido; }
    bool debeCambiarPassword() const { return debeCambiarPassword_; }
    QString nombre() const { return nombre_; }
    QString correo() const { return correo_; }
    QString rol() const { return fuzzy::core::AuthService::nombreRol(sesion_.rol); }
    QString usuarioUuid() const { return sesion_.cuenta_id; }
    QString clinicaUuid() const { return sesion_.clinica_id; }
    QString nombreClinica() const { return nombreClinica_; }
    QString error() const { return error_; }
    QString mensaje() const { return mensaje_; }

    // Nombres de los permisos del rol actual. Es una propiedad con NOTIFY para
    // que las vistas puedan enlazarla: `puede()` es una invocación y un binding
    // que solo la llama no se reevalúa al iniciar o cerrar sesión.
    QStringList permisos() const;

    Q_INVOKABLE void bootstrap(const QString& nombreClinica, const QString& ciudad,
                               const QString& nombreAdmin, const QString& correo,
                               const QString& password);
    Q_INVOKABLE void iniciarSesion(const QString& correo, const QString& password);
    Q_INVOKABLE void cambiarPassword(const QString& actual, const QString& nueva);
    Q_INVOKABLE void cerrarSesion();
    Q_INVOKABLE void limpiarMensajes();

    // Permiso por nombre (`VerPanel`, `GestionarCitas`, …) para ocultar
    // acciones que el rol no puede ejecutar.
    Q_INVOKABLE bool puede(const QString& permiso) const;

signals:
    void sesionCambiada();
    void instalacionCambiada();
    void errorCambiado();
    void mensajeCambiado();

private:
    void aplicar(const fuzzy::core::ResultadoAuth& resultado);
    void refrescarClinica();

    fuzzy::core::AuthService& auth_;
    fuzzy::core::Sesion sesion_;
    fuzzy::core::Politicas politicas_;
    bool hayInstalacion_ = false;
    bool debeCambiarPassword_ = false;
    QString nombre_;
    QString correo_;
    QString nombreClinica_;
    QString error_;
    QString mensaje_;
};

} // namespace fuzzy::desktop
