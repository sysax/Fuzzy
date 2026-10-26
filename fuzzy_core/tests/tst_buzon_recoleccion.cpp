#include "TestSupport.h"

#include "fuzzy/core/BuzonService.h"

#include <QDateTime>
#include <QTemporaryDir>
#include <QtTest>

using fuzzy::core::BuzonService;
using fuzzy::core::SqliteExecutor;
using namespace fuzzy::test;

namespace {

const QString kClinica = QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa1");
const QString kOtraClinica = QStringLiteral("bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbb2");
const QString kAlta1 = QStringLiteral("11111111-1111-4111-8111-111111111111");
const QString kAlta2 = QStringLiteral("22222222-2222-4222-8222-222222222222");
const QString kAlta3 = QStringLiteral("33333333-3333-4333-8333-333333333333");
const QString kAltaAjena = QStringLiteral("44444444-4444-4444-8444-444444444444");

void sembrarClinica(IQueryExecutor& clinica)
{
    QVERIFY(aplicarEsquema(clinica, QStringLiteral("clinic")));
    clinica.execute(
        QStringLiteral("INSERT INTO clinicas (uuid, nombre, zona_horaria, created_at, "
                       "updated_at, activa, sync_state) "
                       "VALUES (?, 'Clínica Norte', 'America/Bogota', 1, 1, 1, 'pendiente')"),
        {QVariant(kClinica)});
}

void sembrarAlta(IQueryExecutor& buzon, const QString& uuid, const QString& clinica_uuid,
                 const QString& nombre, const QString& telefono, const QString& correo,
                 const QString& mascota, const QString& franja)
{
    QVERIFY(buzon.execute(
                      QStringLiteral("INSERT INTO altas "
                                     "(uuid, clinica_uuid, nombre, correo, telefono, mascota, "
                                     " motivo, franja, creada_at) "
                                     "VALUES (?, ?, ?, ?, ?, ?, 'Control', ?, ?)"),
                      {QVariant(uuid), QVariant(clinica_uuid), QVariant(nombre),
                       QVariant(correo), QVariant(telefono), QVariant(mascota),
                       QVariant(franja), QVariant::fromValue<qint64>(1700000000000LL)})
                 .ok);
}

} // namespace

class TstBuzonRecoleccion : public QObject
{
    Q_OBJECT

private slots:
    void recolectaAltasDeSuClinica();
    void esIdempotenteYReutilizaContacto();
    void rechazaFilasInvalidasSinPerderlas();
    void sinBaseDeClinicaFalla();
};

void TstBuzonRecoleccion::recolectaAltasDeSuClinica()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor clinica(nuevaConexion(QStringLiteral("buzon_clinica")),
                           dir.filePath(QStringLiteral("clinic.db")));
    SqliteExecutor buzon(nuevaConexion(QStringLiteral("buzon")),
                         dir.filePath(QStringLiteral("inbox.db")));
    sembrarClinica(clinica);
    QVERIFY(aplicarEsquema(buzon, QStringLiteral("inbox")));

    sembrarAlta(buzon, kAlta1, kClinica, QStringLiteral("Ana Uno"),
                QStringLiteral("3001111111"), QStringLiteral("ana@example.com"),
                QStringLiteral("Luna"), QStringLiteral("09:30"));
    sembrarAlta(buzon, kAlta2, kClinica, QStringLiteral("Beto Dos"),
                QStringLiteral("3002222222"), QStringLiteral("beto@example.com"),
                QString(), QStringLiteral("mañana"));
    sembrarAlta(buzon, kAltaAjena, kOtraClinica, QStringLiteral("Otra Sede"),
                QStringLiteral("3009999999"), QStringLiteral("otra@example.com"),
                QStringLiteral("Nube"), QStringLiteral("10:00"));

    BuzonService servicio(buzon, &clinica);
    const auto resultado = servicio.recolectar(kClinica);
    QVERIFY2(resultado.ok, qPrintable(resultado.errores.join(QStringLiteral(" | "))));
    QCOMPARE(resultado.procesadas, 2);
    QCOMPARE(resultado.rechazadas, 0);
    QCOMPARE(resultado.altas.size(), 2);

    QCOMPARE(contar(clinica, QStringLiteral("clientes")), 2LL);
    QCOMPARE(contar(clinica, QStringLiteral("mascotas")), 2LL);
    QCOMPARE(contar(clinica, QStringLiteral("citas")), 2LL);
    QCOMPARE(contar(buzon, QStringLiteral("altas")), 1LL);

    const auto altas = buzon.query(QStringLiteral("SELECT uuid FROM altas"));
    QCOMPARE(altas.front().values.front().toString(), kAltaAjena);

    const auto citas = clinica.query(
        QStringLiteral("SELECT origen, notas_cliente, fecha_hora_programada FROM citas "
                       "ORDER BY id"));
    QCOMPARE(citas.size(), size_t(2));
    QCOMPARE(citas.at(0).values.at(0).toString(), QStringLiteral("primera_visita"));
    QCOMPARE(citas.at(0).values.at(1).toString(), QStringLiteral("Control"));
    // "09:30" se interpreta como el día siguiente a la creación del alta.
    const QDateTime base = QDateTime::fromMSecsSinceEpoch(1700000000000LL);
    const QDateTime esperada(base.date().addDays(1), QTime(9, 30));
    QCOMPARE(citas.at(0).values.at(2).toLongLong(), esperada.toMSecsSinceEpoch());

    const auto mascotas = clinica.query(
        QStringLiteral("SELECT nombre, especie FROM mascotas ORDER BY id"));
    QCOMPARE(mascotas.size(), size_t(2));
    QCOMPARE(mascotas.at(0).values.at(0).toString(), QStringLiteral("Luna"));
    QCOMPARE(mascotas.at(0).values.at(1).toString(), QStringLiteral("No indicada"));
    QCOMPARE(mascotas.at(1).values.at(0).toString(), QStringLiteral("Mascota de Beto Dos"));
}

