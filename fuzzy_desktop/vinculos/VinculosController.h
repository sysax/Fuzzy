#pragma once

#include "fuzzy/core/PortalBaseStore.h"
#include "fuzzy/core/PortalProvisioner.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace fuzzy::desktop {

// Vínculos con el portal del dueño: aprobar el acceso de una ficha, emitir el
// token de emparejamiento y revocar un portal. La base y el esquema del portal
// los prepara PortalProvisioner (fase 4); aquí solo se orquesta.
class VinculosController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList vinculos READ vinculos NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList sinCuenta READ sinCuenta NOTIFY datosCambiados)
    Q_PROPERTY(QString error READ error NOTIFY errorCambiado)
    Q_PROPERTY(QString mensaje READ mensaje NOTIFY mensajeCambiado)
    Q_PROPERTY(QVariantMap emitido READ emitido NOTIFY emitidoCambiado)
    Q_PROPERTY(QString qr READ qr NOTIFY emitidoCambiado)

public:
    VinculosController(fuzzy::core::IQueryExecutor& db,
                       fuzzy::core::PortalProvisioner& provisioner,
                       fuzzy::core::PortalBaseStore& bases,
                       QObject* parent = nullptr);

    QVariantList vinculos() const { return vinculos_; }
    QVariantList sinCuenta() const { return sinCuenta_; }
    QString error() const { return error_; }
    QString mensaje() const { return mensaje_; }
    QVariantMap emitido() const { return emitido_; }
    QString qr() const { return qr_; }

    Q_INVOKABLE void cargar();
    Q_INVOKABLE void limpiarMensajes();
    Q_INVOKABLE void limpiarEmitido();
    Q_INVOKABLE void aprobar(const QString& clienteUuid, const QString& correo,
                             const QString& password);
    Q_INVOKABLE void revocar(const QString& portalUuid);
    Q_INVOKABLE void eliminar(const QString& portalUuid);

signals:
    void datosCambiados();
    void errorCambiado();
    void mensajeCambiado();
    void emitidoCambiado();

private:
    void recargar();
    void fallo(const QString& texto);
    void exito(const QString& texto);
    QString clinicaUuid() const;

    fuzzy::core::IQueryExecutor& db_;
    fuzzy::core::PortalProvisioner& provisioner_;
    fuzzy::core::PortalBaseStore& bases_;
    QVariantList vinculos_;
    QVariantList sinCuenta_;
    QString error_;
    QString mensaje_;
    QVariantMap emitido_;
    QString qr_;
};

} // namespace fuzzy::desktop
