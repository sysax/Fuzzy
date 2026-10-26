#include "TestSupport.h"

#include "fuzzy/core/PortalBaseStore.h"
#include "fuzzy/core/PortalProvisioner.h"
#include "fuzzy/core/PortalTokenService.h"

#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

using fuzzy::core::LocalPortalBaseStore;
using fuzzy::core::PortalProvisioner;
using fuzzy::core::PortalTokenService;
using fuzzy::core::SqliteExecutor;
using namespace fuzzy::test;

namespace {

const QString kClinica = QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa1");
const QString kOtraClinica = QStringLiteral("bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbb2");
const QString kCliente1 = QStringLiteral("cccccccc-cccc-4ccc-8ccc-ccccccccccc1");
const QString kCliente2 = QStringLiteral("cccccccc-cccc-4ccc-8ccc-ccccccccccc2");
const QString kCuenta1 = QStringLiteral("dddddddd-dddd-4ddd-8ddd-ddddddddddd1");
const QString kCuenta2 = QStringLiteral("dddddddd-dddd-4ddd-8ddd-ddddddddddd2");
const QString kToken1 = QStringLiteral("11111111-1111-4111-8111-111111111111");
const QString kToken2 = QStringLiteral("22222222-2222-4222-8222-222222222222");

void sembrar(IQueryExecutor& executor)
{
    QVERIFY(aplicarEsquema(executor, QStringLiteral("clinic")));
    executor.execute(
        QStringLiteral("INSERT INTO clinicas (uuid, nombre, zona_horaria, created_at, "
                       "updated_at, activa, sync_state) "
                       "VALUES (?, 'Clínica Norte', 'America/Bogota', 1, 1, 1, 'pendiente')"),
        {QVariant(kClinica)});
    executor.execute(
        QStringLiteral("INSERT INTO clientes (id, uuid, nombre_completo, telefono, correo, "
                       "activo, created_at, updated_at, sync_state) "
                       "VALUES (1, ?, 'Ana Uno', '3001111111', 'ana@example.com', 1, 1, 1, "
                       "'pendiente')"),
        {QVariant(kCliente1)});
    executor.execute(
        QStringLiteral("INSERT INTO clientes (id, uuid, nombre_completo, telefono, correo, "
                       "activo, created_at, updated_at, sync_state) "
                       "VALUES (2, ?, 'Beto Dos', '3002222222', 'beto@example.com', 1, 1, 1, "
                       "'pendiente')"),
        {QVariant(kCliente2)});
    executor.execute(
        QStringLiteral("INSERT INTO cuentas (id, uuid, cliente_id, correo, password_hash, "
                       "estado, activo, created_at, updated_at, sync_state) "
                       "VALUES (1, ?, 1, 'ana@example.com', 'hash', 'Activa', 1, 1, 1, "
                       "'pendiente')"),
        {QVariant(kCuenta1)});
    executor.execute(
        QStringLiteral("INSERT INTO cuentas (id, uuid, cliente_id, correo, password_hash, "
                       "estado, activo, created_at, updated_at, sync_state) "
                       "VALUES (2, ?, 2, 'beto@example.com', 'hash', 'Activa', 1, 1, 1, "
                       "'pendiente')"),
        {QVariant(kCuenta2)});
}

} // namespace

class TstPortalProvisioner : public QObject
{
    Q_OBJECT

private slots:
    void creaPortalCompleto();
    void tokenNoCruzaClinicas();
    void migrarEsIdempotente();
    void rechazaCuentaAjena();
};

void TstPortalProvisioner::creaPortalCompleto()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor clinica(nuevaConexion(QStringLiteral("prov")),
                           dir.filePath(QStringLiteral("clinic.db")));
    sembrar(clinica);

    LocalPortalBaseStore bases(dir.filePath(QStringLiteral("portales")));
    PortalTokenService tokens(clinica, &bases);
    PortalProvisioner provisioner(clinica, tokens, bases);

    const auto creado = provisioner.crear(kClinica, 1, 1, QString(), kToken1);
    QVERIFY(creado.has_value());
    QVERIFY(!creado->portal_uuid.isEmpty());
    QCOMPARE(creado->token, kToken1);
    QCOMPARE(creado->huella, PortalTokenService::huella(kToken1));
    QCOMPARE(creado->url, QStringLiteral("local://portal/%1").arg(creado->portal_uuid));
    QCOMPARE(creado->qr.value(QStringLiteral("v")).toInt(), 1);
    QCOMPARE(creado->qr.value(QStringLiteral("portal_uuid")).toString(),
             creado->portal_uuid);
    QCOMPARE(creado->qr.value(QStringLiteral("clinica")).toString(), kClinica);

    QVERIFY(bases.existe(creado->portal_uuid));
    QVERIFY(tokens.verificar(kToken1, kClinica));

    const auto registro = clinica.query(
        QStringLiteral("SELECT url, length(token_hash), estado FROM portales WHERE uuid = ?"),
        {QVariant(creado->portal_uuid)});
    QCOMPARE(registro.size(), size_t(1));
    QCOMPARE(registro.front().values.at(0).toString(), creado->url);
    QCOMPARE(registro.front().values.at(1).toInt(), 64);
    QCOMPARE(registro.front().values.at(2).toString(), QStringLiteral("activo"));

    auto portal = bases.abrir(creado->portal_uuid);
    QVERIFY(portal != nullptr);
    const auto meta = portal->query(
        QStringLiteral("SELECT esquema_version, cuenta_uuid, clinica_uuid, clinica_nombre, "
                       "proyeccion_version FROM portal_meta WHERE id = 1"));
    QCOMPARE(meta.size(), size_t(1));
    QCOMPARE(meta.front().values.at(0).toInt(), 2);
    QCOMPARE(meta.front().values.at(1).toString(), kCuenta1);
    QCOMPARE(meta.front().values.at(2).toString(), kClinica);
    QCOMPARE(meta.front().values.at(3).toString(), QStringLiteral("Clínica Norte"));
    QCOMPARE(meta.front().values.at(4).toInt(), 0);
}