void TstBuzonRecoleccion::esIdempotenteYReutilizaContacto()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor clinica(nuevaConexion(QStringLiteral("buzon_idem_clinica")),
                           dir.filePath(QStringLiteral("clinic.db")));
    SqliteExecutor buzon(nuevaConexion(QStringLiteral("buzon_idem")),
                         dir.filePath(QStringLiteral("inbox.db")));
    sembrarClinica(clinica);
    QVERIFY(aplicarEsquema(buzon, QStringLiteral("inbox")));

    sembrarAlta(buzon, kAlta1, kClinica, QStringLiteral("Ana Uno"),
                QStringLiteral("3001111111"), QStringLiteral("ana@example.com"),
                QStringLiteral("Luna"), QStringLiteral("09:30"));

    BuzonService servicio(buzon, &clinica);
    QVERIFY(servicio.recolectar(kClinica).ok);
    QCOMPARE(contar(clinica, QStringLiteral("clientes")), 1LL);
    QCOMPARE(contar(clinica, QStringLiteral("mascotas")), 1LL);

    // Reintentar el lote no duplica nada: la fila ya no está en el buzón.
    const auto segunda = servicio.recolectar(kClinica);
    QVERIFY(segunda.ok);
    QCOMPARE(segunda.procesadas, 0);
    QCOMPARE(contar(clinica, QStringLiteral("mascotas")), 1LL);

    // Otra alta del mismo dueño reutiliza la ficha por correo/teléfono.
    sembrarAlta(buzon, kAlta2, kClinica, QStringLiteral("Ana Uno"),
                QStringLiteral("3001111111"), QStringLiteral("ana@example.com"),
                QStringLiteral("Sol"), QStringLiteral("11:00"));
    const auto tercera = servicio.recolectar(kClinica);
    QVERIFY(tercera.ok);
    QCOMPARE(tercera.procesadas, 1);
    QCOMPARE(contar(clinica, QStringLiteral("clientes")), 1LL);
    QCOMPARE(contar(clinica, QStringLiteral("mascotas")), 2LL);
    QCOMPARE(contar(clinica, QStringLiteral("citas")), 2LL);
}

void TstBuzonRecoleccion::rechazaFilasInvalidasSinPerderlas()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor clinica(nuevaConexion(QStringLiteral("buzon_invalida_clinica")),
                           dir.filePath(QStringLiteral("clinic.db")));
    SqliteExecutor buzon(nuevaConexion(QStringLiteral("buzon_invalida")),
                         dir.filePath(QStringLiteral("inbox.db")));
    sembrarClinica(clinica);
    QVERIFY(aplicarEsquema(buzon, QStringLiteral("inbox")));

    // Se salta el CHECK solo para simular un escritor defectuoso.
    buzon.execute(QStringLiteral("PRAGMA ignore_check_constraints = ON"));
    QVERIFY(buzon.execute(
                     QStringLiteral("INSERT INTO altas (uuid, clinica_uuid, nombre, telefono, "
                                    "creada_at) VALUES (?, ?, 'X', '123', 1)"),
                     {QVariant(kAlta3), QVariant(kClinica)})
                .ok);
    buzon.execute(QStringLiteral("PRAGMA ignore_check_constraints = OFF"));

    BuzonService servicio(buzon, &clinica);
    const auto resultado = servicio.recolectar(kClinica);
    QVERIFY(!resultado.ok);
    QCOMPARE(resultado.procesadas, 0);
    QCOMPARE(resultado.rechazadas, 1);
    QCOMPARE(contar(clinica, QStringLiteral("clientes")), 0LL);
    QCOMPARE(contar(buzon, QStringLiteral("altas")), 1LL);
}

void TstBuzonRecoleccion::sinBaseDeClinicaFalla()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor buzon(nuevaConexion(QStringLiteral("buzon_sin_clinica")),
                         dir.filePath(QStringLiteral("inbox.db")));
    QVERIFY(aplicarEsquema(buzon, QStringLiteral("inbox")));

    BuzonService servicio(buzon);
    const auto resultado = servicio.recolectar(kClinica);
    QVERIFY(!resultado.ok);
    QCOMPARE(resultado.procesadas, 0);
    QVERIFY(!resultado.errores.isEmpty());
}

QTEST_MAIN(TstBuzonRecoleccion)
#include "tst_buzon_recoleccion.moc"
