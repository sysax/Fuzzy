#include "propietarios/PropietariosController.h"

#include "common/Consulta.h"
#include "fuzzy/core/Uuid.h"

#include <QDateTime>
#include <QVariant>

namespace fuzzy::desktop {

PropietariosController::PropietariosController(fuzzy::core::IQueryExecutor& db, QObject* parent)
    : QObject(parent), db_(db)
{
    recargar();
}

void PropietariosController::fallo(const QString& texto)
{
    error_ = texto;
    mensaje_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

void PropietariosController::exito(const QString& texto)
{
    mensaje_ = texto;
    error_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

void PropietariosController::recargar()
{
    QString sql =
        QStringLiteral("SELECT cl.uuid, cl.nombre_completo, cl.telefono, cl.correo, "
                       "cl.direccion, cl.dni, cl.activo, "
                       "(SELECT count(*) FROM mascotas m WHERE m.cliente_id = cl.id "
                       " AND m.deleted_at IS NULL), "
                       "(SELECT group_concat(m.nombre, ', ') FROM mascotas m "
                       " WHERE m.cliente_id = cl.id AND m.deleted_at IS NULL) "
                       "FROM clientes cl WHERE cl.deleted_at IS NULL ");
    std::vector<QVariant> params;
    if (!filtro_.isEmpty()) {
        sql += QStringLiteral("AND (lower(cl.nombre_completo) LIKE ? "
                              "OR lower(coalesce(cl.telefono, '')) LIKE ? "
                              "OR lower(coalesce(cl.correo, '')) LIKE ? "
                              "OR lower(coalesce(cl.dni, '')) LIKE ?) ");
        const QString patron = QStringLiteral("%") + filtro_.toLower() + QStringLiteral("%");
        for (int i = 0; i < 4; ++i) {
            params.push_back(QVariant(patron));
        }
    }
    sql += QStringLiteral("ORDER BY cl.nombre_completo COLLATE NOCASE LIMIT 200");

    const auto filas = db_.query(sql, params);
    propietarios_.clear();
    for (const auto& fila : filas) {
        QVariantMap item;
        item.insert(QStringLiteral("uuid"), textoDeFila(fila, 0));
        item.insert(QStringLiteral("nombre"), textoDeFila(fila, 1));
        item.insert(QStringLiteral("telefono"), textoDeFila(fila, 2));
        item.insert(QStringLiteral("correo"), textoDeFila(fila, 3));
        item.insert(QStringLiteral("direccion"), textoDeFila(fila, 4));
        item.insert(QStringLiteral("dni"), textoDeFila(fila, 5));
        item.insert(QStringLiteral("activo"), enteroDeFila(fila, 6) == 1);
        item.insert(QStringLiteral("mascotas"), enteroDeFila(fila, 7));
        item.insert(QStringLiteral("mascotasTexto"), textoDeFila(fila, 8));
        propietarios_.append(item);
    }

    // Las cifras de cabecera describen la lista filtrada que ve el usuario.
    total_ = static_cast<int>(propietarios_.size());
    conMascotas_ = 0;
    for (const auto& item : propietarios_) {
        if (item.toMap().value(QStringLiteral("mascotas")).toInt() > 0) {
            ++conMascotas_;
        }
    }
    emit datosCambiados();
}

void PropietariosController::cargar()
{
    recargar();
}

void PropietariosController::buscar(const QString& texto)
{
    filtro_ = texto.trimmed();
    recargar();
}

void PropietariosController::seleccionar(const QString& clienteUuid)
{
    const auto filas = db_.query(
        QStringLiteral("SELECT uuid, nombre_completo, telefono, correo, direccion, dni, "
                       "activo, created_at FROM clientes "
                       "WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(clienteUuid)});
    if (filas.size() != 1) {
        fallo(QStringLiteral("el propietario no existe"));
        return;
    }
    seleccionado_.clear();
    seleccionado_.insert(QStringLiteral("uuid"), textoDeFila(filas.front(), 0));
    seleccionado_.insert(QStringLiteral("nombre"), textoDeFila(filas.front(), 1));
    seleccionado_.insert(QStringLiteral("telefono"), textoDeFila(filas.front(), 2));
    seleccionado_.insert(QStringLiteral("correo"), textoDeFila(filas.front(), 3));
    seleccionado_.insert(QStringLiteral("direccion"), textoDeFila(filas.front(), 4));
    seleccionado_.insert(QStringLiteral("dni"), textoDeFila(filas.front(), 5));
    seleccionado_.insert(QStringLiteral("activo"), enteroDeFila(filas.front(), 6) == 1);
    seleccionado_.insert(QStringLiteral("alta"), fechaTexto(enteroDeFila(filas.front(), 7)));

    mascotas_ = listaDeFilas(
        db_.query(QStringLiteral("SELECT uuid, nombre, especie, raza, activo "
                                 "FROM mascotas WHERE cliente_id = (SELECT id FROM clientes "
                                 "WHERE uuid = ?) AND deleted_at IS NULL ORDER BY nombre"),
                 {QVariant(clienteUuid)}),
        {QStringLiteral("uuid"), QStringLiteral("nombre"), QStringLiteral("especie"),
         QStringLiteral("raza"), QStringLiteral("activo")});
    emit seleccionCambiada();
}

void PropietariosController::limpiarSeleccion()
{
    seleccionado_.clear();
    mascotas_.clear();
    emit seleccionCambiada();
}

void PropietariosController::limpiarMensajes()
{
    error_.clear();
    mensaje_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

void PropietariosController::crear(const QString& nombre, const QString& telefono,
                                   const QString& correo, const QString& direccion,
                                   const QString& dni)
{
    if (nombre.trimmed().size() < 3) {
        fallo(QStringLiteral("el nombre del propietario es obligatorio"));
        return;
    }
    const qint64 ahora = QDateTime::currentMSecsSinceEpoch();
    const auto alta = db_.execute(
        QStringLiteral("INSERT INTO clientes "
                       "(uuid, nombre_completo, telefono, correo, direccion, dni, activo, "
                       " created_at, updated_at, sync_state) "
                       "VALUES (?, ?, ?, ?, ?, ?, 1, ?, ?, 'pendiente')"),
        {QVariant(fuzzy::core::uuidNuevo()), QVariant(nombre.trimmed()),
         telefono.trimmed().isEmpty() ? QVariant() : QVariant(telefono.trimmed()),
         correo.trimmed().isEmpty() ? QVariant() : QVariant(correo.trimmed().toLower()),
         direccion.trimmed().isEmpty() ? QVariant() : QVariant(direccion.trimmed()),
         dni.trimmed().isEmpty() ? QVariant() : QVariant(dni.trimmed()),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!alta.ok) {
        fallo(QStringLiteral("no se pudo registrar el propietario"));
        return;
    }
    recargar();
    exito(QStringLiteral("%1 quedó registrado.").arg(nombre.trimmed()));
}

void PropietariosController::actualizar(const QString& clienteUuid, const QString& nombre,
                                        const QString& telefono, const QString& correo,
                                        const QString& direccion, const QString& dni,
                                        bool activo)
{
    if (clienteUuid.isEmpty() || nombre.trimmed().size() < 3) {
        fallo(QStringLiteral("el nombre del propietario es obligatorio"));
        return;
    }
    const auto cambio = db_.execute(
        QStringLiteral("UPDATE clientes SET nombre_completo = ?, telefono = ?, correo = ?, "
                       "direccion = ?, dni = ?, activo = ?, updated_at = ? "
                       "WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(nombre.trimmed()),
         telefono.trimmed().isEmpty() ? QVariant() : QVariant(telefono.trimmed()),
         correo.trimmed().isEmpty() ? QVariant() : QVariant(correo.trimmed().toLower()),
         direccion.trimmed().isEmpty() ? QVariant() : QVariant(direccion.trimmed()),
         dni.trimmed().isEmpty() ? QVariant() : QVariant(dni.trimmed()),
         QVariant::fromValue(activo ? 1 : 0),
         QVariant::fromValue(QDateTime::currentMSecsSinceEpoch()), QVariant(clienteUuid)});
    if (!cambio.ok || cambio.affected == 0) {
        fallo(QStringLiteral("no se pudo actualizar el propietario"));
        return;
    }
    recargar();
    seleccionar(clienteUuid);
    exito(QStringLiteral("Ficha del propietario actualizada."));
}

} // namespace fuzzy::desktop
