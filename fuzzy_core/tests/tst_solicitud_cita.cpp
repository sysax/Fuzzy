#include "TestSupport.h"

#include "fuzzy/core/PortalBaseStore.h"
#include "fuzzy/core/PortalProjector.h"
#include "fuzzy/core/PortalProvisioner.h"
#include "fuzzy/core/PortalTokenService.h"
#include "fuzzy/core/SolicitudCitaService.h"

#include <QDate>
#include <QDateTime>
#include <QTemporaryDir>
#include <QTime>
#include <QtTest>

using fuzzy::core::LocalPortalBaseStore;
using fuzzy::core::PortalProjector;
using fuzzy::core::PortalProvisioner;
using fuzzy::core::PortalTokenService;
using fuzzy::core::SolicitudCitaService;
using fuzzy::core::SqliteExecutor;
using namespace fuzzy::test;

namespace {

const QString kClinica = QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa1");
const QString kCliente = QStringLiteral("cccccccc-cccc-4ccc-8ccc-ccccccccccc1");
const QString kCuenta = QStringLiteral("dddddddd-dddd-4ddd-8ddd-ddddddddddd1");
const QString kMascota = QStringLiteral("eeeeeeee-eeee-4eee-8eee-eeeeeeeeeee1");
const QString kMascotaAjena = QStringLiteral("eeeeeeee-eeee-4eee-8eee-eeeeeeeeeee2");
const QString kSolicitud1 = QStringLiteral("f0000000-0000-4000-8000-000000000001");
const QString kSolicitud2 = QStringLiteral("f0000000-0000-4000-8000-000000000002");
const QString kSolicitud3 = QStringLiteral("f0000000-0000-4000-8000-000000000003");

void sembrarClinica(IQueryExecutor& clinica)
{
    QVERIFY(aplicarEsquema(clinica, QStringLiteral("clinic")));
    clinica.execute(
        QStringLiteral("INSERT INTO clinicas (uuid, nombre, zona_horaria, created_at, "
                       "updated_at, activa, sync_state) "
                       "VALUES (?, 'Clínica Norte', 'America/Bogota', 1, 1, 1, 'pendiente')"),
        {QVariant(kClinica)});
    clinica.execute(
        QStringLiteral("INSERT INTO clientes (id, uuid, nombre_completo, telefono, correo, "
                       "activo, created_at, updated_at, sync_state) "
                       "VALUES (1, ?, 'Ana Uno', '3001111111', 'ana@example.com', 1, 1, 1, "
                       "'pendiente')"),
        {QVariant(kCliente)});
    clinica.execute(
        QStringLiteral("INSERT INTO cuentas (id, uuid, cliente_id, correo, password_hash, "
                       "estado, activo, created_at, updated_at, sync_state) "
                       "VALUES (1, ?, 1, 'ana@example.com', 'hash', 'Activa', 1, 1, 1, "
                       "'pendiente')"),
        {QVariant(kCuenta)});
    clinica.execute(
        QStringLiteral("INSERT INTO mascotas (id, uuid, cliente_id, nombre, especie, sexo, "
                       "peso_kg, activo, castrado, created_at, updated_at, sync_state) "
                       "VALUES (1, ?, 1, 'Luna', 'Perro', 'Hembra', 12, 1, 0, 1, 1, "
                       "'pendiente')"),
        {QVariant(kMascota)});
}

void insertarSolicitud(IQueryExecutor& portal, const QString& uuid, const QString& mascota,
                       const QString& fecha, const QString& hora)
{
    QVERIFY(portal.execute(
                      QStringLiteral("INSERT INTO solicitudes_cita "
                                     "(uuid, mascota_uuid, tipo_servicio, fecha_preferida, "
                                     " hora_preferida, motivo, estado, created_at, updated_at) "
                                     "VALUES (?, ?, 'Médica', ?, ?, 'Control', 'Pendiente', "
                                     "1, 1)"),
                      {QVariant(uuid), QVariant(mascota), QVariant(fecha), QVariant(hora)})
                 .ok);
}

} // namespace

class TstSolicitudCita : public QObject
{
    Q_OBJECT

private slots:
    void idaYVueltaAceptada();
    void rechazaFranjaOcupada();
    void rechazaFechaPasadaYProcesaUnaSolaVez();
    void rechazaMascotaAjena();
};

