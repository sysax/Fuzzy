#include "fuzzy/core/PortalBaseStore.h"
#include "fuzzy/core/PortalTokenService.h"
#include "fuzzy/core/SqliteExecutor.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QVariant>
#include <QtTest>

using fuzzy::core::IQueryExecutor;
using fuzzy::core::LocalPortalBaseStore;
using fuzzy::core::PortalTokenService;
using fuzzy::core::SqliteExecutor;

namespace {

const QString kClinicaA = QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa1");
const QString kClinicaB = QStringLiteral("bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbb2");
const QString kTokenA = QStringLiteral("11111111-1111-4111-8111-111111111111");
const QString kTokenA2 = QStringLiteral("33333333-3333-4333-8333-333333333333");
const QString kTokenB = QStringLiteral("22222222-2222-4222-8222-222222222222");
const QString kTokenAjeno = QStringLiteral("99999999-9999-4999-8999-999999999999");

int contadorConexiones()
{
    static int siguiente = 0;
    return ++siguiente;
}

QString conexion(const QString& etiqueta)
{
    return QStringLiteral("fase4_%1_%2").arg(etiqueta).arg(contadorConexiones());
}

void crearEsquema(IQueryExecutor& executor)
{
    const QStringList sentencias = {
        QStringLiteral("CREATE TABLE clinicas ("
                       "id INTEGER PRIMARY KEY, uuid TEXT NOT NULL UNIQUE, deleted_at INTEGER)"),
        QStringLiteral("CREATE TABLE clientes ("
                       "id INTEGER PRIMARY KEY, uuid TEXT NOT NULL UNIQUE, deleted_at INTEGER)"),
        QStringLiteral("CREATE TABLE cuentas ("
                       "id INTEGER PRIMARY KEY, uuid TEXT NOT NULL UNIQUE, "
                       "cliente_id INTEGER NOT NULL UNIQUE, deleted_at INTEGER)"),
        QStringLiteral("CREATE TABLE portales ("
                       "id INTEGER PRIMARY KEY, uuid TEXT NOT NULL UNIQUE, "
                       "token_hash TEXT UNIQUE, "
                       "cuenta_id INTEGER NOT NULL UNIQUE, cliente_id INTEGER NOT NULL, "
                       "url TEXT NOT NULL, estado TEXT NOT NULL DEFAULT 'activo', "
                       "pareado_at INTEGER, ultima_sync_at INTEGER, "
                       "created_at INTEGER, updated_at INTEGER, deleted_at INTEGER)"),
    };
    for (const QString& sql : sentencias) {
        executor.execute(sql);
    }
}

bool esquemaListo(IQueryExecutor& executor)
{
    const auto filas = executor.query(
        QStringLiteral("SELECT name FROM sqlite_master WHERE type = 'table' "
                       "AND name IN ('clinicas', 'clientes', 'cuentas', 'portales')"));
    return filas.size() == 4;
}

void sembrarClinica(IQueryExecutor& executor, const QString& clinica_uuid)
{
    executor.execute(
        QStringLiteral("INSERT INTO clinicas (uuid, deleted_at) VALUES (?, NULL)"),
        {QVariant(clinica_uuid)});
}

void sembrarCuenta(IQueryExecutor& executor, qint64 cliente_id, qint64 cuenta_id,
                   const QString& cliente_uuid, const QString& cuenta_uuid)
{
    executor.execute(
        QStringLiteral("INSERT INTO clientes (id, uuid, deleted_at) VALUES (?, ?, NULL)"),
        {QVariant::fromValue(cliente_id), QVariant(cliente_uuid)});
    executor.execute(
        QStringLiteral("INSERT INTO cuentas (id, uuid, cliente_id, deleted_at) "
                       "VALUES (?, ?, ?, NULL)"),
        {QVariant::fromValue(cuenta_id), QVariant(cuenta_uuid),
         QVariant::fromValue(cliente_id)});
}

struct ClinicaFixture
{
    QTemporaryDir dir;
    std::unique_ptr<SqliteExecutor> executor;

