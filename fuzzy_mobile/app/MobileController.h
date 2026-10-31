#pragma once

#include "fuzzy/core/DirectorioCliente.h"
#include "fuzzy/core/MovilStore.h"
#include "fuzzy/core/PortalCliente.h"

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <memory>

namespace fuzzy::core {
class IQueryExecutor;
}

namespace fuzzy::mobile {

// Ejecutor abierto contra una base de portal y su sonda de salud. La sonda es
// opcional: un ejecutor local de pruebas no la necesita.
struct ConexionPortal
{
    std::unique_ptr<fuzzy::core::IQueryExecutor> ejecutor;
    fuzzy::core::PortalCliente::Sonda sonda;
};

// Adaptador entre los servicios de `fuzzy_core` y QML. Toda la lógica de
// negocio (cifrado, LWW, cola, validación) vive en el core; aquí solo se
// orquesta y se expone como propiedades enlazables.
class MobileController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool configurada READ configurada NOTIFY bloqueoCambiado)
    Q_PROPERTY(bool desbloqueada READ desbloqueada NOTIFY bloqueoCambiado)
    Q_PROPERTY(bool hayPortales READ hayPortales NOTIFY portalesCambiados)
    Q_PROPERTY(bool modoAvion READ modoAvion WRITE setModoAvion NOTIFY modoAvionCambiado)
    Q_PROPERTY(bool sinRed READ sinRed NOTIFY syncCambiado)
    Q_PROPERTY(QString portalActivo READ portalActivo NOTIFY portalCambiado)
    Q_PROPERTY(QString nombrePortal READ nombrePortal NOTIFY portalCambiado)
    Q_PROPERTY(QString mascotaActiva READ mascotaActiva NOTIFY mascotaCambiada)
    Q_PROPERTY(QString estadoSync READ estadoSync NOTIFY syncCambiado)
    Q_PROPERTY(QString error READ error NOTIFY errorCambiado)
    Q_PROPERTY(QString mensaje READ mensaje NOTIFY mensajeCambiado)
    Q_PROPERTY(QString huellaPendiente READ huellaPendiente NOTIFY emparejamientoCambiado)
    Q_PROPERTY(QString clinicaPendiente READ clinicaPendiente NOTIFY emparejamientoCambiado)
    Q_PROPERTY(int pendientes READ pendientes NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList portales READ portales NOTIFY portalesCambiados)
    Q_PROPERTY(QVariantList perfil READ perfil NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList mascotas READ mascotas NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList citas READ citas NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList solicitudes READ solicitudes NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList historial READ historial NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList directorio READ directorio NOTIFY directorioCambiado)
    Q_PROPERTY(QVariantList altas READ altas NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList registroPendiente READ registroPendiente NOTIFY datosCambiados)
    Q_PROPERTY(QString busquedaDirectorio READ busquedaDirectorio NOTIFY directorioCambiado)

