#include "TestSupport.h"

#include "fuzzy/core/PortalBaseStore.h"
#include "fuzzy/core/PortalProjector.h"
#include "fuzzy/core/ServicioClinico.h"

#include <QDate>
#include <QDateTime>
#include <QTemporaryDir>
#include <QVariant>
#include <QtTest>

#include <memory>

using fuzzy::core::LocalPortalBaseStore;
using fuzzy::core::Politicas;
using fuzzy::core::PortalProjector;
using fuzzy::core::ResultadoClinico;
using fuzzy::core::Rol;
using fuzzy::core::ServicioClinico;
using fuzzy::core::Sesion;
using fuzzy::core::SqliteExecutor;
using namespace fuzzy::test;

namespace {

const QString kClinica = QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa1");
const QString kPortal = QStringLiteral("bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbb1");
const QString kCliente = QStringLiteral("10000000-0000-4000-8000-000000000001");
const QString kCuenta = QStringLiteral("20000000-0000-4000-8000-000000000001");
const QString kMascota = QStringLiteral("30000000-0000-4000-8000-000000000001");
const QString kCita = QStringLiteral("40000000-0000-4000-8000-000000000001");
const QString kServicio = QStringLiteral("50000000-0000-4000-8000-000000000001");
const QString kProducto = QStringLiteral("60000000-0000-4000-8000-000000000001");
const QString kAdmin = QStringLiteral("70000000-0000-4000-8000-000000000001");
const QString kVet = QStringLiteral("70000000-0000-4000-8000-000000000002");
const QString kRecepcion = QStringLiteral("70000000-0000-4000-8000-000000000003");
const QString kEstilista = QStringLiteral("70000000-0000-4000-8000-000000000004");

const QString kExamen = QStringLiteral("55555555-5555-4555-8555-555555555501");

qint64 msEn(int diasOffset, int hora, int minuto)
{
    const QDate hoy = QDate::currentDate();
    return QDateTime(hoy.addDays(diasOffset), QTime(hora, minuto)).toMSecsSinceEpoch();
}

qint64 escalar(IQueryExecutor& db, const QString& sql,
               const std::vector<QVariant>& params = {})
{
    const auto filas = db.query(sql, params);
    return filas.empty() ? -1 : filas.front().values.front().toLongLong();
}

QString texto(IQueryExecutor& db, const QString& sql,
              const std::vector<QVariant>& params = {})
{
    const auto filas = db.query(sql, params);
    return filas.empty() ? QString() : filas.front().values.front().toString();
}

// Instalación mínima con una clínica, el personal de los cuatro roles y una
// mascota con cita. Los tests ejercitan el servicio contra SQLite real.
class BaseClinica
{
public:
    BaseClinica()
        : db(std::make_unique<SqliteExecutor>(nuevaConexion(QStringLiteral("clinico")),
                                              dir.filePath(QStringLiteral("clinica.db"))))
    {
        QVERIFY(aplicarEsquema(*db, QStringLiteral("clinic")));
        sembrar();
    }

    Sesion admin() const { return {Rol::Admin, kClinica, kAdmin}; }
    Sesion veterinario() const { return {Rol::Veterinario, kClinica, kVet}; }
    Sesion recepcion() const { return {Rol::Recepcion, kClinica, kRecepcion}; }
    Sesion estilista() const { return {Rol::Estilista, kClinica, kEstilista}; }

