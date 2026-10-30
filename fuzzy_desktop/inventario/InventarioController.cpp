#include "inventario/InventarioController.h"

#include "common/Consulta.h"
#include "fuzzy/core/Uuid.h"

#include <QDate>
#include <QDateTime>
#include <QVariant>

namespace fuzzy::desktop {

InventarioController::InventarioController(fuzzy::core::IQueryExecutor& db, QObject* parent)
    : QObject(parent), db_(db)
{
    recargar();
}

QString InventarioController::valorInventarioTexto() const
{
    return moneda(valorInventario_);
}

void InventarioController::fallo(const QString& texto)
{
    error_ = texto;
    mensaje_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

void InventarioController::exito(const QString& texto)
{
    mensaje_ = texto;
    error_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

void InventarioController::recargar()
{
    QString sql =
        QStringLiteral("SELECT p.uuid, p.nombre_comercial, coalesce(p.principio_activo, ''), "
                       "coalesce(c.nombre_categoria, 'Sin categoría'), coalesce(p.codigo, ''), "
                       "p.unidad, p.stock_actual, p.stock_minimo, p.precio_compra, "
                       "p.precio_venta, coalesce(p.fecha_caducidad, ''), "
                       "coalesce(p.proveedor_nombre, ''), p.pedido_en_curso, "
                       "coalesce(c.uuid, '') "
                       "FROM productos_insumos p "
                       "LEFT JOIN categorias_inventario c ON c.id = p.categoria_id "
                       "WHERE p.deleted_at IS NULL ");
    std::vector<QVariant> params;
    if (!filtro_.isEmpty()) {
        sql += QStringLiteral("AND (lower(p.nombre_comercial) LIKE ? "
                              "OR lower(coalesce(p.principio_activo, '')) LIKE ? "
                              "OR lower(coalesce(p.codigo, '')) LIKE ?) ");
        const QString patron = QStringLiteral("%") + filtro_.toLower() + QStringLiteral("%");
        params.push_back(QVariant(patron));
        params.push_back(QVariant(patron));
        params.push_back(QVariant(patron));
    }
    if (vista_ == QStringLiteral("critico")) {
        sql += QStringLiteral("AND p.stock_actual <= p.stock_minimo ");
    } else if (vista_ == QStringLiteral("vencer")) {
        sql += QStringLiteral("AND p.fecha_caducidad IS NOT NULL AND p.fecha_caducidad <> '' "
                              "AND date(p.fecha_caducidad) <= date('now', '+60 days') ");
    }
    sql += QStringLiteral("ORDER BY (p.stock_actual <= p.stock_minimo) DESC, "
                          "p.nombre_comercial COLLATE NOCASE");

    const auto filas = db_.query(sql, params);
    productos_.clear();
    for (const auto& fila : filas) {
        const int stock = static_cast<int>(enteroDeFila(fila, 6));
        const int minimo = static_cast<int>(enteroDeFila(fila, 7));
        QVariantMap item;
        item.insert(QStringLiteral("uuid"), textoDeFila(fila, 0));
        item.insert(QStringLiteral("nombre"), textoDeFila(fila, 1));
        item.insert(QStringLiteral("principio"), textoDeFila(fila, 2));
        item.insert(QStringLiteral("categoria"), textoDeFila(fila, 3));
        item.insert(QStringLiteral("codigo"), textoDeFila(fila, 4));
        item.insert(QStringLiteral("unidad"), textoDeFila(fila, 5));
        item.insert(QStringLiteral("stock"), stock);
        item.insert(QStringLiteral("minimo"), minimo);
        item.insert(QStringLiteral("compra"), realDeFila(fila, 8));
        item.insert(QStringLiteral("venta"), realDeFila(fila, 9));
        item.insert(QStringLiteral("ventaTexto"), moneda(realDeFila(fila, 9)));
        item.insert(QStringLiteral("caducidad"), textoDeFila(fila, 10));
        item.insert(QStringLiteral("proveedor"), textoDeFila(fila, 11));
        item.insert(QStringLiteral("pedido"), enteroDeFila(fila, 12) == 1);
        item.insert(QStringLiteral("categoriaUuid"), textoDeFila(fila, 13));
        item.insert(QStringLiteral("critico"), stock <= minimo);
        item.insert(QStringLiteral("agotado"), stock == 0);
        productos_.append(item);
    }

    const auto resumen = db_.query(
        QStringLiteral("SELECT count(*), "
                       "sum(CASE WHEN stock_actual <= stock_minimo THEN 1 ELSE 0 END), "
                       "sum(CASE WHEN fecha_caducidad IS NOT NULL AND fecha_caducidad <> '' "
                       " AND date(fecha_caducidad) <= date('now', '+60 days') "
                       " THEN 1 ELSE 0 END), "
                       "sum(stock_actual * precio_compra) "
                       "FROM productos_insumos WHERE deleted_at IS NULL"));
    // `total` es lo que ve el usuario: la lista filtrada, no el catálogo
    // completo. Los KPI siguen usando el resumen global.
    total_ = static_cast<int>(productos_.size());
    if (!resumen.empty()) {
        criticos_ = static_cast<int>(enteroDeFila(resumen.front(), 1));
        porVencer_ = static_cast<int>(enteroDeFila(resumen.front(), 2));
        valorInventario_ = realDeFila(resumen.front(), 3);
    }

    categorias_ = listaDeFilas(
        db_.query(QStringLiteral("SELECT uuid, nombre_categoria FROM categorias_inventario "
                                 "WHERE deleted_at IS NULL ORDER BY nombre_categoria")),
        {QStringLiteral("uuid"), QStringLiteral("nombre")});

    emit datosCambiados();
}

void InventarioController::cargar()
{
    recargar();
}

void InventarioController::buscar(const QString& texto)
{
    filtro_ = texto.trimmed();
    recargar();
}

void InventarioController::cambiarVista(const QString& vista)
{
    if (vista != QStringLiteral("todos") && vista != QStringLiteral("critico")
        && vista != QStringLiteral("vencer")) {
        return;
    }
    vista_ = vista;
    recargar();
}

void InventarioController::seleccionar(const QString& productoUuid)
{
    const auto filas = db_.query(
        QStringLiteral("SELECT p.uuid, p.nombre_comercial, coalesce(p.principio_activo, ''), "
                       "coalesce(p.categoria_id, 0), coalesce(p.codigo, ''), p.unidad, "
                       "p.stock_actual, p.stock_minimo, p.precio_compra, p.precio_venta, "
                       "coalesce(p.fecha_caducidad, ''), coalesce(p.proveedor_nombre, ''), "
                       "p.pedido_en_curso, coalesce(p.proveedor_correo, '') "
                       "FROM productos_insumos p WHERE p.uuid = ? AND p.deleted_at IS NULL"),
        {QVariant(productoUuid)});
    if (filas.size() != 1) {
        fallo(QStringLiteral("el producto no existe"));
        return;
    }
    seleccionado_.clear();
    seleccionado_.insert(QStringLiteral("uuid"), textoDeFila(filas.front(), 0));
    seleccionado_.insert(QStringLiteral("nombre"), textoDeFila(filas.front(), 1));
    seleccionado_.insert(QStringLiteral("principio"), textoDeFila(filas.front(), 2));
    seleccionado_.insert(QStringLiteral("categoriaId"), enteroDeFila(filas.front(), 3));
    seleccionado_.insert(QStringLiteral("codigo"), textoDeFila(filas.front(), 4));
    seleccionado_.insert(QStringLiteral("unidad"), textoDeFila(filas.front(), 5));
    seleccionado_.insert(QStringLiteral("stock"), enteroDeFila(filas.front(), 6));
    seleccionado_.insert(QStringLiteral("minimo"), enteroDeFila(filas.front(), 7));
    seleccionado_.insert(QStringLiteral("compra"), realDeFila(filas.front(), 8));
    seleccionado_.insert(QStringLiteral("venta"), realDeFila(filas.front(), 9));
    seleccionado_.insert(QStringLiteral("caducidad"), textoDeFila(filas.front(), 10));
    seleccionado_.insert(QStringLiteral("proveedor"), textoDeFila(filas.front(), 11));
    seleccionado_.insert(QStringLiteral("pedido"), enteroDeFila(filas.front(), 12) == 1);
    seleccionado_.insert(QStringLiteral("proveedorCorreo"), textoDeFila(filas.front(), 13));
    emit seleccionCambiada();
}

void InventarioController::limpiarSeleccion()
{
    seleccionado_.clear();
    emit seleccionCambiada();
}

void InventarioController::limpiarMensajes()
{
    error_.clear();
    mensaje_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

void InventarioController::crearProducto(const QString& nombre, const QString& principio,
                                         const QString& categoriaUuid, const QString& codigo,
                                         const QString& unidad, int stock, int minimo,
                                         double compra, double venta,
                                         const QString& caducidad, const QString& proveedor)
{
    if (nombre.trimmed().size() < 2) {
        fallo(QStringLiteral("el nombre del producto es obligatorio"));
        return;
    }
    if (compra < 0 || venta < 0 || stock < 0 || minimo < 0) {
        fallo(QStringLiteral("los valores del producto no pueden ser negativos"));
        return;
    }
    qint64 categoriaId = 0;
    if (!categoriaUuid.isEmpty()) {
        const auto categoria = db_.query(
            QStringLiteral("SELECT id FROM categorias_inventario WHERE uuid = ?"),
            {QVariant(categoriaUuid)});
        categoriaId = categoria.empty() ? 0 : enteroDeFila(categoria.front(), 0);
    }
    const qint64 ahora = QDateTime::currentMSecsSinceEpoch();
    const auto alta = db_.execute(
        QStringLiteral("INSERT INTO productos_insumos "
                       "(uuid, nombre_comercial, principio_activo, categoria_id, codigo, "
                       " unidad, stock_actual, stock_minimo, precio_compra, precio_venta, "
                       " fecha_caducidad, proveedor_nombre, pedido_en_curso, created_at, "
                       " updated_at, sync_state) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, ?, ?, 'pendiente')"),
        {QVariant(fuzzy::core::uuidNuevo()), QVariant(nombre.trimmed()),
         principio.trimmed().isEmpty() ? QVariant() : QVariant(principio.trimmed()),
         categoriaId > 0 ? QVariant::fromValue(categoriaId) : QVariant(),
         codigo.trimmed().isEmpty() ? QVariant() : QVariant(codigo.trimmed()),
         QVariant(unidad.trimmed().isEmpty() ? QStringLiteral("unidades") : unidad.trimmed()),
         QVariant::fromValue(stock), QVariant::fromValue(minimo),
         QVariant::fromValue(compra), QVariant::fromValue(venta),
         caducidad.trimmed().isEmpty() ? QVariant() : QVariant(caducidad.trimmed()),
         proveedor.trimmed().isEmpty() ? QVariant() : QVariant(proveedor.trimmed()),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!alta.ok) {
        fallo(QStringLiteral("no se pudo crear el producto"));
        return;
    }
    recargar();
    exito(QStringLiteral("%1 añadido al inventario.").arg(nombre.trimmed()));
}

void InventarioController::actualizarProducto(const QString& productoUuid, const QString& nombre,
                                              const QString& principio,
                                              const QString& categoriaUuid,
                                              const QString& codigo, const QString& unidad,
                                              int stock, int minimo, double compra,
                                              double venta, const QString& caducidad,
                                              const QString& proveedor)
{
    if (productoUuid.isEmpty() || nombre.trimmed().size() < 2) {
        fallo(QStringLiteral("el nombre del producto es obligatorio"));
        return;
    }
    qint64 categoriaId = 0;
    if (!categoriaUuid.isEmpty()) {
        const auto categoria = db_.query(
            QStringLiteral("SELECT id FROM categorias_inventario WHERE uuid = ?"),
            {QVariant(categoriaUuid)});
        categoriaId = categoria.empty() ? 0 : enteroDeFila(categoria.front(), 0);
    }
    const auto cambio = db_.execute(
        QStringLiteral("UPDATE productos_insumos SET nombre_comercial = ?, "
                       "principio_activo = ?, categoria_id = ?, codigo = ?, unidad = ?, "
                       "stock_actual = ?, stock_minimo = ?, precio_compra = ?, "
                       "precio_venta = ?, fecha_caducidad = ?, proveedor_nombre = ?, "
                       "updated_at = ? WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(nombre.trimmed()),
         principio.trimmed().isEmpty() ? QVariant() : QVariant(principio.trimmed()),
         categoriaId > 0 ? QVariant::fromValue(categoriaId) : QVariant(),
         codigo.trimmed().isEmpty() ? QVariant() : QVariant(codigo.trimmed()),
         QVariant(unidad.trimmed().isEmpty() ? QStringLiteral("unidades") : unidad.trimmed()),
         QVariant::fromValue(stock), QVariant::fromValue(minimo),
         QVariant::fromValue(compra), QVariant::fromValue(venta),
         caducidad.trimmed().isEmpty() ? QVariant() : QVariant(caducidad.trimmed()),
         proveedor.trimmed().isEmpty() ? QVariant() : QVariant(proveedor.trimmed()),
         QVariant::fromValue(QDateTime::currentMSecsSinceEpoch()), QVariant(productoUuid)});
    if (!cambio.ok || cambio.affected == 0) {
        fallo(QStringLiteral("no se pudo actualizar el producto"));
        return;
    }
    recargar();
    seleccionar(productoUuid);
    exito(QStringLiteral("Producto actualizado."));
}

void InventarioController::ajustarStock(const QString& productoUuid, int delta,
                                        const QString& motivo)
{
    const auto filas = db_.query(
        QStringLiteral("SELECT id, stock_actual FROM productos_insumos "
                       "WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(productoUuid)});
    if (filas.size() != 1) {
        fallo(QStringLiteral("el producto no existe"));
        return;
    }
    const qint64 id = enteroDeFila(filas.front(), 0);
    const int actual = static_cast<int>(enteroDeFila(filas.front(), 1));
    const int nuevo = actual + delta;
    if (nuevo < 0) {
        fallo(QStringLiteral("el stock no puede quedar negativo"));
        return;
    }
    const auto cambio = db_.execute(
        QStringLiteral("UPDATE productos_insumos SET stock_actual = ?, updated_at = ? "
                       "WHERE id = ?"),
        {QVariant::fromValue(nuevo),
         QVariant::fromValue(QDateTime::currentMSecsSinceEpoch()),
         QVariant::fromValue(id)});
    if (!cambio.ok) {
        fallo(QStringLiteral("no se pudo ajustar el stock"));
        return;
    }
    recargar();
    seleccionar(productoUuid);
    exito(QStringLiteral("Stock ajustado (%1) %2 %3.")
              .arg(delta > 0 ? QStringLiteral("+%1").arg(delta) : QString::number(delta),
                   motivo.trimmed(), delta > 0 ? QStringLiteral("unidades")
                                               : QStringLiteral("unidades")));
}

void InventarioController::marcarPedido(const QString& productoUuid, bool activo)
{
    const auto cambio = db_.execute(
        QStringLiteral("UPDATE productos_insumos SET pedido_en_curso = ?, updated_at = ? "
                       "WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant::fromValue(activo ? 1 : 0),
         QVariant::fromValue(QDateTime::currentMSecsSinceEpoch()), QVariant(productoUuid)});
    if (!cambio.ok || cambio.affected == 0) {
        fallo(QStringLiteral("no se pudo actualizar el pedido"));
        return;
    }
    recargar();
    seleccionar(productoUuid);
    exito(activo ? QStringLiteral("Pedido marcado en curso.")
                 : QStringLiteral("Pedido marcado como recibido."));
}

} // namespace fuzzy::desktop
