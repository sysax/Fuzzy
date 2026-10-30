#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace fuzzy::desktop {

class InventarioController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList productos READ productos NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList categorias READ categorias NOTIFY datosCambiados)
    Q_PROPERTY(QString filtro READ filtro NOTIFY datosCambiados)
    Q_PROPERTY(QString vista READ vista NOTIFY datosCambiados)
    Q_PROPERTY(int total READ total NOTIFY datosCambiados)
    Q_PROPERTY(int criticos READ criticos NOTIFY datosCambiados)
    Q_PROPERTY(int porVencer READ porVencer NOTIFY datosCambiados)
    Q_PROPERTY(double valorInventario READ valorInventario NOTIFY datosCambiados)
    Q_PROPERTY(QString valorInventarioTexto READ valorInventarioTexto NOTIFY datosCambiados)
    Q_PROPERTY(QVariantMap seleccionado READ seleccionado NOTIFY seleccionCambiada)
    Q_PROPERTY(QString error READ error NOTIFY errorCambiado)
    Q_PROPERTY(QString mensaje READ mensaje NOTIFY mensajeCambiado)

public:
    explicit InventarioController(fuzzy::core::IQueryExecutor& db, QObject* parent = nullptr);

    QVariantList productos() const { return productos_; }
    QVariantList categorias() const { return categorias_; }
    QString filtro() const { return filtro_; }
    QString vista() const { return vista_; }
    int total() const { return total_; }
    int criticos() const { return criticos_; }
    int porVencer() const { return porVencer_; }
    double valorInventario() const { return valorInventario_; }
    QString valorInventarioTexto() const;
    QVariantMap seleccionado() const { return seleccionado_; }
    QString error() const { return error_; }
    QString mensaje() const { return mensaje_; }

    Q_INVOKABLE void cargar();
    Q_INVOKABLE void buscar(const QString& texto);
    Q_INVOKABLE void cambiarVista(const QString& vista);
    Q_INVOKABLE void seleccionar(const QString& productoUuid);
    Q_INVOKABLE void limpiarSeleccion();
    Q_INVOKABLE void limpiarMensajes();
    Q_INVOKABLE void crearProducto(const QString& nombre, const QString& principio,
                                   const QString& categoriaUuid, const QString& codigo,
                                   const QString& unidad, int stock, int minimo,
                                   double compra, double venta, const QString& caducidad,
                                   const QString& proveedor);
    Q_INVOKABLE void actualizarProducto(const QString& productoUuid, const QString& nombre,
                                        const QString& principio, const QString& categoriaUuid,
                                        const QString& codigo, const QString& unidad,
                                        int stock, int minimo, double compra, double venta,
                                        const QString& caducidad, const QString& proveedor);
    Q_INVOKABLE void ajustarStock(const QString& productoUuid, int delta,
                                  const QString& motivo);
    Q_INVOKABLE void marcarPedido(const QString& productoUuid, bool activo);

signals:
    void datosCambiados();
    void seleccionCambiada();
    void errorCambiado();
    void mensajeCambiado();

private:
    void recargar();
    void fallo(const QString& texto);
    void exito(const QString& texto);

    fuzzy::core::IQueryExecutor& db_;
    QVariantList productos_;
    QVariantList categorias_;
    QString filtro_;
    QString vista_ = QStringLiteral("todos");
    int total_ = 0;
    int criticos_ = 0;
    int porVencer_ = 0;
    double valorInventario_ = 0.0;
    QVariantMap seleccionado_;
    QString error_;
    QString mensaje_;
};

} // namespace fuzzy::desktop