public:
    using AbridorPortal = std::function<ConexionPortal(const fuzzy::core::PortalConocido&,
                                                       const QString& token)>;
    using AbridorBuzon = std::function<std::unique_ptr<fuzzy::core::IQueryExecutor>(
        const fuzzy::core::ClinicaPublica&)>;

    explicit MobileController(fuzzy::core::MovilStore& store,
                              AbridorPortal abridorPortal = {},
                              AbridorBuzon abridorBuzon = {},
                              QObject* parent = nullptr);

    bool configurada() const { return store_.tieneClave(); }
    bool desbloqueada() const { return store_.desbloqueado(); }
    bool hayPortales() const;
    bool modoAvion() const { return modo_avion_; }
    bool sinRed() const { return sin_red_; }
    QString portalActivo() const { return portal_activo_; }
    QString nombrePortal() const;
    QString mascotaActiva() const { return mascota_activa_; }
    QString estadoSync() const { return estado_sync_; }
    QString error() const { return error_; }
    QString mensaje() const { return mensaje_; }
    QString huellaPendiente() const { return huella_pendiente_; }
    QString clinicaPendiente() const { return clinica_pendiente_; }
    QString busquedaDirectorio() const { return busqueda_; }
    int pendientes() const;

    QVariantList portales() const;
    QVariantList perfil() const;
    QVariantList mascotas() const;
    QVariantList citas() const;
    QVariantList solicitudes() const;
    QVariantList historial() const;
    QVariantList directorio() const;
    QVariantList altas() const;
    QVariantList registroPendiente() const;

    Q_INVOKABLE void configurarClave(const QString& password);
    Q_INVOKABLE void desbloquear(const QString& password);
    Q_INVOKABLE void bloquear();
    Q_INVOKABLE void cerrarSesion();
    Q_INVOKABLE void setModoAvion(bool activo);
    Q_INVOKABLE void limpiarMensajes();

    Q_INVOKABLE void seleccionarPortal(const QString& portal_uuid);
    Q_INVOKABLE void olvidarPortal(const QString& portal_uuid);
    Q_INVOKABLE void abrirMascota(const QString& portal_uuid, const QString& mascota_uuid);
    Q_INVOKABLE void cerrarMascota();

    Q_INVOKABLE bool leerQr(const QString& contenido);
    Q_INVOKABLE void confirmarEmparejamiento();
    Q_INVOKABLE void cancelarEmparejamiento();

    Q_INVOKABLE void sincronizar();
    Q_INVOKABLE void limpiarCache();

    Q_INVOKABLE void cargarDirectorio(const QString& json);
    Q_INVOKABLE void buscarDirectorio(const QString& texto);
    Q_INVOKABLE void enviarAlta(const QString& clinica_uuid, const QString& nombre,
                                const QString& correo, const QString& telefono,
                                const QString& mascota, const QString& motivo,
                                const QString& franja);
    Q_INVOKABLE void reintentarAltas();

    Q_INVOKABLE void registrarPerfil(const QString& nombre, const QString& correo,
                                     const QString& telefono, const QString& direccion,
                                     const QString& dni);
    Q_INVOKABLE void registrarMascota(const QString& nombre, const QString& especie,
                                      const QString& raza, const QString& sexo, double peso);

    Q_INVOKABLE void solicitarCita(const QString& mascota_uuid, const QString& tipo_servicio,
                                   const QString& fecha, const QString& hora,
                                   const QString& motivo);
    Q_INVOKABLE void actualizarPerfil(const QString& nombre, const QString& correo,
                                      const QString& telefono, const QString& direccion,
                                      const QString& dni);

signals:
    void bloqueoCambiado();
    void portalesCambiados();
    void portalCambiado();
    void mascotaCambiada();
    void datosCambiados();
    void syncCambiado();
    void modoAvionCambiado();
    void directorioCambiado();
    void emparejamientoCambiado();
    void errorCambiado();
    void mensajeCambiado();

private:
    void refrescarTodo();
    void elegirPortalSiFalta();
    void aplicarEmparejamiento(const QByteArray& contenido);
    void enviarAltaPendiente(const fuzzy::core::AltaLocal& alta);
    const fuzzy::core::ClinicaPublica* clinica(const QString& uuid) const;
    QVariantMap portalMap(const fuzzy::core::PortalConocido& portal) const;
    QString nombreMascota(const QString& portal_uuid, const QString& mascota_uuid) const;
    ConexionPortal abrirPortal(const fuzzy::core::PortalConocido& portal) const;

    fuzzy::core::MovilStore& store_;
    AbridorPortal abridor_portal_;
    AbridorBuzon abridor_buzon_;

    QString portal_activo_;
    QString mascota_activa_;
    QString error_;
    QString mensaje_;
    QString estado_sync_ = QStringLiteral("sin_sincronizar");
    QString huella_pendiente_;
    QString clinica_pendiente_;
    QString qr_pendiente_;
    QString busqueda_;
    QList<fuzzy::core::ClinicaPublica> directorio_;
    bool modo_avion_ = false;
    bool sin_red_ = false;
};

} // namespace fuzzy::mobile
