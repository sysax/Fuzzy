#include "fuzzy/core/ServicioClinico.h"

#include "fuzzy/core/Uuid.h"

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QVariant>

#include <cmath>

namespace fuzzy::core {

namespace {

qint64 ahoraMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

QString textoODefecto(const QString& texto, const QString& defecto = QString())
{
    const QString limpio = texto.trimmed();
    return limpio.isEmpty() ? defecto : limpio;
}

} // namespace

ServicioClinico::ServicioClinico(IQueryExecutor& clinica, Politicas politicas)
    : clinica_(clinica), politicas_(std::move(politicas))
{
}

QString ServicioClinico::clinicaUuid() const
{
    const auto filas = clinica_.query(
        QStringLiteral("SELECT uuid FROM clinicas WHERE deleted_at IS NULL LIMIT 1"));
    return filas.empty() ? QString() : filas.front().values.front().toString();
}

bool ServicioClinico::permitido(const Sesion& sesion, Permiso permiso, Accion accion,
                                const QString& recurso) const
{
    const ContextoPermiso contexto{recurso, accion, QString(), clinicaUuid()};
    return politicas_.exigir(sesion, permiso, contexto);
}

ResultadoClinico ServicioClinico::denegado(const Sesion& sesion, Permiso permiso,
                                           const QString& recurso) const
{
    Q_UNUSED(sesion);
    Q_UNUSED(permiso);
    return {false, QStringLiteral("sin permiso para operar sobre %1").arg(recurso), {}};
}

qint64 ServicioClinico::idDe(const QString& tabla, const QString& uuid,
                             const QString& extra) const
{
    if (uuid.isEmpty()) {
        return -1;
    }
    const auto filas = clinica_.query(
        QStringLiteral("SELECT id FROM %1 WHERE uuid = ? %2").arg(tabla, extra),
        {QVariant(uuid)});
    return filas.empty() ? -1 : filas.front().values.front().toLongLong();
}

qint64 ServicioClinico::usuarioId(const Sesion& sesion) const
{
    return idDe(QStringLiteral("usuarios"), sesion.cuenta_id,
                QStringLiteral("AND deleted_at IS NULL"));
}

void ServicioClinico::auditar(const Sesion& sesion, const QString& accion,
                              const QString& entidad, const QString& entidad_uuid,
                              const QString& detalle)
{
    clinica_.execute(
        QStringLiteral("INSERT INTO auditoria "
                       "(uuid, usuario_id, accion, entidad, entidad_uuid, detalle, "
                       " created_at, sync_state) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, 'pendiente')"),
        {QVariant(uuidNuevo()),
         usuarioId(sesion) > 0 ? QVariant::fromValue(usuarioId(sesion)) : QVariant(),
         QVariant(accion), QVariant(entidad),
         entidad_uuid.isEmpty() ? QVariant() : QVariant(entidad_uuid),
         QVariant(detalle), QVariant::fromValue(ahoraMs())});
}

ResultadoClinico ServicioClinico::recibir(const Sesion& sesion, const QString& cita_uuid)
{
    if (!permitido(sesion, Permiso::GestionarCitas, Accion::Crear,
                   QStringLiteral("citas"))) {
        return denegado(sesion, Permiso::GestionarCitas, QStringLiteral("citas"));
    }
    const auto filas = clinica_.query(
        QStringLiteral("SELECT id, fecha_llegada, estado_cita_id FROM citas "
                       "WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(cita_uuid)});
    if (filas.size() != 1) {
        return {false, QStringLiteral("la cita no existe"), {}};
    }
    if (!filas.front().values.at(1).isNull()) {
        return {false, QStringLiteral("la cita ya fue recibida"), {}};
    }

    const qint64 ahora = ahoraMs();
    const auto cambio = clinica_.execute(
        QStringLiteral("UPDATE citas SET fecha_llegada = ?, "
                       " estado_cita_id = (SELECT id FROM estados_cita "
                       "                    WHERE nombre_estado = 'En espera'), "
                       " fecha_actualizacion = ?, updated_at = ? WHERE id = ?"),
        {QVariant::fromValue(ahora), QVariant::fromValue(ahora),
         QVariant::fromValue(ahora), filas.front().values.at(0)});
    if (!cambio.ok || cambio.affected == 0) {
        return {false, QStringLiteral("no se pudo marcar la llegada"), {}};
    }
    auditar(sesion, QStringLiteral("recepcion"), QStringLiteral("citas"), cita_uuid,
            QStringLiteral("Recepción y triaje"));
    return {true, {}, cita_uuid};
}

ResultadoClinico ServicioClinico::registrarTriaje(
    const Sesion& sesion, const QString& mascota_uuid, const QString& consulta_uuid,
    double peso, double temperatura, int frecuencia_cardiaca, int frecuencia_respiratoria,
    const QString& notas)
{
    if (!permitido(sesion, Permiso::GestionarCitas, Accion::Crear,
                   QStringLiteral("signos_vitales"))) {
        return denegado(sesion, Permiso::GestionarCitas, QStringLiteral("triaje"));
    }
    const qint64 mascotaId = idDe(QStringLiteral("mascotas"), mascota_uuid,
                                  QStringLiteral("AND deleted_at IS NULL"));
    if (mascotaId <= 0) {
        return {false, QStringLiteral("el paciente no existe"), {}};
    }
    const qint64 consultaId = consulta_uuid.isEmpty()
        ? -1
        : idDe(QStringLiteral("consultas_medicas"), consulta_uuid,
               QStringLiteral("AND deleted_at IS NULL"));
    if (!consulta_uuid.isEmpty() && consultaId <= 0) {
        return {false, QStringLiteral("la consulta no existe"), {}};
    }
    if (peso <= 0 && temperatura <= 0 && frecuencia_cardiaca <= 0
        && frecuencia_respiratoria <= 0) {
        return {false, QStringLiteral("registra al menos un signo vital"), {}};
    }

    const QString uuid = uuidNuevo();
    const qint64 ahora = ahoraMs();
    const auto alta = clinica_.execute(
        QStringLiteral("INSERT INTO signos_vitales "
                       "(uuid, mascota_id, consulta_id, fecha, peso_kg, temperatura_c, "
                       " frecuencia_cardiaca, frecuencia_respiratoria, created_at, updated_at, "
                       " sync_state) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'pendiente')"),
        {QVariant(uuid), QVariant::fromValue(mascotaId),
         consultaId > 0 ? QVariant::fromValue(consultaId) : QVariant(),
         QVariant::fromValue(ahora),
         peso > 0 ? QVariant::fromValue(peso) : QVariant(),
         temperatura > 0 ? QVariant::fromValue(temperatura) : QVariant(),
         frecuencia_cardiaca > 0 ? QVariant::fromValue(frecuencia_cardiaca) : QVariant(),
         frecuencia_respiratoria > 0 ? QVariant::fromValue(frecuencia_respiratoria)
                                     : QVariant(),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!alta.ok) {
        return {false, QStringLiteral("no se pudieron registrar los signos"), {}};
    }
    if (peso > 0) {
        clinica_.execute(QStringLiteral("UPDATE mascotas SET peso_kg = ?, updated_at = ? "
                                        "WHERE id = ?"),
                         {QVariant::fromValue(peso), QVariant::fromValue(ahora),
                          QVariant::fromValue(mascotaId)});
    }
    if (consultaId > 0 && !notas.trimmed().isEmpty()) {
        clinica_.execute(
            QStringLiteral("UPDATE consultas_medicas SET triage_notas = ?, updated_at = ? "
                           "WHERE id = ?"),
            {QVariant(notas.trimmed()), QVariant::fromValue(ahora),
             QVariant::fromValue(consultaId)});
    }
    auditar(sesion, QStringLiteral("triaje"), QStringLiteral("signos_vitales"), uuid,
            QStringLiteral("Signos y notas de recepción"));
    return {true, {}, uuid};
}

ResultadoClinico ServicioClinico::abrirConsulta(const Sesion& sesion,
                                                const QString& mascota_uuid,
                                                const QString& cita_uuid,
                                                const QString& motivo, bool emergencia)
{
    if (!permitido(sesion, Permiso::RegistrarConsulta, Accion::Crear,
                   QStringLiteral("consultas_medicas"))) {
        return denegado(sesion, Permiso::RegistrarConsulta,
                        QStringLiteral("consultas"));
    }
    const qint64 mascotaId = idDe(QStringLiteral("mascotas"), mascota_uuid,
                                  QStringLiteral("AND deleted_at IS NULL"));
    if (mascotaId <= 0) {
        return {false, QStringLiteral("el paciente no existe"), {}};
    }
    if (motivo.trimmed().isEmpty()) {
        return {false, QStringLiteral("el motivo de la consulta es obligatorio"), {}};
    }
    qint64 citaId = -1;
    if (!cita_uuid.isEmpty()) {
        citaId = idDe(QStringLiteral("citas"), cita_uuid,
                      QStringLiteral("AND deleted_at IS NULL"));
        if (citaId <= 0) {
            return {false, QStringLiteral("la cita no existe"), {}};
        }
        const auto existente = clinica_.query(
            QStringLiteral("SELECT count(*) FROM consultas_medicas "
                           "WHERE cita_id = ? AND deleted_at IS NULL"),
            {QVariant::fromValue(citaId)});
        if (!existente.empty() && existente.front().values.front().toInt() > 0) {
            return {false, QStringLiteral("la cita ya tiene una consulta"), {}};
        }
    }

    const QString uuid = uuidNuevo();
    const qint64 ahora = ahoraMs();
    if (!clinica_.execute(QStringLiteral("BEGIN")).ok) {
        return {false, QStringLiteral("no se pudo abrir la consulta"), {}};
    }
    const auto alta = clinica_.execute(
        QStringLiteral("INSERT INTO consultas_medicas "
                       "(uuid, mascota_id, veterinario_id, cita_id, fecha_consulta, "
                       " motivo_visita, es_emergencia, estado, created_at, updated_at, "
                       " sync_state) VALUES (?, ?, ?, ?, ?, ?, ?, 'En curso', ?, ?, 'pendiente')"),
        {QVariant(uuid), QVariant::fromValue(mascotaId),
         usuarioId(sesion) > 0 ? QVariant::fromValue(usuarioId(sesion)) : QVariant(),
         citaId > 0 ? QVariant::fromValue(citaId) : QVariant(),
         QVariant::fromValue(ahora), QVariant(motivo.trimmed()),
         QVariant::fromValue(emergencia ? 1 : 0), QVariant::fromValue(ahora),
         QVariant::fromValue(ahora)});
    if (!alta.ok) {
        clinica_.execute(QStringLiteral("ROLLBACK"));
        return {false, QStringLiteral("no se pudo abrir la consulta"), {}};
    }
    if (citaId > 0) {
        clinica_.execute(
            QStringLiteral("UPDATE citas SET estado_cita_id = "
                           "(SELECT id FROM estados_cita WHERE nombre_estado = 'En curso'), "
                           "fecha_actualizacion = ?, updated_at = ? WHERE id = ?"),
            {QVariant::fromValue(ahora), QVariant::fromValue(ahora),
             QVariant::fromValue(citaId)});
    }
    if (!clinica_.execute(QStringLiteral("COMMIT")).ok) {
        clinica_.execute(QStringLiteral("ROLLBACK"));
        return {false, QStringLiteral("no se pudo confirmar la consulta"), {}};
    }
    auditar(sesion, QStringLiteral("abrir_consulta"), QStringLiteral("consultas_medicas"),
            uuid, motivo.trimmed());
    return {true, {}, uuid};
}

ResultadoClinico ServicioClinico::cerrarConsulta(const Sesion& sesion,
                                                 const QString& consulta_uuid,
                                                 const QString& diagnostico,
                                                 const QString& tratamiento,
                                                 const QString& examen_fisico)
{
    if (!permitido(sesion, Permiso::RegistrarConsulta, Accion::Crear,
                   QStringLiteral("consultas_medicas"))) {
        return denegado(sesion, Permiso::RegistrarConsulta,
                        QStringLiteral("consultas"));
    }
    const auto filas = clinica_.query(
        QStringLiteral("SELECT id, cita_id, estado FROM consultas_medicas "
                       "WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(consulta_uuid)});
    if (filas.size() != 1) {
        return {false, QStringLiteral("la consulta no existe"), {}};
    }
    if (filas.front().values.at(2).toString() == QStringLiteral("Cerrada")) {
        return {false, QStringLiteral("la consulta ya está cerrada"), {}};
    }
    if (diagnostico.trimmed().isEmpty()) {
        return {false, QStringLiteral("el diagnóstico es obligatorio para cerrar"), {}};
    }

    const qint64 ahora = ahoraMs();
    const qint64 consultaId = filas.front().values.at(0).toLongLong();
    const auto cambio = clinica_.execute(
        QStringLiteral("UPDATE consultas_medicas SET diagnostico = ?, "
                       " tratamiento_indicado = ?, examen_fisico = ?, estado = 'Cerrada', "
                       " fecha_cierre = ?, updated_at = ? WHERE id = ?"),
        {QVariant(diagnostico.trimmed()),
         tratamiento.trimmed().isEmpty() ? QVariant() : QVariant(tratamiento.trimmed()),
         examen_fisico.trimmed().isEmpty() ? QVariant() : QVariant(examen_fisico.trimmed()),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora),
         QVariant::fromValue(consultaId)});
    if (!cambio.ok || cambio.affected == 0) {
        return {false, QStringLiteral("no se pudo cerrar la consulta"), {}};
    }
    if (!filas.front().values.at(1).isNull()) {
        clinica_.execute(
            QStringLiteral("UPDATE citas SET estado_cita_id = "
                           "(SELECT id FROM estados_cita WHERE nombre_estado = 'Completada'), "
                           "fecha_actualizacion = ?, updated_at = ? WHERE id = ?"),
            {QVariant::fromValue(ahora), QVariant::fromValue(ahora),
             filas.front().values.at(1)});
    }
    auditar(sesion, QStringLiteral("cerrar_consulta"), QStringLiteral("consultas_medicas"),
            consulta_uuid, diagnostico.trimmed());
    return {true, {}, consulta_uuid};
}

ResultadoClinico ServicioClinico::emitirReceta(const Sesion& sesion,
                                               const QString& consulta_uuid,
                                               const QString& indicaciones,
                                               const QList<LineaReceta>& lineas,
                                               bool descontarStock)
{
    if (!permitido(sesion, Permiso::RegistrarConsulta, Accion::Crear,
                   QStringLiteral("recetas"))) {
        return denegado(sesion, Permiso::RegistrarConsulta,
                        QStringLiteral("recetas"));
    }
    const auto consulta = clinica_.query(
        QStringLiteral("SELECT id, mascota_id, veterinario_id FROM consultas_medicas "
                       "WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(consulta_uuid)});
    if (consulta.size() != 1) {
        return {false, QStringLiteral("la consulta no existe"), {}};
    }
    if (lineas.isEmpty()) {
        return {false, QStringLiteral("la receta necesita al menos un medicamento"), {}};
    }
    for (const LineaReceta& linea : lineas) {
        if (linea.medicamento.trimmed().isEmpty() || linea.dosis.trimmed().isEmpty()
            || linea.frecuencia.trimmed().isEmpty()) {
            return {false, QStringLiteral("medicamento, dosis y frecuencia son obligatorios"),
                    {}};
        }
        if (linea.cantidad <= 0 || linea.precio < 0) {
            return {false, QStringLiteral("cantidad y precio no son válidos"), {}};
        }
    }
    const auto existente = clinica_.query(
        QStringLiteral("SELECT count(*) FROM recetas WHERE consulta_id = ? "
                       "AND deleted_at IS NULL"),
        {consulta.front().values.at(0)});
    if (!existente.empty() && existente.front().values.front().toInt() > 0) {
        return {false, QStringLiteral("la consulta ya tiene una receta"), {}};
    }

    // Validar stock antes de abrir la transacción: si falta algo no se
    // escribe ni la receta ni el movimiento.
    struct Descuento {
        qint64 productoId = -1;
        int cantidad = 0;
        int stock = 0;
    };
    QList<Descuento> descuentos;
    if (descontarStock) {
        for (const LineaReceta& linea : lineas) {
            if (linea.producto_uuid.isEmpty()) {
                continue;
            }
            const qint64 productoId = idDe(QStringLiteral("productos_insumos"),
                                           linea.producto_uuid,
                                           QStringLiteral("AND deleted_at IS NULL"));
            if (productoId <= 0) {
                return {false, QStringLiteral("el producto %1 no existe")
                                   .arg(linea.medicamento),
                        {}};
            }
            const auto stock = clinica_.query(
                QStringLiteral("SELECT stock_actual FROM productos_insumos WHERE id = ?"),
                {QVariant::fromValue(productoId)});
            const int disponible = stock.empty() ? 0 : stock.front().values.front().toInt();
            if (disponible < linea.cantidad) {
                return {false, QStringLiteral("stock insuficiente de %1")
                                   .arg(linea.medicamento),
                        {}};
            }
            descuentos.append({productoId, linea.cantidad, disponible});
        }
    }

    const QString uuid = uuidNuevo();
    const qint64 ahora = ahoraMs();
    if (!clinica_.execute(QStringLiteral("BEGIN")).ok) {
        return {false, QStringLiteral("no se pudo iniciar la receta"), {}};
    }
    const auto alta = clinica_.execute(
        QStringLiteral("INSERT INTO recetas "
                       "(uuid, consulta_id, mascota_id, veterinario_id, fecha_emision, "
                       " indicaciones, created_at, updated_at, sync_state) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 'pendiente')"),
        {QVariant(uuid), consulta.front().values.at(0), consulta.front().values.at(1),
         consulta.front().values.at(2), QVariant::fromValue(ahora),
         indicaciones.trimmed().isEmpty() ? QVariant() : QVariant(indicaciones.trimmed()),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!alta.ok) {
        clinica_.execute(QStringLiteral("ROLLBACK"));
        return {false, QStringLiteral("no se pudo emitir la receta"), {}};
    }
    const qint64 recetaId = alta.last_insert_id.toLongLong();
    for (const LineaReceta& linea : lineas) {
        const qint64 productoId = linea.producto_uuid.isEmpty()
            ? -1
            : idDe(QStringLiteral("productos_insumos"), linea.producto_uuid,
                   QStringLiteral("AND deleted_at IS NULL"));
        const auto detalle = clinica_.execute(
            QStringLiteral("INSERT INTO receta_detalles "
                           "(uuid, receta_id, producto_id, medicamento, dosis, frecuencia, "
                           " duracion, cantidad, precio_unitario, created_at, updated_at, "
                           " sync_state) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'pendiente')"),
            {QVariant(uuidNuevo()), QVariant::fromValue(recetaId),
             productoId > 0 ? QVariant::fromValue(productoId) : QVariant(),
             QVariant(linea.medicamento.trimmed()), QVariant(linea.dosis.trimmed()),
             QVariant(linea.frecuencia.trimmed()),
             linea.duracion.trimmed().isEmpty() ? QVariant() : QVariant(linea.duracion.trimmed()),
             QVariant::fromValue(linea.cantidad), QVariant::fromValue(linea.precio),
             QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
        if (!detalle.ok) {
            clinica_.execute(QStringLiteral("ROLLBACK"));
            return {false, QStringLiteral("no se pudo guardar la línea de la receta"), {}};
        }
    }
    if (descontarStock) {
        for (const Descuento& descuento : descuentos) {
            clinica_.execute(
                QStringLiteral("UPDATE productos_insumos SET stock_actual = stock_actual - ?, "
                               "updated_at = ? WHERE id = ?"),
                {QVariant::fromValue(descuento.cantidad), QVariant::fromValue(ahora),
                 QVariant::fromValue(descuento.productoId)});
            clinica_.execute(
                QStringLiteral("INSERT INTO movimientos_inventario "
                               "(uuid, producto_id, usuario_id, tipo, cantidad, motivo, "
                               " referencia_tipo, referencia_id, created_at, updated_at, "
                               " sync_state) "
                               "VALUES (?, ?, ?, 'Salida', ?, 'Receta', 'Receta', ?, ?, ?, "
                               "        'pendiente')"),
                {QVariant(uuidNuevo()), QVariant::fromValue(descuento.productoId),
                 usuarioId(sesion) > 0 ? QVariant::fromValue(usuarioId(sesion)) : QVariant(),
                 QVariant::fromValue(descuento.cantidad), QVariant::fromValue(recetaId),
                 QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
        }
    }
    if (!clinica_.execute(QStringLiteral("COMMIT")).ok) {
        clinica_.execute(QStringLiteral("ROLLBACK"));
        return {false, QStringLiteral("no se pudo confirmar la receta"), {}};
    }
    auditar(sesion, QStringLiteral("emitir_receta"), QStringLiteral("recetas"), uuid,
            QStringLiteral("%1 líneas").arg(lineas.size()));
    return {true, {}, uuid};
}

bool ServicioClinico::crearCargo(const QString& origen_tipo, qint64 origen_id,
                                 const QString& origen_uuid, const QString& concepto,
                                 double monto, QString* error)
{
    if (concepto.trimmed().isEmpty() || monto < 0) {
        *error = QStringLiteral("concepto y monto del cargo no son válidos");
        return false;
    }

    if (!origen_uuid.isEmpty()) {
        const auto existente = clinica_.query(
            QStringLiteral("SELECT id FROM cargos WHERE origen_tipo = ? AND origen_uuid = ? "
                           "AND deleted_at IS NULL"),
            {QVariant(origen_tipo), QVariant(origen_uuid)});
        if (!existente.empty()) {
            clinica_.execute(
                QStringLiteral("UPDATE cargos SET concepto = ?, monto = ?, updated_at = ? "
                               "WHERE id = ?"),
                {QVariant(concepto.trimmed()), QVariant::fromValue(monto),
                 QVariant::fromValue(ahoraMs()),
                 existente.front().values.front()});
            return true;
        }
    } else {
        const auto existente = clinica_.query(
            QStringLiteral("SELECT id FROM cargos WHERE origen_tipo = ? AND origen_id = ? "
                           "AND origen_uuid IS NULL AND deleted_at IS NULL"),
            {QVariant(origen_tipo), QVariant::fromValue(origen_id)});
        if (!existente.empty()) {
            clinica_.execute(
                QStringLiteral("UPDATE cargos SET concepto = ?, monto = ?, updated_at = ? "
                               "WHERE id = ?"),
                {QVariant(concepto.trimmed()), QVariant::fromValue(monto),
                 QVariant::fromValue(ahoraMs()),
                 existente.front().values.front()});
            return true;
        }
    }

    const qint64 ahora = ahoraMs();
    const auto alta = clinica_.execute(
        QStringLiteral("INSERT INTO cargos "
                       "(uuid, origen_tipo, origen_id, origen_uuid, concepto, monto, estado, "
                       " created_at, updated_at, sync_state) "
                       "VALUES (?, ?, ?, ?, ?, ?, 'Pendiente', ?, ?, 'pendiente')"),
        {QVariant(uuidNuevo()), QVariant(origen_tipo), QVariant::fromValue(origen_id),
         origen_uuid.isEmpty() ? QVariant() : QVariant(origen_uuid),
         QVariant(concepto.trimmed()), QVariant::fromValue(monto),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!alta.ok) {
        *error = QStringLiteral("no se pudo registrar el cargo");
        return false;
    }
    return true;
}

ResultadoClinico ServicioClinico::agregarCargo(const Sesion& sesion,
                                               const QString& origen_tipo,
                                               const QString& origen_uuid,
                                               const QString& concepto, double monto)
{
    if (!permitido(sesion, Permiso::RegistrarConsulta, Accion::Crear,
                   QStringLiteral("cargos"))) {
        return denegado(sesion, Permiso::RegistrarConsulta, QStringLiteral("cargos"));
    }
    static const QStringList tipos = {
        QStringLiteral("Consulta"), QStringLiteral("Receta"),
        QStringLiteral("Laboratorio"), QStringLiteral("Cirugia"),
        QStringLiteral("Hospitalizacion"), QStringLiteral("Producto"),
        QStringLiteral("Servicio"), QStringLiteral("Otro")};
    if (!tipos.contains(origen_tipo)) {
        return {false, QStringLiteral("el tipo de origen no es válido"), {}};
    }

    qint64 origenId = 0;
    if (!origen_uuid.isEmpty()) {
        static const QHash<QString, QString> tablas = {
            {QStringLiteral("Consulta"), QStringLiteral("consultas_medicas")},
            {QStringLiteral("Receta"), QStringLiteral("recetas")},
            {QStringLiteral("Laboratorio"), QStringLiteral("laboratorio_solicitudes")},
            {QStringLiteral("Cirugia"), QStringLiteral("cirugias")},
            {QStringLiteral("Hospitalizacion"), QStringLiteral("hospitalizaciones")},
            {QStringLiteral("Producto"), QStringLiteral("productos_insumos")},
            {QStringLiteral("Servicio"), QStringLiteral("servicios")},
        };
        const auto tabla = tablas.constFind(origen_tipo);
        if (tabla != tablas.constEnd()) {
            origenId = idDe(tabla.value(), origen_uuid,
                            QStringLiteral("AND deleted_at IS NULL"));
        }
    }

    QString error;
    if (!crearCargo(origen_tipo, origenId, origen_uuid, concepto, monto, &error)) {
        return {false, error, {}};
    }
    auditar(sesion, QStringLiteral("cargo"), QStringLiteral("cargos"), origen_uuid,
            QStringLiteral("%1 · %2").arg(origen_tipo, concepto));
    return {true, {}, origen_uuid};
}

ResultadoClinico ServicioClinico::guardarServicio(const Sesion& sesion,
                                                  const QString& servicio_uuid,
                                                  const QString& codigo,
                                                  const QString& nombre, double precio,
                                                  bool activo)
{
    const Accion accion = servicio_uuid.isEmpty() ? Accion::Crear : Accion::Actualizar;
    if (!permitido(sesion, Permiso::ConfigurarClinica, accion,
                   QStringLiteral("servicios"))) {
        return denegado(sesion, Permiso::ConfigurarClinica, QStringLiteral("servicios"));
    }
    if (codigo.trimmed().isEmpty() || nombre.trimmed().isEmpty() || precio < 0) {
        return {false, QStringLiteral("código, nombre y precio son obligatorios"), {}};
    }
    const qint64 ahora = ahoraMs();
    if (servicio_uuid.isEmpty()) {
        const QString uuid = uuidNuevo();
        const auto alta = clinica_.execute(
            QStringLiteral("INSERT INTO servicios "
                           "(uuid, codigo, nombre, precio, activo, created_at, updated_at, "
                           " sync_state) VALUES (?, ?, ?, ?, ?, ?, ?, 'pendiente')"),
            {QVariant(uuid), QVariant(codigo.trimmed()), QVariant(nombre.trimmed()),
             QVariant::fromValue(precio), QVariant::fromValue(activo ? 1 : 0),
             QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
        if (!alta.ok) {
            return {false, QStringLiteral("no se pudo crear el servicio"), {}};
        }
        auditar(sesion, QStringLiteral("alta_servicio"), QStringLiteral("servicios"), uuid,
                nombre.trimmed());
        return {true, {}, uuid};
    }

    const auto cambio = clinica_.execute(
        QStringLiteral("UPDATE servicios SET codigo = ?, nombre = ?, precio = ?, activo = ?, "
                       " updated_at = ? WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(codigo.trimmed()), QVariant(nombre.trimmed()), QVariant::fromValue(precio),
         QVariant::fromValue(activo ? 1 : 0), QVariant::fromValue(ahora),
         QVariant(servicio_uuid)});
    if (!cambio.ok || cambio.affected == 0) {
        return {false, QStringLiteral("el servicio no existe"), {}};
    }
    auditar(sesion, QStringLiteral("editar_servicio"), QStringLiteral("servicios"),
            servicio_uuid, nombre.trimmed());
    return {true, {}, servicio_uuid};
}

ResultadoClinico ServicioClinico::guardarTipoExamen(const Sesion& sesion,
                                                    const QString& examen_uuid,
                                                    const QString& codigo,
                                                    const QString& nombre, bool activo)
{
    const Accion accion = examen_uuid.isEmpty() ? Accion::Crear : Accion::Actualizar;
    if (!permitido(sesion, Permiso::ConfigurarClinica, accion,
                   QStringLiteral("tipos_examen_laboratorio"))) {
        return denegado(sesion, Permiso::ConfigurarClinica,
                        QStringLiteral("catálogo de laboratorio"));
    }
    if (codigo.trimmed().isEmpty() || nombre.trimmed().isEmpty()) {
        return {false, QStringLiteral("código y nombre son obligatorios"), {}};
    }
    const qint64 ahora = ahoraMs();
    if (examen_uuid.isEmpty()) {
        const QString uuid = uuidNuevo();
        const auto alta = clinica_.execute(
            QStringLiteral("INSERT INTO tipos_examen_laboratorio "
                           "(uuid, codigo, nombre, activo, created_at, updated_at, sync_state) "
                           "VALUES (?, ?, ?, ?, ?, ?, 'pendiente')"),
            {QVariant(uuid), QVariant(codigo.trimmed()), QVariant(nombre.trimmed()),
             QVariant::fromValue(activo ? 1 : 0), QVariant::fromValue(ahora),
             QVariant::fromValue(ahora)});
        if (!alta.ok) {
            return {false, QStringLiteral("no se pudo crear el examen"), {}};
        }
        auditar(sesion, QStringLiteral("alta_examen"),
                QStringLiteral("tipos_examen_laboratorio"), uuid, nombre.trimmed());
        return {true, {}, uuid};
    }
    const auto cambio = clinica_.execute(
        QStringLiteral("UPDATE tipos_examen_laboratorio SET codigo = ?, nombre = ?, "
                       " activo = ?, updated_at = ? WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(codigo.trimmed()), QVariant(nombre.trimmed()),
         QVariant::fromValue(activo ? 1 : 0), QVariant::fromValue(ahora),
         QVariant(examen_uuid)});
    if (!cambio.ok || cambio.affected == 0) {
        return {false, QStringLiteral("el examen no existe"), {}};
    }
    auditar(sesion, QStringLiteral("editar_examen"),
            QStringLiteral("tipos_examen_laboratorio"), examen_uuid, nombre.trimmed());
    return {true, {}, examen_uuid};
}

ResultadoClinico ServicioClinico::solicitarLaboratorio(
    const Sesion& sesion, const QString& mascota_uuid, const QString& consulta_uuid,
    const QString& tipo_examen_uuid, const QString& prioridad, const QString& notas)
{
    if (!permitido(sesion, Permiso::RegistrarConsulta, Accion::Crear,
                   QStringLiteral("laboratorio_solicitudes"))) {
        return denegado(sesion, Permiso::RegistrarConsulta,
                        QStringLiteral("laboratorio"));
    }
    const qint64 mascotaId = idDe(QStringLiteral("mascotas"), mascota_uuid,
                                  QStringLiteral("AND deleted_at IS NULL"));
    if (mascotaId <= 0) {
        return {false, QStringLiteral("el paciente no existe"), {}};
    }
    const auto examen = clinica_.query(
        QStringLiteral("SELECT id, nombre FROM tipos_examen_laboratorio "
                       "WHERE uuid = ? AND deleted_at IS NULL AND activo = 1"),
        {QVariant(tipo_examen_uuid)});
    if (examen.size() != 1) {
        return {false, QStringLiteral("el tipo de examen no existe o está inactivo"), {}};
    }
    const qint64 consultaId = consulta_uuid.isEmpty()
        ? -1
        : idDe(QStringLiteral("consultas_medicas"), consulta_uuid,
               QStringLiteral("AND deleted_at IS NULL"));
    if (!consulta_uuid.isEmpty() && consultaId <= 0) {
        return {false, QStringLiteral("la consulta no existe"), {}};
    }
    if (prioridad != QStringLiteral("Normal")
        && prioridad != QStringLiteral("Urgente")) {
        return {false, QStringLiteral("la prioridad no es válida"), {}};
    }

    const QString uuid = uuidNuevo();
    const qint64 ahora = ahoraMs();
    const auto alta = clinica_.execute(
        QStringLiteral("INSERT INTO laboratorio_solicitudes "
                       "(uuid, mascota_id, consulta_id, veterinario_id, tipo_examen_id, "
                       " tipo_examen, prioridad, estado, fecha_solicitud, observaciones, "
                       " created_at, updated_at, sync_state) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, 'Solicitado', ?, ?, ?, ?, 'pendiente')"),
        {QVariant(uuid), QVariant::fromValue(mascotaId),
         consultaId > 0 ? QVariant::fromValue(consultaId) : QVariant(),
         usuarioId(sesion) > 0 ? QVariant::fromValue(usuarioId(sesion)) : QVariant(),
         examen.front().values.at(0), examen.front().values.at(1), QVariant(prioridad),
         QVariant::fromValue(ahora),
         notas.trimmed().isEmpty() ? QVariant() : QVariant(notas.trimmed()),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!alta.ok) {
        return {false, QStringLiteral("no se pudo solicitar el examen"), {}};
    }
    auditar(sesion, QStringLiteral("solicitud_laboratorio"),
            QStringLiteral("laboratorio_solicitudes"), uuid, examen.front().values.at(1).toString());
    return {true, {}, uuid};
}

ResultadoClinico ServicioClinico::iniciarLaboratorio(const Sesion& sesion,
                                                     const QString& solicitud_uuid)
{
    if (!permitido(sesion, Permiso::RegistrarConsulta, Accion::Crear,
                   QStringLiteral("laboratorio_solicitudes"))) {
        return denegado(sesion, Permiso::RegistrarConsulta,
                        QStringLiteral("laboratorio"));
    }
    const auto cambio = clinica_.execute(
        QStringLiteral("UPDATE laboratorio_solicitudes SET estado = 'En proceso', "
                       " updated_at = ? WHERE uuid = ? AND estado = 'Solicitado' "
                       " AND deleted_at IS NULL"),
        {QVariant::fromValue(ahoraMs()), QVariant(solicitud_uuid)});
    if (!cambio.ok || cambio.affected == 0) {
        return {false, QStringLiteral("la solicitud no está pendiente"), {}};
    }
    auditar(sesion, QStringLiteral("laboratorio_en_proceso"),
            QStringLiteral("laboratorio_solicitudes"), solicitud_uuid, QString());
    return {true, {}, solicitud_uuid};
}

ResultadoClinico ServicioClinico::registrarResultadoLaboratorio(
    const Sesion& sesion, const QString& solicitud_uuid, const QString& resultado)
{
    if (!permitido(sesion, Permiso::RegistrarConsulta, Accion::Crear,
                   QStringLiteral("laboratorio_solicitudes"))) {
        return denegado(sesion, Permiso::RegistrarConsulta,
                        QStringLiteral("laboratorio"));
    }
    if (resultado.trimmed().isEmpty()) {
        return {false, QStringLiteral("el resultado no puede estar vacío"), {}};
    }
    const qint64 ahora = ahoraMs();
    const auto cambio = clinica_.execute(
        QStringLiteral("UPDATE laboratorio_solicitudes SET resultado = ?, "
                       " fecha_resultado = ?, estado = 'Listo', updated_at = ? "
                       "WHERE uuid = ? AND estado IN ('Solicitado', 'En proceso') "
                       " AND deleted_at IS NULL"),
        {QVariant(resultado.trimmed()), QVariant::fromValue(ahora),
         QVariant::fromValue(ahora), QVariant(solicitud_uuid)});
    if (!cambio.ok || cambio.affected == 0) {
        return {false, QStringLiteral("la solicitud no admite resultado"), {}};
    }
    auditar(sesion, QStringLiteral("resultado_laboratorio"),
            QStringLiteral("laboratorio_solicitudes"), solicitud_uuid, QString());
    return {true, {}, solicitud_uuid};
}

ResultadoClinico ServicioClinico::entregarLaboratorio(const Sesion& sesion,
                                                      const QString& solicitud_uuid)
{
    if (!permitido(sesion, Permiso::RegistrarConsulta, Accion::Crear,
                   QStringLiteral("laboratorio_solicitudes"))) {
        return denegado(sesion, Permiso::RegistrarConsulta,
                        QStringLiteral("laboratorio"));
    }
    const auto cambio = clinica_.execute(
        QStringLiteral("UPDATE laboratorio_solicitudes SET estado = 'Entregado', "
                       " updated_at = ? WHERE uuid = ? AND estado = 'Listo' "
                       " AND deleted_at IS NULL"),
        {QVariant::fromValue(ahoraMs()), QVariant(solicitud_uuid)});
    if (!cambio.ok || cambio.affected == 0) {
        return {false, QStringLiteral("el resultado aún no está listo"), {}};
    }
    auditar(sesion, QStringLiteral("entrega_laboratorio"),
            QStringLiteral("laboratorio_solicitudes"), solicitud_uuid, QString());
    return {true, {}, solicitud_uuid};
}

ResultadoClinico ServicioClinico::ingresarHospitalizacion(
    const Sesion& sesion, const QString& mascota_uuid, const QString& consulta_uuid,
    const QString& motivo, const QString& diagnostico, const QString& area)
{
    if (!permitido(sesion, Permiso::RegistrarConsulta, Accion::Crear,
                   QStringLiteral("hospitalizaciones"))) {
        return denegado(sesion, Permiso::RegistrarConsulta,
                        QStringLiteral("hospitalización"));
    }
    const qint64 mascotaId = idDe(QStringLiteral("mascotas"), mascota_uuid,
                                  QStringLiteral("AND deleted_at IS NULL"));
    if (mascotaId <= 0) {
        return {false, QStringLiteral("el paciente no existe"), {}};
    }
    if (motivo.trimmed().isEmpty()) {
        return {false, QStringLiteral("el motivo de ingreso es obligatorio"), {}};
    }
    const qint64 consultaId = consulta_uuid.isEmpty()
        ? -1
        : idDe(QStringLiteral("consultas_medicas"), consulta_uuid,
               QStringLiteral("AND deleted_at IS NULL"));

    const QString uuid = uuidNuevo();
    const qint64 ahora = ahoraMs();
    const auto alta = clinica_.execute(
        QStringLiteral("INSERT INTO hospitalizaciones "
                       "(uuid, mascota_id, consulta_id, veterinario_id, fecha_ingreso, "
                       " motivo, diagnostico, area, estado, notas, created_at, updated_at, "
                       " sync_state) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 'Activa', ?, ?, ?, 'pendiente')"),
        {QVariant(uuid), QVariant::fromValue(mascotaId),
         consultaId > 0 ? QVariant::fromValue(consultaId) : QVariant(),
         usuarioId(sesion) > 0 ? QVariant::fromValue(usuarioId(sesion)) : QVariant(),
         QVariant::fromValue(ahora), QVariant(motivo.trimmed()),
         diagnostico.trimmed().isEmpty() ? QVariant() : QVariant(diagnostico.trimmed()),
         area.trimmed().isEmpty() ? QVariant() : QVariant(area.trimmed()),
         QVariant(motivo.trimmed()), QVariant::fromValue(ahora),
         QVariant::fromValue(ahora)});
    if (!alta.ok) {
        return {false, QStringLiteral("no se pudo registrar el ingreso"), {}};
    }
    auditar(sesion, QStringLiteral("ingreso_hospitalizacion"),
            QStringLiteral("hospitalizaciones"), uuid, motivo.trimmed());
    return {true, {}, uuid};
}

ResultadoClinico ServicioClinico::registrarEvolucion(
    const Sesion& sesion, const QString& hospitalizacion_uuid, const QString& nota,
    double peso, double temperatura, int frecuencia_cardiaca)
{
    if (!permitido(sesion, Permiso::RegistrarConsulta, Accion::Crear,
                   QStringLiteral("hospitalizacion_evoluciones"))) {
        return denegado(sesion, Permiso::RegistrarConsulta,
                        QStringLiteral("hospitalización"));
    }
    const auto ingreso = clinica_.query(
        QStringLiteral("SELECT id FROM hospitalizaciones WHERE uuid = ? "
                       "AND estado = 'Activa' AND deleted_at IS NULL"),
        {QVariant(hospitalizacion_uuid)});
    if (ingreso.size() != 1) {
        return {false, QStringLiteral("la hospitalización no está activa"), {}};
    }
    if (nota.trimmed().isEmpty()) {
        return {false, QStringLiteral("la evolución necesita una nota"), {}};
    }
    const QString uuid = uuidNuevo();
    const qint64 ahora = ahoraMs();
    const auto alta = clinica_.execute(
        QStringLiteral("INSERT INTO hospitalizacion_evoluciones "
                       "(uuid, hospitalizacion_id, usuario_id, fecha, nota, peso_kg, "
                       " temperatura_c, frecuencia_cardiaca, created_at, updated_at, "
                       " sync_state) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'pendiente')"),
        {QVariant(uuid), ingreso.front().values.at(0),
         usuarioId(sesion) > 0 ? QVariant::fromValue(usuarioId(sesion)) : QVariant(),
         QVariant::fromValue(ahora), QVariant(nota.trimmed()),
         peso > 0 ? QVariant::fromValue(peso) : QVariant(),
         temperatura > 0 ? QVariant::fromValue(temperatura) : QVariant(),
         frecuencia_cardiaca > 0 ? QVariant::fromValue(frecuencia_cardiaca) : QVariant(),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!alta.ok) {
        return {false, QStringLiteral("no se pudo registrar la evolución"), {}};
    }
    clinica_.execute(QStringLiteral("UPDATE hospitalizaciones SET updated_at = ? WHERE id = ?"),
                     {QVariant::fromValue(ahora), ingreso.front().values.at(0)});
    auditar(sesion, QStringLiteral("evolucion"), QStringLiteral("hospitalizaciones"),
            hospitalizacion_uuid, nota.trimmed());
    return {true, {}, uuid};
}

ResultadoClinico ServicioClinico::darAltaHospitalizacion(
    const Sesion& sesion, const QString& hospitalizacion_uuid, const QString& estado)
{
    if (!permitido(sesion, Permiso::RegistrarConsulta, Accion::Crear,
                   QStringLiteral("hospitalizaciones"))) {
        return denegado(sesion, Permiso::RegistrarConsulta,
                        QStringLiteral("hospitalización"));
    }
    const QString destino = estado.isEmpty() ? QStringLiteral("Alta") : estado;
    if (destino != QStringLiteral("Alta") && destino != QStringLiteral("Traslado")
        && destino != QStringLiteral("Fallecido")) {
        return {false, QStringLiteral("el estado de alta no es válido"), {}};
    }
    const qint64 ahora = ahoraMs();
    const auto cambio = clinica_.execute(
        QStringLiteral("UPDATE hospitalizaciones SET estado = ?, fecha_alta = ?, "
                       " updated_at = ? WHERE uuid = ? AND estado = 'Activa' "
                       " AND deleted_at IS NULL"),
        {QVariant(destino), QVariant::fromValue(ahora), QVariant::fromValue(ahora),
         QVariant(hospitalizacion_uuid)});
    if (!cambio.ok || cambio.affected == 0) {
        return {false, QStringLiteral("la hospitalización no está activa"), {}};
    }
    auditar(sesion, QStringLiteral("alta_hospitalizacion"),
            QStringLiteral("hospitalizaciones"), hospitalizacion_uuid, destino);
    return {true, {}, hospitalizacion_uuid};
}

ResultadoClinico ServicioClinico::programarCirugia(
    const Sesion& sesion, const QString& mascota_uuid, const QString& consulta_uuid,
    const QString& hospitalizacion_uuid, const QString& procedimiento,
    const QString& fecha_programada, const QString& anestesia)
{
    if (!permitido(sesion, Permiso::RegistrarConsulta, Accion::Crear,
                   QStringLiteral("cirugias"))) {
        return denegado(sesion, Permiso::RegistrarConsulta, QStringLiteral("cirugía"));
    }
    const qint64 mascotaId = idDe(QStringLiteral("mascotas"), mascota_uuid,
                                  QStringLiteral("AND deleted_at IS NULL"));
    if (mascotaId <= 0) {
        return {false, QStringLiteral("el paciente no existe"), {}};
    }
    if (procedimiento.trimmed().isEmpty()) {
        return {false, QStringLiteral("el procedimiento es obligatorio"), {}};
    }
    QDateTime fecha = QDateTime::fromString(fecha_programada, Qt::ISODate);
    if (!fecha.isValid()) {
        fecha = QDateTime::fromString(fecha_programada,
                                      QStringLiteral("yyyy-MM-dd HH:mm"));
    }
    if (!fecha.isValid()) {
        return {false, QStringLiteral("la fecha de la cirugía no es válida"), {}};
    }
    const qint64 consultaId = consulta_uuid.isEmpty()
        ? -1
        : idDe(QStringLiteral("consultas_medicas"), consulta_uuid,
               QStringLiteral("AND deleted_at IS NULL"));
    const qint64 ingresoId = hospitalizacion_uuid.isEmpty()
        ? -1
        : idDe(QStringLiteral("hospitalizaciones"), hospitalizacion_uuid,
               QStringLiteral("AND deleted_at IS NULL"));

    const QString uuid = uuidNuevo();
    const qint64 ahora = ahoraMs();
    const auto alta = clinica_.execute(
        QStringLiteral("INSERT INTO cirugias "
                       "(uuid, mascota_id, consulta_id, hospitalizacion_id, veterinario_id, "
                       " fecha_programada, procedimiento, anestesia, estado, created_at, "
                       " updated_at, sync_state) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 'Programada', ?, ?, 'pendiente')"),
        {QVariant(uuid), QVariant::fromValue(mascotaId),
         consultaId > 0 ? QVariant::fromValue(consultaId) : QVariant(),
         ingresoId > 0 ? QVariant::fromValue(ingresoId) : QVariant(),
         usuarioId(sesion) > 0 ? QVariant::fromValue(usuarioId(sesion)) : QVariant(),
         QVariant::fromValue(fecha.toMSecsSinceEpoch()), QVariant(procedimiento.trimmed()),
         anestesia.trimmed().isEmpty() ? QVariant() : QVariant(anestesia.trimmed()),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!alta.ok) {
        return {false, QStringLiteral("no se pudo programar la cirugía"), {}};
    }
    auditar(sesion, QStringLiteral("programar_cirugia"), QStringLiteral("cirugias"), uuid,
            procedimiento.trimmed());
    return {true, {}, uuid};
}

ResultadoClinico ServicioClinico::iniciarCirugia(const Sesion& sesion,
                                                 const QString& cirugia_uuid)
{
    if (!permitido(sesion, Permiso::RegistrarConsulta, Accion::Crear,
                   QStringLiteral("cirugias"))) {
        return denegado(sesion, Permiso::RegistrarConsulta, QStringLiteral("cirugía"));
    }
    const qint64 ahora = ahoraMs();
    const auto cambio = clinica_.execute(
        QStringLiteral("UPDATE cirugias SET estado = 'En curso', fecha_inicio = ?, "
                       " updated_at = ? WHERE uuid = ? AND estado = 'Programada' "
                       " AND deleted_at IS NULL"),
        {QVariant::fromValue(ahora), QVariant::fromValue(ahora), QVariant(cirugia_uuid)});
    if (!cambio.ok || cambio.affected == 0) {
        return {false, QStringLiteral("la cirugía no está programada"), {}};
    }
    auditar(sesion, QStringLiteral("iniciar_cirugia"), QStringLiteral("cirugias"),
            cirugia_uuid, QString());
    return {true, {}, cirugia_uuid};
}

ResultadoClinico ServicioClinico::cerrarCirugia(const Sesion& sesion,
                                                const QString& cirugia_uuid,
                                                const QString& diagnostico_post,
                                                const QString& notas)
{
    if (!permitido(sesion, Permiso::RegistrarConsulta, Accion::Crear,
                   QStringLiteral("cirugias"))) {
        return denegado(sesion, Permiso::RegistrarConsulta, QStringLiteral("cirugía"));
    }
    const qint64 ahora = ahoraMs();
    const auto cambio = clinica_.execute(
        QStringLiteral("UPDATE cirugias SET estado = 'Completada', fecha_fin = ?, "
                       " diagnostico_post = ?, notas = ?, updated_at = ? "
                       "WHERE uuid = ? AND estado = 'En curso' AND deleted_at IS NULL"),
        {QVariant::fromValue(ahora),
         diagnostico_post.trimmed().isEmpty() ? QVariant()
                                              : QVariant(diagnostico_post.trimmed()),
         notas.trimmed().isEmpty() ? QVariant() : QVariant(notas.trimmed()),
         QVariant::fromValue(ahora), QVariant(cirugia_uuid)});
    if (!cambio.ok || cambio.affected == 0) {
        return {false, QStringLiteral("la cirugía no está en curso"), {}};
    }
    auditar(sesion, QStringLiteral("cerrar_cirugia"), QStringLiteral("cirugias"),
            cirugia_uuid, diagnostico_post.trimmed());
    return {true, {}, cirugia_uuid};
}

ResultadoClinico ServicioClinico::registrarEstetica(
    const Sesion& sesion, const QString& mascota_uuid, const QString& cita_uuid,
    const QString& servicio_uuid, const QString& tipo, double precio,
    const QString& observaciones, bool cobrar)
{
    if (!permitido(sesion, Permiso::RegistrarEstetica, Accion::Crear,
                   QStringLiteral("estetica_servicios"))) {
        return denegado(sesion, Permiso::RegistrarEstetica,
                        QStringLiteral("estética"));
    }
    const qint64 mascotaId = idDe(QStringLiteral("mascotas"), mascota_uuid,
                                  QStringLiteral("AND deleted_at IS NULL"));
    if (mascotaId <= 0) {
        return {false, QStringLiteral("el paciente no existe"), {}};
    }
    static const QStringList tipos = {
        QStringLiteral("Baño"), QStringLiteral("Corte"),
        QStringLiteral("Baño y corte"), QStringLiteral("Deslanado"),
        QStringLiteral("Otro")};
    if (!tipos.contains(tipo)) {
        return {false, QStringLiteral("el tipo de servicio estético no es válido"), {}};
    }
    if (precio < 0) {
        return {false, QStringLiteral("el precio no puede ser negativo"), {}};
    }
    const qint64 citaId = cita_uuid.isEmpty()
        ? -1
        : idDe(QStringLiteral("citas"), cita_uuid,
               QStringLiteral("AND deleted_at IS NULL"));
    const qint64 servicioId = servicio_uuid.isEmpty()
        ? -1
        : idDe(QStringLiteral("servicios"), servicio_uuid,
               QStringLiteral("AND deleted_at IS NULL"));

    const QString uuid = uuidNuevo();
    const qint64 ahora = ahoraMs();
    if (!clinica_.execute(QStringLiteral("BEGIN")).ok) {
        return {false, QStringLiteral("no se pudo registrar la estética"), {}};
    }
    const auto alta = clinica_.execute(
        QStringLiteral("INSERT INTO estetica_servicios "
                       "(uuid, mascota_id, cita_id, estilista_id, servicio_id, fecha, tipo, "
                       " precio, observaciones, estado, created_at, updated_at, sync_state) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 'Realizado', ?, ?, 'pendiente')"),
        {QVariant(uuid), QVariant::fromValue(mascotaId),
         citaId > 0 ? QVariant::fromValue(citaId) : QVariant(),
         usuarioId(sesion) > 0 ? QVariant::fromValue(usuarioId(sesion)) : QVariant(),
         servicioId > 0 ? QVariant::fromValue(servicioId) : QVariant(),
         QVariant::fromValue(ahora), QVariant(tipo), QVariant::fromValue(precio),
         observaciones.trimmed().isEmpty() ? QVariant() : QVariant(observaciones.trimmed()),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!alta.ok) {
        clinica_.execute(QStringLiteral("ROLLBACK"));
        return {false, QStringLiteral("no se pudo registrar la estética"), {}};
    }
    if (citaId > 0) {
        clinica_.execute(
            QStringLiteral("UPDATE citas SET estado_cita_id = "
                           "(SELECT id FROM estados_cita WHERE nombre_estado = 'Completada'), "
                           "fecha_actualizacion = ?, updated_at = ? WHERE id = ?"),
            {QVariant::fromValue(ahora), QVariant::fromValue(ahora),
             QVariant::fromValue(citaId)});
    }
    if (cobrar && precio > 0) {
        QString error;
        if (!crearCargo(QStringLiteral("Otro"), 0, uuid,
                        QStringLiteral("Estética: %1").arg(tipo), precio, &error)) {
            clinica_.execute(QStringLiteral("ROLLBACK"));
            return {false, error, {}};
        }
    }
    if (!clinica_.execute(QStringLiteral("COMMIT")).ok) {
        clinica_.execute(QStringLiteral("ROLLBACK"));
        return {false, QStringLiteral("no se pudo confirmar la estética"), {}};
    }
    auditar(sesion, QStringLiteral("estetica"), QStringLiteral("estetica_servicios"), uuid,
            tipo);
    return {true, {}, uuid};
}

ResultadoClinico ServicioClinico::facturarCargos(
    const Sesion& sesion, const QString& cliente_uuid, const QVariantList& cargo_uuids,
    const QString& metodo_pago, double descuento, double impuesto)
{
    if (!permitido(sesion, Permiso::EmitirFactura, Accion::Crear,
                   QStringLiteral("facturas"))) {
        return denegado(sesion, Permiso::EmitirFactura, QStringLiteral("facturación"));
    }
    if (cargo_uuids.isEmpty()) {
        return {false, QStringLiteral("selecciona al menos un cargo"), {}};
    }
    if (descuento < 0 || impuesto < 0) {
        return {false, QStringLiteral("descuento e impuesto no pueden ser negativos"), {}};
    }

    QStringList marcadores;
    std::vector<QVariant> params;
    for (const QVariant& uuid : cargo_uuids) {
        marcadores.append(QStringLiteral("?"));
        params.push_back(uuid);
    }
    const auto cargos = clinica_.query(
        QStringLiteral("SELECT uuid, concepto, monto, estado FROM cargos "
                       "WHERE uuid IN (%1) AND deleted_at IS NULL")
            .arg(marcadores.join(QStringLiteral(", "))),
        params);
    if (static_cast<int>(cargos.size()) != cargo_uuids.size()) {
        return {false, QStringLiteral("uno de los cargos no existe"), {}};
    }
    double subtotal = 0.0;
    for (const QueryRow& fila : cargos) {
        if (fila.values.at(3).toString() != QStringLiteral("Pendiente")) {
            return {false, QStringLiteral("el cargo %1 ya fue facturado")
                               .arg(fila.values.at(1).toString()),
                    {}};
        }
        subtotal += fila.values.at(2).toDouble();
    }
    const double total = subtotal + impuesto - descuento;
    if (total < 0) {
        return {false, QStringLiteral("el descuento supera el total de los cargos"), {}};
    }

    const qint64 clienteId = cliente_uuid.isEmpty()
        ? -1
        : idDe(QStringLiteral("clientes"), cliente_uuid,
               QStringLiteral("AND deleted_at IS NULL"));
    const qint64 cajeroId = usuarioId(sesion);
    const QString uuid = uuidNuevo();
    const qint64 ahora = ahoraMs();
    const QString numero =
        QStringLiteral("FV-%1-%2")
            .arg(QDateTime::fromMSecsSinceEpoch(ahora).toString(QStringLiteral("yyyyMMdd")),
                 QString::number(ahora % 100000).rightJustified(5, QLatin1Char('0')));
    const bool pagada = !metodo_pago.isEmpty();
    const QString estadoCargo = pagada ? QStringLiteral("Pagado")
                                       : QStringLiteral("Facturado");

    if (!clinica_.execute(QStringLiteral("BEGIN")).ok) {
        return {false, QStringLiteral("no se pudo iniciar la factura"), {}};
    }
    const auto alta = clinica_.execute(
        QStringLiteral("INSERT INTO facturas "
                       "(uuid, cliente_id, cajero_id, numero_factura, fecha_emision, subtotal, "
                       " impuesto, descuento, monto_total, metodo_pago, estado, created_at, "
                       " updated_at, sync_state) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'pendiente')"),
        {QVariant(uuid), clienteId > 0 ? QVariant::fromValue(clienteId) : QVariant(),
         cajeroId > 0 ? QVariant::fromValue(cajeroId) : QVariant(), QVariant(numero),
         QVariant::fromValue(ahora), QVariant::fromValue(subtotal),
         QVariant::fromValue(impuesto), QVariant::fromValue(descuento),
         QVariant::fromValue(total),
         pagada ? QVariant(metodo_pago) : QVariant(),
         QVariant(pagada ? QStringLiteral("Pagada") : QStringLiteral("Emitida")),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!alta.ok) {
        clinica_.execute(QStringLiteral("ROLLBACK"));
        return {false, QStringLiteral("no se pudo emitir la factura"), {}};
    }
    const qint64 facturaId = alta.last_insert_id.toLongLong();

    for (const QueryRow& fila : cargos) {
        const auto detalle = clinica_.execute(
            QStringLiteral("INSERT INTO detalles_factura "
                           "(uuid, factura_id, codigo, nombre, tipo, cantidad, "
                           " precio_unitario, created_at, updated_at, sync_state) "
                           "VALUES (?, ?, 'CARGO', ?, 'Servicio', 1, ?, ?, ?, 'pendiente')"),
            {QVariant(uuidNuevo()), QVariant::fromValue(facturaId),
             fila.values.at(1), fila.values.at(2), QVariant::fromValue(ahora),
             QVariant::fromValue(ahora)});
        if (!detalle.ok) {
            clinica_.execute(QStringLiteral("ROLLBACK"));
            return {false, QStringLiteral("no se pudo guardar el detalle"), {}};
        }
        const auto cambio = clinica_.execute(
            QStringLiteral("UPDATE cargos SET estado = ?, factura_id = ?, updated_at = ? "
                           "WHERE uuid = ? AND estado = 'Pendiente'"),
            {QVariant(estadoCargo), QVariant::fromValue(facturaId),
             QVariant::fromValue(ahora), fila.values.at(0)});
        if (!cambio.ok || cambio.affected == 0) {
            clinica_.execute(QStringLiteral("ROLLBACK"));
            return {false, QStringLiteral("no se pudo cerrar el cargo"), {}};
        }
    }
    if (!clinica_.execute(QStringLiteral("COMMIT")).ok) {
        clinica_.execute(QStringLiteral("ROLLBACK"));
        return {false, QStringLiteral("no se pudo confirmar la factura"), {}};
    }
    auditar(sesion, QStringLiteral("facturar_cargos"), QStringLiteral("facturas"), uuid,
            numero);
    return {true, {}, uuid};
}

ResultadoClinico ServicioClinico::cerrarCaja(const Sesion& sesion,
                                             double ingresos_reales,
                                             const QString& observaciones)
{
    if (!permitido(sesion, Permiso::CerrarCaja, Accion::Crear,
                   QStringLiteral("cierres_caja"))) {
        return denegado(sesion, Permiso::CerrarCaja, QStringLiteral("caja"));
    }
    if (ingresos_reales < 0) {
        return {false, QStringLiteral("los ingresos reales no pueden ser negativos"), {}};
    }
    const QDateTime inicio = QDate::currentDate().startOfDay();
    const qint64 desde = inicio.toMSecsSinceEpoch();
    const qint64 hasta = inicio.addDays(1).toMSecsSinceEpoch();
    const auto calculado = clinica_.query(
        QStringLiteral("SELECT coalesce(sum(monto_total), 0) FROM facturas "
                       "WHERE estado = 'Pagada' AND deleted_at IS NULL "
                       "AND fecha_emision >= ? AND fecha_emision < ?"),
        {QVariant::fromValue(desde), QVariant::fromValue(hasta)});
    const double esperado = calculado.empty()
        ? 0.0
        : calculado.front().values.front().toDouble();

    const QString uuid = uuidNuevo();
    const qint64 ahora = ahoraMs();
    const auto alta = clinica_.execute(
        QStringLiteral("INSERT INTO cierres_caja "
                       "(uuid, usuario_cierre_id, fecha_cierre, ingresos_calculados, "
                       " ingresos_reales, desfase, observaciones, created_at, updated_at, "
                       " sync_state) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 'pendiente')"),
        {QVariant(uuid),
         usuarioId(sesion) > 0 ? QVariant::fromValue(usuarioId(sesion)) : QVariant(),
         QVariant::fromValue(ahora), QVariant::fromValue(esperado),
         QVariant::fromValue(ingresos_reales),
         QVariant::fromValue(ingresos_reales - esperado),
         observaciones.trimmed().isEmpty() ? QVariant() : QVariant(observaciones.trimmed()),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!alta.ok) {
        return {false, QStringLiteral("no se pudo cerrar la caja"), {}};
    }
    auditar(sesion, QStringLiteral("cierre_caja"), QStringLiteral("cierres_caja"), uuid,
            QStringLiteral("%1").arg(ingresos_reales));
    return {true, {}, uuid};
}

ResultadoClinico ServicioClinico::ajustarStock(const Sesion& sesion,
                                               const QString& producto_uuid, int delta,
                                               const QString& motivo)
{
    if (!permitido(sesion, Permiso::GestionarInventario, Accion::Crear,
                   QStringLiteral("movimientos_inventario"))) {
        return denegado(sesion, Permiso::GestionarInventario,
                        QStringLiteral("inventario"));
    }
    if (delta == 0) {
        return {false, QStringLiteral("el ajuste no puede ser cero"), {}};
    }
    const auto producto = clinica_.query(
        QStringLiteral("SELECT id, stock_actual FROM productos_insumos "
                       "WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(producto_uuid)});
    if (producto.size() != 1) {
        return {false, QStringLiteral("el producto no existe"), {}};
    }
    const qint64 productoId = producto.front().values.at(0).toLongLong();
    const int stock = producto.front().values.at(1).toInt();
    if (stock + delta < 0) {
        return {false, QStringLiteral("el stock no puede quedar negativo"), {}};
    }
    const qint64 ahora = ahoraMs();
    if (!clinica_.execute(QStringLiteral("BEGIN")).ok) {
        return {false, QStringLiteral("no se pudo ajustar el stock"), {}};
    }
    const auto cambio = clinica_.execute(
        QStringLiteral("UPDATE productos_insumos SET stock_actual = ?, updated_at = ? "
                       "WHERE id = ?"),
        {QVariant::fromValue(stock + delta), QVariant::fromValue(ahora),
         QVariant::fromValue(productoId)});
    const QString uuid = uuidNuevo();
    const auto movimiento = clinica_.execute(
        QStringLiteral("INSERT INTO movimientos_inventario "
                       "(uuid, producto_id, usuario_id, tipo, cantidad, motivo, created_at, "
                       " updated_at, sync_state) VALUES (?, ?, ?, ?, ?, ?, ?, ?, 'pendiente')"),
        {QVariant(uuid), QVariant::fromValue(productoId),
         usuarioId(sesion) > 0 ? QVariant::fromValue(usuarioId(sesion)) : QVariant(),
         QVariant(delta > 0 ? QStringLiteral("Entrada") : QStringLiteral("Salida")),
         QVariant::fromValue(std::abs(delta)),
         motivo.trimmed().isEmpty() ? QVariant() : QVariant(motivo.trimmed()),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!cambio.ok || !movimiento.ok || !clinica_.execute(QStringLiteral("COMMIT")).ok) {
        clinica_.execute(QStringLiteral("ROLLBACK"));
        return {false, QStringLiteral("no se pudo ajustar el stock"), {}};
    }
    auditar(sesion, QStringLiteral("ajuste_stock"),
            QStringLiteral("productos_insumos"), producto_uuid,
            QStringLiteral("%1 (%2)").arg(delta).arg(motivo.trimmed()));
    return {true, {}, uuid};
}

QVariantList ServicioClinico::filasDeCargos(IQueryExecutor& clinica,
                                            const QString& cliente_uuid)
{
    const QString filtro = cliente_uuid.isEmpty()
        ? QString()
        : QStringLiteral("AND m.cliente_id = (SELECT id FROM clientes WHERE uuid = ?) ");
    std::vector<QVariant> params;
    if (!cliente_uuid.isEmpty()) {
        params.push_back(cliente_uuid);
    }
    QString sql =
        QStringLiteral("SELECT c.uuid, c.origen_tipo, c.origen_uuid, c.concepto, c.monto, "
                       "c.estado, c.created_at, m.nombre, cl.nombre_completo, m.uuid, "
                       "cl.uuid "
                       "FROM cargos c "
                       "LEFT JOIN consultas_medicas cm ON cm.id = c.origen_id "
                       " AND c.origen_tipo = 'Consulta' "
                       "LEFT JOIN recetas r ON r.id = c.origen_id "
                       " AND c.origen_tipo = 'Receta' "
                       "LEFT JOIN mascotas m ON m.id = COALESCE(cm.mascota_id, r.mascota_id) "
                       "LEFT JOIN clientes cl ON cl.id = m.cliente_id "
                       "WHERE c.deleted_at IS NULL AND c.estado = 'Pendiente' ");
    sql += filtro;
    sql += QStringLiteral("ORDER BY c.created_at DESC");
    const auto filas = clinica.query(sql, params);
    QVariantList lista;
    for (const auto& fila : filas) {
        QVariantMap cargo;
        cargo.insert(QStringLiteral("uuid"), fila.values.at(0));
        cargo.insert(QStringLiteral("origenTipo"), fila.values.at(1));
        cargo.insert(QStringLiteral("origenUuid"), fila.values.at(2));
        cargo.insert(QStringLiteral("concepto"), fila.values.at(3));
        cargo.insert(QStringLiteral("monto"), fila.values.at(4));
        cargo.insert(QStringLiteral("estado"), fila.values.at(5));
        cargo.insert(QStringLiteral("fecha"), fila.values.at(6));
        cargo.insert(QStringLiteral("mascota"), fila.values.at(7));
        cargo.insert(QStringLiteral("propietario"), fila.values.at(8));
        cargo.insert(QStringLiteral("mascotaUuid"), fila.values.at(9));
        cargo.insert(QStringLiteral("clienteUuid"), fila.values.at(10));
        lista.append(cargo);
    }
    return lista;
}

} // namespace fuzzy::core