    QTemporaryDir dir;
    std::unique_ptr<SqliteExecutor> db;

private:
    void sembrar()
    {
        QVERIFY(db->execute(
                           QStringLiteral("INSERT INTO clinicas "
                                          "(id, uuid, nombre, zona_horaria, created_at, "
                                          " updated_at, activa, sync_state) "
                                          "VALUES (1, ?, 'Clínica Norte', 'America/Bogota', "
                                          "1, 1, 1, 'pendiente')"),
                           {QVariant(kClinica)})
                    .ok);

        struct Usuario
        {
            const QString& uuid;
            const char* nombre;
            const char* correo;
            int rol;
        };
        const Usuario usuarios[] = {
            {kAdmin, "Dra. Elena Ruiz", "elena@aurora.test", 1},
            {kVet, "Dr. Marco Soler", "marco@aurora.test", 2},
            {kRecepcion, "Lucía Prado", "lucia@aurora.test", 3},
            {kEstilista, "Nadia Ferrer", "nadia@aurora.test", 4},
        };
        for (const Usuario& usuario : usuarios) {
            QVERIFY(db->execute(
                               QStringLiteral("INSERT INTO usuarios "
                                              "(uuid, nombre_completo, correo, password_hash, "
                                              " rol_id, created_at, updated_at, sync_state) "
                                              "VALUES (?, ?, ?, 'hash', ?, 1, 1, 'pendiente')"),
                               {QVariant(usuario.uuid),
                                QVariant(QString::fromUtf8(usuario.nombre)),
                                QVariant(QString::fromUtf8(usuario.correo)),
                                QVariant::fromValue(usuario.rol)})
                        .ok);
        }

        QVERIFY(db->execute(
                           QStringLiteral("INSERT INTO clientes "
                                          "(id, uuid, nombre_completo, telefono, correo, dni, "
                                          " activo, created_at, updated_at, sync_state) "
                                          "VALUES (1, ?, 'Ana Uno', '3001111111', "
                                          "'ana@example.com', '1020304050', 1, 1, 1, "
                                          "'pendiente')"),
                           {QVariant(kCliente)})
                    .ok);
        QVERIFY(db->execute(
                           QStringLiteral("INSERT INTO cuentas "
                                          "(id, uuid, cliente_id, correo, password_hash, "
                                          " estado, activo, created_at, updated_at, sync_state) "
                                          "VALUES (1, ?, 1, 'ana@example.com', 'hash', "
                                          "'Activa', 1, 1, 1, 'pendiente')"),
                           {QVariant(kCuenta)})
                    .ok);
        QVERIFY(db->execute(
                           QStringLiteral("INSERT INTO mascotas "
                                          "(id, uuid, cliente_id, nombre, especie, raza, sexo, "
                                          " peso_kg, activo, castrado, created_at, updated_at, "
                                          " sync_state) "
                                          "VALUES (1, ?, 1, 'Luna', 'Perro', 'Mestizo', "
                                          "'Hembra', 24.5, 1, 0, 1, 1, 'pendiente')"),
                           {QVariant(kMascota)})
                    .ok);
        QVERIFY(db->execute(
                           QStringLiteral("INSERT INTO citas "
                                          "(id, uuid, mascota_id, tipo_servicio_id, "
                                          " estado_cita_id, fecha_hora_programada, prioridad, "
                                          " es_emergencia, origen, fecha_creacion, "
                                          " fecha_actualizacion, created_at, updated_at, "
                                          " sync_state) "
                                          "VALUES (1, ?, 1, 1, 1, ?, 'Normal', 0, "
                                          "'mostrador', 1, 1, 1, 1, 'pendiente')"),
                           {QVariant(kCita), QVariant::fromValue(msEn(0, 9, 0))})
                    .ok);
        QVERIFY(db->execute(
                           QStringLiteral("INSERT INTO servicios "
                                          "(id, uuid, codigo, nombre, precio, activo, "
                                          " created_at, updated_at, sync_state) "
                                          "VALUES (1, ?, 'CON-001', 'Consulta general', "
                                          "85000, 1, 1, 1, 'pendiente')"),
                           {QVariant(kServicio)})
                    .ok);
        QVERIFY(db->execute(
                           QStringLiteral("INSERT INTO productos_insumos "
                                          "(id, uuid, nombre_comercial, categoria_id, codigo, "
                                          " unidad, stock_actual, stock_minimo, precio_compra, "
                                          " precio_venta, created_at, updated_at, sync_state) "
                                          "VALUES (1, ?, 'Amoxicilina 250 mg', 1, 'MED-001', "
                                          "'unidades', 10, 5, 12000, 22000, 1, 1, "
                                          "'pendiente')"),
                           {QVariant(kProducto)})
                    .ok);
    }
};

} // namespace

class TstClinico : public QObject
{
    Q_OBJECT

private slots:
    void recepcionConsultaRecetaFacturaYCaja();
    void catalogoLaboratorioYProyeccionAlPortal();
    void hospitalizacionConEvolucionYAlta();
    void cirugiaDeProgramadaACompletada();
    void esteticaGeneraCargo();
    void inventarioAjustaStockYDejaRastro();
    void matrizDeRolesYClinica();
};

