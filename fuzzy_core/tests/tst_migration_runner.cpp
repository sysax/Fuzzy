#include "TestSupport.h"

#include "fuzzy/core/MigrationRunner.h"

#include <QTemporaryDir>
#include <QtTest>

using fuzzy::core::MigrationRunner;
using fuzzy::core::SqliteExecutor;
using namespace fuzzy::test;

class TstMigrationRunner : public QObject
{
    Q_OBJECT

private slots:
    void runnerDelPortalEstaIncrustado();
    void aplicaUnaSolaVez();
    void esquemaClinicoCompleto();
};

void TstMigrationRunner::runnerDelPortalEstaIncrustado()
{
    const MigrationRunner runner = MigrationRunner::portal();
    QVERIFY2(runner.valido(), "el recurso :/fuzzy/migrations/portal no está disponible");
    QCOMPARE(runner.versionActual(), QStringLiteral("0002"));
    QCOMPARE(runner.versiones(),
             (QStringList{QStringLiteral("0001"), QStringLiteral("0002")}));
}

void TstMigrationRunner::aplicaUnaSolaVez()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor executor(nuevaConexion(QStringLiteral("migracion")),
                            dir.filePath(QStringLiteral("portal.db")));

    const MigrationRunner runner = MigrationRunner::portal();
    QVERIFY(runner.aplicar(executor));
    QVERIFY(existeTabla(executor, QStringLiteral("portal_meta")));
    QVERIFY(existeTabla(executor, QStringLiteral("perfil")));
    QVERIFY(existeTabla(executor, QStringLiteral("solicitudes_cita")));
    QCOMPARE(contar(executor, QStringLiteral("_fuzzy_migraciones")), 2);

    // Volver a aplicar es no-op y no duplica el historial.
    QVERIFY(runner.aplicar(executor));
    QCOMPARE(contar(executor, QStringLiteral("_fuzzy_migraciones")), 2);
}

void TstMigrationRunner::esquemaClinicoCompleto()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor executor(nuevaConexion(QStringLiteral("clinica")),
                            dir.filePath(QStringLiteral("clinic.db")));

    const MigrationRunner runner(migracionesDir() + QStringLiteral("/clinic"));
    QVERIFY2(runner.valido(), "no se encontraron las migraciones de la clínica");
    QVERIFY(runner.aplicar(executor));
    QCOMPARE(runner.versionActual(), QStringLiteral("0005"));

    QVERIFY(existeTabla(executor, QStringLiteral("sync_outbox")));
    QVERIFY(existeTabla(executor, QStringLiteral("sync_change_log")));
    QVERIFY(existeTabla(executor, QStringLiteral("sync_aplicado")));
    QVERIFY(existeTabla(executor, QStringLiteral("sync_meta")));
    QVERIFY(existeColumna(executor, QStringLiteral("sync_meta"),
                          QStringLiteral("ultimo_pull_id")));

    // La fase 6 dejó metadatos verificables en la galería clínica; el
    // binario sigue viviendo fuera de la base.
    QVERIFY(existeColumna(executor, QStringLiteral("mascota_galeria"),
                          QStringLiteral("mime")));
    QVERIFY(existeColumna(executor, QStringLiteral("mascota_galeria"),
                          QStringLiteral("sha256")));
    QVERIFY(existeColumna(executor, QStringLiteral("mascota_galeria"),
                          QStringLiteral("tamano")));
    QVERIFY(existeColumna(executor, QStringLiteral("mascota_galeria"),
                          QStringLiteral("visita_id")));
    QVERIFY(!existeColumna(executor, QStringLiteral("mascota_galeria"),
                           QStringLiteral("foto")));

    // Las migraciones sembraron catálogos estables para resolver referencias.
    QCOMPARE(contar(executor, QStringLiteral("estados_cita")), 6LL);
    QCOMPARE(contar(executor, QStringLiteral("tipos_servicio_cita")), 3LL);

    QVERIFY(runner.aplicar(executor));
    QCOMPARE(contar(executor, QStringLiteral("_fuzzy_migraciones")), 5LL);
}

QTEST_MAIN(TstMigrationRunner)
#include "tst_migration_runner.moc"
