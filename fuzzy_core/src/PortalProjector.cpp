#include "fuzzy/core/PortalProjector.h"

#include "fuzzy/core/MigrationRunner.h"
#include "fuzzy/core/PortalBaseStore.h"

#include <QDateTime>
#include <QStringList>
#include <QVariant>

#include <vector>

namespace fuzzy::core {

namespace {

const QString kAhora = QStringLiteral("CAST(strftime('%s', 'now') AS INTEGER) * 1000");

bool upsert(IQueryExecutor& executor, const QString& tabla,
            const QStringList& columnas, const std::vector<QVariant>& valores,
            const QString& conflicto)
{
    if (columnas.size() != static_cast<int>(valores.size())) {
        return false;
    }

    QStringList marcadores;
    QStringList actualizaciones;
    for (const QString& columna : columnas) {
        marcadores.append(QStringLiteral("?"));
        actualizaciones.append(QStringLiteral("%1 = excluded.%1").arg(columna));
    }

    const QString sql = QStringLiteral("INSERT INTO %1 (%2) VALUES (%3) "
                                       "ON CONFLICT(%4) DO UPDATE SET %5 "
                                       "WHERE excluded.updated_at >= %1.updated_at")
                            .arg(tabla, columnas.join(QStringLiteral(", ")),
                                 marcadores.join(QStringLiteral(", ")), conflicto,
                                 actualizaciones.join(QStringLiteral(", ")));
    return executor.execute(sql, valores).ok;
}

// Marca como borradas las filas que ya no existen en la clínica. Solo se usa
// en tablas proyectadas; nunca en las que escribe la app.
bool marcarAusentes(IQueryExecutor& executor, const QString& tabla,
                    const QStringList& uuids, qint64 ahora)
{
    QString sql = QStringLiteral("UPDATE %1 SET deleted_at = ?, updated_at = ? "
                                 "WHERE deleted_at IS NULL")
                      .arg(tabla);
    std::vector<QVariant> params{QVariant::fromValue(ahora), QVariant::fromValue(ahora)};

    if (!uuids.isEmpty()) {
        QStringList marcadores;
        for (const QString& uuid : uuids) {
            marcadores.append(QStringLiteral("?"));
            params.push_back(QVariant(uuid));
        }
        sql += QStringLiteral(" AND uuid NOT IN (%1)")
                   .arg(marcadores.join(QStringLiteral(", ")));
    }
    return executor.execute(sql, params).ok;
}

QStringList textos(const QString& texto)
{
    // Un solo elemento: evita repetir el manejo de valores nulos en cada tabla.
    return {texto};
}

} // namespace

PortalProjector::PortalProjector(IQueryExecutor& clinica, PortalBaseStore& bases)
    : clinica_(clinica), bases_(bases)
{
}

ResultadoProyeccion PortalProjector::proyectar(const QString& portal_uuid)
{
    ResultadoProyeccion resultado;

    std::unique_ptr<IQueryExecutor> portal = bases_.abrir(portal_uuid);
    if (portal == nullptr) {
        resultado.error = QStringLiteral("no existe la base del portal");
        return resultado;
    }

    const MigrationRunner runner = MigrationRunner::portal();
    if (!runner.aplicar(*portal)) {
        resultado.error = QStringLiteral("no se pudo aplicar el esquema del portal");
        return resultado;
    }
    if (!portal->execute(
                    QStringLiteral("UPDATE portal_meta SET esquema_version = ? WHERE id = 1"),
                    {QVariant(runner.versionActual().toInt())})
             .ok) {
        resultado.error = QStringLiteral("no se pudo actualizar portal_meta");
        return resultado;
    }

    const auto meta = portal->query(
        QStringLiteral("SELECT clinica_uuid, cuenta_uuid FROM portal_meta WHERE id = 1"));
    if (meta.size() != 1 || meta.front().values.size() != 2) {
        resultado.error = QStringLiteral("portal_meta no tiene la fila única");
        return resultado;
    }
    const QString clinica_uuid = meta.front().values.at(0).toString();
    const QString cuenta_uuid = meta.front().values.at(1).toString();

    const auto clinica = clinica_.query(
        QStringLiteral("SELECT uuid, nombre FROM clinicas "
                       "WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(clinica_uuid)});
    if (clinica.size() != 1 || clinica.front().values.size() != 2) {
        resultado.error = QStringLiteral("la clínica del portal no coincide con la base");
        return resultado;
    }

    // La cuenta identifica una sola ficha dentro de esta clínica.
    const auto cliente = clinica_.query(
        QStringLiteral("SELECT cl.id, cl.uuid, cl.nombre_completo, cl.telefono, "
                       "cl.correo, cl.direccion, cl.dni, cl.activo, cl.created_at, "
                       "cl.updated_at, cl.deleted_at "
                       "FROM clientes cl JOIN cuentas cu ON cu.cliente_id = cl.id "
                       "WHERE cu.uuid = ? AND cu.deleted_at IS NULL"),
        {QVariant(cuenta_uuid)});
    if (cliente.size() != 1 || cliente.front().values.size() != 11) {
        resultado.error = QStringLiteral("no se encontró la ficha de la cuenta");
        return resultado;
    }
    const auto& c = cliente.front().values;
    const qint64 cliente_id = c.at(0).toLongLong();
    const QString cliente_uuid = c.at(1).toString();

    if (!portal->execute(QStringLiteral("BEGIN")).ok) {
        resultado.error = QStringLiteral("no se pudo abrir la transacción del portal");
        return resultado;
    }

    bool ok = true;

    // Perfil: el nodo crea la fila, pero no pisa una edición más nueva de la app.
    ok = ok && upsert(*portal, QStringLiteral("perfil"),
                      QStringList{QStringLiteral("id"), QStringLiteral("uuid"),
                                  QStringLiteral("nombre_completo"), QStringLiteral("correo"),
                                  QStringLiteral("telefono"), QStringLiteral("direccion"),
                                  QStringLiteral("dni"), QStringLiteral("activo"),
                                  QStringLiteral("created_at"), QStringLiteral("updated_at"),
                                  QStringLiteral("deleted_at")},
                      {QVariant::fromValue<qint64>(1), QVariant(cliente_uuid),
                       QVariant(c.at(2)), QVariant(c.at(4)), QVariant(c.at(3)),
                       QVariant(c.at(5)), QVariant(c.at(6)), QVariant(c.at(7)),
                       QVariant(c.at(8)), QVariant(c.at(9)), QVariant(c.at(10))},
                      QStringLiteral("id"));
    ++resultado.filas;

    // ── Mascotas ─────────────────────────────────────────────
    const auto mascotas = clinica_.query(
        QStringLiteral("SELECT uuid, nombre, especie, raza, sexo, peso_kg, numero_chip, "
                       "fecha_nacimiento, activo, castrado, alergias, atencion_especial, "
                       "created_at, updated_at, deleted_at "
                       "FROM mascotas WHERE cliente_id = ?"),
        {QVariant::fromValue(cliente_id)});
    QStringList mascotaUuids;
    for (const QueryRow& fila : mascotas) {
        const auto& v = fila.values;
        mascotaUuids.append(v.at(0).toString());
        ok = ok && upsert(*portal, QStringLiteral("mascotas"),
                          QStringList{QStringLiteral("uuid"), QStringLiteral("nombre"),
                                      QStringLiteral("especie"), QStringLiteral("raza"),
                                      QStringLiteral("sexo"), QStringLiteral("peso_kg"),
                                      QStringLiteral("numero_chip"),
                                      QStringLiteral("fecha_nacimiento"),
                                      QStringLiteral("activo"), QStringLiteral("castrado"),
                                      QStringLiteral("alergias"),
                                      QStringLiteral("atencion_especial"),
                                      QStringLiteral("created_at"), QStringLiteral("updated_at"),
                                      QStringLiteral("deleted_at")},
                          {v.at(0), v.at(1), v.at(2), v.at(3), v.at(4), v.at(5), v.at(6),
                           v.at(7), v.at(8), v.at(9), v.at(10), v.at(11), v.at(12), v.at(13),
                           v.at(14)},
                          QStringLiteral("uuid"));
        ++resultado.filas;
    }

    // ── Visitas ──────────────────────────────────────────────
    const auto visitas = clinica_.query(
        QStringLiteral("SELECT cm.uuid, m.uuid, cm.fecha_consulta, cm.motivo_visita, "
                       "cm.diagnostico, cm.anamnesis, cm.examen_fisico, "
                       "cm.tratamiento_indicado, COALESCE(u.nombre_completo, ''), "
                       "cm.es_emergencia, cm.estado, cm.fecha_cierre, cm.created_at, "
                       "cm.updated_at, cm.deleted_at "
                       "FROM consultas_medicas cm "
                       "JOIN mascotas m ON m.id = cm.mascota_id "
                       "LEFT JOIN usuarios u ON u.id = cm.veterinario_id "
                       "WHERE m.cliente_id = ?"),
        {QVariant::fromValue(cliente_id)});
    QStringList visitaUuids;
    for (const QueryRow& fila : visitas) {
        const auto& v = fila.values;
        visitaUuids.append(v.at(0).toString());
        ok = ok && upsert(*portal, QStringLiteral("visitas"),
                          QStringList{QStringLiteral("uuid"), QStringLiteral("mascota_uuid"),
                                      QStringLiteral("fecha"), QStringLiteral("motivo"),
                                      QStringLiteral("diagnostico"),
                                      QStringLiteral("anamnesis"),
                                      QStringLiteral("examen_fisico"),
                                      QStringLiteral("tratamiento"),
                                      QStringLiteral("veterinario"),
                                      QStringLiteral("es_emergencia"),
                                      QStringLiteral("estado"), QStringLiteral("cerrada_at"),
                                      QStringLiteral("created_at"), QStringLiteral("updated_at"),
                                      QStringLiteral("deleted_at")},
                          {v.at(0), v.at(1), v.at(2), v.at(3), v.at(4), v.at(5), v.at(6),
                           v.at(7), v.at(8), v.at(9), v.at(10), v.at(11), v.at(12), v.at(13),
                           v.at(14)},
                          QStringLiteral("uuid"));
        ++resultado.filas;
    }

    // ── Recetas y sus líneas ─────────────────────────────────
    const auto recetas = clinica_.query(
        QStringLiteral("SELECT r.uuid, m.uuid, cs.uuid, r.fecha_emision, r.indicaciones, "
                       "COALESCE(u.nombre_completo, ''), r.created_at, r.updated_at, "
                       "r.deleted_at "
                       "FROM recetas r "
                       "JOIN mascotas m ON m.id = r.mascota_id "
                       "JOIN consultas_medicas cs ON cs.id = r.consulta_id "
                       "LEFT JOIN usuarios u ON u.id = r.veterinario_id "
                       "WHERE m.cliente_id = ?"),
        {QVariant::fromValue(cliente_id)});
    QStringList recetaUuids;
    for (const QueryRow& fila : recetas) {
        const auto& v = fila.values;
        recetaUuids.append(v.at(0).toString());
        ok = ok && upsert(*portal, QStringLiteral("recetas"),
                          QStringList{QStringLiteral("uuid"), QStringLiteral("mascota_uuid"),
                                      QStringLiteral("visita_uuid"),
                                      QStringLiteral("fecha_emision"),
                                      QStringLiteral("indicaciones"),
                                      QStringLiteral("veterinario"),
                                      QStringLiteral("created_at"), QStringLiteral("updated_at"),
                                      QStringLiteral("deleted_at")},
                          {v.at(0), v.at(1), v.at(2), v.at(3), v.at(4), v.at(5), v.at(6),
                           v.at(7), v.at(8)},
                          QStringLiteral("uuid"));
        ++resultado.filas;
    }

    const auto items = clinica_.query(
        QStringLiteral("SELECT d.uuid, r.uuid, d.medicamento, d.dosis, d.frecuencia, "
                       "d.duracion, d.cantidad, d.created_at, d.updated_at, d.deleted_at "
                       "FROM receta_detalles d "
                       "JOIN recetas r ON r.id = d.receta_id "
                       "JOIN mascotas m ON m.id = r.mascota_id "
                       "WHERE m.cliente_id = ?"),
        {QVariant::fromValue(cliente_id)});
    QStringList itemUuids;
    for (const QueryRow& fila : items) {
        const auto& v = fila.values;
        itemUuids.append(v.at(0).toString());
        ok = ok && upsert(*portal, QStringLiteral("receta_items"),
                          QStringList{QStringLiteral("uuid"), QStringLiteral("receta_uuid"),
                                      QStringLiteral("medicamento"), QStringLiteral("dosis"),
                                      QStringLiteral("frecuencia"), QStringLiteral("duracion"),
                                      QStringLiteral("cantidad"), QStringLiteral("created_at"),
                                      QStringLiteral("updated_at"), QStringLiteral("deleted_at")},
                          {v.at(0), v.at(1), v.at(2), v.at(3), v.at(4), v.at(5), v.at(6),
                           v.at(7), v.at(8), v.at(9)},
                          QStringLiteral("uuid"));
        ++resultado.filas;
    }

    // ── Vacunas ──────────────────────────────────────────────
    const auto vacunas = clinica_.query(
        QStringLiteral("SELECT v.uuid, m.uuid, v.nombre_vacuna, v.fecha_aplicacion, "
                       "v.proxima_dosis, v.lote, v.notas, "
                       "COALESCE(u.nombre_completo, ''), v.created_at, v.updated_at, "
                       "v.deleted_at "
                       "FROM vacunas v "
                       "JOIN mascotas m ON m.id = v.mascota_id "
                       "LEFT JOIN usuarios u ON u.id = v.veterinario_id "
                       "WHERE m.cliente_id = ?"),
        {QVariant::fromValue(cliente_id)});
    QStringList vacunaUuids;
    for (const QueryRow& fila : vacunas) {
        const auto& v = fila.values;
        vacunaUuids.append(v.at(0).toString());
        ok = ok && upsert(*portal, QStringLiteral("vacunas"),
                          QStringList{QStringLiteral("uuid"), QStringLiteral("mascota_uuid"),
                                      QStringLiteral("nombre_vacuna"),
                                      QStringLiteral("fecha_aplicacion"),
                                      QStringLiteral("proxima_dosis"), QStringLiteral("lote"),
                                      QStringLiteral("notas"), QStringLiteral("veterinario"),
                                      QStringLiteral("created_at"), QStringLiteral("updated_at"),
                                      QStringLiteral("deleted_at")},
                          {v.at(0), v.at(1), v.at(2), v.at(3), v.at(4), v.at(5), v.at(6),
                           v.at(7), v.at(8), v.at(9), v.at(10)},
                          QStringLiteral("uuid"));
        ++resultado.filas;
    }

    // ── Pesos (signos vitales con peso) ──────────────────────
    const auto pesos = clinica_.query(
        QStringLiteral("SELECT s.uuid, m.uuid, s.fecha, s.peso_kg, s.created_at, "
                       "s.updated_at, s.deleted_at "
                       "FROM signos_vitales s "
                       "JOIN mascotas m ON m.id = s.mascota_id "
                       "WHERE m.cliente_id = ? AND s.peso_kg IS NOT NULL"),
        {QVariant::fromValue(cliente_id)});
    QStringList pesoUuids;
    for (const QueryRow& fila : pesos) {
        const auto& v = fila.values;
        pesoUuids.append(v.at(0).toString());
        ok = ok && upsert(*portal, QStringLiteral("pesos"),
                          QStringList{QStringLiteral("uuid"), QStringLiteral("mascota_uuid"),
                                      QStringLiteral("fecha"), QStringLiteral("peso_kg"),
                                      QStringLiteral("created_at"), QStringLiteral("updated_at"),
                                      QStringLiteral("deleted_at")},
                          {v.at(0), v.at(1), v.at(2), v.at(3), v.at(4), v.at(5), v.at(6)},
                          QStringLiteral("uuid"));
        ++resultado.filas;
    }

    // ── Citas ────────────────────────────────────────────────
    const auto citas = clinica_.query(
        QStringLiteral("SELECT ci.uuid, m.uuid, ci.fecha_hora_programada, ts.nombre_servicio, "
                       "ec.nombre_estado, ci.direccion_domicilio, ci.notas_cliente, "
                       "ci.origen, ci.created_at, ci.updated_at, ci.deleted_at "
                       "FROM citas ci "
                       "JOIN mascotas m ON m.id = ci.mascota_id "
                       "JOIN tipos_servicio_cita ts ON ts.id = ci.tipo_servicio_id "
                       "JOIN estados_cita ec ON ec.id = ci.estado_cita_id "
                       "WHERE m.cliente_id = ?"),
        {QVariant::fromValue(cliente_id)});
    QStringList citaUuids;
    for (const QueryRow& fila : citas) {
        const auto& v = fila.values;
        citaUuids.append(v.at(0).toString());
        ok = ok && upsert(*portal, QStringLiteral("citas"),
                          QStringList{QStringLiteral("uuid"), QStringLiteral("mascota_uuid"),
                                      QStringLiteral("fecha_hora"), QStringLiteral("tipo_servicio"),
                                      QStringLiteral("estado"), QStringLiteral("direccion"),
                                      QStringLiteral("notas"), QStringLiteral("origen"),
                                      QStringLiteral("created_at"), QStringLiteral("updated_at"),
                                      QStringLiteral("deleted_at")},
                          {v.at(0), v.at(1), v.at(2), v.at(3), v.at(4), v.at(5), v.at(6),
                           v.at(7), v.at(8), v.at(9), v.at(10)},
                          QStringLiteral("uuid"));
        ++resultado.filas;
    }

    // ── Tombstones de lo que ya no está en la clínica ────────
    const qint64 ahora = QDateTime::currentMSecsSinceEpoch();
    ok = ok && marcarAusentes(*portal, QStringLiteral("mascotas"), mascotaUuids, ahora);
    ok = ok && marcarAusentes(*portal, QStringLiteral("visitas"), visitaUuids, ahora);
    ok = ok && marcarAusentes(*portal, QStringLiteral("recetas"), recetaUuids, ahora);
    ok = ok && marcarAusentes(*portal, QStringLiteral("receta_items"), itemUuids, ahora);
    ok = ok && marcarAusentes(*portal, QStringLiteral("vacunas"), vacunaUuids, ahora);
    ok = ok && marcarAusentes(*portal, QStringLiteral("pesos"), pesoUuids, ahora);
    ok = ok && marcarAusentes(*portal, QStringLiteral("citas"), citaUuids, ahora);

    if (!ok) {
        portal->execute(QStringLiteral("ROLLBACK"));
        resultado.error = QStringLiteral("falló una escritura de la proyección");
        return resultado;
    }

    const ExecResult metaActualizada = portal->execute(
        QStringLiteral("UPDATE portal_meta SET proyeccion_version = 1, proyectado_at = %1 "
                       "WHERE id = 1")
            .arg(kAhora));
    if (!metaActualizada.ok || !portal->execute(QStringLiteral("COMMIT")).ok) {
        portal->execute(QStringLiteral("ROLLBACK"));
        resultado.error = QStringLiteral("no se pudo cerrar la proyección");
        return resultado;
    }

    resultado.ok = true;
    return resultado;
}

} // namespace fuzzy::core
