#include "TestSupport.h"

#include "fuzzy/core/SyncEngine.h"

#include <QDateTime>
#include <QTemporaryDir>
#include <QtTest>

using fuzzy::core::SqliteExecutor;
using fuzzy::core::SyncEngine;
using namespace fuzzy::test;

namespace {

const QString kCliente = QStringLiteral("10000000-0000-4000-8000-000000000001");
const QString kMascota = QStringLiteral("30000000-0000-4000-8000-000000000001");

void sembrarNodoA(IQueryExecutor& a)
{
    QVERIFY(aplicarEsquema(a, QStringLiteral("clinic")));
    a.execute(
        QStringLiteral("INSERT INTO clientes (id, uuid, nombre_completo, telefono, correo, "
                       "activo, created_at, updated_at, sync_state) "
                       "VALUES (1, ?, 'Ana Uno', '3001111111', 'ana@example.com', 1, 1000, "
                       "1000, 'pendiente')"),
        {QVariant(kCliente)});
    a.execute(
        QStringLiteral("INSERT INTO mascotas (id, uuid, cliente_id, nombre, especie, sexo, "
                       "peso_kg, activo, castrado, created_at, updated_at, sync_state) "
                       "VALUES (1, ?, 1, 'Luna', 'Perro', 'Hembra', 12, 1, 0, 1000, 1000, "
                       "'pendiente')"),
        {QVariant(kMascota)});
}

QString nombreMascota(IQueryExecutor& executor)
{
    const auto filas = executor.query(
        QStringLiteral("SELECT nombre FROM mascotas WHERE uuid = ?"), {QVariant(kMascota)});
    return filas.empty() ? QString() : filas.front().values.front().toString();
}

qint64 cambioRemoto(IQueryExecutor& remoto)
{
    return contar(remoto, QStringLiteral("sync_change_log"));
}

void actualizarMascota(IQueryExecutor& executor, const QString& nombre, qint64 updated_at)
{
    QVERIFY(executor.execute(
                        QStringLiteral("UPDATE mascotas SET nombre = ?, updated_at = ?, "
                                       "sync_state = 'pendiente' WHERE uuid = ?"),
                        {QVariant(nombre), QVariant::fromValue(updated_at),
                         QVariant(kMascota)})
                .ok);
}

} // namespace

class TstSyncEngine : public QObject
{
    Q_OBJECT

private slots:
    void dosNodosConvergen();
    void lwwYReintentoIdempotente();
};

void TstSyncEngine::dosNodosConvergen()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor nodoA(nuevaConexion(QStringLiteral("sync_a")),
                         dir.filePath(QStringLiteral("a.db")));
    SqliteExecutor nodoB(nuevaConexion(QStringLiteral("sync_b")),
                         dir.filePath(QStringLiteral("b.db")));
    SqliteExecutor remoto(nuevaConexion(QStringLiteral("sync_remoto")),
                          dir.filePath(QStringLiteral("remote.db")));

    sembrarNodoA(nodoA);
    QVERIFY(aplicarEsquema(nodoB, QStringLiteral("clinic")));
    QVERIFY(aplicarEsquema(remoto, QStringLiteral("clinic")));

    SyncEngine motorA(nodoA, remoto, QStringLiteral("nodo-a"));
    SyncEngine motorB(nodoB, remoto, QStringLiteral("nodo-b"));

    const auto primero = motorA.sincronizar();
    QVERIFY2(primero.ok, qPrintable(primero.error));
    QCOMPARE(primero.detectados, 2);
    QCOMPARE(primero.empujados, 2);
    QCOMPARE(primero.aplicados, 0);
    QCOMPARE(cambioRemoto(remoto), 2LL);

    const auto segundo = motorB.sincronizar();
    QVERIFY2(segundo.ok, qPrintable(segundo.error));
    QCOMPARE(segundo.detectados, 0);
    QCOMPARE(segundo.empujados, 0);
    QCOMPARE(segundo.aplicados, 2);
    QCOMPARE(contar(nodoB, QStringLiteral("clientes")), 1LL);
    QCOMPARE(contar(nodoB, QStringLiteral("mascotas")), 1LL);
    QCOMPARE(nombreMascota(nodoB), QStringLiteral("Luna"));

    const auto estado = nodoB.query(
        QStringLiteral("SELECT sync_state FROM mascotas WHERE uuid = ?"), {QVariant(kMascota)});
    QCOMPARE(estado.front().values.front().toString(), QStringLiteral("sincronizado"));

    // Un cambio en B llega a A por el change log.
    actualizarMascota(nodoB, QStringLiteral("Luna de B"), 2000);
    const auto tercero = motorB.sincronizar();
    QVERIFY2(tercero.ok, qPrintable(tercero.error));
    QCOMPARE(tercero.empujados, 1);
    const auto cuarto = motorA.sincronizar();
    QVERIFY2(cuarto.ok, qPrintable(cuarto.error));
    QCOMPARE(cuarto.aplicados, 1);
    QCOMPARE(nombreMascota(nodoA), QStringLiteral("Luna de B"));

    // Dos nodos no vuelven a publicar lo que acaban de aplicar.
    const auto quinto = motorA.sincronizar();
    QVERIFY(quinto.ok);
    QCOMPARE(quinto.detectados, 0);
    QCOMPARE(quinto.empujados, 0);
}

