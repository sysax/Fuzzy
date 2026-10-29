#include "fuzzy/core/GaleriaClinicaService.h"

#include "fuzzy/core/FileStore.h"
#include "fuzzy/core/Uuid.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QVariant>

namespace fuzzy::core {

namespace {

void asignarError(QString* error, const QString& texto)
{
    if (error != nullptr) {
        *error = texto;
    }
}

QString sha256Hex(const QByteArray& datos)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(datos, QCryptographicHash::Sha256).toHex());
}

// Solo formatos de documento o imagen que la clínica usa de verdad. El tipo
// se deduce del contenido, nunca de una extensión que pueda mentir.
QString mimeDetectado(const QByteArray& datos)
{
    if (datos.startsWith(QByteArrayLiteral("\x89PNG\r\n\x1a\n"))) {
        return QStringLiteral("image/png");
    }
    if (datos.size() >= 3 && static_cast<unsigned char>(datos.at(0)) == 0xFF
        && static_cast<unsigned char>(datos.at(1)) == 0xD8
        && static_cast<unsigned char>(datos.at(2)) == 0xFF) {
        return QStringLiteral("image/jpeg");
    }
    if (datos.size() >= 12 && datos.startsWith(QByteArrayLiteral("RIFF"))
        && datos.mid(8, 4) == QByteArrayLiteral("WEBP")) {
        return QStringLiteral("image/webp");
    }
    if (datos.startsWith(QByteArrayLiteral("%PDF-"))) {
        return QStringLiteral("application/pdf");
    }
    if (datos.size() >= 4
        && ((datos.at(0) == 'I' && datos.at(1) == 'I' && datos.at(2) == '*'
             && datos.at(3) == '\0')
            || (datos.at(0) == 'M' && datos.at(1) == 'M' && datos.at(2) == '\0'
                && datos.at(3) == '*'))) {
        return QStringLiteral("image/tiff");
    }
    return {};
}

QString extensionDe(const QString& mime)
{
    if (mime == QStringLiteral("image/png")) {
        return QStringLiteral("png");
    }
    if (mime == QStringLiteral("image/webp")) {
        return QStringLiteral("webp");
    }
    if (mime == QStringLiteral("application/pdf")) {
        return QStringLiteral("pdf");
    }
    if (mime == QStringLiteral("image/tiff")) {
        return QStringLiteral("tif");
    }
    return QStringLiteral("jpg");
}

bool tipoValido(const QString& tipo)
{
    return tipo == QStringLiteral("Foto") || tipo == QStringLiteral("Radiografia")
        || tipo == QStringLiteral("Ecografia") || tipo == QStringLiteral("Laboratorio")
        || tipo == QStringLiteral("Otro");
}

} // namespace

GaleriaClinicaService::GaleriaClinicaService(IQueryExecutor& clinica, FileStore& almacen)
    : clinica_(clinica), almacen_(almacen)
{
}

