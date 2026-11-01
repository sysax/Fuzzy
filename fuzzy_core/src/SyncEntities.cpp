#include "fuzzy/core/SyncEntities.h"

#include <QJsonValue>
#include <QVariant>

#include <cmath>
#include <limits>

namespace fuzzy::core {

namespace {

EntidadSync entidadCliente()
{
    return {
        QStringLiteral("clientes"),
        QStringLiteral("clientes"),
        {QStringLiteral("uuid"), QStringLiteral("nombre_completo"),
         QStringLiteral("telefono"), QStringLiteral("correo"),
         QStringLiteral("direccion"), QStringLiteral("dni"),
         QStringLiteral("activo"), QStringLiteral("created_at"),
         QStringLiteral("updated_at"), QStringLiteral("deleted_at")},
        {},
        10,
    };
}

EntidadSync entidadMascota()
{
    return {
        QStringLiteral("mascotas"),
        QStringLiteral("mascotas"),
        {QStringLiteral("uuid"), QStringLiteral("nombre"), QStringLiteral("especie"),
         QStringLiteral("raza"), QStringLiteral("sexo"), QStringLiteral("peso_kg"),
         QStringLiteral("numero_chip"), QStringLiteral("fecha_nacimiento"),
         QStringLiteral("activo"), QStringLiteral("castrado"), QStringLiteral("alergias"),
         QStringLiteral("atencion_especial"), QStringLiteral("created_at"),
         QStringLiteral("updated_at"), QStringLiteral("deleted_at")},
        {{QStringLiteral("cliente_id"), QStringLiteral("clientes"),
          QStringLiteral("cliente_uuid"), true}},
        20,
    };
}

EntidadSync entidadCita()
{
    return {
        QStringLiteral("citas"),
        QStringLiteral("citas"),
        {QStringLiteral("uuid"), QStringLiteral("fecha_hora_programada"),
         QStringLiteral("fecha_llegada"), QStringLiteral("prioridad"),
         QStringLiteral("es_emergencia"), QStringLiteral("origen"),
         QStringLiteral("direccion_domicilio"), QStringLiteral("notas_cliente"),
         QStringLiteral("fecha_creacion"), QStringLiteral("fecha_actualizacion"),
         QStringLiteral("created_at"), QStringLiteral("updated_at"),
         QStringLiteral("deleted_at")},
        {{QStringLiteral("mascota_id"), QStringLiteral("mascotas"),
          QStringLiteral("mascota_uuid"), true},
         {QStringLiteral("tipo_servicio_id"), QStringLiteral("tipos_servicio_cita"),
          QStringLiteral("tipo_servicio_uuid"), true},
         {QStringLiteral("estado_cita_id"), QStringLiteral("estados_cita"),
          QStringLiteral("estado_cita_uuid"), true},
         {QStringLiteral("empleado_asignado_id"), QStringLiteral("usuarios"),
          QStringLiteral("empleado_asignado_uuid"), false}},
        30,
    };
}

EntidadSync entidadConsulta()
{
    return {
        QStringLiteral("consultas_medicas"),
        QStringLiteral("consultas_medicas"),
        {QStringLiteral("uuid"), QStringLiteral("fecha_consulta"),
         QStringLiteral("motivo_visita"), QStringLiteral("es_emergencia"),
         QStringLiteral("diagnostico"), QStringLiteral("tratamiento_indicado"),
         QStringLiteral("anamnesis"), QStringLiteral("examen_fisico"),
         QStringLiteral("estado"), QStringLiteral("fecha_cierre"),
         QStringLiteral("triage_notas"), QStringLiteral("created_at"),
         QStringLiteral("updated_at"), QStringLiteral("deleted_at")},
        {{QStringLiteral("mascota_id"), QStringLiteral("mascotas"),
          QStringLiteral("mascota_uuid"), true},
         {QStringLiteral("veterinario_id"), QStringLiteral("usuarios"),
          QStringLiteral("veterinario_uuid"), false},
         {QStringLiteral("cita_id"), QStringLiteral("citas"),
          QStringLiteral("cita_uuid"), false}},
        30,
    };
}

EntidadSync entidadReceta()
{
    return {
        QStringLiteral("recetas"),
        QStringLiteral("recetas"),
        {QStringLiteral("uuid"), QStringLiteral("fecha_emision"),
         QStringLiteral("indicaciones"), QStringLiteral("created_at"),
         QStringLiteral("updated_at"), QStringLiteral("deleted_at")},
        {{QStringLiteral("consulta_id"), QStringLiteral("consultas_medicas"),
          QStringLiteral("consulta_uuid"), true},
         {QStringLiteral("mascota_id"), QStringLiteral("mascotas"),
          QStringLiteral("mascota_uuid"), true},
         {QStringLiteral("veterinario_id"), QStringLiteral("usuarios"),
          QStringLiteral("veterinario_uuid"), false}},
        40,
    };
}

EntidadSync entidadRecetaDetalle()
{
    return {
        QStringLiteral("receta_detalles"),
        QStringLiteral("receta_detalles"),
        {QStringLiteral("uuid"), QStringLiteral("medicamento"),
         QStringLiteral("dosis"), QStringLiteral("frecuencia"),
         QStringLiteral("duracion"), QStringLiteral("cantidad"),
         QStringLiteral("precio_unitario"), QStringLiteral("created_at"),
         QStringLiteral("updated_at"), QStringLiteral("deleted_at")},
        {{QStringLiteral("receta_id"), QStringLiteral("recetas"),
          QStringLiteral("receta_uuid"), true},
         {QStringLiteral("producto_id"), QStringLiteral("productos_insumos"),
          QStringLiteral("producto_uuid"), false}},
        50,
    };
}

EntidadSync entidadVacuna()
{
    return {
        QStringLiteral("vacunas"),
        QStringLiteral("vacunas"),
        {QStringLiteral("uuid"), QStringLiteral("nombre_vacuna"),
         QStringLiteral("fecha_aplicacion"), QStringLiteral("proxima_dosis"),
         QStringLiteral("lote"), QStringLiteral("notas"),
         QStringLiteral("created_at"), QStringLiteral("updated_at"),
         QStringLiteral("deleted_at")},
        {{QStringLiteral("mascota_id"), QStringLiteral("mascotas"),
          QStringLiteral("mascota_uuid"), true},
         {QStringLiteral("veterinario_id"), QStringLiteral("usuarios"),
          QStringLiteral("veterinario_uuid"), false}},
        40,
    };
}

EntidadSync entidadSignos()
{
    return {
        QStringLiteral("signos_vitales"),
        QStringLiteral("signos_vitales"),
        {QStringLiteral("uuid"), QStringLiteral("fecha"), QStringLiteral("peso_kg"),
         QStringLiteral("temperatura_c"), QStringLiteral("frecuencia_cardiaca"),
         QStringLiteral("frecuencia_respiratoria"), QStringLiteral("created_at"),
         QStringLiteral("updated_at"), QStringLiteral("deleted_at")},
        {{QStringLiteral("mascota_id"), QStringLiteral("mascotas"),
          QStringLiteral("mascota_uuid"), true},
         {QStringLiteral("consulta_id"), QStringLiteral("consultas_medicas"),
          QStringLiteral("consulta_uuid"), false}},
        40,
    };
}

EntidadSync entidadLaboratorio()
{
    return {
        QStringLiteral("laboratorio_solicitudes"),
        QStringLiteral("laboratorio_solicitudes"),
        {QStringLiteral("uuid"), QStringLiteral("tipo_examen"),
         QStringLiteral("prioridad"), QStringLiteral("estado"),
         QStringLiteral("fecha_solicitud"), QStringLiteral("fecha_resultado"),
         QStringLiteral("resultado"), QStringLiteral("observaciones"),
         QStringLiteral("created_at"), QStringLiteral("updated_at"),
         QStringLiteral("deleted_at")},
        {{QStringLiteral("mascota_id"), QStringLiteral("mascotas"),
          QStringLiteral("mascota_uuid"), true},
         {QStringLiteral("consulta_id"), QStringLiteral("consultas_medicas"),
          QStringLiteral("consulta_uuid"), false},
         {QStringLiteral("veterinario_id"), QStringLiteral("usuarios"),
          QStringLiteral("veterinario_uuid"), false},
         {QStringLiteral("tipo_examen_id"), QStringLiteral("tipos_examen_laboratorio"),
          QStringLiteral("tipo_examen_uuid"), false}},
        45,
    };
}

EntidadSync entidadHospitalizacion()
{
    return {
        QStringLiteral("hospitalizaciones"),
        QStringLiteral("hospitalizaciones"),
        {QStringLiteral("uuid"), QStringLiteral("fecha_ingreso"),
         QStringLiteral("fecha_alta"), QStringLiteral("motivo"),
         QStringLiteral("diagnostico"), QStringLiteral("area"),
         QStringLiteral("estado"), QStringLiteral("notas"),
         QStringLiteral("created_at"), QStringLiteral("updated_at"),
         QStringLiteral("deleted_at")},
        {{QStringLiteral("mascota_id"), QStringLiteral("mascotas"),
          QStringLiteral("mascota_uuid"), true},
         {QStringLiteral("consulta_id"), QStringLiteral("consultas_medicas"),
          QStringLiteral("consulta_uuid"), false},
         {QStringLiteral("veterinario_id"), QStringLiteral("usuarios"),
          QStringLiteral("veterinario_uuid"), false}},
        45,
    };
}

EntidadSync entidadEvolucion()
{
    return {
        QStringLiteral("hospitalizacion_evoluciones"),
        QStringLiteral("hospitalizacion_evoluciones"),
        {QStringLiteral("uuid"), QStringLiteral("fecha"), QStringLiteral("nota"),
         QStringLiteral("peso_kg"), QStringLiteral("temperatura_c"),
         QStringLiteral("frecuencia_cardiaca"), QStringLiteral("created_at"),
         QStringLiteral("updated_at"), QStringLiteral("deleted_at")},
        {{QStringLiteral("hospitalizacion_id"), QStringLiteral("hospitalizaciones"),
          QStringLiteral("hospitalizacion_uuid"), true},
         {QStringLiteral("usuario_id"), QStringLiteral("usuarios"),
          QStringLiteral("usuario_uuid"), false}},
        50,
    };
}

EntidadSync entidadCirugia()
{
    return {
        QStringLiteral("cirugias"),
        QStringLiteral("cirugias"),
        {QStringLiteral("uuid"), QStringLiteral("fecha_programada"),
         QStringLiteral("fecha_inicio"), QStringLiteral("fecha_fin"),
         QStringLiteral("procedimiento"), QStringLiteral("anestesia"),
         QStringLiteral("diagnostico_pre"), QStringLiteral("diagnostico_post"),
         QStringLiteral("estado"), QStringLiteral("notas"),
         QStringLiteral("created_at"), QStringLiteral("updated_at"),
         QStringLiteral("deleted_at")},
        {{QStringLiteral("mascota_id"), QStringLiteral("mascotas"),
          QStringLiteral("mascota_uuid"), true},
         {QStringLiteral("consulta_id"), QStringLiteral("consultas_medicas"),
          QStringLiteral("consulta_uuid"), false},
         {QStringLiteral("hospitalizacion_id"), QStringLiteral("hospitalizaciones"),
          QStringLiteral("hospitalizacion_uuid"), false},
         {QStringLiteral("veterinario_id"), QStringLiteral("usuarios"),
          QStringLiteral("veterinario_uuid"), false}},
        50,
    };
}

EntidadSync entidadEstetica()
{
    return {
        QStringLiteral("estetica_servicios"),
        QStringLiteral("estetica_servicios"),
        {QStringLiteral("uuid"), QStringLiteral("fecha"), QStringLiteral("tipo"),
         QStringLiteral("precio"), QStringLiteral("observaciones"),
         QStringLiteral("estado"), QStringLiteral("created_at"),
         QStringLiteral("updated_at"), QStringLiteral("deleted_at")},
        {{QStringLiteral("mascota_id"), QStringLiteral("mascotas"),
          QStringLiteral("mascota_uuid"), true},
         {QStringLiteral("cita_id"), QStringLiteral("citas"),
          QStringLiteral("cita_uuid"), false},
         {QStringLiteral("estilista_id"), QStringLiteral("usuarios"),
          QStringLiteral("estilista_uuid"), false},
         {QStringLiteral("servicio_id"), QStringLiteral("servicios"),
          QStringLiteral("servicio_uuid"), false}},
        50,
    };
}

EntidadSync entidadMovimientoInventario()
{
    return {
        QStringLiteral("movimientos_inventario"),
        QStringLiteral("movimientos_inventario"),
        {QStringLiteral("uuid"), QStringLiteral("tipo"), QStringLiteral("cantidad"),
         QStringLiteral("motivo"), QStringLiteral("referencia_tipo"),
         QStringLiteral("referencia_id"), QStringLiteral("created_at"),
         QStringLiteral("updated_at"), QStringLiteral("deleted_at")},
        {{QStringLiteral("producto_id"), QStringLiteral("productos_insumos"),
          QStringLiteral("producto_uuid"), true},
         {QStringLiteral("usuario_id"), QStringLiteral("usuarios"),
          QStringLiteral("usuario_uuid"), false}},
        50,
    };
}

QString sqlDeFila(const EntidadSync& entidad)
{
    QStringList seleccion;
    for (const QString& columna : entidad.columnas) {
        seleccion.append(QStringLiteral("t.%1").arg(columna));
    }

    QStringList uniones;
    for (const ReferenciaSync& referencia : entidad.referencias) {
        const QString alias = QStringLiteral("p_%1").arg(referencia.columna);
        seleccion.append(QStringLiteral("%1.uuid").arg(alias));
        uniones.append(
            QStringLiteral("LEFT JOIN %1 %2 ON %2.id = t.%3")
                .arg(referencia.tabla, alias, referencia.columna));
    }

    return QStringLiteral("SELECT %1 FROM %2 t %3 WHERE t.uuid = ?")
        .arg(seleccion.join(QStringLiteral(", ")), entidad.tabla,
             uniones.join(QStringLiteral(" ")));
}

QVariant valorDeJson(const QJsonValue& valor)
{
    if (valor.isNull() || valor.isUndefined()) {
        return {};
    }
    if (valor.isBool()) {
        return QVariant(valor.toBool());
    }
    if (valor.isString()) {
        return QVariant(valor.toString());
    }
    if (valor.isDouble()) {
        const double numero = valor.toDouble();
        if (numero >= static_cast<double>(std::numeric_limits<qint64>::min())
            && numero <= static_cast<double>(std::numeric_limits<qint64>::max())
            && std::floor(numero) == numero) {
            return QVariant::fromValue<qint64>(static_cast<qint64>(numero));
        }
        return QVariant(numero);
    }
    return {};
}

bool resolverReferencia(IQueryExecutor& local, const ReferenciaSync& referencia,
                        const QJsonObject& payload, QVariant& valor, QString* error)
{
    const QJsonValue uuid = payload.value(referencia.clave);
    if (uuid.isNull() || uuid.isUndefined() || uuid.toString().isEmpty()) {
        valor = {};
        if (referencia.obligatoria) {
            if (error != nullptr) {
                *error = QStringLiteral("falta la referencia %1").arg(referencia.clave);
            }
            return false;
        }
        return true;
    }

    const auto filas = local.query(
        QStringLiteral("SELECT id FROM %1 WHERE uuid = ?").arg(referencia.tabla),
        {QVariant(uuid.toString())});
    if (filas.size() != 1 || filas.front().values.empty()) {
        if (error != nullptr) {
            *error = QStringLiteral("no existe %1 con uuid %2")
                         .arg(referencia.tabla, uuid.toString());
        }
        return false;
    }
    valor = filas.front().values.front();
    return true;
}

} // namespace

const QList<EntidadSync>& entidadesSync()
{
    static const QList<EntidadSync> entidades = {
        entidadCliente(),  entidadMascota(),       entidadCita(),
        entidadConsulta(), entidadReceta(),        entidadRecetaDetalle(),
        entidadVacuna(),   entidadSignos(),        entidadLaboratorio(),
        entidadHospitalizacion(), entidadEvolucion(), entidadCirugia(),
        entidadEstetica(), entidadMovimientoInventario(),
    };
    return entidades;
}

const EntidadSync* entidadSync(const QString& nombre)
{
    for (const EntidadSync& entidad : entidadesSync()) {
        if (entidad.nombre == nombre) {
            return &entidad;
        }
    }
    return nullptr;
}

bool serializarEntidad(IQueryExecutor& local, const EntidadSync& entidad,
                       const QString& uuid, QJsonObject& payload, QString* error)
{
    const auto filas = local.query(sqlDeFila(entidad), {QVariant(uuid)});
    if (filas.size() != 1) {
        if (error != nullptr) {
            *error = QStringLiteral("no existe %1 con uuid %2").arg(entidad.tabla, uuid);
        }
        return false;
    }

    const auto& valores = filas.front().values;
    const int total = entidad.columnas.size() + entidad.referencias.size();
    if (static_cast<int>(valores.size()) != total) {
        if (error != nullptr) {
            *error = QStringLiteral("columnas inesperadas al serializar %1").arg(entidad.tabla);
        }
        return false;
    }

    payload = {};
    int indice = 0;
    for (const QString& columna : entidad.columnas) {
        const QVariant& valor = valores[static_cast<size_t>(indice++)];
        payload.insert(columna, valor.isNull() ? QJsonValue(QJsonValue::Null)
                                               : QJsonValue::fromVariant(valor));
    }
    for (const ReferenciaSync& referencia : entidad.referencias) {
        const QVariant& valor = valores[static_cast<size_t>(indice++)];
        payload.insert(referencia.clave,
                       valor.isNull() ? QJsonValue(QJsonValue::Null)
                                      : QJsonValue(valor.toString()));
    }
    return true;
}

bool aplicarUpsert(IQueryExecutor& local, const EntidadSync& entidad,
                   const QJsonObject& payload, QString* error)
{
    const QString uuid = payload.value(QStringLiteral("uuid")).toString();
    if (uuid.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("evento sin uuid para %1").arg(entidad.tabla);
        }
        return false;
    }

