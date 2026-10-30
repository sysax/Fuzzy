#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace fuzzy::desktop {

// Facturación y cierre de caja. El borrador vive en memoria hasta `emitir`,
// que escribe factura + detalles en una transacción y descuenta el stock de
// los productos incluidos.
class FacturacionController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString usuarioUuid READ usuarioUuid WRITE setUsuarioUuid NOTIFY usuarioCambiado)
    Q_PROPERTY(QVariantList facturas READ facturas NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList servicios READ servicios NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList productos READ productos NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList clientes READ clientes NOTIFY datosCambiados)
    Q_PROPERTY(QString filtro READ filtro NOTIFY datosCambiados)
    Q_PROPERTY(QString estadoFiltro READ estadoFiltro NOTIFY datosCambiados)
    Q_PROPERTY(int total READ total NOTIFY datosCambiados)
    Q_PROPERTY(int pendientes READ pendientes NOTIFY datosCambiados)
    Q_PROPERTY(double totalFacturado READ totalFacturado NOTIFY datosCambiados)
    Q_PROPERTY(QString totalFacturadoTexto READ totalFacturadoTexto NOTIFY datosCambiados)
    Q_PROPERTY(QVariantMap seleccionada READ seleccionada NOTIFY seleccionCambiada)
    Q_PROPERTY(QVariantList detalles READ detalles NOTIFY seleccionCambiada)
    Q_PROPERTY(QVariantList lineas READ lineas NOTIFY borradorCambiado)
    Q_PROPERTY(QString clienteBorradorUuid READ clienteBorradorUuid NOTIFY borradorCambiado)
    Q_PROPERTY(double subtotalBorrador READ subtotalBorrador NOTIFY borradorCambiado)
    Q_PROPERTY(double impuestoBorrador READ impuestoBorrador NOTIFY borradorCambiado)
    Q_PROPERTY(double descuentoBorrador READ descuentoBorrador NOTIFY borradorCambiado)
    Q_PROPERTY(double totalBorrador READ totalBorrador NOTIFY borradorCambiado)
    Q_PROPERTY(QString totalBorradorTexto READ totalBorradorTexto NOTIFY borradorCambiado)
    Q_PROPERTY(QVariantMap resumenCaja READ resumenCaja NOTIFY cajaCambiada)
    Q_PROPERTY(QString error READ error NOTIFY errorCambiado)
    Q_PROPERTY(QString mensaje READ mensaje NOTIFY mensajeCambiado)

public:
    explicit FacturacionController(fuzzy::core::IQueryExecutor& db, QObject* parent = nullptr);

    QString usuarioUuid() const { return usuarioUuid_; }
    void setUsuarioUuid(const QString& uuid);
    QVariantList facturas() const { return facturas_; }
    QVariantList servicios() const { return servicios_; }
    QVariantList productos() const { return productos_; }
    QVariantList clientes() const { return clientes_; }
    QString filtro() const { return filtro_; }
    QString estadoFiltro() const { return estadoFiltro_; }
    int total() const { return total_; }
    int pendientes() const { return pendientes_; }
    double totalFacturado() const { return totalFacturado_; }
    QString totalFacturadoTexto() const;
    QVariantMap seleccionada() const { return seleccionada_; }
    QVariantList detalles() const { return detalles_; }
    QVariantList lineas() const { return lineas_; }
    QString clienteBorradorUuid() const { return clienteBorradorUuid_; }
    double subtotalBorrador() const { return subtotalBorrador_; }
    double impuestoBorrador() const { return impuestoBorrador_; }
    double descuentoBorrador() const { return descuentoBorrador_; }
    double totalBorrador() const { return totalBorrador_; }
    QString totalBorradorTexto() const;
    QVariantMap resumenCaja() const { return resumenCaja_; }
    QString error() const { return error_; }
    QString mensaje() const { return mensaje_; }

    Q_INVOKABLE void cargar();
    Q_INVOKABLE void buscar(const QString& texto);
    Q_INVOKABLE void filtrarEstado(const QString& estado);
    Q_INVOKABLE void seleccionar(const QString& facturaUuid);
    Q_INVOKABLE void limpiarSeleccion();
    Q_INVOKABLE void limpiarMensajes();

    Q_INVOKABLE void iniciarBorrador(const QString& clienteUuid);
    Q_INVOKABLE void agregarServicio(const QString& servicioUuid, int cantidad);
    Q_INVOKABLE void agregarProducto(const QString& productoUuid, int cantidad);
    Q_INVOKABLE void quitarLinea(int indice);
    Q_INVOKABLE void cancelarBorrador();
    Q_INVOKABLE void emitir(const QString& metodoPago, double descuento, double impuesto);

    Q_INVOKABLE void marcarPagada(const QString& facturaUuid);
    Q_INVOKABLE void anular(const QString& facturaUuid);
    Q_INVOKABLE void recargarCaja();
    Q_INVOKABLE void cerrarCaja(double ingresosReales, const QString& observaciones);

signals:
    void usuarioCambiado();
    void datosCambiados();
    void seleccionCambiada();
    void borradorCambiado();
    void cajaCambiada();
    void errorCambiado();
    void mensajeCambiado();

private:
    void recargar();
    void recalcular();
    void fallo(const QString& texto);
    void exito(const QString& texto);
    qint64 idDe(const QString& tabla, const QString& uuid) const;

    fuzzy::core::IQueryExecutor& db_;
    QString usuarioUuid_;
    QVariantList facturas_;
    QVariantList servicios_;
    QVariantList productos_;
    QVariantList clientes_;
    QString filtro_;
    QString estadoFiltro_;
    int total_ = 0;
    int pendientes_ = 0;
    double totalFacturado_ = 0.0;
    QVariantMap seleccionada_;
    QVariantList detalles_;
    QVariantList lineas_;
    QString clienteBorradorUuid_;
    double subtotalBorrador_ = 0.0;
    double impuestoBorrador_ = 0.0;
    double descuentoBorrador_ = 0.0;
    double totalBorrador_ = 0.0;
    QVariantMap resumenCaja_;
    QString error_;
    QString mensaje_;
};

} // namespace fuzzy::desktop
