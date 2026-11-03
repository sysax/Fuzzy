#include "TestSupport.h"

#include "fuzzy/core/AuthService.h"
#include "fuzzy/core/OperadorService.h"
#include "fuzzy/core/SqliteExecutor.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QVariant>
#include <QtTest>

#include <memory>

using fuzzy::core::AuthService;
using fuzzy::core::OperadorService;
using fuzzy::core::SqliteExecutor;
using namespace fuzzy::test;

namespace {

qint64 escalar(IQueryExecutor& db, const QString& sql,
               const std::vector<QVariant>& params = {})
{
    const auto filas = db.query(sql, params);
    return filas.empty() ? -1 : filas.front().values.front().toLongLong();
}

} // namespace

class TestOperador : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void estadoYRespaldo();
    void respaldoDetectaAlteracion();
    void restauracionReversible();
    void recuperarAdminSinSesion();
    void migrarBaseNueva();

private:
    void preparar();

    std::unique_ptr<QTemporaryDir> dir_;
    QString dbPath_;
    QString adminUuid_;
};

void TestOperador::init()
{
    preparar();
}

void TestOperador::cleanup()
{
    dir_.reset();
}

void TestOperador::preparar()
{
    dir_ = std::make_unique<QTemporaryDir>();
    QVERIFY(dir_->isValid());
    dbPath_ = dir_->filePath(QStringLiteral("clinica.db"));

    SqliteExecutor db(nuevaConexion(QStringLiteral("operador_base")), dbPath_);
    QVERIFY(aplicarEsquema(db, QStringLiteral("clinic")));

    AuthService auth(db);
    const auto alta = auth.bootstrap(QStringLiteral("Clínica Operador"),
                                     QStringLiteral("Bogotá"),
                                     QStringLiteral("Ana Admin"),
                                     QStringLiteral("ana@operador.test"),
                                     QStringLiteral("ClaveSegura1"));
    QVERIFY2(alta.ok, qPrintable(alta.error));
    adminUuid_ = alta.usuario_uuid;
}

void TestOperador::estadoYRespaldo()
{
    OperadorService operador(dbPath_);

    const auto estado = operador.estado();
    QVERIFY2(estado.ok, qPrintable(estado.error));
    QCOMPARE(estado.clinicaNombre, QStringLiteral("Clínica Operador"));
    QCOMPARE(estado.versionEsquema, QStringLiteral("0006"));
    QCOMPARE(estado.usuarios, 1);
    QVERIFY(estado.integridadOk);
    QCOMPARE(estado.integridad, QStringLiteral("ok"));

    const QString respaldo = dir_->filePath(QStringLiteral("respaldo.db"));
    const auto creado = operador.respaldar(respaldo);
    QVERIFY2(creado.ok, qPrintable(creado.error));
    QCOMPARE(creado.sha256.size(), 64);
    QVERIFY(QFileInfo::exists(respaldo));
    QVERIFY(QFileInfo::exists(respaldo + QStringLiteral(".sha256")));
    QVERIFY(QFileInfo::exists(respaldo + QStringLiteral(".json")));
    QVERIFY(QFileInfo(respaldo).size() > 0);

    QString error;
    QVERIFY2(operador.verificarRespaldo(respaldo, &error), qPrintable(error));

    QFile manifiesto(respaldo + QStringLiteral(".json"));
    QVERIFY(manifiesto.open(QIODevice::ReadOnly));
    const auto json = QJsonDocument::fromJson(manifiesto.readAll()).object();
    QCOMPARE(json.value(QStringLiteral("sha256")).toString(), creado.sha256);
    QCOMPARE(json.value(QStringLiteral("version_esquema")).toString(),
             QStringLiteral("0006"));

    // El destino no se sobrescribe en silencio.
    const auto repetido = operador.respaldar(respaldo);
    QVERIFY(!repetido.ok);
}

void TestOperador::respaldoDetectaAlteracion()
{
    OperadorService operador(dbPath_);
    const QString respaldo = dir_->filePath(QStringLiteral("respaldo.db"));
    QVERIFY(operador.respaldar(respaldo).ok);

    QFile archivo(respaldo);
    QVERIFY(archivo.open(QIODevice::Append));
    QCOMPARE(archivo.write("alterado"), qint64(8));
    archivo.close();

    QString error;
    QVERIFY(!operador.verificarRespaldo(respaldo, &error));
    QVERIFY(!error.isEmpty());
}