void TstClinico::recepcionConsultaRecetaFacturaYCaja()
{
    BaseClinica base;
    ServicioClinico servicio(*base.db);

    // Recepción: la cita pasa a espera y no se puede recibir dos veces.
    QVERIFY(servicio.recibir(base.admin(), kCita).ok);
    QVERIFY(!servicio.recibir(base.admin(), kCita).ok);
    QCOMPARE(texto(*base.db,
                   QStringLiteral("SELECT e.nombre_estado FROM citas c "
                                  "JOIN estados_cita e ON e.id = c.estado_cita_id "
                                  "WHERE c.uuid = ?"),
                   {QVariant(kCita)}),
             QStringLiteral("En espera"));

    // Triaje.
    const ResultadoClinico triaje = servicio.registrarTriaje(
        base.recepcion(), kMascota, QString(), 24.8, 38.4, 84, 22,
        QStringLiteral("Tranquila, ayuno de 8 h"));
    QVERIFY2(triaje.ok, qPrintable(triaje.error));
    QCOMPARE(escalar(*base.db, QStringLiteral("SELECT count(*) FROM signos_vitales")), 1LL);
    QCOMPARE(escalar(*base.db,
                     QStringLiteral("SELECT CAST(peso_kg * 10 AS INTEGER) FROM mascotas "
                                    "WHERE uuid = ?"),
                     {QVariant(kMascota)}),
             248LL);

    // Consulta: abrir, triaje con consulta y cerrar con diagnóstico.
    const ResultadoClinico consulta = servicio.abrirConsulta(
        base.veterinario(), kMascota, kCita, QStringLiteral("Control anual"), false);
    QVERIFY2(consulta.ok, qPrintable(consulta.error));
    QVERIFY(!servicio.abrirConsulta(base.veterinario(), kMascota, kCita,
                                    QStringLiteral("Repetida"), false)
                 .ok);
    QVERIFY(servicio.registrarTriaje(base.recepcion(), kMascota, consulta.uuid, 24.8,
                                     38.4, 84, 22, QStringLiteral("Sin novedad"))
                .ok);
    QVERIFY(servicio.cerrarConsulta(base.veterinario(), consulta.uuid,
                                    QStringLiteral("Paciente sana"),
                                    QStringLiteral("Observación 24 h"),
                                    QStringLiteral("Mucosas rosadas"))
                .ok);
    QVERIFY(!servicio.cerrarConsulta(base.veterinario(), consulta.uuid,
                                     QStringLiteral("Otra vez"), QString(), QString())
                 .ok);
    QCOMPARE(texto(*base.db,
                   QStringLiteral("SELECT e.nombre_estado FROM citas c "
                                  "JOIN estados_cita e ON e.id = c.estado_cita_id "
                                  "WHERE c.uuid = ?"),
                   {QVariant(kCita)}),
             QStringLiteral("Completada"));

    // Receta con descuento de stock.
    fuzzy::core::LineaReceta linea;
    linea.producto_uuid = kProducto;
    linea.medicamento = QStringLiteral("Amoxicilina 250 mg");
    linea.dosis = QStringLiteral("1 tableta");
    linea.frecuencia = QStringLiteral("cada 12 horas");
    linea.duracion = QStringLiteral("7 días");
    linea.cantidad = 2;
    linea.precio = 22000.0;
    const ResultadoClinico receta = servicio.emitirReceta(
        base.veterinario(), consulta.uuid, QStringLiteral("Con alimento"),
        {linea}, true);
    QVERIFY2(receta.ok, qPrintable(receta.error));
    QVERIFY(!servicio.emitirReceta(base.veterinario(), consulta.uuid, QString(),
                                   {linea}, false)
                 .ok);
    QCOMPARE(escalar(*base.db,
                     QStringLiteral("SELECT stock_actual FROM productos_insumos "
                                    "WHERE uuid = ?"),
                     {QVariant(kProducto)}),
             8LL);
    QCOMPARE(escalar(*base.db,
                     QStringLiteral("SELECT count(*) FROM movimientos_inventario "
                                    "WHERE tipo = 'Salida'")),
             1LL);

    // Cargos y factura.
    QVERIFY(servicio.agregarCargo(base.veterinario(), QStringLiteral("Consulta"),
                                  consulta.uuid, QStringLiteral("Consulta general"),
                                  85000)
                .ok);
    QVERIFY(servicio.agregarCargo(base.veterinario(), QStringLiteral("Receta"),
                                  receta.uuid, QStringLiteral("Medicamentos"), 44000)
                .ok);
    const QVariantList cargos = ServicioClinico::filasDeCargos(*base.db, kCliente);
    QCOMPARE(cargos.size(), 2);

    QVariantList cargosUuid;
    for (const QVariant& cargo : cargos) {
        QCOMPARE(cargo.toMap().value(QStringLiteral("clienteUuid")).toString(), kCliente);
        cargosUuid.append(cargo.toMap().value(QStringLiteral("uuid")));
    }
    const ResultadoClinico factura = servicio.facturarCargos(
        base.recepcion(), kCliente, cargosUuid, QStringLiteral("Efectivo"), 5000.0, 0.0);
    QVERIFY2(factura.ok, qPrintable(factura.error));
    QCOMPARE(ServicioClinico::filasDeCargos(*base.db, kCliente).size(), 0);
    QCOMPARE(escalar(*base.db,
                     QStringLiteral("SELECT monto_total FROM facturas WHERE uuid = ?"),
                     {QVariant(factura.uuid)}),
             124000LL);
    QCOMPARE(escalar(*base.db,
                     QStringLiteral("SELECT count(*) FROM cargos WHERE estado = 'Pagado'")),
             2LL);

    // Caja: el esperado sale de las facturas pagadas del día.
    const ResultadoClinico cierre = servicio.cerrarCaja(
        base.recepcion(), 124000.0, QStringLiteral("Sin novedad"));
    QVERIFY2(cierre.ok, qPrintable(cierre.error));
    QCOMPARE(escalar(*base.db,
                     QStringLiteral("SELECT ingresos_calculados FROM cierres_caja "
                                    "WHERE uuid = ?"),
                     {QVariant(cierre.uuid)}),
             124000LL);
    QCOMPARE(escalar(*base.db,
                     QStringLiteral("SELECT desfase FROM cierres_caja WHERE uuid = ?"),
                     {QVariant(cierre.uuid)}),
             0LL);
}