void TstPortalProvisioner::tokenNoCruzaClinicas()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor clinica(nuevaConexion(QStringLiteral("prov_cruza")),
                           dir.filePath(QStringLiteral("clinic.db")));
    sembrar(clinica);

    LocalPortalBaseStore bases(dir.filePath(QStringLiteral("portales")));
    PortalTokenService tokens(clinica, &bases);
    PortalProvisioner provisioner(clinica, tokens, bases);

    const auto uno = provisioner.crear(kClinica, 1, 1, QString(), kToken1);
    const auto dos = provisioner.crear(kClinica, 2, 2, QString(), kToken2);
    QVERIFY(uno.has_value());
    QVERIFY(dos.has_value());
    QVERIFY(uno->portal_uuid != dos->portal_uuid);

    QVERIFY(tokens.verificar(kToken1, kClinica));
    QVERIFY(tokens.verificar(kToken2, kClinica));
    QVERIFY(!tokens.verificar(kToken1, kOtraClinica));
    QVERIFY(!tokens.verificar(kToken2, kOtraClinica));

    // El token de un portal no abre la base del otro: son archivos distintos.
    QVERIFY(bases.existe(uno->portal_uuid));
    QVERIFY(bases.existe(dos->portal_uuid));
    auto portalUno = bases.abrir(uno->portal_uuid);
    auto portalDos = bases.abrir(dos->portal_uuid);
    QVERIFY(portalUno != nullptr);
    QVERIFY(portalDos != nullptr);
    QCOMPARE(portalUno->query(QStringLiteral("SELECT cuenta_uuid FROM portal_meta"))
                 .front()
                 .values.front()
                 .toString(),
             kCuenta1);
    QCOMPARE(portalDos->query(QStringLiteral("SELECT cuenta_uuid FROM portal_meta"))
                 .front()
                 .values.front()
                 .toString(),
             kCuenta2);
}

void TstPortalProvisioner::migrarEsIdempotente()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor clinica(nuevaConexion(QStringLiteral("prov_migrar")),
                           dir.filePath(QStringLiteral("clinic.db")));
    sembrar(clinica);

    LocalPortalBaseStore bases(dir.filePath(QStringLiteral("portales")));
    PortalTokenService tokens(clinica, &bases);
    PortalProvisioner provisioner(clinica, tokens, bases);

    const auto creado = provisioner.crear(kClinica, 1, 1, QString(), kToken1);
    QVERIFY(creado.has_value());
    QVERIFY(provisioner.migrar(creado->portal_uuid));
    QVERIFY(provisioner.migrar(creado->portal_uuid));

    auto portal = bases.abrir(creado->portal_uuid);
    const auto meta = portal->query(
        QStringLiteral("SELECT esquema_version FROM portal_meta WHERE id = 1"));
    QCOMPARE(meta.front().values.front().toInt(), 2);
    QCOMPARE(contar(*portal, QStringLiteral("_fuzzy_migraciones")), 2LL);
}

void TstPortalProvisioner::rechazaCuentaAjena()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor clinica(nuevaConexion(QStringLiteral("prov_ajena")),
                           dir.filePath(QStringLiteral("clinic.db")));
    sembrar(clinica);

    LocalPortalBaseStore bases(dir.filePath(QStringLiteral("portales")));
    PortalTokenService tokens(clinica, &bases);
    PortalProvisioner provisioner(clinica, tokens, bases);

    // La cuenta 2 pertenece a la ficha 2, no a la 1.
    const auto creado = provisioner.crear(kClinica, 2, 1, QString(), kToken1);
    QVERIFY(!creado.has_value());
    QCOMPARE(contar(clinica, QStringLiteral("portales")), 0LL);

    // Tampoco se puede crear contra una clínica que no es la del ejecutor.
    const auto otra = provisioner.crear(kOtraClinica, 1, 1, QString(), kToken1);
    QVERIFY(!otra.has_value());
    QCOMPARE(contar(clinica, QStringLiteral("portales")), 0LL);
}

QTEST_MAIN(TstPortalProvisioner)
#include "tst_portal_provisioner.moc"