void TestOperador::restauracionReversible()
{
    OperadorService operador(dbPath_);
    const QString respaldo = dir_->filePath(QStringLiteral("respaldo.db"));
    QVERIFY(operador.respaldar(respaldo).ok);

    {
        SqliteExecutor db(nuevaConexion(QStringLiteral("operador_marca")), dbPath_);
        QVERIFY(db.execute(QStringLiteral("CREATE TABLE marcador (x INTEGER)")).ok);
        QVERIFY(db.execute(QStringLiteral("INSERT INTO marcador VALUES (1)")).ok);
    }

    const auto sinConfirmar = operador.restaurar(respaldo, false);
    QVERIFY(!sinConfirmar.ok);

    const auto restaurado = operador.restaurar(respaldo, true);
    QVERIFY2(restaurado.ok, qPrintable(restaurado.error));
    QVERIFY(QFileInfo::exists(restaurado.respaldoPrevio));

    {
        SqliteExecutor db(nuevaConexion(QStringLiteral("operador_restaurado")), dbPath_);
        QCOMPARE(escalar(db,
                         QStringLiteral("SELECT count(*) FROM sqlite_master "
                                        "WHERE type = 'table' AND name = 'marcador'")),
                 qint64(0));
    }

    {
        SqliteExecutor previo(nuevaConexion(QStringLiteral("operador_previo")),
                              restaurado.respaldoPrevio);
        QCOMPARE(escalar(previo, QStringLiteral("SELECT count(*) FROM marcador")),
                 qint64(1));
    }

    const auto estado = operador.estado();
    QVERIFY2(estado.ok, qPrintable(estado.error));
    QCOMPARE(estado.usuarios, 1);
}

void TestOperador::recuperarAdminSinSesion()
{
    {
        SqliteExecutor db(nuevaConexion(QStringLiteral("operador_vet")), dbPath_);
        AuthService auth(db);
        const auto vet = auth.crearUsuario(adminUuid_, QStringLiteral("Vet Prueba"),
                                           QStringLiteral("vet@operador.test"),
                                           QStringLiteral("Veterinario"));
        QVERIFY2(vet.ok, qPrintable(vet.error));
        QVERIFY(db.execute(QStringLiteral("UPDATE usuarios SET activo = 0 "
                                          "WHERE lower(correo) = 'ana@operador.test'"))
                    .ok);
    }

    OperadorService operador(dbPath_);
    const auto recuperado = operador.recuperarAdmin(QStringLiteral("ANA@OPERADOR.TEST"));
    QVERIFY2(recuperado.ok, qPrintable(recuperado.error));
    QCOMPARE(recuperado.correo, QStringLiteral("ana@operador.test"));
    QCOMPARE(recuperado.passwordTemporal.size(), 12);

    {
        SqliteExecutor db(nuevaConexion(QStringLiteral("operador_login")), dbPath_);
        AuthService auth(db);
        const auto login =
            auth.iniciarSesion(QStringLiteral("ana@operador.test"),
                               recuperado.passwordTemporal);
        QVERIFY2(login.ok, qPrintable(login.error));
        QVERIFY(login.debeCambiarPassword);
        QCOMPARE(AuthService::nombreRol(login.sesion.rol), QStringLiteral("Admin"));
        QCOMPARE(escalar(db,
                         QStringLiteral("SELECT count(*) FROM auditoria "
                                        "WHERE accion = 'recuperacion_admin_cli' "
                                        "AND usuario_id IS NULL")),
                 qint64(1));
    }

    const auto noAdmin = operador.recuperarAdmin(QStringLiteral("vet@operador.test"));
    QVERIFY(!noAdmin.ok);
    const auto inexistente = operador.recuperarAdmin(QStringLiteral("nadie@operador.test"));
    QVERIFY(!inexistente.ok);
}

void TestOperador::migrarBaseNueva()
{
    const QString nueva = dir_->filePath(QStringLiteral("nueva.db"));
    OperadorService operador(nueva);

    QString error;
    QVERIFY2(operador.migrar(&error), qPrintable(error));
    QVERIFY(QFileInfo::exists(nueva));

    const auto estado = operador.estado();
    QVERIFY2(estado.ok, qPrintable(estado.error));
    QCOMPARE(estado.versionEsquema, QStringLiteral("0006"));
    QCOMPARE(estado.usuarios, qint64(0));
    QVERIFY(estado.clinicaUuid.isEmpty());
}

QTEST_GUILESS_MAIN(TestOperador)

#include "tst_operador.moc"
