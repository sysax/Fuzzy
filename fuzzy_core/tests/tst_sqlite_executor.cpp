#include "fuzzy/core/SqliteExecutor.h"

#include <QtTest>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

class TstSqliteExecutor : public QObject
{
    Q_OBJECT

private slots:
    void ejecutaSentenciasYReportaAfectadas();
    void operacionDirectaConQSqlite();
};

void TstSqliteExecutor::ejecutaSentenciasYReportaAfectadas()
{
    const QString db_path = QDir::tempPath() + QStringLiteral("/fuzzy_phase3_test.db");
    fuzzy::core::SqliteExecutor executor(QStringLiteral("fuzzy_core"), db_path);

    // Sentencia sin parámetros: via exec(sql).
    const fuzzy::core::ExecResult drop = executor.execute(
        QStringLiteral("DROP TABLE IF EXISTS fuzzy_test"));
    QCOMPARE(drop.affected, 0LL);

    const fuzzy::core::ExecResult create = executor.execute(
        QStringLiteral("CREATE TABLE fuzzy_test (id INTEGER PRIMARY KEY, valor TEXT NOT NULL)"));
    QCOMPARE(create.affected, 0LL);

    const fuzzy::core::ExecResult insert = executor.execute(
        QStringLiteral("INSERT INTO fuzzy_test (valor) VALUES ('primero')"));
    QCOMPARE(insert.affected, 1LL);

    const fuzzy::core::ExecResult update = executor.execute(
        QStringLiteral("UPDATE fuzzy_test SET valor = 'segundo' WHERE id = 1"));
    QCOMPARE(update.affected, 1LL);
}

void TstSqliteExecutor::operacionDirectaConQSqlite()
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("fuzzy_core"));
    db.setDatabaseName(QDir::tempPath() + QStringLiteral("/fuzzy_phase3_direct.db"));
    db.open();

    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT 1"));
    const bool ok = query.exec();
    QVERIFY(ok);
}

QTEST_MAIN(TstSqliteExecutor)

#include "tst_sqlite_executor.moc"
