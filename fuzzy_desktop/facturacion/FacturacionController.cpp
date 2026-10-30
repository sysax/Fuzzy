#include "facturacion/FacturacionController.h"

#include "common/Consulta.h"
#include "fuzzy/core/Uuid.h"

#include <QDate>
#include <QDateTime>
#include <QVariant>

namespace fuzzy::desktop {

FacturacionController::FacturacionController(fuzzy::core::IQueryExecutor& db, QObject* parent)
    : QObject(parent), db_(db)
{
    recargar();
}

void FacturacionController::setUsuarioUuid(const QString& uuid)
{
    if (usuarioUuid_ == uuid) {
        return;
    }
    usuarioUuid_ = uuid;
    emit usuarioCambiado();
}

QString FacturacionController::totalFacturadoTexto() const
{
    return moneda(totalFacturado_);
}

QString FacturacionController::totalBorradorTexto() const
{
    return moneda(totalBorrador_);
}

qint64 FacturacionController::idDe(const QString& tabla, const QString& uuid) const
{
    if (uuid.isEmpty()) {
        return 0;
    }
    const auto filas = db_.query(
        QStringLiteral("SELECT id FROM %1 WHERE uuid = ? AND deleted_at IS NULL").arg(tabla),
        {QVariant(uuid)});
    return filas.empty() ? 0 : enteroDeFila(filas.front(), 0);
}

void FacturacionController::fallo(const QString& texto)
{
    error_ = texto;
    mensaje_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

void FacturacionController::exito(const QString& texto)
{
    mensaje_ = texto;
    error_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

void FacturacionController::recargar()
{
    QString sql =
        QStringLiteral("SELECT f.uuid, coalesce(f.numero_factura, '—'), f.fecha_emision, "
                       "coalesce(cl.nombre_completo, 'Consumidor final'), f.subtotal, "
                       "f.impuesto, f.descuento, f.monto_total, f.estado, "
                       "coalesce(f.metodo_pago, ''), coalesce(f.nota_interna, '') "
                       "FROM facturas f LEFT JOIN clientes cl ON cl.id = f.cliente_id "
                       "WHERE f.deleted_at IS NULL ");
    std::vector<QVariant> params;
    if (!filtro_.isEmpty()) {
        sql += QStringLiteral("AND (lower(coalesce(f.numero_factura, '')) LIKE ? "
                              "OR lower(coalesce(cl.nombre_completo, '')) LIKE ?) ");
        const QString patron = QStringLiteral("%") + filtro_.toLower() + QStringLiteral("%");
        params.push_back(QVariant(patron));
        params.push_back(QVariant(patron));
    }
    if (!estadoFiltro_.isEmpty()) {
        sql += QStringLiteral("AND f.estado = ? ");
        params.push_back(QVariant(estadoFiltro_));
    }
    sql += QStringLiteral("ORDER BY f.fecha_emision DESC LIMIT 200");

    const auto filas = db_.query(sql, params);
    facturas_.clear();
    for (const auto& fila : filas) {
        QVariantMap item;
        item.insert(QStringLiteral("uuid"), textoDeFila(fila, 0));
        item.insert(QStringLiteral("numero"), textoDeFila(fila, 1));
        item.insert(QStringLiteral("fecha"), fechaHoraTexto(enteroDeFila(fila, 2)));
        item.insert(QStringLiteral("cliente"), textoDeFila(fila, 3));
        item.insert(QStringLiteral("subtotal"), realDeFila(fila, 4));
        item.insert(QStringLiteral("impuesto"), realDeFila(fila, 5));
        item.insert(QStringLiteral("descuento"), realDeFila(fila, 6));
        item.insert(QStringLiteral("total"), realDeFila(fila, 7));
        item.insert(QStringLiteral("totalTexto"), moneda(realDeFila(fila, 7)));
        item.insert(QStringLiteral("estado"), textoDeFila(fila, 8));
        item.insert(QStringLiteral("metodo"), textoDeFila(fila, 9));
        item.insert(QStringLiteral("nota"), textoDeFila(fila, 10));
        facturas_.append(item);
    }

    const auto resumen = db_.query(
        QStringLiteral("SELECT count(*), "
                       "sum(CASE WHEN estado IN ('Emitida', 'Pendiente') THEN 1 ELSE 0 END), "
                       "sum(CASE WHEN estado = 'Pagada' THEN monto_total ELSE 0 END) "
                       "FROM facturas WHERE deleted_at IS NULL"));
    if (!resumen.empty()) {
        total_ = static_cast<int>(enteroDeFila(resumen.front(), 0));
        pendientes_ = static_cast<int>(enteroDeFila(resumen.front(), 1));
        totalFacturado_ = realDeFila(resumen.front(), 2);
    }

    servicios_ = listaDeFilas(
        db_.query(QStringLiteral("SELECT uuid, codigo, nombre, precio FROM servicios "
                                 "WHERE deleted_at IS NULL AND activo = 1 ORDER BY nombre")),
        {QStringLiteral("uuid"), QStringLiteral("codigo"), QStringLiteral("nombre"),
         QStringLiteral("precio")});

    const auto productosFilas = db_.query(
        QStringLiteral("SELECT uuid, coalesce(codigo, ''), nombre_comercial, precio_venta, "
                       "stock_actual FROM productos_insumos "
                       "WHERE deleted_at IS NULL ORDER BY nombre_comercial"));
    productos_.clear();
    for (const auto& fila : productosFilas) {
        QVariantMap item;
        item.insert(QStringLiteral("uuid"), textoDeFila(fila, 0));
        item.insert(QStringLiteral("codigo"), textoDeFila(fila, 1));
        item.insert(QStringLiteral("nombre"), textoDeFila(fila, 2));
        item.insert(QStringLiteral("precio"), realDeFila(fila, 3));
        item.insert(QStringLiteral("stock"), enteroDeFila(fila, 4));
        productos_.append(item);
    }

    clientes_ = listaDeFilas(
        db_.query(QStringLiteral("SELECT uuid, nombre_completo FROM clientes "
                                 "WHERE deleted_at IS NULL ORDER BY nombre_completo")),
        {QStringLiteral("uuid"), QStringLiteral("nombre")});

    emit datosCambiados();
    recargarCaja();
}

void FacturacionController::cargar()
{
    recargar();
}

void FacturacionController::buscar(const QString& texto)
{
    filtro_ = texto.trimmed();
    recargar();
}

void FacturacionController::filtrarEstado(const QString& estado)
{
    estadoFiltro_ = estado;
    recargar();
}

void FacturacionController::seleccionar(const QString& facturaUuid)
{
    const auto filas = db_.query(
        QStringLiteral("SELECT f.uuid, coalesce(f.numero_factura, '—'), f.fecha_emision, "
                       "coalesce(cl.nombre_completo, 'Consumidor final'), "
                       "coalesce(cl.uuid, ''), f.subtotal, f.impuesto, f.descuento, "
                       "f.monto_total, f.estado, coalesce(f.metodo_pago, ''), "
                       "coalesce(f.nota_interna, '') "
                       "FROM facturas f LEFT JOIN clientes cl ON cl.id = f.cliente_id "
                       "WHERE f.uuid = ? AND f.deleted_at IS NULL"),
        {QVariant(facturaUuid)});
    if (filas.size() != 1) {
        fallo(QStringLiteral("la factura no existe"));
        return;
    }
    seleccionada_.clear();
    seleccionada_.insert(QStringLiteral("uuid"), textoDeFila(filas.front(), 0));
    seleccionada_.insert(QStringLiteral("numero"), textoDeFila(filas.front(), 1));
    seleccionada_.insert(QStringLiteral("fecha"), fechaHoraTexto(enteroDeFila(filas.front(), 2)));
    seleccionada_.insert(QStringLiteral("cliente"), textoDeFila(filas.front(), 3));
    seleccionada_.insert(QStringLiteral("clienteUuid"), textoDeFila(filas.front(), 4));
    seleccionada_.insert(QStringLiteral("subtotal"), realDeFila(filas.front(), 5));
    seleccionada_.insert(QStringLiteral("impuesto"), realDeFila(filas.front(), 6));
    seleccionada_.insert(QStringLiteral("descuento"), realDeFila(filas.front(), 7));
    seleccionada_.insert(QStringLiteral("total"), realDeFila(filas.front(), 8));
    seleccionada_.insert(QStringLiteral("totalTexto"), moneda(realDeFila(filas.front(), 8)));
    seleccionada_.insert(QStringLiteral("estado"), textoDeFila(filas.front(), 9));
    seleccionada_.insert(QStringLiteral("metodo"), textoDeFila(filas.front(), 10));
    seleccionada_.insert(QStringLiteral("nota"), textoDeFila(filas.front(), 11));

    detalles_ = listaDeFilas(
        db_.query(QStringLiteral("SELECT uuid, coalesce(codigo, ''), coalesce(nombre, ''), "
                                 "coalesce(tipo, ''), cantidad, precio_unitario, "
                                 "cantidad * precio_unitario "
                                 "FROM detalles_factura WHERE factura_id = "
                                 "(SELECT id FROM facturas WHERE uuid = ?) "
                                 "AND deleted_at IS NULL ORDER BY id"),
                 {QVariant(facturaUuid)}),
        {QStringLiteral("uuid"), QStringLiteral("codigo"), QStringLiteral("nombre"),
         QStringLiteral("tipo"), QStringLiteral("cantidad"), QStringLiteral("precio"),
         QStringLiteral("total")});
    emit seleccionCambiada();
}

void FacturacionController::limpiarSeleccion()
{
    seleccionada_.clear();
    detalles_.clear();
    emit seleccionCambiada();
}

void FacturacionController::limpiarMensajes()
{
    error_.clear();
    mensaje_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

void FacturacionController::iniciarBorrador(const QString& clienteUuid)
{
    lineas_.clear();
    clienteBorradorUuid_ = clienteUuid;
    descuentoBorrador_ = 0.0;
    impuestoBorrador_ = 0.0;
    recalcular();
    exito(QStringLiteral("Nueva factura iniciada."));
}

void FacturacionController::recalcular()
{
    subtotalBorrador_ = 0.0;
    for (const auto& linea : lineas_) {
        const auto mapa = linea.toMap();
        subtotalBorrador_ += mapa.value(QStringLiteral("total")).toDouble();
    }
    totalBorrador_ = qMax(0.0, subtotalBorrador_ + impuestoBorrador_ - descuentoBorrador_);
    emit borradorCambiado();
}

void FacturacionController::agregarServicio(const QString& servicioUuid, int cantidad)
{
    const auto filas = db_.query(
        QStringLiteral("SELECT id, codigo, nombre, precio FROM servicios "
                       "WHERE uuid = ? AND deleted_at IS NULL AND activo = 1"),
        {QVariant(servicioUuid)});
    if (filas.size() != 1) {
        fallo(QStringLiteral("el servicio no existe"));
        return;
    }
    const int cantidadFinal = qMax(1, cantidad);
    const double precio = realDeFila(filas.front(), 3);
    QVariantMap linea;
    linea.insert(QStringLiteral("tipo"), QStringLiteral("Servicio"));
    linea.insert(QStringLiteral("refId"), enteroDeFila(filas.front(), 0));
    linea.insert(QStringLiteral("codigo"), textoDeFila(filas.front(), 1));
    linea.insert(QStringLiteral("nombre"), textoDeFila(filas.front(), 2));
    linea.insert(QStringLiteral("cantidad"), cantidadFinal);
    linea.insert(QStringLiteral("precio"), precio);
    linea.insert(QStringLiteral("precioTexto"), moneda(precio));
    linea.insert(QStringLiteral("total"), precio * cantidadFinal);
    linea.insert(QStringLiteral("totalTexto"), moneda(precio * cantidadFinal));
    lineas_.append(linea);
    recalcular();
}

void FacturacionController::agregarProducto(const QString& productoUuid, int cantidad)
{
    const auto filas = db_.query(
        QStringLiteral("SELECT id, coalesce(codigo, ''), nombre_comercial, precio_venta, "
                       "stock_actual FROM productos_insumos "
                       "WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(productoUuid)});
    if (filas.size() != 1) {
        fallo(QStringLiteral("el producto no existe"));
        return;
    }
    const int cantidadFinal = qMax(1, cantidad);
    const int stock = static_cast<int>(enteroDeFila(filas.front(), 4));
    if (cantidadFinal > stock) {
        fallo(QStringLiteral("solo hay %1 unidades en stock").arg(stock));
        return;
    }
    const double precio = realDeFila(filas.front(), 3);
    QVariantMap linea;
    linea.insert(QStringLiteral("tipo"), QStringLiteral("Producto"));
    linea.insert(QStringLiteral("refId"), enteroDeFila(filas.front(), 0));
    linea.insert(QStringLiteral("codigo"), textoDeFila(filas.front(), 1));
    linea.insert(QStringLiteral("nombre"), textoDeFila(filas.front(), 2));
    linea.insert(QStringLiteral("cantidad"), cantidadFinal);
    linea.insert(QStringLiteral("precio"), precio);
    linea.insert(QStringLiteral("precioTexto"), moneda(precio));
    linea.insert(QStringLiteral("total"), precio * cantidadFinal);
    linea.insert(QStringLiteral("totalTexto"), moneda(precio * cantidadFinal));
    lineas_.append(linea);
    recalcular();
}

void FacturacionController::quitarLinea(int indice)
{
    if (indice < 0 || indice >= lineas_.size()) {
        return;
    }
    lineas_.removeAt(indice);
    recalcular();
}

void FacturacionController::cancelarBorrador()
{
    lineas_.clear();
    clienteBorradorUuid_.clear();
    descuentoBorrador_ = 0.0;
    impuestoBorrador_ = 0.0;
    recalcular();
}

void FacturacionController::emitir(const QString& metodoPago, double descuento, double impuesto)
{
    if (lineas_.isEmpty()) {
        fallo(QStringLiteral("añade al menos un concepto a la factura"));
        return;
    }
    if (descuento < 0 || impuesto < 0) {
        fallo(QStringLiteral("el descuento y el impuesto no pueden ser negativos"));
        return;
    }
    descuentoBorrador_ = descuento;
    impuestoBorrador_ = impuesto;
    recalcular();
    if (descuentoBorrador_ > subtotalBorrador_ + impuestoBorrador_) {
        fallo(QStringLiteral("el descuento no puede superar el total"));
        return;
    }

    const qint64 clienteId = idDe(QStringLiteral("clientes"), clienteBorradorUuid_);
    const qint64 cajeroId = idDe(QStringLiteral("usuarios"), usuarioUuid_);
    const QString uuidFactura = fuzzy::core::uuidNuevo();
    const qint64 ahora = QDateTime::currentMSecsSinceEpoch();
    const QString numero =
        QStringLiteral("FV-%1-%2")
            .arg(QDateTime::fromMSecsSinceEpoch(ahora).toString(QStringLiteral("yyyyMMdd")),
                 QString::number(ahora % 100000).rightJustified(5, QLatin1Char('0')));
    const QString estado = metodoPago.isEmpty() ? QStringLiteral("Emitida")
                                                : QStringLiteral("Pagada");

    if (!db_.execute(QStringLiteral("BEGIN")).ok) {
        fallo(QStringLiteral("no se pudo iniciar la factura"));
        return;
    }

    const auto alta = db_.execute(
        QStringLiteral("INSERT INTO facturas "
                       "(uuid, cliente_id, cajero_id, numero_factura, fecha_emision, "
                       " subtotal, impuesto, descuento, monto_total, metodo_pago, estado, "
                       " created_at, updated_at, sync_state) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'pendiente')"),
        {QVariant(uuidFactura),
         clienteId > 0 ? QVariant::fromValue(clienteId) : QVariant(),
         cajeroId > 0 ? QVariant::fromValue(cajeroId) : QVariant(), QVariant(numero),
         QVariant::fromValue(ahora), QVariant::fromValue(subtotalBorrador_),
         QVariant::fromValue(impuestoBorrador_), QVariant::fromValue(descuentoBorrador_),
         QVariant::fromValue(totalBorrador_),
         metodoPago.isEmpty() ? QVariant() : QVariant(metodoPago), QVariant(estado),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!alta.ok) {
        db_.execute(QStringLiteral("ROLLBACK"));
        fallo(QStringLiteral("no se pudo emitir la factura"));
        return;
    }

    const qint64 facturaId =
        alta.last_insert_id.toLongLong() > 0
        ? alta.last_insert_id.toLongLong()
        : idDe(QStringLiteral("facturas"), uuidFactura);
    for (const auto& linea : lineas_) {
        const auto mapa = linea.toMap();
        const bool esProducto = mapa.value(QStringLiteral("tipo")).toString()
            == QStringLiteral("Producto");
        const auto detalle = db_.execute(
            QStringLiteral("INSERT INTO detalles_factura "
                           "(uuid, factura_id, producto_id, servicio_id, codigo, nombre, "
                           " tipo, cantidad, precio_unitario, created_at, updated_at, "
                           " sync_state) "
                           "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'pendiente')"),
            {QVariant(fuzzy::core::uuidNuevo()), QVariant::fromValue(facturaId),
             esProducto ? QVariant::fromValue(mapa.value(QStringLiteral("refId")).toLongLong())
                        : QVariant(),
             esProducto ? QVariant()
                        : QVariant::fromValue(mapa.value(QStringLiteral("refId")).toLongLong()),
             QVariant(mapa.value(QStringLiteral("codigo")).toString()),
             QVariant(mapa.value(QStringLiteral("nombre")).toString()),
             QVariant(mapa.value(QStringLiteral("tipo")).toString()),
             QVariant::fromValue(mapa.value(QStringLiteral("cantidad")).toInt()),
             QVariant::fromValue(mapa.value(QStringLiteral("precio")).toDouble()),
             QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
        if (!detalle.ok) {
            db_.execute(QStringLiteral("ROLLBACK"));
            fallo(QStringLiteral("no se pudo guardar el detalle"));
            return;
        }
        if (esProducto) {
            db_.execute(
                QStringLiteral("UPDATE productos_insumos SET stock_actual = "
                               "max(0, stock_actual - ?), updated_at = ? WHERE id = ?"),
                {QVariant::fromValue(mapa.value(QStringLiteral("cantidad")).toInt()),
                 QVariant::fromValue(ahora),
                 QVariant::fromValue(mapa.value(QStringLiteral("refId")).toLongLong())});
        }
    }

    if (!db_.execute(QStringLiteral("COMMIT")).ok) {
        db_.execute(QStringLiteral("ROLLBACK"));
        fallo(QStringLiteral("no se pudo confirmar la factura"));
        return;
    }

    const double totalEmitido = totalBorrador_;
    cancelarBorrador();
    recargar();
    seleccionar(uuidFactura);
    exito(QStringLiteral("Factura %1 emitida por %2.")
              .arg(numero, moneda(totalEmitido)));
}

void FacturacionController::marcarPagada(const QString& facturaUuid)
{
    const auto cambio = db_.execute(
        QStringLiteral("UPDATE facturas SET estado = 'Pagada', updated_at = ? "
                       "WHERE uuid = ? AND deleted_at IS NULL "
                       "AND estado IN ('Emitida', 'Pendiente')"),
        {QVariant::fromValue(QDateTime::currentMSecsSinceEpoch()), QVariant(facturaUuid)});
    if (!cambio.ok || cambio.affected == 0) {
        fallo(QStringLiteral("la factura no se puede marcar como pagada"));
        return;
    }
    recargar();
    seleccionar(facturaUuid);
    exito(QStringLiteral("Factura marcada como pagada."));
}

void FacturacionController::anular(const QString& facturaUuid)
{
    const auto cambio = db_.execute(
        QStringLiteral("UPDATE facturas SET estado = 'Pendiente', updated_at = ? "
                       "WHERE uuid = ? AND deleted_at IS NULL AND estado <> 'Pagada'"),
        {QVariant::fromValue(QDateTime::currentMSecsSinceEpoch()), QVariant(facturaUuid)});
    if (!cambio.ok || cambio.affected == 0) {
        fallo(QStringLiteral("solo se pueden anular facturas no pagadas"));
        return;
    }
    recargar();
    seleccionar(facturaUuid);
    exito(QStringLiteral("Factura devuelta a pendiente."));
}

void FacturacionController::recargarCaja()
{
    const QDate hoy = fechaHoy();
    const qint64 desde = inicioDiaMs(hoy);
    const qint64 hasta = finDiaMs(hoy);
    const auto filas = db_.query(
        QStringLiteral("SELECT "
                       "coalesce(sum(CASE WHEN estado = 'Pagada' THEN monto_total END), 0), "
                       "sum(CASE WHEN estado = 'Pagada' THEN 1 ELSE 0 END), "
                       "coalesce(sum(CASE WHEN estado = 'Pagada' AND metodo_pago = 'Efectivo' "
                       "THEN monto_total END), 0), "
                       "coalesce(sum(CASE WHEN estado = 'Pagada' AND metodo_pago = 'Tarjeta' "
                       "THEN monto_total END), 0), "
                       "coalesce(sum(CASE WHEN estado = 'Pagada' "
                       "AND metodo_pago = 'Transferencia' THEN monto_total END), 0), "
                       "coalesce(sum(CASE WHEN estado IN ('Emitida', 'Pendiente') "
                       "THEN monto_total END), 0) "
                       "FROM facturas WHERE deleted_at IS NULL "
                       "AND fecha_emision BETWEEN ? AND ?"),
        {QVariant::fromValue(desde), QVariant::fromValue(hasta)});
    resumenCaja_.clear();
    if (!filas.empty()) {
        resumenCaja_.insert(QStringLiteral("total"), realDeFila(filas.front(), 0));
        resumenCaja_.insert(QStringLiteral("totalTexto"), moneda(realDeFila(filas.front(), 0)));
        resumenCaja_.insert(QStringLiteral("cantidad"), enteroDeFila(filas.front(), 1));
        resumenCaja_.insert(QStringLiteral("efectivo"), realDeFila(filas.front(), 2));
        resumenCaja_.insert(QStringLiteral("efectivoTexto"), moneda(realDeFila(filas.front(), 2)));
        resumenCaja_.insert(QStringLiteral("tarjeta"), realDeFila(filas.front(), 3));
        resumenCaja_.insert(QStringLiteral("tarjetaTexto"), moneda(realDeFila(filas.front(), 3)));
        resumenCaja_.insert(QStringLiteral("transferencia"), realDeFila(filas.front(), 4));
        resumenCaja_.insert(QStringLiteral("transferenciaTexto"),
                            moneda(realDeFila(filas.front(), 4)));
        resumenCaja_.insert(QStringLiteral("pendiente"), realDeFila(filas.front(), 5));
        resumenCaja_.insert(QStringLiteral("pendienteTexto"),
                            moneda(realDeFila(filas.front(), 5)));
        resumenCaja_.insert(QStringLiteral("fecha"), fechaTexto(desde));
    }
    const auto cierre = db_.query(
        QStringLiteral("SELECT fecha_cierre, ingresos_calculados, ingresos_reales, desfase "
                       "FROM cierres_caja WHERE deleted_at IS NULL "
                       "ORDER BY fecha_cierre DESC LIMIT 1"));
    if (!cierre.empty()) {
        resumenCaja_.insert(QStringLiteral("ultimoCierre"),
                            fechaHoraTexto(enteroDeFila(cierre.front(), 0)));
        resumenCaja_.insert(QStringLiteral("ultimoCalculado"),
                            moneda(realDeFila(cierre.front(), 1)));
        resumenCaja_.insert(QStringLiteral("ultimoReal"), moneda(realDeFila(cierre.front(), 2)));
        resumenCaja_.insert(QStringLiteral("ultimoDesfase"), moneda(realDeFila(cierre.front(), 3)));
    }
    emit cajaCambiada();
}

void FacturacionController::cerrarCaja(double ingresosReales, const QString& observaciones)
{
    const double calculado = resumenCaja_.value(QStringLiteral("total")).toDouble();
    if (ingresosReales < 0) {
        fallo(QStringLiteral("los ingresos reales no pueden ser negativos"));
        return;
    }
    const qint64 ahora = QDateTime::currentMSecsSinceEpoch();
    const qint64 cajeroId = idDe(QStringLiteral("usuarios"), usuarioUuid_);
    const auto alta = db_.execute(
        QStringLiteral("INSERT INTO cierres_caja "
                       "(uuid, usuario_cierre_id, fecha_cierre, ingresos_calculados, "
                       " ingresos_reales, desfase, observaciones, created_at, updated_at, "
                       " sync_state) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 'pendiente')"),
        {QVariant(fuzzy::core::uuidNuevo()),
         cajeroId > 0 ? QVariant::fromValue(cajeroId) : QVariant(),
         QVariant::fromValue(ahora), QVariant::fromValue(calculado),
         QVariant::fromValue(ingresosReales), QVariant::fromValue(ingresosReales - calculado),
         observaciones.trimmed().isEmpty() ? QVariant() : QVariant(observaciones.trimmed()),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!alta.ok) {
        fallo(QStringLiteral("no se pudo cerrar la caja"));
        return;
    }
    recargarCaja();
    exito(QStringLiteral("Caja cerrada. Desfase: %1.").arg(
        moneda(ingresosReales - calculado)));
}

} // namespace fuzzy::desktop
