#include "TestSupport.h"

#include "fuzzy/core/MascotaFotoService.h"

#include <QBuffer>
#include <QColor>
#include <QImage>
#include <QTemporaryDir>
#include <QVariant>
#include <QtTest>

using fuzzy::core::MascotaFotoService;
using fuzzy::core::SqliteExecutor;
using namespace fuzzy::test;

namespace {

const QString kMascota = QStringLiteral("30000000-0000-4000-8000-000000000001");

bool prepararPortal(SqliteExecutor& portal)
{
    if (!aplicarEsquema(portal, QStringLiteral("portal"))) {
        return false;
    }
    const auto meta = portal.execute(
        QStringLiteral("INSERT INTO portal_meta "
                       "(id, esquema_version, cuenta_uuid, clinica_uuid, "
                       " generado_at, clinica_nombre, proyeccion_version) "
                       "VALUES (1, 2, 'cuenta-1', 'clinica-1', 1, "
                       "'Clínica Norte', 0)"));
    if (!meta.ok) {
        return false;
    }
    return portal.execute(
                     QStringLiteral("INSERT INTO mascotas "
                                    "(uuid, nombre, especie, created_at, updated_at) "
                                    "VALUES (?, 'Luna', 'Perro', 1, 1)"),
                     {QVariant(kMascota)})
        .ok;
}

QByteArray imagenPng(int ancho, int alto, const QColor& color)
{
    QImage imagen(ancho, alto, QImage::Format_RGB32);
    imagen.fill(color);
    QByteArray datos;
    QBuffer buffer(&datos);
    buffer.open(QIODevice::WriteOnly);
    imagen.save(&buffer, "PNG");
    return datos;
}

} // namespace

class TstMascotaFoto : public QObject
{
    Q_OBJECT

private slots:
    void reemplazaSinResiduo();
    void rechazaImagenMayorDeDosMegas();
    void rechazaContenidoQueNoEsImagen();
    void noAmpliaImagenesPequenas();
    void exigeMascotaDelPortal();
};

void TstMascotaFoto::reemplazaSinResiduo()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor portal(nuevaConexion(QStringLiteral("foto")),
                          dir.filePath(QStringLiteral("portal.db")));
    QVERIFY(prepararPortal(portal));

    MascotaFotoService servicio(portal);
    const auto primera = servicio.reemplazar(
        kMascota, imagenPng(1600, 900, QColor(200, 30, 30)));
    QVERIFY2(primera.ok, qPrintable(primera.error));
    QCOMPARE(primera.ancho, 1024);
    QCOMPARE(primera.alto, 576);
    QCOMPARE(primera.sha256.size(), 64);
    QVERIFY(primera.bytes > 0);
    QVERIFY(primera.bytes <= MascotaFotoService::kBytesMaximos);

    QCOMPARE(contar(portal, QStringLiteral("mascota_fotos")), 1LL);
    const auto fila = portal.query(
        QStringLiteral("SELECT uuid, mime, sha256, length(foto), ancho, alto "
                       "FROM mascota_fotos WHERE mascota_uuid = ?"),
        {QVariant(kMascota)});
    QCOMPARE(fila.size(), size_t(1));
    QCOMPARE(fila.front().values.at(0).toString(), primera.uuid);
    QCOMPARE(fila.front().values.at(1).toString(), QStringLiteral("image/jpeg"));
    QCOMPARE(fila.front().values.at(2).toString(), primera.sha256);
    QCOMPARE(fila.front().values.at(3).toLongLong(), static_cast<qint64>(primera.bytes));

    const auto bytes = portal.query(
        QStringLiteral("SELECT foto FROM mascota_fotos WHERE mascota_uuid = ?"),
        {QVariant(kMascota)});
    const QByteArray jpeg = bytes.front().values.front().toByteArray();
    QVERIFY(jpeg.startsWith(QByteArrayLiteral("\xFF\xD8\xFF")));

    // Segundo reemplazo: misma fila, mismo uuid y sin rastro de la anterior.
    const auto segunda = servicio.reemplazar(
        kMascota, imagenPng(900, 1600, QColor(30, 30, 200)));
    QVERIFY2(segunda.ok, qPrintable(segunda.error));
    QCOMPARE(segunda.uuid, primera.uuid);
    QCOMPARE(segunda.ancho, 576);
    QCOMPARE(segunda.alto, 1024);
    QVERIFY(segunda.sha256 != primera.sha256);
    QCOMPARE(contar(portal, QStringLiteral("mascota_fotos")), 1LL);

    const auto anterior = portal.query(
        QStringLiteral("SELECT count(*) FROM mascota_fotos WHERE sha256 = ?"),
        {QVariant(primera.sha256)});
    QCOMPARE(anterior.front().values.front().toLongLong(), 0LL);
}