    QStringList columnas = entidad.columnas;
    std::vector<QVariant> valores;
    valores.reserve(static_cast<size_t>(entidad.columnas.size() + entidad.referencias.size()));
    for (const QString& columna : entidad.columnas) {
        valores.push_back(valorDeJson(payload.value(columna)));
    }

    for (const ReferenciaSync& referencia : entidad.referencias) {
        QVariant valor;
        if (!resolverReferencia(local, referencia, payload, valor, error)) {
            return false;
        }
        columnas.append(referencia.columna);
        valores.push_back(valor);
    }

    QStringList marcadores;
    QStringList actualizaciones;
    for (const QString& columna : columnas) {
        marcadores.append(QStringLiteral("?"));
        if (columna == QStringLiteral("uuid")) {
            continue;
        }
        actualizaciones.append(QStringLiteral("%1 = excluded.%1").arg(columna));
    }
    actualizaciones.append(QStringLiteral("sync_state = 'sincronizado'"));

    const QString sql =
        QStringLiteral("INSERT INTO %1 (%2, sync_state) VALUES (%3, 'sincronizado') "
                       "ON CONFLICT(uuid) DO UPDATE SET %4 "
                       "WHERE excluded.updated_at >= %1.updated_at")
            .arg(entidad.tabla, columnas.join(QStringLiteral(", ")),
                 marcadores.join(QStringLiteral(", ")),
                 actualizaciones.join(QStringLiteral(", ")));

    const ExecResult aplicado = local.execute(sql, valores);
    if (!aplicado.ok && error != nullptr) {
        *error = QStringLiteral("no se pudo aplicar %1 %2").arg(entidad.tabla, uuid);
    }
    return aplicado.ok;
}

bool aplicarDelete(IQueryExecutor& local, const EntidadSync& entidad,
                   const QString& uuid, qint64 updated_at, QString* error)
{
    const ExecResult aplicado = local.execute(
        QStringLiteral("UPDATE %1 SET deleted_at = ?, updated_at = ?, "
                       "sync_state = 'sincronizado' "
                       "WHERE uuid = ? AND updated_at <= ?")
            .arg(entidad.tabla),
        {QVariant::fromValue(updated_at), QVariant::fromValue(updated_at),
         QVariant(uuid), QVariant::fromValue(updated_at)});
    if (!aplicado.ok && error != nullptr) {
        *error = QStringLiteral("no se pudo aplicar el borrado de %1 %2")
                     .arg(entidad.tabla, uuid);
    }
    return aplicado.ok;
}

} // namespace fuzzy::core