ResultadoImagen GaleriaClinicaService::guardar(const QString& mascota_uuid,
                                               const QString& titulo,
                                               const QString& tipo,
                                               const QByteArray& datos,
                                               const QString& visita_uuid,
                                               const QString& usuario_uuid)
{
    ResultadoImagen resultado;

    const QString tituloLimpio = titulo.trimmed();
    if (mascota_uuid.isEmpty()) {
        resultado.error = QStringLiteral("falta la mascota de la imagen");
        return resultado;
    }
    if (tituloLimpio.isEmpty()) {
        resultado.error = QStringLiteral("el título de la imagen no puede estar vacío");
        return resultado;
    }
    if (!tipoValido(tipo)) {
        resultado.error = QStringLiteral("tipo de imagen no válido");
        return resultado;
    }
    if (datos.isEmpty()) {
        resultado.error = QStringLiteral("la imagen está vacía");
        return resultado;
    }
    if (datos.size() > kBytesMaximos) {
        resultado.error = QStringLiteral("la imagen supera 10 MB");
        return resultado;
    }

    const QString mime = mimeDetectado(datos);
    if (mime.isEmpty()) {
        resultado.error = QStringLiteral("formato de imagen no permitido");
        return resultado;
    }

    const auto mascota = clinica_.query(
        QStringLiteral("SELECT id FROM mascotas WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(mascota_uuid)});
    if (mascota.size() != 1) {
        resultado.error = QStringLiteral("la mascota no existe en la clínica");
        return resultado;
    }
    const qint64 mascota_id = mascota.front().values.front().toLongLong();

    QVariant visitaId;
    if (!visita_uuid.isEmpty()) {
        const auto visita = clinica_.query(
            QStringLiteral("SELECT id FROM consultas_medicas "
                           "WHERE uuid = ? AND mascota_id = ?"),
            {QVariant(visita_uuid), QVariant::fromValue(mascota_id)});
        if (visita.size() != 1) {
            resultado.error = QStringLiteral("la visita no existe o no es de esa mascota");
            return resultado;
        }
        visitaId = visita.front().values.front();
    }

    QVariant usuarioId;
    if (!usuario_uuid.isEmpty()) {
        const auto usuario = clinica_.query(
            QStringLiteral("SELECT id FROM usuarios WHERE uuid = ? AND deleted_at IS NULL"),
            {QVariant(usuario_uuid)});
        if (usuario.size() != 1) {
            resultado.error = QStringLiteral("el usuario no existe");
            return resultado;
        }
        usuarioId = usuario.front().values.front();
    }

    const QString sha256 = sha256Hex(datos);
    const QString uuid = uuidDeterminista(
        QStringLiteral("mascota_galeria:%1:%2:%3:%4:%5")
            .arg(mascota_uuid, sha256, tituloLimpio, tipo, visita_uuid));
    const QString storagePath = QStringLiteral("%1/%2.%3")
                                    .arg(mascota_uuid, uuid, extensionDe(mime));

    const auto previa = clinica_.query(
        QStringLiteral("SELECT storage_path FROM mascota_galeria WHERE uuid = ?"),
        {QVariant(uuid)});
    resultado.existente = previa.size() == 1;

    const bool archivoExistia = almacen_.existe(storagePath);
    QString errorArchivo;
    if (!almacen_.guardar(storagePath, datos, &errorArchivo)) {
        resultado.error = errorArchivo;
        return resultado;
    }

    const qint64 ahora = QDateTime::currentMSecsSinceEpoch();
    const ExecResult insercion = clinica_.execute(
        QStringLiteral("INSERT INTO mascota_galeria "
                       "(uuid, mascota_id, titulo, tipo, storage_path, mime, sha256, "
                       " tamano, visita_id, usuario_id, created_at, updated_at, sync_state) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'sincronizado') "
                       "ON CONFLICT(uuid) DO NOTHING"),
        {QVariant(uuid), QVariant::fromValue(mascota_id), QVariant(tituloLimpio),
         QVariant(tipo), QVariant(storagePath), QVariant(mime), QVariant(sha256),
         QVariant::fromValue<qint64>(datos.size()), visitaId, usuarioId,
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!insercion.ok) {
        if (!archivoExistia) {
            almacen_.eliminar(storagePath);
        }
        resultado.error = QStringLiteral("no se pudo registrar la imagen");
        return resultado;
    }

    const auto fila = clinica_.query(
        QStringLiteral("SELECT storage_path, mime, sha256, tamano "
                       "FROM mascota_galeria WHERE uuid = ?"),
        {QVariant(uuid)});
    if (fila.size() != 1 || fila.front().values.size() != 4) {
        resultado.error = QStringLiteral("la imagen no quedó registrada");
        return resultado;
    }
    const auto& valores = fila.front().values;
    if (valores.at(2).toString() != sha256 || valores.at(3).toLongLong() != datos.size()) {
        resultado.error = QStringLiteral("los metadatos no coinciden con la imagen");
        return resultado;
    }

    resultado.ok = true;
    resultado.uuid = uuid;
    resultado.storagePath = valores.at(0).toString();
    resultado.mime = valores.at(1).toString();
    resultado.sha256 = sha256;
    resultado.bytes = datos.size();
    return resultado;
}

bool GaleriaClinicaService::eliminar(const QString& uuid, QString* error)
{
    if (uuid.isEmpty()) {
        asignarError(error, QStringLiteral("falta la imagen a borrar"));
        return false;
    }

    const auto fila = clinica_.query(
        QStringLiteral("SELECT storage_path FROM mascota_galeria WHERE uuid = ?"),
        {QVariant(uuid)});
    if (fila.size() != 1 || fila.front().values.empty()) {
        asignarError(error, QStringLiteral("la imagen no existe"));
        return false;
    }
    const QString storagePath = fila.front().values.front().toString();

    if (!clinica_.execute(QStringLiteral("BEGIN")).ok) {
        asignarError(error, QStringLiteral("no se pudo abrir la transacción"));
        return false;
    }
    if (!clinica_.execute(QStringLiteral("DELETE FROM mascota_galeria WHERE uuid = ?"),
                          {QVariant(uuid)})
             .ok) {
        clinica_.execute(QStringLiteral("ROLLBACK"));
        asignarError(error, QStringLiteral("no se pudo borrar el metadato"));
        return false;
    }

    QString errorArchivo;
    if (!almacen_.eliminar(storagePath, &errorArchivo)) {
        clinica_.execute(QStringLiteral("ROLLBACK"));
        asignarError(error, errorArchivo);
        return false;
    }

    if (!clinica_.execute(QStringLiteral("COMMIT")).ok) {
        clinica_.execute(QStringLiteral("ROLLBACK"));
        asignarError(error, QStringLiteral("no se pudo confirmar el borrado"));
        return false;
    }
    return true;
}

bool GaleriaClinicaService::leer(const QString& uuid, QByteArray& datos,
                                 QString* error) const
{
    if (uuid.isEmpty()) {
        asignarError(error, QStringLiteral("falta la imagen a leer"));
        return false;
    }

    const auto fila = clinica_.query(
        QStringLiteral("SELECT storage_path FROM mascota_galeria WHERE uuid = ?"),
        {QVariant(uuid)});
    if (fila.size() != 1 || fila.front().values.empty()) {
        asignarError(error, QStringLiteral("la imagen no existe"));
        return false;
    }
    return almacen_.leer(fila.front().values.front().toString(), datos, error);
}

} // namespace fuzzy::core
