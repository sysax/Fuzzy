#include "fuzzy/core/PortalCliente.h"

#include "fuzzy/core/MovilStore.h"
#include "fuzzy/core/Uuid.h"

#include <QDateTime>
#include <QVariant>

namespace fuzzy::core {

namespace {

const QString kTablaMeta = QStringLiteral("portal_meta");
const QString kUuidMeta = QStringLiteral("portal");
const QString kFoto = QStringLiteral("foto");
const QString kFotoB64 = QStringLiteral("foto_b64");
const QString kAhora = QStringLiteral("CAST(strftime('%s', 'now') AS INTEGER) * 1000");

qint64 msAhora()
{
    return QDateTime::currentMSecsSinceEpoch();
}

// Solo se aceptan columnas declaradas en la lista blanca. Un campo que llega
// de la interfaz con otro nombre se descarta aquí, antes de tocar SQL.
QStringList columnasValidas(const QString& tabla, const QVariantMap& datos)
{
    QStringList salida;
    for (const QString& columna : PortalCliente::columnasDe(tabla)) {
        if (columna == QLatin1String("created_at") || columna == QLatin1String("updated_at")) {
            continue;
        }
        if (datos.contains(columna)) {
            salida.append(columna);
        } else if (columna == QLatin1String("foto") && datos.contains(kFotoB64)) {
            salida.append(columna);
        }
    }
    return salida;
}

QVariant valorDe(const QVariantMap& datos, const QString& columna)
{
    if (columna == QLatin1String("foto") && !datos.contains(kFoto)
        && datos.contains(kFotoB64)) {
        return QVariant(QByteArray::fromBase64(
            datos.value(kFotoB64).toString().toLatin1()));
    }
    return datos.value(columna);
}

} // namespace

PortalCliente::PortalCliente(IQueryExecutor& portal, Sonda sonda)
    : portal_(portal), sonda_(std::move(sonda))
{
}

const QList<TablaPortal>& PortalCliente::tablas()
{
    // `portal_meta` se guarda con uuid sintético "portal" porque la tabla tiene
    // una sola fila y no lleva uuid propio.
    static const QList<TablaPortal> lista = {
        {kTablaMeta,
         {QStringLiteral("esquema_version"), QStringLiteral("cuenta_uuid"),
          QStringLiteral("clinica_uuid"), QStringLiteral("generado_at"),
          QStringLiteral("clinica_nombre"), QStringLiteral("proyeccion_version"),
          QStringLiteral("proyectado_at")}},
        {QStringLiteral("perfil"),
         {QStringLiteral("uuid"), QStringLiteral("nombre_completo"), QStringLiteral("correo"),
          QStringLiteral("telefono"), QStringLiteral("direccion"), QStringLiteral("dni"),
          QStringLiteral("activo"), QStringLiteral("created_at"), QStringLiteral("updated_at"),
          QStringLiteral("deleted_at")}},
        {QStringLiteral("mascotas"),
         {QStringLiteral("uuid"), QStringLiteral("nombre"), QStringLiteral("especie"),
          QStringLiteral("raza"), QStringLiteral("sexo"), QStringLiteral("peso_kg"),
          QStringLiteral("numero_chip"), QStringLiteral("fecha_nacimiento"),
          QStringLiteral("activo"), QStringLiteral("castrado"), QStringLiteral("alergias"),
          QStringLiteral("atencion_especial"), QStringLiteral("created_at"),
          QStringLiteral("updated_at"), QStringLiteral("deleted_at")}},
        {QStringLiteral("mascota_fotos"),
         {QStringLiteral("uuid"), QStringLiteral("mascota_uuid"), QStringLiteral("foto"),
          QStringLiteral("mime"), QStringLiteral("sha256"), QStringLiteral("ancho"),
          QStringLiteral("alto"), QStringLiteral("actualizada_at"),
          QStringLiteral("created_at"), QStringLiteral("updated_at"),
          QStringLiteral("deleted_at")}},
        {QStringLiteral("visitas"),
         {QStringLiteral("uuid"), QStringLiteral("mascota_uuid"), QStringLiteral("fecha"),
          QStringLiteral("motivo"), QStringLiteral("diagnostico"),
          QStringLiteral("anamnesis"), QStringLiteral("examen_fisico"),
          QStringLiteral("tratamiento"), QStringLiteral("veterinario"),
          QStringLiteral("es_emergencia"), QStringLiteral("estado"),
          QStringLiteral("cerrada_at"), QStringLiteral("created_at"),
          QStringLiteral("updated_at"), QStringLiteral("deleted_at")}},
        {QStringLiteral("recetas"),
         {QStringLiteral("uuid"), QStringLiteral("mascota_uuid"), QStringLiteral("visita_uuid"),
          QStringLiteral("fecha_emision"), QStringLiteral("indicaciones"),
          QStringLiteral("veterinario"), QStringLiteral("created_at"),
          QStringLiteral("updated_at"), QStringLiteral("deleted_at")}},
        {QStringLiteral("receta_items"),
         {QStringLiteral("uuid"), QStringLiteral("receta_uuid"),
          QStringLiteral("medicamento"), QStringLiteral("dosis"),
          QStringLiteral("frecuencia"), QStringLiteral("duracion"),
          QStringLiteral("cantidad"), QStringLiteral("created_at"),
          QStringLiteral("updated_at"), QStringLiteral("deleted_at")}},
        {QStringLiteral("vacunas"),
         {QStringLiteral("uuid"), QStringLiteral("mascota_uuid"),
          QStringLiteral("nombre_vacuna"), QStringLiteral("fecha_aplicacion"),
          QStringLiteral("proxima_dosis"), QStringLiteral("lote"), QStringLiteral("notas"),
          QStringLiteral("veterinario"), QStringLiteral("created_at"),
          QStringLiteral("updated_at"), QStringLiteral("deleted_at")}},
        {QStringLiteral("laboratorio"),
         {QStringLiteral("uuid"), QStringLiteral("mascota_uuid"),
          QStringLiteral("fecha_solicitud"), QStringLiteral("fecha_resultado"),
          QStringLiteral("tipo_examen"), QStringLiteral("resultado"),
          QStringLiteral("estado"), QStringLiteral("solicitado_por"),
          QStringLiteral("created_at"), QStringLiteral("updated_at"),
          QStringLiteral("deleted_at")}},
        {QStringLiteral("pesos"),
         {QStringLiteral("uuid"), QStringLiteral("mascota_uuid"), QStringLiteral("fecha"),
          QStringLiteral("peso_kg"), QStringLiteral("created_at"),
          QStringLiteral("updated_at"), QStringLiteral("deleted_at")}},
        {QStringLiteral("citas"),
         {QStringLiteral("uuid"), QStringLiteral("mascota_uuid"),
          QStringLiteral("fecha_hora"), QStringLiteral("tipo_servicio"),
          QStringLiteral("estado"), QStringLiteral("direccion"), QStringLiteral("notas"),
          QStringLiteral("origen"), QStringLiteral("created_at"),
          QStringLiteral("updated_at"), QStringLiteral("deleted_at")}},
        {QStringLiteral("solicitudes_cita"),
         {QStringLiteral("uuid"), QStringLiteral("mascota_uuid"),
          QStringLiteral("tipo_servicio"), QStringLiteral("fecha_preferida"),
          QStringLiteral("hora_preferida"), QStringLiteral("motivo"),
          QStringLiteral("estado"), QStringLiteral("respuesta"),
          QStringLiteral("cita_uuid"), QStringLiteral("created_at"),
          QStringLiteral("updated_at"), QStringLiteral("deleted_at")}},
        {QStringLiteral("adjuntos"),
         {QStringLiteral("uuid"), QStringLiteral("mascota_uuid"),
          QStringLiteral("visita_uuid"), QStringLiteral("nombre"), QStringLiteral("mime"),
          QStringLiteral("sha256"), QStringLiteral("tamano"), QStringLiteral("created_at"),
          QStringLiteral("updated_at"), QStringLiteral("deleted_at")}},
    };
    return lista;
}

bool PortalCliente::tablaConocida(const QString& tabla)
{
    for (const TablaPortal& conocida : tablas()) {
        if (conocida.nombre == tabla) {
            return true;
        }
    }
    return false;
}

QStringList PortalCliente::columnasDe(const QString& tabla)
{
    for (const TablaPortal& conocida : tablas()) {
        if (conocida.nombre == tabla) {
            return conocida.columnas;
        }
    }
    return {};
}

ResultadoDescarga PortalCliente::descargar(MovilStore& store, const QString& portal_uuid,
                                           const QString& clinica_esperada)
{
    ResultadoDescarga resultado;
    if (!store.desbloqueado()) {
        resultado.error = QStringLiteral("La caché está bloqueada");
        return resultado;
    }

    // ── Verificación de sede antes de escribir una sola fila ──
    const QStringList columnasMeta = columnasDe(kTablaMeta);
    const auto meta = portal_.query(
        QStringLiteral("SELECT %1 FROM %2 WHERE id = 1")
            .arg(columnasMeta.join(QStringLiteral(", ")), kTablaMeta));
    if (meta.size() != 1) {
        resultado.error = QStringLiteral("No se pudo leer el portal; "
                                         "pide re-emparejar en la clínica");
        ultimo_error_ = resultado.error;
        return resultado;
    }
    QVariantMap filaMeta;
    for (int i = 0; i < columnasMeta.size(); ++i) {
        filaMeta.insert(columnasMeta.at(i), meta.front().values.at(i));
    }
    const QString clinica = filaMeta.value(QStringLiteral("clinica_uuid")).toString();
    if (clinica.isEmpty() || filaMeta.value(QStringLiteral("cuenta_uuid")).toString().isEmpty()) {
        resultado.error = QStringLiteral("El portal no tiene identidad de sede");
        ultimo_error_ = resultado.error;
        return resultado;
    }
    if (!clinica_esperada.isEmpty() && clinica != clinica_esperada) {
        // Un portal de otra clínica no se escribe jamás en esta caché: sería
        // mezclar dos dueños en el mismo teléfono.
        resultado.error = QStringLiteral("El portal pertenece a otra clínica");
        ultimo_error_ = resultado.error;
        return resultado;
    }

    filaMeta.insert(QStringLiteral("uuid"), kUuidMeta);
    filaMeta.insert(QStringLiteral("updated_at"),
                    filaMeta.value(QStringLiteral("generado_at")));
    if (!store.guardarFila(portal_uuid, kTablaMeta, filaMeta, false, true)) {
        resultado.error = QStringLiteral("No se pudo guardar la cabecera del portal");
        ultimo_error_ = resultado.error;
        return resultado;
    }
    ++resultado.filas;

    for (const TablaPortal& tabla : tablas()) {
        if (tabla.nombre == kTablaMeta) {
            continue;
        }
        const auto filas = portal_.query(
            QStringLiteral("SELECT %1 FROM %2")
                .arg(tabla.columnas.join(QStringLiteral(", ")), tabla.nombre));
        for (const auto& cruda : filas) {
            QVariantMap fila;
            for (int i = 0; i < tabla.columnas.size(); ++i) {
                const QString columna = tabla.columnas.at(i);
                const QVariant valor = cruda.values.at(i);
                if (columna == kFoto) {
                    // Un BLOB no cabe en el JSON cifrado: se guarda en base64
                    // y se vuelve a binario al subir una foto nueva.
                    fila.insert(kFotoB64,
                                QString::fromLatin1(valor.toByteArray().toBase64()));
                } else {
                    fila.insert(columna, valor);
                }
            }
            if (!store.guardarFila(portal_uuid, tabla.nombre, fila, false, true)) {
                continue;
            }
            ++resultado.filas;
        }
    }

    resultado.ok = true;
    ultimo_error_.clear();
    return resultado;
}

bool PortalCliente::subirPendiente(const QString& tabla, const QVariantMap& datos)
{
    if (!tablaConocida(tabla) || tabla == kTablaMeta) {
        ultimo_error_ = QStringLiteral("Tabla no permitida: %1").arg(tabla);
        return false;
    }
    const QString uuid = datos.value(QStringLiteral("uuid")).toString();
    if (uuid.isEmpty()) {
        ultimo_error_ = QStringLiteral("La escritura no tiene uuid");
        return false;
    }

    QStringList columnas = columnasValidas(tabla, datos);
    if (!columnas.contains(QStringLiteral("uuid"))) {
        columnas.prepend(QStringLiteral("uuid"));
    }
    if (!columnas.contains(QStringLiteral("created_at"))) {
        columnas.append(QStringLiteral("created_at"));
    }
    if (!columnas.contains(QStringLiteral("updated_at"))) {
        columnas.append(QStringLiteral("updated_at"));
    }

    QStringList marcadores;
    std::vector<QVariant> valores;
    for (const QString& columna : columnas) {
        marcadores.append(QStringLiteral("?"));
        if (columna == QLatin1String("created_at") || columna == QLatin1String("updated_at")) {
            const QVariant valor = datos.value(columna);
            valores.push_back(valor.isValid() ? valor
                                              : QVariant::fromValue(msAhora()));
        } else {
            valores.push_back(valorDe(datos, columna));
        }
    }

    // Las solicitudes de cita son de solo inserción: el nodo es el dueño del
    // estado y de la respuesta. El perfil y la foto sí se actualizan, siempre
    // con la versión más reciente.
    QString conflicto;
    if (tabla == QLatin1String("perfil") || tabla == QLatin1String("mascota_fotos")) {
        QStringList actualizaciones;
        for (const QString& columna : columnas) {
            if (columna != QLatin1String("uuid")) {
                actualizaciones.append(QStringLiteral("%1 = excluded.%1").arg(columna));
            }
        }
        conflicto = QStringLiteral("ON CONFLICT(uuid) DO UPDATE SET %1 "
                                   "WHERE excluded.updated_at >= %2.updated_at")
                        .arg(actualizaciones.join(QStringLiteral(", ")), tabla);
    } else {
        conflicto = QStringLiteral("ON CONFLICT(uuid) DO NOTHING");
    }

    const QString sql = QStringLiteral("INSERT INTO %1 (%2) VALUES (%3) %4")
                            .arg(tabla, columnas.join(QStringLiteral(", ")),
                                 marcadores.join(QStringLiteral(", ")), conflicto);
    const bool ok = portal_.execute(sql, valores).ok;
    if (!ok) {
        ultimo_error_ = QStringLiteral("La clínica rechazó la escritura de %1").arg(tabla);
    }
    return ok;
}

bool PortalCliente::empujar(MovilStore& store, const QString& portal_uuid,
                            ResultadoSync& resultado)
{
    const QList<EntradaOutbox> pendientes = store.pendientes(portal_uuid);
    for (const EntradaOutbox& entrada : pendientes) {
        if (subirPendiente(entrada.tabla, entrada.datos)) {
            store.quitarPendiente(entrada.id);
            ++resultado.enviadas;
        } else {
            store.registrarFallo(entrada.id, ultimo_error_);
            ++resultado.fallidas;
        }
    }
    return resultado.fallidas == 0;
}

ResultadoSync PortalCliente::sincronizar(MovilStore& store, const QString& portal_uuid)
{
    ResultadoSync resultado;
    PortalConocido conocido;
    if (!store.portal(portal_uuid, &conocido)) {
        resultado.estado = QStringLiteral("sin_portal");
        resultado.error = QStringLiteral("El portal no está emparejado");
        return resultado;
    }
    if (!store.desbloqueado()) {
        resultado.estado = QStringLiteral("sin_clave");
        resultado.error = QStringLiteral("La caché está bloqueada");
        return resultado;
    }

    if (sonda_) {
        const QString salud = sonda_();
        if (salud != QLatin1String("ok")) {
            resultado.estado = salud.isEmpty() ? QStringLiteral("sin_red") : salud;
            if (resultado.estado == QLatin1String("sin_acceso")) {
                // La siguiente consulta niega: el portal fue revocado o su
                // token rotó. Se conserva la caché (cifrada, caduca a 30 días)
                // y se pide re-emparejar.
                store.marcarEstado(portal_uuid, QStringLiteral("revocado"));
                resultado.error = QStringLiteral("La clínica revocó el acceso; "
                                                 "pide un nuevo emparejamiento");
            } else {
                resultado.error = QStringLiteral("Sin conexión; "
                                                 "los cambios quedan en cola");
            }
            return resultado;
        }
    }

    // 1. Subir lo pendiente. Un fallo deja la cola para el siguiente intento.
    empujar(store, portal_uuid, resultado);

    // 2. Bajar el portal. La verificación de `portal_meta.clinica_uuid` ocurre
    //    dentro de `descargar` y usa el `clinica_uuid` del emparejamiento.
    const ResultadoDescarga descarga = descargar(store, portal_uuid, conocido.clinica_uuid);
    if (!descarga.ok) {
        resultado.error = descarga.error;
        if (sonda_ && sonda_() == QLatin1String("sin_acceso")) {
            resultado.estado = QStringLiteral("sin_acceso");
            store.marcarEstado(portal_uuid, QStringLiteral("revocado"));
        } else {
            resultado.estado = QStringLiteral("sin_red");
        }
        return resultado;
    }
    resultado.descargadas = descarga.filas;

    store.registrarSync(portal_uuid, msAhora());
    resultado.ok = true;
    resultado.estado = QStringLiteral("ok");
    return resultado;
}

bool PortalCliente::solicitarCita(MovilStore& store, const QString& portal_uuid,
                                  const QString& mascota_uuid, const QString& tipo_servicio,
                                  const QString& fecha_preferida,
                                  const QString& hora_preferida, const QString& motivo,
                                  QString uuid, qint64 ahora)
{
    if (mascota_uuid.trimmed().isEmpty() || tipo_servicio.trimmed().isEmpty()
        || fecha_preferida.trimmed().isEmpty() || hora_preferida.trimmed().isEmpty()) {
        ultimo_error_ = QStringLiteral("Faltan datos de la cita");
        return false;
    }
    const qint64 momento = ahora > 0 ? ahora : msAhora();
    const QString solicitud = uuid.trimmed().isEmpty() ? uuidNuevo() : uuid.trimmed();

    QVariantMap fila;
    fila.insert(QStringLiteral("uuid"), solicitud);
    fila.insert(QStringLiteral("mascota_uuid"), mascota_uuid.trimmed());
    fila.insert(QStringLiteral("tipo_servicio"), tipo_servicio.trimmed());
    fila.insert(QStringLiteral("fecha_preferida"), fecha_preferida.trimmed());
    fila.insert(QStringLiteral("hora_preferida"), hora_preferida.trimmed());
    fila.insert(QStringLiteral("motivo"),
                motivo.trimmed().isEmpty() ? QVariant() : QVariant(motivo.trimmed()));
    fila.insert(QStringLiteral("estado"), QStringLiteral("Pendiente"));
    fila.insert(QStringLiteral("created_at"), momento);
    fila.insert(QStringLiteral("updated_at"), momento);

    if (!store.guardarFila(portal_uuid, QStringLiteral("solicitudes_cita"), fila, true)) {
        ultimo_error_ = QStringLiteral("No se pudo guardar la solicitud en el teléfono");
        return false;
    }
    if (!store.encolar(portal_uuid, QStringLiteral("solicitudes_cita"), solicitud, fila,
                       momento)) {
        ultimo_error_ = QStringLiteral("No se pudo encolar la solicitud");
        return false;
    }
    return true;
}

bool PortalCliente::actualizarPerfil(MovilStore& store, const QString& portal_uuid,
                                     const QVariantMap& campos, qint64 ahora)
{
    const QList<QVariantMap> perfiles = store.filas(portal_uuid, QStringLiteral("perfil"));
    if (perfiles.isEmpty()) {
        ultimo_error_ = QStringLiteral("El portal todavía no se ha descargado; "
                                       "no hay perfil que actualizar");
        return false;
    }
    QVariantMap fila = perfiles.front();
    const QStringList editables = {
        QStringLiteral("nombre_completo"), QStringLiteral("correo"),
        QStringLiteral("telefono"), QStringLiteral("direccion"), QStringLiteral("dni")};
    for (const QString& campo : editables) {
        if (campos.contains(campo)) {
            fila.insert(campo, campos.value(campo));
        }
    }
    const qint64 momento = ahora > 0 ? ahora : msAhora();
    fila.insert(QStringLiteral("updated_at"), momento);

    const QString uuid = fila.value(QStringLiteral("uuid")).toString();
    if (uuid.isEmpty()
        || !store.guardarFila(portal_uuid, QStringLiteral("perfil"), fila, true)
        || !store.encolar(portal_uuid, QStringLiteral("perfil"), uuid, fila, momento)) {
        ultimo_error_ = QStringLiteral("No se pudo guardar el perfil en el teléfono");
        return false;
    }
    return true;
}

} // namespace fuzzy::core
