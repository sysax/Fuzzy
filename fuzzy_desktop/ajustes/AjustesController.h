#pragma once

#include "fuzzy/core/AuthService.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace fuzzy::desktop {

// Ajustes de la clínica y gestión del equipo. Las reglas (último Admin,
// correo único, contraseña temporal de 72 h) viven en AuthService.
class AjustesController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString adminUuid READ adminUuid WRITE setAdminUuid NOTIFY adminCambiado)
    Q_PROPERTY(QVariantMap clinica READ clinica NOTIFY clinicaCambiada)
    Q_PROPERTY(QVariantList equipo READ equipo NOTIFY equipoCambiado)
    Q_PROPERTY(QVariantList roles READ roles CONSTANT)
    Q_PROPERTY(QString passwordTemporal READ passwordTemporal NOTIFY equipoCambiado)
    Q_PROPERTY(QString error READ error NOTIFY errorCambiado)
    Q_PROPERTY(QString mensaje READ mensaje NOTIFY mensajeCambiado)

public:
    explicit AjustesController(fuzzy::core::AuthService& auth, QObject* parent = nullptr);

    QString adminUuid() const { return adminUuid_; }
    void setAdminUuid(const QString& uuid);
    QVariantMap clinica() const { return clinica_; }
    QVariantList equipo() const { return equipo_; }
    QVariantList roles() const;
    QString passwordTemporal() const { return passwordTemporal_; }
    QString error() const { return error_; }
    QString mensaje() const { return mensaje_; }

    Q_INVOKABLE void cargar();
    Q_INVOKABLE void limpiarMensajes();
    Q_INVOKABLE void limpiarPasswordTemporal();
    Q_INVOKABLE void guardarClinica(const QString& nombre, const QString& ciudad,
                                    const QString& direccion, const QString& telefono);
    Q_INVOKABLE void crearUsuario(const QString& nombre, const QString& correo,
                                  const QString& rol);
    Q_INVOKABLE void actualizarUsuario(const QString& usuarioUuid, const QString& nombre,
                                       const QString& correo, const QString& rol, bool activo);
    Q_INVOKABLE void restablecerPassword(const QString& usuarioUuid);

signals:
    void adminCambiado();
    void clinicaCambiada();
    void equipoCambiado();
    void errorCambiado();
    void mensajeCambiado();

private:
    void recargarClinica();
    void recargarEquipo();
    void fallo(const QString& texto);
    void exito(const QString& texto);

    fuzzy::core::AuthService& auth_;
    QString adminUuid_;
    QVariantMap clinica_;
    QVariantList equipo_;
    QString passwordTemporal_;
    QString error_;
    QString mensaje_;
};

} // namespace fuzzy::desktop