void TstSolicitudCita::idaYVueltaAceptada()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor clinica(nuevaConexion(QStringLiteral("solicitud_clinica")),
                           dir.filePath(QStringLiteral("clinic.db")));
    sembrarClinica(clinica);
    LocalPortalBaseStore bases(dir.filePath(QStringLiteral("portales")));
    PortalTokenService tokens(clinica, &bases);
    PortalProvisioner provisioner(clinica, tokens, bases);
    PortalProjector proyector(clinica, bases);

    const auto creado = provisioner.crear(kClinica, 1, 1, QString(), QStringLiteral("token"));
    QVERIFY(creado.has_value());
    QVERIFY(proyector.proyectar(creado->portal_uuid).ok);

    auto portal = bases.abrir(creado->portal_uuid);
    QVERIFY(portal != nullptr);

    const QDate dia = QDate::currentDate().addDays(10);
    const QString fecha = dia.toString(QStringLiteral("yyyy-MM-dd"));
    const QString hora = QStringLiteral("10:00");
    insertarSolicitud(*portal, kSolicitud1, kMascota, fecha, hora);

    SolicitudCitaService servicio(clinica, bases);
    const auto resultado = servicio.procesar(creado->portal_uuid);
    QVERIFY2(resultado.ok, qPrintable(resultado.error));
    QCOMPARE(resultado.procesadas.size(), 1);
    QCOMPARE(resultado.procesadas.front().estado, QStringLiteral("Aceptada"));
    QVERIFY(!resultado.procesadas.front().cita_uuid.isEmpty());

    const auto citaClinica = clinica.query(
        QStringLiteral("SELECT uuid, fecha_hora_programada, origen FROM citas WHERE uuid = ?"),
        {QVariant(resultado.procesadas.front().cita_uuid)});
    QCOMPARE(citaClinica.size(), size_t(1));
    QCOMPARE(citaClinica.front().values.at(1).toLongLong(),
             QDateTime(dia, QTime(10, 0)).toMSecsSinceEpoch());
    QCOMPARE(citaClinica.front().values.at(2).toString(), QStringLiteral("app"));

    const auto solicitud = portal->query(
        QStringLiteral("SELECT estado, cita_uuid, respuesta FROM solicitudes_cita "
                       "WHERE uuid = ?"),
        {QVariant(kSolicitud1)});
    QCOMPARE(solicitud.front().values.at(0).toString(), QStringLiteral("Aceptada"));
    QCOMPARE(solicitud.front().values.at(1).toString(),
             resultado.procesadas.front().cita_uuid);
    QVERIFY(!solicitud.front().values.at(2).toString().isEmpty());

    const auto citaPortal = portal->query(
        QStringLiteral("SELECT uuid, estado, origen, mascota_uuid FROM citas"));
    QCOMPARE(citaPortal.size(), size_t(1));
    QCOMPARE(citaPortal.front().values.at(0).toString(),
             resultado.procesadas.front().cita_uuid);
    QCOMPARE(citaPortal.front().values.at(1).toString(), QStringLiteral("Agendada"));
    QCOMPARE(citaPortal.front().values.at(2).toString(), QStringLiteral("app"));
    QCOMPARE(citaPortal.front().values.at(3).toString(), kMascota);

    // Repetir no vuelve a procesar ni duplica la cita.
    const auto repetida = servicio.procesar(creado->portal_uuid);
    QVERIFY(repetida.ok);
    QCOMPARE(repetida.procesadas.size(), 0);
    QCOMPARE(contar(clinica, QStringLiteral("citas")), 1LL);
    QCOMPARE(contar(*portal, QStringLiteral("citas")), 1LL);
}

