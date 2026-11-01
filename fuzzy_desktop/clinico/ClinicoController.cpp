#include "clinico/ClinicoController.h"

#include "common/Consulta.h"
#include "fuzzy/core/AuthService.h"

#include <QDate>
#include <QDateTime>
#include <QVariant>

namespace fuzzy::desktop {

namespace {

QVariantMap filaCita(const fuzzy::core::QueryRow& fila)
{
    QVariantMap cita;
    cita.insert(QStringLiteral("uuid"), textoDeFila(fila, 0));
    cita.insert(QStringLiteral("fechaHora"), fechaHoraTexto(enteroDeFila(fila, 1)));
    cita.insert(QStringLiteral("hora"), horaTexto(enteroDeFila(fila, 1)));
    cita.insert(QStringLiteral("mascotaUuid"), textoDeFila(fila, 2));
    cita.insert(QStringLiteral("mascota"), textoDeFila(fila, 3));
    cita.insert(QStringLiteral("propietario"), textoDeFila(fila, 4));
    cita.insert(QStringLiteral("clienteUuid"), textoDeFila(fila, 5));
    cita.insert(QStringLiteral("tipo"), textoDeFila(fila, 6));
    cita.insert(QStringLiteral("estado"), textoDeFila(fila, 7));
    cita.insert(QStringLiteral("prioridad"), textoDeFila(fila, 8));
    cita.insert(QStringLiteral("llegada"), !fila.values.at(9).isNull());
    cita.insert(QStringLiteral("emergencia"), enteroDeFila(fila, 10) == 1);
    return cita;
}

} // namespace

ClinicoController::ClinicoController(fuzzy::core::IQueryExecutor& db, QObject* parent)
    : QObject(parent), db_(db), servicio_(db)
{
    recargar();
}

void ClinicoController::setSesion(const QString& rol, const QString& clinicaUuid,
                                  const QString& usuarioUuid)
{
    rol_ = rol;
    sesion_.rol = fuzzy::core::AuthService::rolDesdeNombre(rol);
    sesion_.clinica_id = clinicaUuid;
    sesion_.cuenta_id = usuarioUuid;
    emit sesionCambiada();
    recargar();
}

void ClinicoController::cargar()
{
    recargar();
}

void ClinicoController::limpiarMensajes()
{
    error_.clear();
    mensaje_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

void ClinicoController::fallo(const QString& texto)
{
    error_ = texto;
    mensaje_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

void ClinicoController::aplicar(const fuzzy::core::ResultadoClinico& resultado,
                                const QString& exito)
{
    if (!resultado.ok) {
        fallo(resultado.error);
        return;
    }
    mensaje_ = exito;
    error_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
    recargar();
}

fuzzy::core::Sesion ClinicoController::sesionActual() const
{
    return sesion_;
}

void ClinicoController::recargar()
{
    const qint64 desde = inicioDiaMs(fechaHoy());
    const qint64 hasta = finDiaMs(fechaHoy());

    agendaDelDia_.clear();
    const auto citas = db_.query(
        QStringLiteral("SELECT c.uuid, c.fecha_hora_programada, m.uuid, m.nombre, "
                       "cl.nombre_completo, cl.uuid, ts.nombre_servicio, ec.nombre_estado, "
                       "c.prioridad, c.fecha_llegada, c.es_emergencia "
                       "FROM citas c "
                       "JOIN mascotas m ON m.id = c.mascota_id "
                       "JOIN clientes cl ON cl.id = m.cliente_id "
                       "JOIN tipos_servicio_cita ts ON ts.id = c.tipo_servicio_id "
                       "JOIN estados_cita ec ON ec.id = c.estado_cita_id "
                       "WHERE c.deleted_at IS NULL AND c.fecha_hora_programada BETWEEN ? AND ? "
                       "AND ec.nombre_estado NOT IN ('Cancelada', 'Reprogramada') "
                       "ORDER BY c.fecha_hora_programada"),
        {QVariant::fromValue(desde), QVariant::fromValue(hasta)});
    for (const auto& fila : citas) {
        agendaDelDia_.append(filaCita(fila));
    }

    consultasAbiertas_ = listaDeFilas(
        db_.query(QStringLiteral(
            "SELECT cm.uuid, m.uuid, m.nombre, cl.nombre_completo, cm.fecha_consulta, "
            "cm.motivo_visita, cm.es_emergencia "
            "FROM consultas_medicas cm "
            "JOIN mascotas m ON m.id = cm.mascota_id "
            "JOIN clientes cl ON cl.id = m.cliente_id "
            "WHERE cm.deleted_at IS NULL AND cm.estado = 'En curso' "
            "ORDER BY cm.fecha_consulta DESC")),
        {QStringLiteral("uuid"), QStringLiteral("mascotaUuid"), QStringLiteral("mascota"),
         QStringLiteral("propietario"), QStringLiteral("fecha"), QStringLiteral("motivo"),
         QStringLiteral("emergencia")});

    laboratorio_.clear();
    const auto labs = db_.query(
        QStringLiteral("SELECT l.uuid, m.uuid, m.nombre, cl.nombre_completo, l.tipo_examen, "
                       "l.prioridad, l.estado, l.fecha_solicitud, l.fecha_resultado, "
                       "l.resultado, COALESCE(u.nombre_completo, '') "
                       "FROM laboratorio_solicitudes l "
                       "JOIN mascotas m ON m.id = l.mascota_id "
                       "JOIN clientes cl ON cl.id = m.cliente_id "
                       "LEFT JOIN usuarios u ON u.id = l.veterinario_id "
                       "WHERE l.deleted_at IS NULL "
                       "ORDER BY CASE l.estado WHEN 'Solicitado' THEN 0 "
                       "         WHEN 'En proceso' THEN 1 WHEN 'Listo' THEN 2 ELSE 3 END, "
                       "         l.fecha_solicitud DESC LIMIT 100"));
    for (const auto& fila : labs) {
        QVariantMap item;
        item.insert(QStringLiteral("uuid"), textoDeFila(fila, 0));
        item.insert(QStringLiteral("mascotaUuid"), textoDeFila(fila, 1));
        item.insert(QStringLiteral("mascota"), textoDeFila(fila, 2));
        item.insert(QStringLiteral("propietario"), textoDeFila(fila, 3));
        item.insert(QStringLiteral("examen"), textoDeFila(fila, 4));
        item.insert(QStringLiteral("prioridad"), textoDeFila(fila, 5));
        item.insert(QStringLiteral("estado"), textoDeFila(fila, 6));
        item.insert(QStringLiteral("fecha"), fechaTexto(enteroDeFila(fila, 7)));
        item.insert(QStringLiteral("fechaResultado"),
                    fila.values.at(8).isNull() ? QString()
                                               : fechaTexto(enteroDeFila(fila, 8)));
        item.insert(QStringLiteral("resultado"), textoDeFila(fila, 9));
        item.insert(QStringLiteral("veterinario"), textoDeFila(fila, 10));
        laboratorio_.append(item);
    }

    hospitalizaciones_.clear();
    const auto ingresos = db_.query(
        QStringLiteral("SELECT h.uuid, m.uuid, m.nombre, cl.nombre_completo, h.fecha_ingreso, "
                       "h.fecha_alta, h.motivo, h.diagnostico, h.area, h.estado, "
                       "COALESCE(u.nombre_completo, '') "
                       "FROM hospitalizaciones h "
                       "JOIN mascotas m ON m.id = h.mascota_id "
                       "JOIN clientes cl ON cl.id = m.cliente_id "
                       "LEFT JOIN usuarios u ON u.id = h.veterinario_id "
                       "WHERE h.deleted_at IS NULL "
                       "ORDER BY CASE h.estado WHEN 'Activa' THEN 0 ELSE 1 END, "
                       "         h.fecha_ingreso DESC LIMIT 100"));
    for (const auto& fila : ingresos) {
        QVariantMap item;
        item.insert(QStringLiteral("uuid"), textoDeFila(fila, 0));
        item.insert(QStringLiteral("mascotaUuid"), textoDeFila(fila, 1));
        item.insert(QStringLiteral("mascota"), textoDeFila(fila, 2));
        item.insert(QStringLiteral("propietario"), textoDeFila(fila, 3));
        item.insert(QStringLiteral("ingreso"), fechaHoraTexto(enteroDeFila(fila, 4)));
        item.insert(QStringLiteral("alta"),
                    fila.values.at(5).isNull() ? QString()
                                               : fechaHoraTexto(enteroDeFila(fila, 5)));
        item.insert(QStringLiteral("motivo"), textoDeFila(fila, 6));
        item.insert(QStringLiteral("diagnostico"), textoDeFila(fila, 7));
        item.insert(QStringLiteral("area"), textoDeFila(fila, 8));
        item.insert(QStringLiteral("estado"), textoDeFila(fila, 9));
        item.insert(QStringLiteral("veterinario"), textoDeFila(fila, 10));
        hospitalizaciones_.append(item);
    }

    cirugias_.clear();
    const auto procedimientos = db_.query(
        QStringLiteral("SELECT c.uuid, m.uuid, m.nombre, cl.nombre_completo, "
                       "c.fecha_programada, c.procedimiento, c.anestesia, c.estado, "
                       "c.diagnostico_post "
                       "FROM cirugias c "
                       "JOIN mascotas m ON m.id = c.mascota_id "
                       "JOIN clientes cl ON cl.id = m.cliente_id "
                       "WHERE c.deleted_at IS NULL "
                       "ORDER BY CASE c.estado WHEN 'En curso' THEN 0 "
                       "         WHEN 'Programada' THEN 1 ELSE 2 END, "
                       "         c.fecha_programada DESC LIMIT 100"));
    for (const auto& fila : procedimientos) {
        QVariantMap item;
        item.insert(QStringLiteral("uuid"), textoDeFila(fila, 0));
        item.insert(QStringLiteral("mascotaUuid"), textoDeFila(fila, 1));
        item.insert(QStringLiteral("mascota"), textoDeFila(fila, 2));
        item.insert(QStringLiteral("propietario"), textoDeFila(fila, 3));
        item.insert(QStringLiteral("fecha"), fechaHoraTexto(enteroDeFila(fila, 4)));
        item.insert(QStringLiteral("procedimiento"), textoDeFila(fila, 5));
        item.insert(QStringLiteral("anestesia"), textoDeFila(fila, 6));
        item.insert(QStringLiteral("estado"), textoDeFila(fila, 7));
        item.insert(QStringLiteral("diagnosticoPost"), textoDeFila(fila, 8));
        cirugias_.append(item);
    }

    estetica_.clear();
    const auto esteticas = db_.query(
        QStringLiteral("SELECT e.uuid, m.uuid, m.nombre, cl.nombre_completo, e.fecha, "
                       "e.tipo, e.precio, e.observaciones, e.estado, "
                       "COALESCE(u.nombre_completo, '') "
                       "FROM estetica_servicios e "
                       "JOIN mascotas m ON m.id = e.mascota_id "
                       "JOIN clientes cl ON cl.id = m.cliente_id "
                       "LEFT JOIN usuarios u ON u.id = e.estilista_id "
                       "WHERE e.deleted_at IS NULL ORDER BY e.fecha DESC LIMIT 100"));
    for (const auto& fila : esteticas) {
        QVariantMap item;
        item.insert(QStringLiteral("uuid"), textoDeFila(fila, 0));
        item.insert(QStringLiteral("mascotaUuid"), textoDeFila(fila, 1));
        item.insert(QStringLiteral("mascota"), textoDeFila(fila, 2));
        item.insert(QStringLiteral("propietario"), textoDeFila(fila, 3));
        item.insert(QStringLiteral("fecha"), fechaHoraTexto(enteroDeFila(fila, 4)));
        item.insert(QStringLiteral("tipo"), textoDeFila(fila, 5));
        item.insert(QStringLiteral("precio"), realDeFila(fila, 6));
        item.insert(QStringLiteral("precioTexto"), moneda(realDeFila(fila, 6)));
        item.insert(QStringLiteral("observaciones"), textoDeFila(fila, 7));
        item.insert(QStringLiteral("estado"), textoDeFila(fila, 8));
        item.insert(QStringLiteral("estilista"), textoDeFila(fila, 9));
        estetica_.append(item);
    }

    cargos_ = fuzzy::core::ServicioClinico::filasDeCargos(db_, QString());

    examenes_ = listaDeFilas(
        db_.query(QStringLiteral(
            "SELECT uuid, codigo, nombre, activo FROM tipos_examen_laboratorio "
            "WHERE deleted_at IS NULL ORDER BY nombre")),
        {QStringLiteral("uuid"), QStringLiteral("codigo"), QStringLiteral("nombre"),
         QStringLiteral("activo")});

    servicios_ = listaDeFilas(
        db_.query(QStringLiteral(
            "SELECT uuid, codigo, nombre, precio, activo FROM servicios "
            "WHERE deleted_at IS NULL ORDER BY nombre")),
        {QStringLiteral("uuid"), QStringLiteral("codigo"), QStringLiteral("nombre"),
         QStringLiteral("precio"), QStringLiteral("activo")});

    productos_ = listaDeFilas(
        db_.query(QStringLiteral(
            "SELECT uuid, nombre_comercial, stock_actual, precio_venta "
            "FROM productos_insumos WHERE deleted_at IS NULL "
            "ORDER BY nombre_comercial")),
        {QStringLiteral("uuid"), QStringLiteral("nombre"), QStringLiteral("stock"),
         QStringLiteral("precio")});

    mascotas_ = listaDeFilas(
        db_.query(QStringLiteral(
            "SELECT m.uuid, m.nombre, cl.nombre_completo, cl.uuid, m.especie "
            "FROM mascotas m JOIN clientes cl ON cl.id = m.cliente_id "
            "WHERE m.deleted_at IS NULL AND m.activo = 1 ORDER BY m.nombre")),
        {QStringLiteral("uuid"), QStringLiteral("nombre"), QStringLiteral("propietario"),
         QStringLiteral("clienteUuid"), QStringLiteral("especie")});

    clientes_ = listaDeFilas(
        db_.query(QStringLiteral(
            "SELECT uuid, nombre_completo FROM clientes "
            "WHERE deleted_at IS NULL AND activo = 1 ORDER BY nombre_completo")),
        {QStringLiteral("uuid"), QStringLiteral("nombre")});

    const auto contar = [this](const QString& sql) {
        const auto filas = db_.query(sql);
        return filas.empty() ? 0 : filas.front().values.front().toInt();
    };
    resumen_.clear();
    resumen_.insert(QStringLiteral("citasHoy"), agendaDelDia_.size());
    resumen_.insert(QStringLiteral("enEspera"),
                    contar(QStringLiteral(
                        "SELECT count(*) FROM citas c JOIN estados_cita e ON e.id = c.estado_cita_id "
                        "WHERE c.deleted_at IS NULL AND e.nombre_estado = 'En espera'")));
    resumen_.insert(QStringLiteral("consultasAbiertas"), consultasAbiertas_.size());
    resumen_.insert(QStringLiteral("laboratorioPendiente"),
                    contar(QStringLiteral(
                        "SELECT count(*) FROM laboratorio_solicitudes "
                        "WHERE deleted_at IS NULL AND estado IN ('Solicitado', 'En proceso')")));
    resumen_.insert(QStringLiteral("hospitalizados"),
                    contar(QStringLiteral(
                        "SELECT count(*) FROM hospitalizaciones "
                        "WHERE deleted_at IS NULL AND estado = 'Activa'")));
    resumen_.insert(QStringLiteral("cirugiasProgramadas"),
                    contar(QStringLiteral(
                        "SELECT count(*) FROM cirugias "
                        "WHERE deleted_at IS NULL AND estado IN ('Programada', 'En curso')")));
    resumen_.insert(QStringLiteral("cargosPendientes"), cargos_.size());
    resumen_.insert(QStringLiteral("esteticaHoy"),
                    contar(QStringLiteral(
                        "SELECT count(*) FROM estetica_servicios "
                        "WHERE deleted_at IS NULL AND estado = 'Realizado'")));

    emit datosCambiados();
}

void ClinicoController::recibir(const QString& citaUuid)
{
    aplicar(servicio_.recibir(sesionActual(), citaUuid),
            QStringLiteral("Llegada registrada; la mascota pasa a espera."));
}

void ClinicoController::registrarTriaje(const QString& mascotaUuid,
                                        const QString& consultaUuid, double peso,
                                        double temperatura, int cardiaca,
                                        int respiratoria, const QString& notas)
{
    aplicar(servicio_.registrarTriaje(sesionActual(), mascotaUuid, consultaUuid, peso,
                                      temperatura, cardiaca, respiratoria, notas),
            QStringLiteral("Triaje registrado."));
}

void ClinicoController::abrirConsulta(const QString& mascotaUuid, const QString& citaUuid,
                                      const QString& motivo, bool emergencia)
{
    aplicar(servicio_.abrirConsulta(sesionActual(), mascotaUuid, citaUuid, motivo,
                                    emergencia),
            QStringLiteral("Consulta abierta."));
}

void ClinicoController::cerrarConsulta(const QString& consultaUuid,
                                       const QString& diagnostico,
                                       const QString& tratamiento,
                                       const QString& examen)
{
    aplicar(servicio_.cerrarConsulta(sesionActual(), consultaUuid, diagnostico,
                                     tratamiento, examen),
            QStringLiteral("Consulta cerrada y firmada."));
}

void ClinicoController::emitirReceta(const QString& consultaUuid,
                                     const QString& indicaciones,
                                     const QVariantList& lineas, bool descontarStock)
{
    QList<fuzzy::core::LineaReceta> receta;
    for (const QVariant& item : lineas) {
        const QVariantMap mapa = item.toMap();
        fuzzy::core::LineaReceta linea;
        linea.producto_uuid = mapa.value(QStringLiteral("productoUuid")).toString();
        linea.medicamento = mapa.value(QStringLiteral("medicamento")).toString();
        linea.dosis = mapa.value(QStringLiteral("dosis")).toString();
        linea.frecuencia = mapa.value(QStringLiteral("frecuencia")).toString();
        linea.duracion = mapa.value(QStringLiteral("duracion")).toString();
        linea.cantidad = mapa.value(QStringLiteral("cantidad"), 1).toInt();
        linea.precio = mapa.value(QStringLiteral("precio")).toDouble();
        receta.append(linea);
    }
    aplicar(servicio_.emitirReceta(sesionActual(), consultaUuid, indicaciones, receta,
                                   descontarStock),
            QStringLiteral("Receta emitida."));
}

void ClinicoController::agregarCargo(const QString& origenTipo, const QString& origenUuid,
                                     const QString& concepto, double monto)
{
    aplicar(servicio_.agregarCargo(sesionActual(), origenTipo, origenUuid, concepto, monto),
            QStringLiteral("Cargo añadido a la cuenta."));
}

void ClinicoController::guardarServicio(const QString& servicioUuid,
                                        const QString& codigo, const QString& nombre,
                                        double precio, bool activo)
{
    aplicar(servicio_.guardarServicio(sesionActual(), servicioUuid, codigo, nombre, precio,
                                      activo),
            QStringLiteral("Catálogo de servicios actualizado."));
}

void ClinicoController::guardarExamen(const QString& examenUuid, const QString& codigo,
                                      const QString& nombre, bool activo)
{
    aplicar(servicio_.guardarTipoExamen(sesionActual(), examenUuid, codigo, nombre, activo),
            QStringLiteral("Catálogo de laboratorio actualizado."));
}

void ClinicoController::solicitarLaboratorio(const QString& mascotaUuid,
                                             const QString& consultaUuid,
                                             const QString& examenUuid,
                                             const QString& prioridad,
                                             const QString& notas)
{
    aplicar(servicio_.solicitarLaboratorio(sesionActual(), mascotaUuid, consultaUuid,
                                           examenUuid, prioridad, notas),
            QStringLiteral("Examen solicitado."));
}

void ClinicoController::iniciarLaboratorio(const QString& solicitudUuid)
{
    aplicar(servicio_.iniciarLaboratorio(sesionActual(), solicitudUuid),
            QStringLiteral("Muestra en proceso."));
}

void ClinicoController::resultadoLaboratorio(const QString& solicitudUuid,
                                             const QString& resultado)
{
    aplicar(servicio_.registrarResultadoLaboratorio(sesionActual(), solicitudUuid,
                                                    resultado),
            QStringLiteral("Resultado registrado; listo para el dueño."));
}

void ClinicoController::entregarLaboratorio(const QString& solicitudUuid)
{
    aplicar(servicio_.entregarLaboratorio(sesionActual(), solicitudUuid),
            QStringLiteral("Resultado entregado."));
}

void ClinicoController::ingresarHospitalizacion(const QString& mascotaUuid,
                                                const QString& consultaUuid,
                                                const QString& motivo,
                                                const QString& diagnostico,
                                                const QString& area)
{
    aplicar(servicio_.ingresarHospitalizacion(sesionActual(), mascotaUuid, consultaUuid,
                                              motivo, diagnostico, area),
            QStringLiteral("Paciente hospitalizado."));
}

void ClinicoController::registrarEvolucion(const QString& hospitalizacionUuid,
                                           const QString& nota, double peso,
                                           double temperatura, int cardiaca)
{
    aplicar(servicio_.registrarEvolucion(sesionActual(), hospitalizacionUuid, nota, peso,
                                         temperatura, cardiaca),
            QStringLiteral("Evolución registrada."));
}

void ClinicoController::darAlta(const QString& hospitalizacionUuid, const QString& estado)
{
    aplicar(servicio_.darAltaHospitalizacion(sesionActual(), hospitalizacionUuid, estado),
            QStringLiteral("Alta registrada."));
}

void ClinicoController::programarCirugia(const QString& mascotaUuid,
                                         const QString& consultaUuid,
                                         const QString& hospitalizacionUuid,
                                         const QString& procedimiento,
                                         const QString& fecha, const QString& anestesia)
{
    aplicar(servicio_.programarCirugia(sesionActual(), mascotaUuid, consultaUuid,
                                       hospitalizacionUuid, procedimiento, fecha, anestesia),
            QStringLiteral("Cirugía programada."));
}

void ClinicoController::iniciarCirugia(const QString& cirugiaUuid)
{
    aplicar(servicio_.iniciarCirugia(sesionActual(), cirugiaUuid),
            QStringLiteral("Cirugía en curso."));
}

void ClinicoController::cerrarCirugia(const QString& cirugiaUuid,
                                      const QString& diagnosticoPost,
                                      const QString& notas)
{
    aplicar(servicio_.cerrarCirugia(sesionActual(), cirugiaUuid, diagnosticoPost, notas),
            QStringLiteral("Cirugía completada."));
}

void ClinicoController::registrarEstetica(const QString& mascotaUuid,
                                          const QString& citaUuid,
                                          const QString& servicioUuid, const QString& tipo,
                                          double precio, const QString& observaciones,
                                          bool cobrar)
{
    aplicar(servicio_.registrarEstetica(sesionActual(), mascotaUuid, citaUuid, servicioUuid,
                                        tipo, precio, observaciones, cobrar),
            QStringLiteral("Servicio de estética registrado."));
}

void ClinicoController::facturarCargos(const QString& clienteUuid,
                                       const QVariantList& cargoUuids,
                                       const QString& metodoPago, double descuento,
                                       double impuesto)
{
    aplicar(servicio_.facturarCargos(sesionActual(), clienteUuid, cargoUuids, metodoPago,
                                     descuento, impuesto),
            QStringLiteral("Cargos facturados."));
}

void ClinicoController::cerrarCaja(double ingresosReales, const QString& observaciones)
{
    aplicar(servicio_.cerrarCaja(sesionActual(), ingresosReales, observaciones),
            QStringLiteral("Caja cerrada."));
}

void ClinicoController::ajustarStock(const QString& productoUuid, int delta,
                                     const QString& motivo)
{
    aplicar(servicio_.ajustarStock(sesionActual(), productoUuid, delta, motivo),
            QStringLiteral("Inventario ajustado."));
}

} // namespace fuzzy::desktop