void TstClinico::catalogoLaboratorioYProyeccionAlPortal()
{
    BaseClinica base;
    ServicioClinico servicio(*base.db);

    const ResultadoClinico examen = servicio.guardarTipoExamen(
        base.admin(), QString(), QStringLiteral("LAB-ECO"), QStringLiteral("Ecografía"), true);
    QVERIFY2(examen.ok, qPrintable(examen.error));
    QVERIFY(!servicio.guardarTipoExamen(base.veterinario(), QString(), QStringLiteral("LAB-X"),
                                        QStringLiteral("Sin permiso"), true)
                 .ok);

    const ResultadoClinico solicitud = servicio.solicitarLaboratorio(
        base.veterinario(), kMascota, QString(), kExamen, QStringLiteral("Urgente"),
        QStringLiteral("Ayuno de 8 horas"));
    QVERIFY2(solicitud.ok, qPrintable(solicitud.error));
    QVERIFY(servicio.iniciarLaboratorio(base.veterinario(), solicitud.uuid).ok);
    QVERIFY(servicio.registrarResultadoLaboratorio(
                base.veterinario(), solicitud.uuid, QStringLiteral("Serie roja normal"))
                .ok);
    QVERIFY(servicio.entregarLaboratorio(base.veterinario(), solicitud.uuid).ok);
    QCOMPARE(texto(*base.db,
                   QStringLiteral("SELECT estado FROM laboratorio_solicitudes WHERE uuid = ?"),
                   {QVariant(solicitud.uuid)}),
             QStringLiteral("Entregado"));

    // El dueño ve la solicitud y el resultado en su portal.
    QTemporaryDir portalDir;
    QVERIFY(portalDir.isValid());
    LocalPortalBaseStore bases(portalDir.path());
    QVERIFY(bases.crear(kPortal));
    {
        auto portal = bases.abrir(kPortal);
        QVERIFY(portal != nullptr);
        QVERIFY(aplicarEsquema(*portal, QStringLiteral("portal")));
        QVERIFY(portal->execute(
                           QStringLiteral("INSERT INTO portal_meta "
                                          "(id, esquema_version, cuenta_uuid, clinica_uuid, "
                                          " generado_at, clinica_nombre, proyeccion_version) "
                                          "VALUES (1, 2, ?, ?, 1, 'Clínica Norte', 0)"),
                           {QVariant(kCuenta), QVariant(kClinica)})
                    .ok);
    }

    PortalProjector proyector(*base.db, bases);
    const auto proyeccion = proyector.proyectar(kPortal);
    QVERIFY2(proyeccion.ok, qPrintable(proyeccion.error));

    auto portal = bases.abrir(kPortal);
    QVERIFY(portal != nullptr);
    const auto filas = portal->query(
        QStringLiteral("SELECT tipo_examen, estado, resultado FROM laboratorio"));
    QCOMPARE(static_cast<int>(filas.size()), 1);
    QCOMPARE(filas.front().values.at(0).toString(), QStringLiteral("Hemograma completo"));
    QCOMPARE(filas.front().values.at(1).toString(), QStringLiteral("Entregado"));
    QCOMPARE(filas.front().values.at(2).toString(), QStringLiteral("Serie roja normal"));
}