void TstSolicitudCita::rechazaFranjaOcupada()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor clinica(nuevaConexion(QStringLiteral("solicitud_ocupada")),
                           dir.filePath(QStringLiteral("clinic.db")));
    sembrarClinica(clinica);
    LocalPortalBaseStore bases(dir.filePath(QStringLiteral("portales")));
    PortalTokenService tokens(clinica, &bases);
    PortalProvisioner provisioner(clinica, tokens, bases);
    PortalProjector proyector(clinica, bases);

    const auto creado = provisioner.crear(kClinica, 1, 1, QString(), QStringLiteral("token"));
    QVERIFY(creado.has_value());
    QVERIFY(proyector.proyectar(creado->portal_uuid).ok);
    auto portal = bases.abrir(creado->portal_uuid);

    const QDate dia = QDate::currentDate().addDays(10);
    const QString fecha = dia.toString(QStringLiteral("yyyy-MM-dd"));
    insertarSolicitud(*portal, kSolicitud1, kMascota, fecha, QStringLiteral("10:00"));
    insertarSolicitud(*portal, kSolicitud2, kMascota, fecha, QStringLiteral("10:00"));

    SolicitudCitaService servicio(clinica, bases);
    const auto resultado = servicio.procesar(creado->portal_uuid);
    QVERIFY(resultado.ok);
    QCOMPARE(resultado.procesadas.size(), 2);
    QCOMPARE(resultado.procesadas.at(0).estado, QStringLiteral("Aceptada"));
    QCOMPARE(resultado.procesadas.at(1).estado, QStringLiteral("Rechazada"));
    QCOMPARE(contar(clinica, QStringLiteral("citas")), 1LL);
    QCOMPARE(contar(*portal, QStringLiteral("citas")), 1LL);

    const auto rechazada = portal->query(
        QStringLiteral("SELECT estado, cita_uuid, respuesta FROM solicitudes_cita "
                       "WHERE uuid = ?"),
        {QVariant(kSolicitud2)});
    QCOMPARE(rechazada.front().values.at(0).toString(), QStringLiteral("Rechazada"));
    QVERIFY(rechazada.front().values.at(1).isNull());
    QVERIFY(rechazada.front().values.at(2).toString().contains(QStringLiteral("franja")));
}

void TstSolicitudCita::rechazaFechaPasadaYProcesaUnaSolaVez()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor clinica(nuevaConexion(QStringLiteral("solicitud_pasada")),
                           dir.filePath(QStringLiteral("clinic.db")));
    sembrarClinica(clinica);
    LocalPortalBaseStore bases(dir.filePath(QStringLiteral("portales")));
    PortalTokenService tokens(clinica, &bases);
    PortalProvisioner provisioner(clinica, tokens, bases);
    PortalProjector proyector(clinica, bases);

    const auto creado = provisioner.crear(kClinica, 1, 1, QString(), QStringLiteral("token"));
    QVERIFY(creado.has_value());
    QVERIFY(proyector.proyectar(creado->portal_uuid).ok);
    auto portal = bases.abrir(creado->portal_uuid);

    insertarSolicitud(*portal, kSolicitud3, kMascota, QStringLiteral("2000-01-01"),
                      QStringLiteral("10:00"));

    SolicitudCitaService servicio(clinica, bases);
    const auto resultado = servicio.procesar(creado->portal_uuid);
    QVERIFY(resultado.ok);
    QCOMPARE(resultado.procesadas.size(), 1);
    QCOMPARE(resultado.procesadas.front().estado, QStringLiteral("Rechazada"));
    QCOMPARE(contar(clinica, QStringLiteral("citas")), 0LL);

    const auto otraVez = servicio.procesar(creado->portal_uuid);
    QVERIFY(otraVez.ok);
    QCOMPARE(otraVez.procesadas.size(), 0);
}

void TstSolicitudCita::rechazaMascotaAjena()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor clinica(nuevaConexion(QStringLiteral("solicitud_ajena")),
                           dir.filePath(QStringLiteral("clinic.db")));
    sembrarClinica(clinica);
    LocalPortalBaseStore bases(dir.filePath(QStringLiteral("portales")));
    PortalTokenService tokens(clinica, &bases);
    PortalProvisioner provisioner(clinica, tokens, bases);
    PortalProjector proyector(clinica, bases);

    const auto creado = provisioner.crear(kClinica, 1, 1, QString(), QStringLiteral("token"));
    QVERIFY(creado.has_value());
    QVERIFY(proyector.proyectar(creado->portal_uuid).ok);
    auto portal = bases.abrir(creado->portal_uuid);

    const QDate dia = QDate::currentDate().addDays(10);
    insertarSolicitud(*portal, kSolicitud1, kMascotaAjena,
                      dia.toString(QStringLiteral("yyyy-MM-dd")), QStringLiteral("11:00"));

    SolicitudCitaService servicio(clinica, bases);
    const auto resultado = servicio.procesar(creado->portal_uuid);
    QVERIFY(resultado.ok);
    QCOMPARE(resultado.procesadas.size(), 1);
    QCOMPARE(resultado.procesadas.front().estado, QStringLiteral("Rechazada"));
    QCOMPARE(contar(clinica, QStringLiteral("citas")), 0LL);
}

QTEST_MAIN(TstSolicitudCita)
#include "tst_solicitud_cita.moc"
