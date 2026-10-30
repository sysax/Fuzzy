#include "pacientes/PacientesController.h"

#include "common/Consulta.h"
#include "fuzzy/core/Uuid.h"

#include <QDate>
#include <QDateTime>
#include <QVariant>

namespace fuzzy::desktop {

using fuzzy::core::QueryRow;

PacientesController::PacientesController(fuzzy::core::IQueryExecutor& db, QObject* parent)
    : QObject(parent), db_(db)
{
    recargar();
}

int PacientesController::totalPaginas() const
{
    if (total_ <= 0) {
        return 1;
    }
    return (total_ + porPagina_ - 1) / porPagina_;
}

qint64 PacientesController::idDe(const QString& tabla, const QString& uuid) const
{
    if (uuid.isEmpty()) {
        return 0;
    }
    const auto filas = db_.query(
        QStringLiteral("SELECT id FROM %1 WHERE uuid = ? AND deleted_at IS NULL").arg(tabla),
        {QVariant(uuid)});
    return filas.empty() ? 0 : enteroDeFila(filas.front(), 0);
}

void PacientesController::fallo(const QString& texto)
{
    error_ = texto;
    mensaje_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

void PacientesController::exito(const QString& texto)
{
    mensaje_ = texto;
    error_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

void PacientesController::recargar()
{
    QString sql =
        QStringLiteral("SELECT m.uuid, m.nombre, m.especie, m.raza, m.sexo, m.peso_kg, "
                       "cl.nombre_completo, cl.uuid, m.activo, m.alergias "
                       "FROM mascotas m JOIN clientes cl ON cl.id = m.cliente_id "
                       "WHERE m.deleted_at IS NULL AND cl.deleted_at IS NULL ");
    std::vector<QVariant> params;
    if (!filtro_.isEmpty()) {
        sql += QStringLiteral("AND (lower(m.nombre) LIKE ? OR lower(cl.nombre_completo) LIKE ? "
                              "OR lower(coalesce(m.numero_chip, '')) LIKE ?) ");
        const QString patron = QStringLiteral("%") + filtro_.toLower() + QStringLiteral("%");
        params.push_back(QVariant(patron));
        params.push_back(QVariant(patron));
        params.push_back(QVariant(patron));
    }
    if (!especieFiltro_.isEmpty()) {
        sql += QStringLiteral("AND m.especie = ? ");
        params.push_back(QVariant(especieFiltro_));
    }
    sql += QStringLiteral("ORDER BY m.nombre COLLATE NOCASE LIMIT ? OFFSET ?");
    params.push_back(QVariant::fromValue(porPagina_));
    params.push_back(QVariant::fromValue(pagina_ * porPagina_));

    const auto filas = db_.query(sql, params);
    pacientes_.clear();
    for (const auto& fila : filas) {
        QVariantMap paciente;
        paciente.insert(QStringLiteral("uuid"), textoDeFila(fila, 0));
        paciente.insert(QStringLiteral("nombre"), textoDeFila(fila, 1));
        paciente.insert(QStringLiteral("especie"), textoDeFila(fila, 2));
        paciente.insert(QStringLiteral("raza"), textoDeFila(fila, 3));
        paciente.insert(QStringLiteral("sexo"), textoDeFila(fila, 4));
        paciente.insert(QStringLiteral("peso"), realDeFila(fila, 5));
        paciente.insert(QStringLiteral("propietario"), textoDeFila(fila, 6));
        paciente.insert(QStringLiteral("propietarioUuid"), textoDeFila(fila, 7));
        paciente.insert(QStringLiteral("activo"), enteroDeFila(fila, 8) == 1);
        paciente.insert(QStringLiteral("alergias"), textoDeFila(fila, 9));
        pacientes_.append(paciente);
    }

    QString conteo =
        QStringLiteral("SELECT count(*) FROM mascotas m JOIN clientes cl ON cl.id = m.cliente_id "
                       "WHERE m.deleted_at IS NULL AND cl.deleted_at IS NULL ");
    std::vector<QVariant> paramsConteo;
    if (!filtro_.isEmpty()) {
        conteo += QStringLiteral("AND (lower(m.nombre) LIKE ? OR lower(cl.nombre_completo) LIKE ? "
                                 "OR lower(coalesce(m.numero_chip, '')) LIKE ?) ");
        const QString patron = QStringLiteral("%") + filtro_.toLower() + QStringLiteral("%");
        paramsConteo.push_back(QVariant(patron));
        paramsConteo.push_back(QVariant(patron));
        paramsConteo.push_back(QVariant(patron));
    }
    if (!especieFiltro_.isEmpty()) {
        conteo += QStringLiteral("AND m.especie = ? ");
        paramsConteo.push_back(QVariant(especieFiltro_));
    }
    const auto totalFilas = db_.query(conteo, paramsConteo);
    total_ = totalFilas.empty() ? 0 : static_cast<int>(enteroDeFila(totalFilas.front(), 0));

    especies_ = listaDeFilas(
        db_.query(QStringLiteral("SELECT DISTINCT especie FROM mascotas "
                                 "WHERE deleted_at IS NULL ORDER BY especie")),
        {QStringLiteral("especie")});

    emit datosCambiados();
}

void PacientesController::cargar()
{
    recargar();
}

void PacientesController::buscar(const QString& texto)
{
    filtro_ = texto.trimmed();
    pagina_ = 0;
    recargar();
}

void PacientesController::filtrarEspecie(const QString& especie)
{
    especieFiltro_ = especie;
    pagina_ = 0;
    recargar();
}

void PacientesController::irPagina(int pagina)
{
    pagina_ = qBound(0, pagina, totalPaginas() - 1);
    recargar();
}

void PacientesController::seleccionar(const QString& mascotaUuid)
{
    const auto filas = db_.query(
        QStringLiteral("SELECT m.uuid, m.nombre, m.especie, m.raza, m.sexo, m.peso_kg, "
                       "m.numero_chip, m.fecha_nacimiento, m.activo, m.castrado, "
                       "m.alergias, m.atencion_especial, cl.nombre_completo, cl.telefono, "
                       "cl.correo, cl.direccion, cl.uuid, m.created_at "
                       "FROM mascotas m JOIN clientes cl ON cl.id = m.cliente_id "
                       "WHERE m.uuid = ? AND m.deleted_at IS NULL"),
        {QVariant(mascotaUuid)});
    if (filas.size() != 1) {
        fallo(QStringLiteral("el paciente no existe"));
        return;
    }

    const auto& fila = filas.front();
    ficha_.clear();
    ficha_.insert(QStringLiteral("uuid"), textoDeFila(fila, 0));
    ficha_.insert(QStringLiteral("nombre"), textoDeFila(fila, 1));
    ficha_.insert(QStringLiteral("especie"), textoDeFila(fila, 2));
    ficha_.insert(QStringLiteral("raza"), textoDeFila(fila, 3));
    ficha_.insert(QStringLiteral("sexo"), textoDeFila(fila, 4));
    ficha_.insert(QStringLiteral("peso"), realDeFila(fila, 5));
    ficha_.insert(QStringLiteral("chip"), textoDeFila(fila, 6));
    ficha_.insert(QStringLiteral("nacimiento"), textoDeFila(fila, 7));
    ficha_.insert(QStringLiteral("activo"), enteroDeFila(fila, 8) == 1);
    ficha_.insert(QStringLiteral("castrado"), enteroDeFila(fila, 9) == 1);
    ficha_.insert(QStringLiteral("alergias"), textoDeFila(fila, 10));
    ficha_.insert(QStringLiteral("atencionEspecial"), textoDeFila(fila, 11));
    ficha_.insert(QStringLiteral("propietario"), textoDeFila(fila, 12));
    ficha_.insert(QStringLiteral("telefono"), textoDeFila(fila, 13));
    ficha_.insert(QStringLiteral("correo"), textoDeFila(fila, 14));
    ficha_.insert(QStringLiteral("direccion"), textoDeFila(fila, 15));
    ficha_.insert(QStringLiteral("propietarioUuid"), textoDeFila(fila, 16));
    ficha_.insert(QStringLiteral("alta"), fechaTexto(enteroDeFila(fila, 17)));

    const auto consultas = db_.query(
        QStringLiteral("SELECT c.uuid, c.fecha_consulta, coalesce(c.motivo_visita, ''), "
                       "coalesce(c.diagnostico, ''), coalesce(c.tratamiento_indicado, ''), "
                       "c.estado, coalesce(u.nombre_completo, 'Sin asignar'), c.es_emergencia "
                       "FROM consultas_medicas c LEFT JOIN usuarios u ON u.id = c.veterinario_id "
                       "WHERE c.mascota_id = (SELECT id FROM mascotas WHERE uuid = ?) "
                       "AND c.deleted_at IS NULL ORDER BY c.fecha_consulta DESC"),
        {QVariant(mascotaUuid)});
    historial_.clear();
    for (const auto& consulta : consultas) {
        QVariantMap item;
        item.insert(QStringLiteral("uuid"), textoDeFila(consulta, 0));
        item.insert(QStringLiteral("fecha"), fechaHoraTexto(enteroDeFila(consulta, 1)));
        item.insert(QStringLiteral("motivo"), textoDeFila(consulta, 2));
        item.insert(QStringLiteral("diagnostico"), textoDeFila(consulta, 3));
        item.insert(QStringLiteral("tratamiento"), textoDeFila(consulta, 4));
        item.insert(QStringLiteral("estado"), textoDeFila(consulta, 5));
        item.insert(QStringLiteral("veterinario"), textoDeFila(consulta, 6));
        item.insert(QStringLiteral("emergencia"), enteroDeFila(consulta, 7) == 1);
        historial_.append(item);
    }

    vacunas_ = listaDeFilas(
        db_.query(QStringLiteral("SELECT uuid, nombre_vacuna, fecha_aplicacion, "
                                 "coalesce(proxima_dosis, ''), coalesce(lote, '') "
                                 "FROM vacunas WHERE mascota_id = (SELECT id FROM mascotas "
                                 "WHERE uuid = ?) AND deleted_at IS NULL "
                                 "ORDER BY fecha_aplicacion DESC"),
                 {QVariant(mascotaUuid)}),
        {QStringLiteral("uuid"), QStringLiteral("nombre"), QStringLiteral("aplicacion"),
         QStringLiteral("proxima"), QStringLiteral("lote")});

    signos_ = listaDeFilas(
        db_.query(QStringLiteral("SELECT uuid, fecha, coalesce(peso_kg, 0), "
                                 "coalesce(temperatura_c, 0), "
                                 "coalesce(frecuencia_cardiaca, 0) "
                                 "FROM signos_vitales WHERE mascota_id = (SELECT id FROM mascotas "
                                 "WHERE uuid = ?) AND deleted_at IS NULL "
                                 "ORDER BY fecha DESC LIMIT 8"),
                 {QVariant(mascotaUuid)}),
        {QStringLiteral("uuid"), QStringLiteral("fechaMs"), QStringLiteral("peso"),
         QStringLiteral("temperatura"), QStringLiteral("cardiaca")});

    galeria_ = listaDeFilas(
        db_.query(QStringLiteral("SELECT uuid, titulo, storage_path FROM mascota_galeria "
                                 "WHERE mascota_id = (SELECT id FROM mascotas WHERE uuid = ?) "
                                 "AND deleted_at IS NULL ORDER BY created_at DESC"),
                 {QVariant(mascotaUuid)}),
        {QStringLiteral("uuid"), QStringLiteral("titulo"), QStringLiteral("ruta")});

    error_.clear();
    emit errorCambiado();
    emit fichaCambiada();
}

void PacientesController::limpiarSeleccion()
{
    ficha_.clear();
    historial_.clear();
    vacunas_.clear();
    signos_.clear();
    galeria_.clear();
    emit fichaCambiada();
}

void PacientesController::buscarHistorial(const QString& texto)
{
    const QString patron = QStringLiteral("%") + texto.trimmed().toLower() + QStringLiteral("%");
    const auto filas = db_.query(
        QStringLiteral("SELECT c.uuid, c.fecha_consulta, m.nombre, m.uuid, "
                       "cl.nombre_completo, coalesce(c.motivo_visita, ''), "
                       "coalesce(c.diagnostico, ''), c.estado "
                       "FROM consultas_medicas c JOIN mascotas m ON m.id = c.mascota_id "
                       "JOIN clientes cl ON cl.id = m.cliente_id "
                       "WHERE c.deleted_at IS NULL AND m.deleted_at IS NULL "
                       "AND (lower(m.nombre) LIKE ? OR lower(cl.nombre_completo) LIKE ? "
                       "OR lower(coalesce(c.motivo_visita, '')) LIKE ? "
                       "OR lower(coalesce(c.diagnostico, '')) LIKE ?) "
                       "ORDER BY c.fecha_consulta DESC LIMIT 60"),
        {QVariant(patron), QVariant(patron), QVariant(patron), QVariant(patron)});
    historialGlobal_.clear();
    for (const auto& fila : filas) {
        QVariantMap item;
        item.insert(QStringLiteral("uuid"), textoDeFila(fila, 0));
        item.insert(QStringLiteral("fecha"), fechaHoraTexto(enteroDeFila(fila, 1)));
        item.insert(QStringLiteral("mascota"), textoDeFila(fila, 2));
        item.insert(QStringLiteral("mascotaUuid"), textoDeFila(fila, 3));
        item.insert(QStringLiteral("propietario"), textoDeFila(fila, 4));
        item.insert(QStringLiteral("motivo"), textoDeFila(fila, 5));
        item.insert(QStringLiteral("diagnostico"), textoDeFila(fila, 6));
        item.insert(QStringLiteral("estado"), textoDeFila(fila, 7));
        historialGlobal_.append(item);
    }
    emit historialCambiado();
}

void PacientesController::limpiarMensajes()
{
    error_.clear();
    mensaje_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

void PacientesController::registrarMascota(const QString& clienteUuid, const QString& nombre,
                                           const QString& especie, const QString& raza,
                                           const QString& sexo, double peso, bool castrado,
                                           const QString& nacimiento, const QString& chip,
                                           const QString& alergias,
                                           const QString& atencionEspecial)
{
    const QString nombreLimpio = nombre.trimmed();
    const QString especieLimpia = especie.trimmed();
    if (clienteUuid.isEmpty() || nombreLimpio.isEmpty() || especieLimpia.isEmpty()) {
        fallo(QStringLiteral("el propietario, el nombre y la especie son obligatorios"));
        return;
    }
    const qint64 clienteId = idDe(QStringLiteral("clientes"), clienteUuid);
    if (clienteId <= 0) {
        fallo(QStringLiteral("el propietario no existe"));
        return;
    }
    const qint64 ahora = QDateTime::currentMSecsSinceEpoch();
    const auto alta = db_.execute(
        QStringLiteral("INSERT INTO mascotas "
                       "(uuid, cliente_id, nombre, especie, raza, sexo, peso_kg, "
                       " numero_chip, fecha_nacimiento, activo, castrado, alergias, "
                       " atencion_especial, created_at, updated_at, sync_state) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 1, ?, ?, ?, ?, ?, 'pendiente')"),
        {QVariant(fuzzy::core::uuidNuevo()), QVariant::fromValue(clienteId),
         QVariant(nombreLimpio), QVariant(especieLimpia),
         raza.trimmed().isEmpty() ? QVariant() : QVariant(raza.trimmed()),
         QVariant(sexo.isEmpty() ? QStringLiteral("Desconocido") : sexo),
         QVariant::fromValue(peso), chip.trimmed().isEmpty() ? QVariant() : QVariant(chip.trimmed()),
         nacimiento.trimmed().isEmpty() ? QVariant() : QVariant(nacimiento.trimmed()),
         QVariant::fromValue(castrado ? 1 : 0),
         alergias.trimmed().isEmpty() ? QVariant() : QVariant(alergias.trimmed()),
         atencionEspecial.trimmed().isEmpty() ? QVariant()
                                              : QVariant(atencionEspecial.trimmed()),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!alta.ok) {
        fallo(QStringLiteral("no se pudo registrar el paciente"));
        return;
    }
    recargar();
    exito(QStringLiteral("%1 quedó registrado.").arg(nombreLimpio));
}

void PacientesController::actualizarMascota(const QString& mascotaUuid, const QString& nombre,
                                            const QString& especie, const QString& raza,
                                            const QString& sexo, double peso, bool castrado,
                                            const QString& nacimiento, const QString& chip,
                                            const QString& alergias,
                                            const QString& atencionEspecial, bool activo)
{
    if (mascotaUuid.isEmpty() || nombre.trimmed().isEmpty()) {
        fallo(QStringLiteral("el nombre del paciente es obligatorio"));
        return;
    }
    const auto cambio = db_.execute(
        QStringLiteral("UPDATE mascotas SET nombre = ?, especie = ?, raza = ?, sexo = ?, "
                       "peso_kg = ?, castrado = ?, fecha_nacimiento = ?, numero_chip = ?, "
                       "alergias = ?, atencion_especial = ?, activo = ?, updated_at = ? "
                       "WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(nombre.trimmed()), QVariant(especie.trimmed()),
         raza.trimmed().isEmpty() ? QVariant() : QVariant(raza.trimmed()),
         QVariant(sexo.isEmpty() ? QStringLiteral("Desconocido") : sexo),
         QVariant::fromValue(peso), QVariant::fromValue(castrado ? 1 : 0),
         nacimiento.trimmed().isEmpty() ? QVariant() : QVariant(nacimiento.trimmed()),
         chip.trimmed().isEmpty() ? QVariant() : QVariant(chip.trimmed()),
         alergias.trimmed().isEmpty() ? QVariant() : QVariant(alergias.trimmed()),
         atencionEspecial.trimmed().isEmpty() ? QVariant()
                                              : QVariant(atencionEspecial.trimmed()),
         QVariant::fromValue(activo ? 1 : 0),
         QVariant::fromValue(QDateTime::currentMSecsSinceEpoch()), QVariant(mascotaUuid)});
    if (!cambio.ok || cambio.affected == 0) {
        fallo(QStringLiteral("no se pudo actualizar el paciente"));
        return;
    }
    recargar();
    seleccionar(mascotaUuid);
    exito(QStringLiteral("Ficha actualizada."));
}

void PacientesController::archivarMascota(const QString& mascotaUuid)
{
    const auto baja = db_.execute(
        QStringLiteral("UPDATE mascotas SET activo = 0, updated_at = ? "
                       "WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant::fromValue(QDateTime::currentMSecsSinceEpoch()), QVariant(mascotaUuid)});
    if (!baja.ok || baja.affected == 0) {
        fallo(QStringLiteral("no se pudo archivar el paciente"));
        return;
    }
    recargar();
    if (ficha_.value(QStringLiteral("uuid")).toString() == mascotaUuid) {
        seleccionar(mascotaUuid);
    }
    exito(QStringLiteral("Paciente archivado; el historial se conserva."));
}

void PacientesController::registrarConsulta(const QString& mascotaUuid,
                                            const QString& motivo, const QString& anamnesis,
                                            const QString& examen, const QString& diagnostico,
                                            const QString& tratamiento, bool emergencia)
{
    const qint64 mascotaId = idDe(QStringLiteral("mascotas"), mascotaUuid);
    if (mascotaId <= 0) {
        fallo(QStringLiteral("el paciente no existe"));
        return;
    }
    if (motivo.trimmed().isEmpty()) {
        fallo(QStringLiteral("el motivo de la consulta es obligatorio"));
        return;
    }
    const qint64 ahora = QDateTime::currentMSecsSinceEpoch();
    const auto alta = db_.execute(
        QStringLiteral("INSERT INTO consultas_medicas "
                       "(uuid, mascota_id, fecha_consulta, motivo_visita, es_emergencia, "
                       " diagnostico, tratamiento_indicado, anamnesis, examen_fisico, estado, "
                       " created_at, updated_at, sync_state) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 'Cerrada', ?, ?, 'pendiente')"),
        {QVariant(fuzzy::core::uuidNuevo()), QVariant::fromValue(mascotaId),
         QVariant::fromValue(ahora), QVariant(motivo.trimmed()),
         QVariant::fromValue(emergencia ? 1 : 0),
         diagnostico.trimmed().isEmpty() ? QVariant() : QVariant(diagnostico.trimmed()),
         tratamiento.trimmed().isEmpty() ? QVariant() : QVariant(tratamiento.trimmed()),
         anamnesis.trimmed().isEmpty() ? QVariant() : QVariant(anamnesis.trimmed()),
         examen.trimmed().isEmpty() ? QVariant() : QVariant(examen.trimmed()),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!alta.ok) {
        fallo(QStringLiteral("no se pudo registrar la consulta"));
        return;
    }
    seleccionar(mascotaUuid);
    exito(QStringLiteral("Consulta añadida al historial."));
}

void PacientesController::registrarSignos(const QString& mascotaUuid, double peso,
                                          double temperatura, int cardiaca, int respiratoria,
                                          const QString& notas)
{
    const qint64 mascotaId = idDe(QStringLiteral("mascotas"), mascotaUuid);
    if (mascotaId <= 0) {
        fallo(QStringLiteral("el paciente no existe"));
        return;
    }
    if (peso <= 0 && temperatura <= 0 && cardiaca <= 0 && respiratoria <= 0) {
        fallo(QStringLiteral("registra al menos un signo vital"));
        return;
    }
    const qint64 ahora = QDateTime::currentMSecsSinceEpoch();
    const auto alta = db_.execute(
        QStringLiteral("INSERT INTO signos_vitales "
                       "(uuid, mascota_id, fecha, peso_kg, temperatura_c, "
                       " frecuencia_cardiaca, frecuencia_respiratoria, created_at, updated_at, "
                       " sync_state) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 'pendiente')"),
        {QVariant(fuzzy::core::uuidNuevo()), QVariant::fromValue(mascotaId),
         QVariant::fromValue(ahora), peso > 0 ? QVariant::fromValue(peso) : QVariant(),
         temperatura > 0 ? QVariant::fromValue(temperatura) : QVariant(),
         cardiaca > 0 ? QVariant::fromValue(cardiaca) : QVariant(),
         respiratoria > 0 ? QVariant::fromValue(respiratoria) : QVariant(),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!alta.ok) {
        fallo(QStringLiteral("no se pudieron registrar los signos"));
        return;
    }
    if (peso > 0) {
        db_.execute(QStringLiteral("UPDATE mascotas SET peso_kg = ?, updated_at = ? "
                                   "WHERE id = ?"),
                    {QVariant::fromValue(peso), QVariant::fromValue(ahora),
                     QVariant::fromValue(mascotaId)});
    }
    seleccionar(mascotaUuid);
    exito(QStringLiteral("Signos vitales registrados.") + notas);
}

void PacientesController::registrarVacuna(const QString& mascotaUuid, const QString& nombre,
                                          const QString& aplicacion, const QString& proxima,
                                          const QString& lote, const QString& notas)
{
    const qint64 mascotaId = idDe(QStringLiteral("mascotas"), mascotaUuid);
    if (mascotaId <= 0) {
        fallo(QStringLiteral("el paciente no existe"));
        return;
    }
    const QDate fecha = QDate::fromString(aplicacion, QStringLiteral("yyyy-MM-dd"));
    if (nombre.trimmed().isEmpty() || !fecha.isValid()) {
        fallo(QStringLiteral("la vacuna y la fecha de aplicación son obligatorias"));
        return;
    }
    const qint64 ahora = QDateTime::currentMSecsSinceEpoch();
    const auto alta = db_.execute(
        QStringLiteral("INSERT INTO vacunas "
                       "(uuid, mascota_id, nombre_vacuna, fecha_aplicacion, proxima_dosis, "
                       " lote, notas, created_at, updated_at, sync_state) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 'pendiente')"),
        {QVariant(fuzzy::core::uuidNuevo()), QVariant::fromValue(mascotaId),
         QVariant(nombre.trimmed()), QVariant(aplicacion),
         proxima.trimmed().isEmpty() ? QVariant() : QVariant(proxima.trimmed()),
         lote.trimmed().isEmpty() ? QVariant() : QVariant(lote.trimmed()),
         notas.trimmed().isEmpty() ? QVariant() : QVariant(notas.trimmed()),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!alta.ok) {
        fallo(QStringLiteral("no se pudo registrar la vacuna"));
        return;
    }
    seleccionar(mascotaUuid);
    exito(QStringLiteral("Vacuna registrada."));
}

} // namespace fuzzy::desktop
