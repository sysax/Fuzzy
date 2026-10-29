#include "fuzzy/core/MascotaFotoService.h"

#include "fuzzy/core/Uuid.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QPainter>
#include <QVariant>

namespace fuzzy::core {

namespace {

QString sha256Hex(const QByteArray& datos)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(datos, QCryptographicHash::Sha256).toHex());
}

void asignarError(QString* error, const QString& texto)
{
    if (error != nullptr) {
        *error = texto;
    }
}

QImage sinTransparencia(const QImage& imagen)
{
    if (!imagen.hasAlphaChannel()) {
        return imagen;
    }

    QImage fondo(imagen.size(), QImage::Format_RGB32);
    fondo.fill(Qt::white);
    QPainter pincel(&fondo);
    pincel.drawImage(0, 0, imagen);
    pincel.end();
    return fondo;
}

} // namespace

MascotaFotoService::MascotaFotoService(IQueryExecutor& portal) : portal_(portal)
{
}

bool MascotaFotoService::reencodar(const QByteArray& original, QByteArray& jpeg,
                                   int& ancho, int& alto, QString* error)
{
    jpeg.clear();
    ancho = 0;
    alto = 0;

    if (original.isEmpty()) {
        asignarError(error, QStringLiteral("la foto está vacía"));
        return false;
    }
    if (original.size() > kBytesMaximos) {
        asignarError(error, QStringLiteral("la foto supera 2 MB"));
        return false;
    }

    QBuffer entrada;
    entrada.setData(original);
    if (!entrada.open(QIODevice::ReadOnly)) {
        asignarError(error, QStringLiteral("no se pudo leer la foto"));
        return false;
    }

    QImageReader lector(&entrada);
    lector.setDecideFormatFromContent(true);
    lector.setAutoTransform(true);

    const QSize declarado = lector.size();
    if (declarado.isValid()
        && static_cast<qint64>(declarado.width()) * declarado.height() > kPixelesMaximos) {
        asignarError(error, QStringLiteral("la foto tiene demasiados píxeles"));
        return false;
    }

    QImage imagen = lector.read();
    if (imagen.isNull()) {
        asignarError(error, QStringLiteral("la foto no es una imagen válida"));
        return false;
    }
    if (static_cast<qint64>(imagen.width()) * imagen.height() > kPixelesMaximos) {
        asignarError(error, QStringLiteral("la foto tiene demasiados píxeles"));
        return false;
    }

    if (imagen.width() > kLadoMaximo || imagen.height() > kLadoMaximo) {
        imagen = imagen.scaled(kLadoMaximo, kLadoMaximo, Qt::KeepAspectRatio,
                               Qt::SmoothTransformation);
    }
    if (imagen.isNull()) {
        asignarError(error, QStringLiteral("no se pudo escalar la foto"));
        return false;
    }
    imagen = sinTransparencia(imagen);

    // Baja la calidad solo si el JPEG resultante supera el límite. A 1024 px
    // la primera pasada suele bastar; el bucle existe por si acaso.
    for (const int calidad : {85, 75, 65, 55, 45}) {
        QByteArray salida;
        QBuffer destino(&salida);
        if (!destino.open(QIODevice::WriteOnly)) {
            asignarError(error, QStringLiteral("no se pudo preparar el JPEG"));
            return false;
        }

        QImageWriter escritor(&destino, "jpeg");
        escritor.setQuality(calidad);
        escritor.setOptimizedWrite(true);
        if (!escritor.write(imagen)) {
            asignarError(error, QStringLiteral("no se pudo reencodar la foto: %1")
                                    .arg(escritor.errorString()));
            return false;
        }
        destino.close();

        if (salida.size() <= kBytesMaximos) {
            jpeg = salida;
            ancho = imagen.width();
            alto = imagen.height();
            return true;
        }
    }

    asignarError(error, QStringLiteral("la foto reencodada supera 2 MB"));
    return false;
}

