#include "fuzzy/core/FileStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>

namespace fuzzy::core {

namespace {

// Un segmento empieza por letra o número: así se rechazan `.`, `..` y los
// archivos ocultos sin necesidad de normalizar la ruta.
const QRegularExpression& patronSegmento()
{
    static const QRegularExpression patron(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]*$"));
    return patron;
}

void asignarError(QString* error, const QString& texto)
{
    if (error != nullptr) {
        *error = texto;
    }
}

} // namespace

FileStore::FileStore(QString raiz) : raiz_(std::move(raiz))
{
}

bool FileStore::rutaRelativaValida(const QString& rutaRelativa)
{
    if (rutaRelativa.isEmpty() || rutaRelativa.size() > 512) {
        return false;
    }
    if (rutaRelativa.contains(QChar(u'\0')) || rutaRelativa.contains(QLatin1Char('\\'))) {
        return false;
    }
    if (QDir::isAbsolutePath(rutaRelativa)) {
        return false;
    }

    const QStringList segmentos = rutaRelativa.split(QLatin1Char('/'), Qt::KeepEmptyParts);
    if (segmentos.isEmpty()) {
        return false;
    }
    for (const QString& segmento : segmentos) {
        if (!patronSegmento().match(segmento).hasMatch()) {
            return false;
        }
    }
    return true;
}

QString FileStore::rutaAbsoluta(const QString& rutaRelativa) const
{
    if (raiz_.isEmpty() || !rutaRelativaValida(rutaRelativa)) {
        return {};
    }
    return QDir(raiz_).absoluteFilePath(rutaRelativa);
}

bool FileStore::guardar(const QString& rutaRelativa, const QByteArray& datos,
                        QString* error)
{
    const QString destino = rutaAbsoluta(rutaRelativa);
    if (destino.isEmpty()) {
        asignarError(error, QStringLiteral("ruta de almacenamiento no válida"));
        return false;
    }
    if (!QDir().mkpath(QFileInfo(destino).absolutePath())) {
        asignarError(error, QStringLiteral("no se pudo crear el directorio de destino"));
        return false;
    }

    QSaveFile archivo(destino);
    if (!archivo.open(QIODevice::WriteOnly)) {
        asignarError(error, QStringLiteral("no se pudo abrir el archivo de destino"));
        return false;
    }
    if (archivo.write(datos) != datos.size()) {
        asignarError(error, QStringLiteral("no se pudo escribir el archivo completo"));
        archivo.cancelWriting();
        return false;
    }
    if (!archivo.commit()) {
        asignarError(error, QStringLiteral("no se pudo confirmar el archivo"));
        return false;
    }

    // Material clínico: solo el dueño del proceso puede leerlo o escribirlo.
    QFile::setPermissions(destino, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

bool FileStore::leer(const QString& rutaRelativa, QByteArray& datos,
                     QString* error) const
{
    const QString ruta = rutaAbsoluta(rutaRelativa);
    if (ruta.isEmpty()) {
        asignarError(error, QStringLiteral("ruta de almacenamiento no válida"));
        return false;
    }

    QFile archivo(ruta);
    if (!archivo.open(QIODevice::ReadOnly)) {
        asignarError(error, QStringLiteral("no se pudo abrir el archivo"));
        return false;
    }
    datos = archivo.readAll();
    if (archivo.error() != QFileDevice::NoError) {
        asignarError(error, QStringLiteral("no se pudo leer el archivo"));
        return false;
    }
    return true;
}

bool FileStore::existe(const QString& rutaRelativa) const
{
    const QString ruta = rutaAbsoluta(rutaRelativa);
    return !ruta.isEmpty() && QFileInfo(ruta).isFile();
}

bool FileStore::eliminar(const QString& rutaRelativa, QString* error)
{
    const QString ruta = rutaAbsoluta(rutaRelativa);
    if (ruta.isEmpty()) {
        asignarError(error, QStringLiteral("ruta de almacenamiento no válida"));
        return false;
    }

    QFile archivo(ruta);
    if (!archivo.exists()) {
        return true;
    }
    if (!archivo.remove()) {
        asignarError(error, QStringLiteral("no se pudo borrar el archivo"));
        return false;
    }
    return true;
}

} // namespace fuzzy::core