void TstMascotaFoto::rechazaImagenMayorDeDosMegas()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor portal(nuevaConexion(QStringLiteral("foto_grande")),
                          dir.filePath(QStringLiteral("portal.db")));
    QVERIFY(prepararPortal(portal));

    MascotaFotoService servicio(portal);
    const auto valida = servicio.reemplazar(
        kMascota, imagenPng(400, 300, QColor(10, 120, 10)));
    QVERIFY2(valida.ok, qPrintable(valida.error));

    const QByteArray grande(MascotaFotoService::kBytesMaximos + 1, 'x');
    const auto rechazada = servicio.reemplazar(kMascota, grande);
    QVERIFY(!rechazada.ok);
    QVERIFY(rechazada.error.contains(QStringLiteral("2 MB")));

    QCOMPARE(contar(portal, QStringLiteral("mascota_fotos")), 1LL);
    const auto fila = portal.query(
        QStringLiteral("SELECT sha256 FROM mascota_fotos WHERE mascota_uuid = ?"),
        {QVariant(kMascota)});
    QCOMPARE(fila.front().values.front().toString(), valida.sha256);
}

void TstMascotaFoto::rechazaContenidoQueNoEsImagen()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor portal(nuevaConexion(QStringLiteral("foto_invalida")),
                          dir.filePath(QStringLiteral("portal.db")));
    QVERIFY(prepararPortal(portal));

    MascotaFotoService servicio(portal);
    const auto resultado =
        servicio.reemplazar(kMascota, QByteArrayLiteral("esto no es una imagen"));
    QVERIFY(!resultado.ok);
    QCOMPARE(contar(portal, QStringLiteral("mascota_fotos")), 0LL);
}

void TstMascotaFoto::noAmpliaImagenesPequenas()
{
    QByteArray jpeg;
    int ancho = 0;
    int alto = 0;
    QString error;
    QVERIFY2(MascotaFotoService::reencodar(
                 imagenPng(300, 200, QColor(90, 90, 90)), jpeg, ancho, alto, &error),
             qPrintable(error));
    QCOMPARE(ancho, 300);
    QCOMPARE(alto, 200);
    QVERIFY(jpeg.startsWith(QByteArrayLiteral("\xFF\xD8\xFF")));
}

void TstMascotaFoto::exigeMascotaDelPortal()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor portal(nuevaConexion(QStringLiteral("foto_ajena")),
                          dir.filePath(QStringLiteral("portal.db")));
    QVERIFY(prepararPortal(portal));

    MascotaFotoService servicio(portal);
    const auto resultado = servicio.reemplazar(
        QStringLiteral("99999999-9999-4999-8999-999999999999"),
        imagenPng(100, 100, QColor(1, 2, 3)));
    QVERIFY(!resultado.ok);
    QCOMPARE(contar(portal, QStringLiteral("mascota_fotos")), 0LL);
}

QTEST_GUILESS_MAIN(TstMascotaFoto)
#include "tst_mascota_foto.moc"