    ClinicaFixture(const QString& etiqueta, const QString& clinica_uuid)
        : executor(std::make_unique<SqliteExecutor>(conexion(etiqueta),
                                                    dir.filePath("clinic.db")))
    {
        crearEsquema(*executor);
        sembrarClinica(*executor, clinica_uuid);
        sembrarCuenta(*executor, 1, 1, QStringLiteral("cliente-1"),
                      QStringLiteral("cuenta-1"));
    }
};

QString estadoDelPortal(IQueryExecutor& executor, const QString& portal_uuid)
{
    const auto filas = executor.query(
        QStringLiteral("SELECT estado FROM portales WHERE uuid = ?"),
        {QVariant(portal_uuid)});
    if (filas.size() != 1 || filas.front().values.size() != 1) {
        return {};
    }
    return filas.front().values.front().toString();
}

qlonglong contarPortales(IQueryExecutor& executor)
{
    const auto filas = executor.query(QStringLiteral("SELECT count(*) FROM portales"));
    if (filas.size() != 1 || filas.front().values.empty()) {
        return -1;
    }
    return filas.front().values.front().toLongLong();
}

// Cuenta las filas donde el token en claro aparezca en cualquier columna de
// texto del registro del portal. Debe ser siempre cero (D27).
qlonglong filasConElTokenEnClaro(IQueryExecutor& executor, const QString& token)
{
    const auto filas = executor.query(
        QStringLiteral("SELECT count(*) FROM portales "
                       "WHERE uuid = ? OR token_hash = ? OR url = ?"),
        {QVariant(token), QVariant(token), QVariant(token)});
    if (filas.size() != 1 || filas.front().values.empty()) {
        return -1;
    }
    return filas.front().values.front().toLongLong();
}

} // namespace

class TstPortalTokenService : public QObject
{
    Q_OBJECT

private slots:
    void emiteYVerificaDentroDeSuClinica();
    void tokenNoAbreOtraClinicaNiOtroPortal();
    void revocacionDeniegaYNoSeRepite();
    void rotacionInvalidaElTokenAnteriorYConservaElPortal();
    void borradoDeniegaYEliminaLaBaseLocal();
    void duenoConDosPortalesNoCruzaDatos();
    void emitirRechazaVinculoInvalidoYDuplicado();
    void tokenNoSeGuardaEnClaro();
    void huellaEsCortaYEstable();
};

void TstPortalTokenService::emiteYVerificaDentroDeSuClinica()
{
    ClinicaFixture fixture(QStringLiteral("emite"), kClinicaA);
    QVERIFY(fixture.dir.isValid());
    QVERIFY(esquemaListo(*fixture.executor));

    PortalTokenService servicio(*fixture.executor);
    const QString portal = servicio.emitir(kClinicaA, 1, 1,
                                           QStringLiteral("libsql://portal-a"), kTokenA);
    QVERIFY(!portal.isEmpty());
    QVERIFY(portal != kTokenA);

    QVERIFY(servicio.verificar(kTokenA, kClinicaA));
    QCOMPARE(estadoDelPortal(*fixture.executor, portal), QStringLiteral("activo"));
    QCOMPARE(contarPortales(*fixture.executor), 1LL);
}

void TstPortalTokenService::tokenNoAbreOtraClinicaNiOtroPortal()
{
    ClinicaFixture fixtureA(QStringLiteral("aislaA"), kClinicaA);
    ClinicaFixture fixtureB(QStringLiteral("aislaB"), kClinicaB);

    PortalTokenService servicioA(*fixtureA.executor);
    PortalTokenService servicioB(*fixtureB.executor);

    const QString portalA = servicioA.emitir(kClinicaA, 1, 1,
                                             QStringLiteral("libsql://portal-a"), kTokenA);
    const QString portalB = servicioB.emitir(kClinicaB, 1, 1,
                                             QStringLiteral("libsql://portal-b"), kTokenB);
    QVERIFY(!portalA.isEmpty());
    QVERIFY(!portalB.isEmpty());

    // Cada token abre solo su portal y solo dentro de su clínica.
    QVERIFY(servicioA.verificar(kTokenA, kClinicaA));
    QVERIFY(servicioB.verificar(kTokenB, kClinicaB));
    QVERIFY(!servicioA.verificar(kTokenB, kClinicaA));
    QVERIFY(!servicioB.verificar(kTokenA, kClinicaB));

    // Un token existente no se valida contra la base de otra sede.
    QVERIFY(!servicioA.verificar(kTokenA, kClinicaB));

    // Un token que no existe en ningún registro tampoco abre nada.
    QVERIFY(!servicioA.verificar(kTokenAjeno, kClinicaA));
    QVERIFY(!servicioB.verificar(kTokenAjeno, kClinicaB));
}