void TstClinico::hospitalizacionConEvolucionYAlta()
{
    BaseClinica base;
    ServicioClinico servicio(*base.db);

    const ResultadoClinico ingreso = servicio.ingresarHospitalizacion(
        base.veterinario(), kMascota, QString(), QStringLiteral("Deshidratación"),
        QStringLiteral("Gastroenteritis"), QStringLiteral("UCI"));
    QVERIFY2(ingreso.ok, qPrintable(ingreso.error));
    QVERIFY(!servicio.ingresarHospitalizacion(base.veterinario(), kMascota, QString(),
                                              QString(), QString(), QString())
                 .ok);

    const ResultadoClinico evolucion = servicio.registrarEvolucion(
        base.veterinario(), ingreso.uuid, QStringLiteral("Mejor apetito"), 25.0, 38.1, 88);
    QVERIFY2(evolucion.ok, qPrintable(evolucion.error));
    QCOMPARE(escalar(*base.db,
                     QStringLiteral("SELECT count(*) FROM hospitalizacion_evoluciones")),
             1LL);

    QVERIFY(servicio.darAltaHospitalizacion(base.veterinario(), ingreso.uuid,
                                            QStringLiteral("Alta"))
                .ok);
    QVERIFY(!servicio.darAltaHospitalizacion(base.veterinario(), ingreso.uuid,
                                             QStringLiteral("Alta"))
                 .ok);
    QVERIFY(!servicio.registrarEvolucion(base.veterinario(), ingreso.uuid,
                                         QStringLiteral("Ya no está activa"), 0, 0, 0)
                 .ok);
    QCOMPARE(texto(*base.db,
                   QStringLiteral("SELECT estado FROM hospitalizaciones WHERE uuid = ?"),
                   {QVariant(ingreso.uuid)}),
             QStringLiteral("Alta"));
}

void TstClinico::cirugiaDeProgramadaACompletada()
{
    BaseClinica base;
    ServicioClinico servicio(*base.db);

    const ResultadoClinico cirugia = servicio.programarCirugia(
        base.veterinario(), kMascota, QString(), QString(),
        QStringLiteral("Esterilización"),
        QDateTime::currentDateTime().addDays(1).toString(Qt::ISODate),
        QStringLiteral("General"));
    QVERIFY2(cirugia.ok, qPrintable(cirugia.error));
    QVERIFY(!servicio.programarCirugia(base.veterinario(), kMascota, QString(), QString(),
                                       QStringLiteral("Sin fecha"), QString(), QString())
                 .ok);
    QVERIFY(!servicio.cerrarCirugia(base.veterinario(), cirugia.uuid, QString(), QString())
                 .ok);

    QVERIFY(servicio.iniciarCirugia(base.veterinario(), cirugia.uuid).ok);
    QVERIFY(!servicio.iniciarCirugia(base.veterinario(), cirugia.uuid).ok);
    QVERIFY(servicio.cerrarCirugia(base.veterinario(), cirugia.uuid,
                                   QStringLiteral("Postoperatorio estable"),
                                   QStringLiteral("Control en 7 días"))
                .ok);
    QCOMPARE(texto(*base.db, QStringLiteral("SELECT estado FROM cirugias WHERE uuid = ?"),
                   {QVariant(cirugia.uuid)}),
             QStringLiteral("Completada"));
}