void TstSyncEngine::lwwYReintentoIdempotente()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor nodoA(nuevaConexion(QStringLiteral("lww_a")),
                         dir.filePath(QStringLiteral("a.db")));
    SqliteExecutor nodoB(nuevaConexion(QStringLiteral("lww_b")),
                         dir.filePath(QStringLiteral("b.db")));
    SqliteExecutor remoto(nuevaConexion(QStringLiteral("lww_remoto")),
                          dir.filePath(QStringLiteral("remote.db")));

    sembrarNodoA(nodoA);
    QVERIFY(aplicarEsquema(nodoB, QStringLiteral("clinic")));
    QVERIFY(aplicarEsquema(remoto, QStringLiteral("clinic")));
    SyncEngine motorA(nodoA, remoto, QStringLiteral("nodo-a"));
    SyncEngine motorB(nodoB, remoto, QStringLiteral("nodo-b"));

    QVERIFY(motorA.sincronizar().ok);
    QVERIFY(motorB.sincronizar().ok);
    QCOMPARE(nombreMascota(nodoB), QStringLiteral("Luna"));

    // Ediciones concurrentes: gana la de updated_at mayor, sin importar el
    // orden en que lleguen los eventos.
    actualizarMascota(nodoA, QStringLiteral("Edición A"), 3000);
    actualizarMascota(nodoB, QStringLiteral("Edición B"), 4000);
    QVERIFY(motorA.sincronizar().ok);
    QVERIFY(motorB.sincronizar().ok);
    QVERIFY(motorA.sincronizar().ok);
    QCOMPARE(nombreMascota(nodoA), QStringLiteral("Edición B"));
    QCOMPARE(nombreMascota(nodoB), QStringLiteral("Edición B"));

    const auto estadoA = nodoA.query(
        QStringLiteral("SELECT updated_at, sync_state FROM mascotas WHERE uuid = ?"),
        {QVariant(kMascota)});
    QCOMPARE(estadoA.front().values.at(0).toLongLong(), 4000LL);
    QCOMPARE(estadoA.front().values.at(1).toString(), QStringLiteral("sincronizado"));

    // Tombstone: el borrado lógico viaja y no resucita la fila.
    QVERIFY(nodoA.execute(
                       QStringLiteral("UPDATE mascotas SET deleted_at = 5000, "
                                      "updated_at = 5000, sync_state = 'pendiente' "
                                      "WHERE uuid = ?"),
                       {QVariant(kMascota)})
                .ok);
    QVERIFY(motorA.sincronizar().ok);
    const auto pullTombstone = motorB.sincronizar();
    QVERIFY2(pullTombstone.ok, qPrintable(pullTombstone.error));

    for (SqliteExecutor* nodo : {&nodoA, &nodoB}) {
        const auto fila = nodo->query(
            QStringLiteral("SELECT deleted_at, updated_at FROM mascotas WHERE uuid = ?"),
            {QVariant(kMascota)});
        QCOMPARE(fila.size(), size_t(1));
        QCOMPARE(fila.front().values.at(0).toLongLong(), 5000LL);
        QCOMPARE(fila.front().values.at(1).toLongLong(), 5000LL);
    }

    // Corte entre publicar y marcar el outbox: al reintentar se reinsertan
    // los mismos event_uuid y el change log no crece.
    const qint64 antes = cambioRemoto(remoto);
    QVERIFY(nodoA.execute(QStringLiteral("UPDATE sync_outbox SET enviado_at = NULL")).ok);
    const auto reintento = motorA.empujar();
    QVERIFY2(reintento.ok, qPrintable(reintento.error));
    QCOMPARE(reintento.empujados, 2);
    QCOMPARE(cambioRemoto(remoto), antes);

    // El nodo B tampoco reaplica lo ya aplicado.
    const auto despues = motorB.sincronizar();
    QVERIFY2(despues.ok, qPrintable(despues.error));
    QCOMPARE(despues.aplicados, 0);
    QCOMPARE(contar(nodoB, QStringLiteral("mascotas")), 1LL);
}

QTEST_MAIN(TstSyncEngine)
#include "tst_sync_engine.moc"