void TstPortalTokenService::revocacionDeniegaYNoSeRepite()
{
    ClinicaFixture fixture(QStringLiteral("revoca"), kClinicaA);
    PortalTokenService servicio(*fixture.executor);

    const QString portal = servicio.emitir(kClinicaA, 1, 1,
                                           QStringLiteral("libsql://portal-a"), kTokenA);
    QVERIFY(!portal.isEmpty());
    QVERIFY(servicio.verificar(kTokenA, kClinicaA));

    QVERIFY(servicio.revocar(kTokenA));
    QVERIFY(!servicio.verificar(kTokenA, kClinicaA));
    QCOMPARE(estadoDelPortal(*fixture.executor, portal), QStringLiteral("revocado"));

    // Una revocación ya aplicada no vuelve a afectar filas.
    QVERIFY(!servicio.revocar(kTokenA));
}

void TstPortalTokenService::rotacionInvalidaElTokenAnteriorYConservaElPortal()
{
    ClinicaFixture fixture(QStringLiteral("rota"), kClinicaA);
    const QString directorio_bases = fixture.dir.filePath(QStringLiteral("bases"));
    QVERIFY(QDir().mkpath(directorio_bases));
    LocalPortalBaseStore bases(directorio_bases);
    PortalTokenService servicio(*fixture.executor, &bases);

    const QString portal = servicio.emitir(kClinicaA, 1, 1,
                                           QStringLiteral("libsql://portal-a"), kTokenA);
    QVERIFY(!portal.isEmpty());

    // La base del portal existe antes de rotar.
    const QString ruta = bases.rutaDe(portal);
    QFile base(ruta);
    QVERIFY(base.open(QIODevice::WriteOnly));
    QCOMPARE(base.write("base del portal"), qint64(15));
    base.close();

    QVERIFY(servicio.rotar(kTokenA, kTokenA2));

    QVERIFY(!servicio.verificar(kTokenA, kClinicaA));
    QVERIFY(servicio.verificar(kTokenA2, kClinicaA));

    // La rotación conserva uuid, cuenta, ficha y base: no renombra el portal.
    QCOMPARE(contarPortales(*fixture.executor), 1LL);
    QCOMPARE(estadoDelPortal(*fixture.executor, portal), QStringLiteral("activo"));
    QVERIFY(!ruta.isEmpty());
    QVERIFY(QFile::exists(ruta));
}

void TstPortalTokenService::borradoDeniegaYEliminaLaBaseLocal()
{
    ClinicaFixture fixture(QStringLiteral("borra"), kClinicaA);
    const QString directorio_bases = fixture.dir.filePath(QStringLiteral("bases"));
    QVERIFY(QDir().mkpath(directorio_bases));
    LocalPortalBaseStore bases(directorio_bases);
    PortalTokenService servicio(*fixture.executor, &bases);

    const QString portal = servicio.emitir(kClinicaA, 1, 1,
                                           QStringLiteral("libsql://portal-a"), kTokenA);
    QVERIFY(!portal.isEmpty());

    // La base local existe antes del borrado.
    const QString ruta = bases.rutaDe(portal);
    QVERIFY(!ruta.isEmpty());
    QFile base(ruta);
    QVERIFY(base.open(QIODevice::WriteOnly));
    QCOMPARE(base.write("base del portal"), qint64(15));
    base.close();
    QVERIFY(QFile::exists(ruta));

    QVERIFY(servicio.borrar(kTokenA));
    QVERIFY(!QFile::exists(ruta));
    QVERIFY(!servicio.verificar(kTokenA, kClinicaA));
    QCOMPARE(estadoDelPortal(*fixture.executor, portal), QStringLiteral("borrado"));

    // El uuid no puede salir del directorio de bases.
    QVERIFY(bases.rutaDe(QStringLiteral("../fuga")).isEmpty());
    QVERIFY(!bases.eliminar(QStringLiteral("../fuga")));
}