void TstClinico::esteticaGeneraCargo()
{
    BaseClinica base;
    ServicioClinico servicio(*base.db);

    const ResultadoClinico estetica = servicio.registrarEstetica(
        base.estilista(), kMascota, QString(), kServicio, QStringLiteral("Baño y corte"),
        60000.0, QStringLiteral("Sin nudos"), true);
    QVERIFY2(estetica.ok, qPrintable(estetica.error));
    QCOMPARE(escalar(*base.db, QStringLiteral("SELECT count(*) FROM estetica_servicios")),
             1LL);
    QCOMPARE(escalar(*base.db, QStringLiteral("SELECT count(*) FROM cargos")), 1LL);
    QCOMPARE(texto(*base.db, QStringLiteral("SELECT concepto FROM cargos")),
             QStringLiteral("Estética: Baño y corte"));

    // Sin cobro no hay cargo; el tipo debe ser válido.
    QVERIFY(servicio.registrarEstetica(base.estilista(), kMascota, QString(), kServicio,
                                       QStringLiteral("Corte"), 30000.0, QString(), false)
                .ok);
    QCOMPARE(escalar(*base.db, QStringLiteral("SELECT count(*) FROM cargos")), 1LL);
    QVERIFY(!servicio.registrarEstetica(base.estilista(), kMascota, QString(), kServicio,
                                        QStringLiteral("Cirugía"), 30000.0, QString(), false)
                 .ok);
}

void TstClinico::inventarioAjustaStockYDejaRastro()
{
    BaseClinica base;
    ServicioClinico servicio(*base.db);

    QVERIFY(servicio.ajustarStock(base.admin(), kProducto, 5, QStringLiteral("Compra"))
                .ok);
    QVERIFY(servicio.ajustarStock(base.admin(), kProducto, -2, QStringLiteral("Merma"))
                .ok);
    QCOMPARE(escalar(*base.db,
                     QStringLiteral("SELECT stock_actual FROM productos_insumos "
                                    "WHERE uuid = ?"),
                     {QVariant(kProducto)}),
             13LL);
    QCOMPARE(escalar(*base.db, QStringLiteral("SELECT count(*) FROM movimientos_inventario")),
             2LL);
    QCOMPARE(escalar(*base.db,
                     QStringLiteral("SELECT count(*) FROM movimientos_inventario "
                                    "WHERE tipo = 'Entrada'")),
             1LL);
    QCOMPARE(escalar(*base.db,
                     QStringLiteral("SELECT count(*) FROM movimientos_inventario "
                                    "WHERE tipo = 'Salida'")),
             1LL);
    QVERIFY(!servicio.ajustarStock(base.admin(), kProducto, 0, QString()).ok);
    QVERIFY(!servicio.ajustarStock(base.admin(), kProducto, -100, QString()).ok);
}

void TstClinico::matrizDeRolesYClinica()
{
    BaseClinica base;
    ServicioClinico servicio(*base.db);

    // Recepción no abre consultas ni receta; sí recibe y factura.
    QVERIFY(!servicio.abrirConsulta(base.recepcion(), kMascota, QString(),
                                    QStringLiteral("No permitido"), false)
                 .ok);
    QVERIFY(!servicio.emitirReceta(base.recepcion(), QString(), QString(), {}, false).ok);
    QVERIFY(servicio.recibir(base.recepcion(), kCita).ok);

    // Estilista solo estética; tampoco puede facturar.
    QVERIFY(!servicio.abrirConsulta(base.estilista(), kMascota, QString(),
                                    QStringLiteral("No permitido"), false)
                 .ok);
    QVERIFY(!servicio.facturarCargos(base.estilista(), kCliente, {}, QStringLiteral("Efectivo"),
                                     0.0, 0.0)
                 .ok);
    QVERIFY(servicio.registrarEstetica(base.estilista(), kMascota, QString(), kServicio,
                                       QStringLiteral("Baño"), 30000.0, QString(), false)
                .ok);

    // Veterinario registra clínica pero no emite facturas ni cierra caja.
    QVERIFY(!servicio.facturarCargos(base.veterinario(), kCliente, {}, QStringLiteral("Efectivo"),
                                     0.0, 0.0)
                 .ok);
    QVERIFY(!servicio.cerrarCaja(base.veterinario(), 0.0, QString()).ok);

    // Una sesión de otra clínica no puede tocar esta instalación.
    const Sesion intruso{Rol::Admin, QStringLiteral("clinica-ajena"), kAdmin};
    QVERIFY(!servicio.recibir(intruso, kCita).ok);
    QVERIFY(!servicio.ajustarStock(intruso, kProducto, 1, QStringLiteral("Intruso")).ok);

    // Rol desconocido no hace nada.
    const Sesion nadie{Rol::Desconocido, kClinica, kAdmin};
    QVERIFY(!servicio.recibir(nadie, kCita).ok);
    QVERIFY(!servicio.registrarEstetica(nadie, kMascota, QString(), kServicio,
                                        QStringLiteral("Baño"), 0.0, QString(), false)
                 .ok);
}

QTEST_MAIN(TstClinico)
#include "tst_clinico.moc"