ResultadoFoto MascotaFotoService::reemplazar(const QString& mascota_uuid,
                                             const QByteArray& original)
{
    ResultadoFoto resultado;

    if (mascota_uuid.isEmpty()) {
        resultado.error = QStringLiteral("falta la mascota de la foto");
        return resultado;
    }

    const auto mascota = portal_.query(
        QStringLiteral("SELECT id FROM mascotas WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(mascota_uuid)});
    if (mascota.size() != 1) {
        resultado.error = QStringLiteral("la mascota no existe en el portal");
        return resultado;
    }

    QByteArray jpeg;
    int ancho = 0;
    int alto = 0;
    QString error;
    if (!reencodar(original, jpeg, ancho, alto, &error)) {
        resultado.error = error;
        return resultado;
    }

    // La fila existente conserva su uuid: el reemplazo es un UPDATE, no una
    // fila nueva. Si es la primera foto, el uuid se deriva de la mascota para
    // que un reintento no duplique la fila.
    const auto existente = portal_.query(
        QStringLiteral("SELECT uuid FROM mascota_fotos WHERE mascota_uuid = ?"),
        {QVariant(mascota_uuid)});
    const QString uuid = existente.empty()
                             ? uuidDeterminista(QStringLiteral("mascota_foto:%1").arg(mascota_uuid))
                             : existente.front().values.front().toString();
    const QString sha256 = sha256Hex(jpeg);
    const qint64 ahora = QDateTime::currentMSecsSinceEpoch();

    // En SQLite local, secure_delete borra el contenido liberado por el
    // UPDATE anterior. En Turso el PRAGMA es inocuo si no aplica.
    portal_.execute(QStringLiteral("PRAGMA secure_delete = ON"));

    const ExecResult escritura = portal_.execute(
        QStringLiteral("INSERT INTO mascota_fotos "
                       "(uuid, mascota_uuid, foto, mime, sha256, ancho, alto, "
                       " actualizada_at, created_at, updated_at) "
                       "VALUES (?, ?, ?, 'image/jpeg', ?, ?, ?, ?, ?, ?) "
                       "ON CONFLICT(mascota_uuid) DO UPDATE SET "
                       "foto = excluded.foto, "
                       "mime = excluded.mime, "
                       "sha256 = excluded.sha256, "
                       "ancho = excluded.ancho, "
                       "alto = excluded.alto, "
                       "actualizada_at = excluded.actualizada_at, "
                       "updated_at = excluded.updated_at"),
        {QVariant(uuid), QVariant(mascota_uuid), QVariant(jpeg), QVariant(sha256),
         QVariant::fromValue(ancho), QVariant::fromValue(alto),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora),
         QVariant::fromValue(ahora)});
    if (!escritura.ok) {
        resultado.error = QStringLiteral("no se pudo guardar la foto");
        return resultado;
    }

    const auto comprobacion = portal_.query(
        QStringLiteral("SELECT uuid, sha256, length(foto), ancho, alto "
                       "FROM mascota_fotos WHERE mascota_uuid = ?"),
        {QVariant(mascota_uuid)});
    if (comprobacion.size() != 1 || comprobacion.front().values.size() != 5) {
        resultado.error = QStringLiteral("el reemplazo dejó filas duplicadas");
        return resultado;
    }
    const auto& fila = comprobacion.front().values;
    if (fila.at(1).toString() != sha256 || fila.at(2).toLongLong() != jpeg.size()) {
        resultado.error = QStringLiteral("la foto guardada no coincide con la reencodada");
        return resultado;
    }

    resultado.ok = true;
    resultado.uuid = fila.at(0).toString();
    resultado.sha256 = sha256;
    resultado.ancho = fila.at(3).toInt();
    resultado.alto = fila.at(4).toInt();
    resultado.bytes = jpeg.size();
    return resultado;
}

} // namespace fuzzy::core