void TstPortalTokenService::duenoConDosPortalesNoCruzaDatos()
{
    // La misma persona con ficha en dos clínicas tiene dos cuentas y dos
    // portales independientes (D26). Revocar uno no afecta al otro.
    ClinicaFixture fixtureA(QStringLiteral("dosA"), kClinicaA);
    ClinicaFixture fixtureB(QStringLiteral("dosB"), kClinicaB);

    PortalTokenService servicioA(*fixtureA.executor);
    PortalTokenService servicioB(*fixtureB.executor);

    const QString portalA = servicioA.emitir(kClinicaA, 1, 1,
                                             QStringLiteral("libsql://portal-a"), kTokenA);
    const QString portalB = servicioB.emitir(kClinicaB, 1, 1,
                                             QStringLiteral("libsql://portal-b"), kTokenB);
    QVERIFY(!portalA.isEmpty());
    QVERIFY(!portalB.isEmpty());
    QVERIFY(portalA != portalB);

    QVERIFY(servicioA.revocar(kTokenA));
    QVERIFY(!servicioA.verificar(kTokenA, kClinicaA));
    QVERIFY(servicioB.verificar(kTokenB, kClinicaB));

    QCOMPARE(contarPortales(*fixtureA.executor), 1LL);
    QCOMPARE(contarPortales(*fixtureB.executor), 1LL);
    QCOMPARE(estadoDelPortal(*fixtureB.executor, portalB), QStringLiteral("activo"));
}

void TstPortalTokenService::emitirRechazaVinculoInvalidoYDuplicado()
{
    ClinicaFixture fixture(QStringLiteral("rechaza"), kClinicaA);
    sembrarCuenta(*fixture.executor, 2, 2, QStringLiteral("cliente-2"),
                  QStringLiteral("cuenta-2"));
    PortalTokenService servicio(*fixture.executor);

    // La clínica del ejecutor no coincide con la solicitada.
    QVERIFY(servicio.emitir(kClinicaB, 1, 1, QStringLiteral("libsql://portal-a"), kTokenA)
                .isEmpty());

    // Cuenta y ficha de personas distintas.
    QVERIFY(servicio.emitir(kClinicaA, 1, 2, QStringLiteral("libsql://portal-a"), kTokenA)
                .isEmpty());

    // Cuenta inexistente.
    QVERIFY(servicio.emitir(kClinicaA, 99, 99, QStringLiteral("libsql://portal-a"), kTokenA)
                .isEmpty());
    QCOMPARE(contarPortales(*fixture.executor), 0LL);

    const QString portal = servicio.emitir(kClinicaA, 1, 1,
                                           QStringLiteral("libsql://portal-a"), kTokenA);
    QVERIFY(!portal.isEmpty());

    // Token duplicado y cuenta duplicada no crean un segundo portal.
    QVERIFY(servicio.emitir(kClinicaA, 1, 1, QStringLiteral("libsql://otro"), kTokenA)
                .isEmpty());
    QVERIFY(servicio.emitir(kClinicaA, 1, 1, QStringLiteral("libsql://otro"), kTokenA2)
                .isEmpty());
    QCOMPARE(contarPortales(*fixture.executor), 1LL);

    // Otra cuenta de la misma clínica sí puede tener su propio portal.
    QVERIFY(!servicio.emitir(kClinicaA, 2, 2, QStringLiteral("libsql://portal-b"), kTokenB)
                 .isEmpty());
    QCOMPARE(contarPortales(*fixture.executor), 2LL);
}

void TstPortalTokenService::tokenNoSeGuardaEnClaro()
{
    ClinicaFixture fixture(QStringLiteral("secreto"), kClinicaA);
    PortalTokenService servicio(*fixture.executor);

    const QString portal = servicio.emitir(kClinicaA, 1, 1,
                                           QStringLiteral("libsql://portal-a"), kTokenA);
    QVERIFY(!portal.isEmpty());

    // Ni el uuid ni la huella ni la URL contienen el token en claro (D27).
    QCOMPARE(filasConElTokenEnClaro(*fixture.executor, kTokenA), 0LL);

    // La huella persistida es SHA-256 completa, no el token.
    const auto filas = fixture.executor->query(
        QStringLiteral("SELECT token_hash FROM portales WHERE uuid = ?"),
        {QVariant(portal)});
    QCOMPARE(static_cast<int>(filas.size()), 1);
    const QString hash = filas.front().values.front().toString();
    QCOMPARE(hash.size(), 64);
    QVERIFY(hash != kTokenA);
    QCOMPARE(PortalTokenService::huella(kTokenA), hash.left(6).toUpper());
}

void TstPortalTokenService::huellaEsCortaYEstable()
{
    const QString huella = PortalTokenService::huella(kTokenA);
    QCOMPARE(huella.size(), 6);
    QVERIFY(QRegularExpression(QStringLiteral("^[0-9A-F]{6}$")).match(huella).hasMatch());
    QCOMPARE(PortalTokenService::huella(kTokenA), huella);
    QVERIFY(PortalTokenService::huella(kTokenB) != huella);
}

QTEST_MAIN(TstPortalTokenService)

#include "tst_portal_token_service.moc"
