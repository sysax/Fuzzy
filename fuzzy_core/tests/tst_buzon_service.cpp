#include "fuzzy/core/BuzonService.h"
#include "fuzzy/core/SqliteExecutor.h"

#include <QTemporaryDir>
#include <QVariant>
#include <QtTest>

using fuzzy::core::BuzonService;
using fuzzy::core::IQueryExecutor;
using fuzzy::core::SqliteExecutor;

namespace {

const QString kClinicaA = QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa1");
const QString kClinicaB = QStringLiteral("bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbb2");
const QString kAlta1 = QStringLiteral("11111111-1111-4111-8111-111111111111");
const QString kAlta2 = QStringLiteral("22222222-2222-4222-8222-222222222222");

int contadorConexiones()
{
    static int siguiente = 0;
    return ++siguiente;
}

QString conexion(const QString& etiqueta)
{
    return QStringLiteral("fase4_buzon_%1_%2").arg(etiqueta).arg(contadorConexiones());
}

void crearEsquema(IQueryExecutor& executor)
{
    executor.execute(QStringLiteral(
        "CREATE TABLE altas ("
        "id INTEGER PRIMARY KEY, "
        "uuid TEXT NOT NULL UNIQUE, "
        "clinica_uuid TEXT NOT NULL, "
        "nombre TEXT NOT NULL, "
        "correo TEXT, "
        "telefono TEXT NOT NULL, "
        "mascota TEXT, "
        "motivo TEXT, "
        "franja TEXT, "
        "creada_at INTEGER NOT NULL, "
        "CHECK (length(nombre) BETWEEN 3 AND 120), "
        "CHECK (length(telefono) BETWEEN 7 AND 20), "
        "CHECK (correo IS NULL OR length(correo) BETWEEN 5 AND 254), "
        "CHECK (mascota IS NULL OR length(mascota) <= 120), "
        "CHECK (motivo IS NULL OR length(motivo) <= 500), "
        "CHECK (franja IS NULL OR length(franja) <= 40))"));
}

bool esquemaListo(IQueryExecutor& executor)
{
    const auto filas = executor.query(
        QStringLiteral("SELECT name FROM sqlite_master WHERE type = 'table' AND name = 'altas'"));
    return filas.size() == 1;
}

void sembrarAlta(IQueryExecutor& executor, const QString& uuid, const QString& clinica_uuid,
                 const QString& nombre)
{
    executor.execute(
        QStringLiteral("INSERT INTO altas "
                       "(uuid, clinica_uuid, nombre, correo, telefono, mascota, motivo, "
                       "franja, creada_at) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"),
        {QVariant(uuid), QVariant(clinica_uuid), QVariant(nombre),
         QVariant(QStringLiteral("ana@example.com")), QVariant(QStringLiteral("3001234567")),
         QVariant(QStringLiteral("Luna")), QVariant(QStringLiteral("Vacunación")),
         QVariant(QStringLiteral("mañana")), QVariant::fromValue<qint64>(1700000000000LL)});
}

qlonglong contarAltas(IQueryExecutor& executor)
{
    const auto filas = executor.query(QStringLiteral("SELECT count(*) FROM altas"));
    if (filas.size() != 1 || filas.front().values.empty()) {
        return -1;
    }
    return filas.front().values.front().toLongLong();
}

struct BuzonFixture
{
    QTemporaryDir dir;
    std::unique_ptr<SqliteExecutor> executor;

    explicit BuzonFixture(const QString& etiqueta)
        : executor(std::make_unique<SqliteExecutor>(conexion(etiqueta),
                                                    dir.filePath("inbox.db")))
    {
        crearEsquema(*executor);
    }
};

} // namespace

class TstBuzonService : public QObject
{
    Q_OBJECT

private slots:
    void consumeUnaSolaVez();
    void consumeSoloLaFilaObjetivo();
    void noConsumeAltasDeOtraClinica();
    void uuidDesconocidoNoConsume();
    void parametrosVaciosNoConsumen();
};

void TstBuzonService::consumeUnaSolaVez()
{
    BuzonFixture fixture(QStringLiteral("unica"));
    QVERIFY(fixture.dir.isValid());
    QVERIFY(esquemaListo(*fixture.executor));
    sembrarAlta(*fixture.executor, kAlta1, kClinicaA, QStringLiteral("Ana Pérez"));

    BuzonService buzon(*fixture.executor);
    QVERIFY(buzon.consumir(kClinicaA, kAlta1));
    QCOMPARE(contarAltas(*fixture.executor), 0LL);

    // La segunda llamada ya no encuentra la fila: el alta se procesó una vez.
    QVERIFY(!buzon.consumir(kClinicaA, kAlta1));
    QCOMPARE(contarAltas(*fixture.executor), 0LL);
}

void TstBuzonService::consumeSoloLaFilaObjetivo()
{
    BuzonFixture fixture(QStringLiteral("objetivo"));
    sembrarAlta(*fixture.executor, kAlta1, kClinicaA, QStringLiteral("Ana Pérez"));
    sembrarAlta(*fixture.executor, kAlta2, kClinicaA, QStringLiteral("Luis Gómez"));
    QCOMPARE(contarAltas(*fixture.executor), 2LL);

    BuzonService buzon(*fixture.executor);
    QVERIFY(buzon.consumir(kClinicaA, kAlta1));
    QCOMPARE(contarAltas(*fixture.executor), 1LL);

    // La otra alta sigue disponible para su propia consumición.
    QVERIFY(buzon.consumir(kClinicaA, kAlta2));
    QCOMPARE(contarAltas(*fixture.executor), 0LL);
}

void TstBuzonService::noConsumeAltasDeOtraClinica()
{
    BuzonFixture fixture(QStringLiteral("ajena"));

    // Fila ajena en el buzón de B: la clínica B no debe consumirla.
    sembrarAlta(*fixture.executor, kAlta1, kClinicaA, QStringLiteral("Ana Pérez"));

    BuzonService buzon(*fixture.executor);
    QVERIFY(!buzon.consumir(kClinicaB, kAlta1));
    QCOMPARE(contarAltas(*fixture.executor), 1LL);

    // Solo la clínica dueña de la fila puede consumirla.
    QVERIFY(buzon.consumir(kClinicaA, kAlta1));
    QCOMPARE(contarAltas(*fixture.executor), 0LL);
}

void TstBuzonService::uuidDesconocidoNoConsume()
{
    BuzonFixture fixture(QStringLiteral("desconocido"));
    sembrarAlta(*fixture.executor, kAlta1, kClinicaA, QStringLiteral("Ana Pérez"));

    BuzonService buzon(*fixture.executor);
    QVERIFY(!buzon.consumir(kClinicaA, kAlta2));
    QCOMPARE(contarAltas(*fixture.executor), 1LL);
}

void TstBuzonService::parametrosVaciosNoConsumen()
{
    BuzonFixture fixture(QStringLiteral("vacios"));
    sembrarAlta(*fixture.executor, kAlta1, kClinicaA, QStringLiteral("Ana Pérez"));

    BuzonService buzon(*fixture.executor);
    QVERIFY(!buzon.consumir(QString(), kAlta1));
    QVERIFY(!buzon.consumir(kClinicaA, QString()));
    QCOMPARE(contarAltas(*fixture.executor), 1LL);
}

QTEST_MAIN(TstBuzonService)

#include "tst_buzon_service.moc"
