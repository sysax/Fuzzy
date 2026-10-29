#include "fuzzy/core/FileStore.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

using fuzzy::core::FileStore;

class TstFileStore : public QObject
{
    Q_OBJECT

private slots:
    void guardaYLee();
    void reemplazaSinDejarTemporales();
    void rechazaRutasInseguras();
    void eliminarEsIdempotente();
};

void TstFileStore::guardaYLee()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileStore almacen(dir.path());

    QByteArray datos;
    for (int i = 0; i < 4096; ++i) {
        datos.append(static_cast<char>(i % 251));
    }

    const QString ruta = QStringLiteral("mascota/foto.jpg");
    QString error;
    QVERIFY2(almacen.guardar(ruta, datos, &error), qPrintable(error));
    QVERIFY(almacen.existe(ruta));

    const QString absoluta = almacen.rutaAbsoluta(ruta);
    QVERIFY(!absoluta.isEmpty());
    QVERIFY(absoluta.startsWith(QDir(dir.path()).absolutePath() + QLatin1Char('/')));
    QVERIFY(QFileInfo::exists(absoluta));

    QByteArray leidos;
    QVERIFY2(almacen.leer(ruta, leidos, &error), qPrintable(error));
    QCOMPARE(leidos, datos);

#ifdef Q_OS_UNIX
    const QFile::Permissions permisos = QFileInfo(absoluta).permissions();
    QVERIFY(permisos.testFlag(QFileDevice::ReadOwner));
    QVERIFY(permisos.testFlag(QFileDevice::WriteOwner));
    QVERIFY(!permisos.testFlag(QFileDevice::ReadOther));
#endif
}

void TstFileStore::reemplazaSinDejarTemporales()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileStore almacen(dir.path());

    const QString ruta = QStringLiteral("mascota/foto.jpg");
    QVERIFY(almacen.guardar(ruta, QByteArrayLiteral("primera")));
    QVERIFY(almacen.guardar(ruta, QByteArrayLiteral("segunda")));

    QByteArray leidos;
    QVERIFY(almacen.leer(ruta, leidos));
    QCOMPARE(leidos, QByteArrayLiteral("segunda"));

    int archivos = 0;
    QDirIterator it(dir.path(), QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        ++archivos;
    }
    QCOMPARE(archivos, 1);
}

void TstFileStore::rechazaRutasInseguras()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileStore almacen(dir.path());

    const QStringList rutas = {
        QString(),
        QStringLiteral("."),
        QStringLiteral(".."),
        QStringLiteral("../fuera.txt"),
        QStringLiteral("mascota/../../fuera.txt"),
        QStringLiteral("/etc/passwd"),
        QStringLiteral("C:/temporal/foto.jpg"),
        QStringLiteral("mascota\\foto.jpg"),
        QStringLiteral("mascota//foto.jpg"),
        QStringLiteral("mascota/.oculta"),
        QStringLiteral("mascota/ foto.jpg"),
    };

    for (const QString& ruta : rutas) {
        QVERIFY2(!almacen.rutaRelativaValida(ruta), qPrintable(ruta));
        QVERIFY(almacen.rutaAbsoluta(ruta).isEmpty());
        QString error;
        QVERIFY(!almacen.guardar(ruta, QByteArrayLiteral("x"), &error));
        QVERIFY(!error.isEmpty());
    }
}

void TstFileStore::eliminarEsIdempotente()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileStore almacen(dir.path());

    const QString ruta = QStringLiteral("mascota/foto.jpg");
    QVERIFY(almacen.guardar(ruta, QByteArrayLiteral("datos")));
    QVERIFY(almacen.eliminar(ruta));
    QVERIFY(!almacen.existe(ruta));
    QVERIFY(almacen.eliminar(ruta));
}

QTEST_MAIN(TstFileStore)
#include "tst_file_store.moc"
