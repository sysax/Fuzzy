#include "fuzzy/core/OperadorService.h"

#include "fuzzy/core/MigrationRunner.h"
#include "fuzzy/core/PasswordHasher.h"
#include "fuzzy/core/SqliteExecutor.h"
#include "fuzzy/core/Uuid.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>

#include <atomic>
#include <memory>

namespace fuzzy::core {

namespace {

constexpr qint64 kHorasPasswordTemporal = 72;

qint64 ahoraMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

QString textoDe(const QueryRow& fila, int indice)
{
    if (indice < 0 || indice >= static_cast<int>(fila.values.size())) {
        return {};
    }
    return fila.values[static_cast<size_t>(indice)].toString();
}

QString escaparSql(const QString& texto)
{
    QString copia = texto;
    copia.replace(QLatin1Char('\''), QStringLiteral("''"));
    return copia;
}

QString sha256De(const QString& archivo)
{
    QFile entrada(archivo);
    if (!entrada.open(QIODevice::ReadOnly)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&entrada)) {
        return {};
    }
    return QString::fromLatin1(hash.result().toHex());
}

QString nuevaConexion(const QString& etiqueta)
{
    static std::atomic<int> siguiente{0};
    return QStringLiteral("operador_%1_%2_%3")
        .arg(etiqueta)
        .arg(QCoreApplication::applicationPid())
        .arg(++siguiente);
}

std::unique_ptr<SqliteExecutor> abrir(const QString& ruta, const QString& etiqueta)
{
    return std::make_unique<SqliteExecutor>(nuevaConexion(etiqueta), ruta);
}

// Devuelve "ok" o el detalle del primer problema. Vacío significa que la
// consulta de integridad no se pudo ejecutar.
QString integridadDe(IQueryExecutor& db, bool comprobarForaneas, QString* error)
{
    const auto filas = db.query(QStringLiteral("PRAGMA integrity_check"));
    if (filas.empty() || filas.front().values.empty()) {
        if (error != nullptr) {
            *error = QStringLiteral("no se pudo ejecutar PRAGMA integrity_check");
        }
        return {};
    }
    const QString resultado = filas.front().values.front().toString();
    if (resultado.compare(QStringLiteral("ok"), Qt::CaseInsensitive) != 0) {
        if (error != nullptr) {
            *error = QStringLiteral("integridad: %1").arg(resultado);
        }
        return resultado;
    }

    if (comprobarForaneas) {
        const auto rotas = db.query(QStringLiteral("PRAGMA foreign_key_check"));
        if (!rotas.empty()) {
            if (error != nullptr) {
                *error = QStringLiteral("hay %1 referencia(s) foránea(s) rota(s)")
                             .arg(rotas.size());
            }
            return QStringLiteral("foreign_key_check");
        }
    }
    return QStringLiteral("ok");
}

qint64 contar(IQueryExecutor& db, const QString& sql)
{
    const auto filas = db.query(sql);
    if (filas.empty() || filas.front().values.empty()) {
        return 0;
    }
    return filas.front().values.front().toLongLong();
}

QString versionDe(IQueryExecutor& db)
{
    const auto filas =
        db.query(QStringLiteral("SELECT max(version) FROM _fuzzy_migraciones"));
    if (filas.empty() || filas.front().values.empty()) {
        return {};
    }
    return filas.front().values.front().toString();
}

} // namespace

OperadorService::OperadorService(QString rutaBase)
    : ruta_(QFileInfo(std::move(rutaBase)).absoluteFilePath())
{
}

EstadoOperador OperadorService::estado() const
{
    EstadoOperador estado;
    estado.ruta = ruta_;

    const QFileInfo info(ruta_);
    if (!info.exists() || !info.isFile()) {
        estado.error = QStringLiteral("no existe la base: %1").arg(ruta_);
        return estado;
    }
    estado.bytes = info.size();

    auto db = abrir(ruta_, QStringLiteral("estado"));
    estado.integridad = integridadDe(*db, false, &estado.error);
    if (estado.integridad.isEmpty()) {
        return estado;
    }
    estado.integridadOk =
        estado.integridad.compare(QStringLiteral("ok"), Qt::CaseInsensitive) == 0;
    if (!estado.integridadOk) {
        estado.error = QStringLiteral("la base no pasó la verificación de integridad");
        return estado;
    }

    const auto clinica = db->query(
        QStringLiteral("SELECT uuid, nombre FROM clinicas ORDER BY id LIMIT 1"));
    if (!clinica.empty()) {
        estado.clinicaUuid = textoDe(clinica.front(), 0);
        estado.clinicaNombre = textoDe(clinica.front(), 1);
    }

    estado.versionEsquema = versionDe(*db);
    estado.usuarios = contar(
        *db, QStringLiteral("SELECT count(*) FROM usuarios WHERE deleted_at IS NULL"));
    estado.clientes = contar(
        *db, QStringLiteral("SELECT count(*) FROM clientes WHERE deleted_at IS NULL"));
    estado.mascotas = contar(
        *db, QStringLiteral("SELECT count(*) FROM mascotas WHERE deleted_at IS NULL"));
    estado.portales = contar(*db, QStringLiteral("SELECT count(*) FROM portales"));

    estado.ok = true;
    return estado;
}

ResultadoRespaldo OperadorService::respaldar(const QString& destino) const
{
    ResultadoRespaldo resultado;

    if (!QFileInfo::exists(ruta_)) {
        resultado.error = QStringLiteral("no existe la base: %1").arg(ruta_);
        return resultado;
    }
    if (destino.trimmed().isEmpty()) {
        resultado.error = QStringLiteral("falta la ruta del respaldo");
        return resultado;
    }

    const QFileInfo destinoInfo(QFileInfo(destino).absoluteFilePath());
    if (destinoInfo.exists()) {
        resultado.error =
            QStringLiteral("el respaldo ya existe: %1").arg(destinoInfo.absoluteFilePath());
        return resultado;
    }
    if (!QDir().mkpath(destinoInfo.absolutePath())) {
        resultado.error = QStringLiteral("no se pudo crear el directorio del respaldo");
        return resultado;
    }

    {
        auto db = abrir(ruta_, QStringLiteral("respaldo"));
        const QString sql = QStringLiteral("VACUUM INTO '%1'")
                                .arg(escaparSql(destinoInfo.absoluteFilePath()));
        if (!db->execute(sql).ok) {
            resultado.error = QStringLiteral("no se pudo crear el respaldo");
            return resultado;
        }
    }

    const QFileInfo copia(destinoInfo.absoluteFilePath());
    if (!copia.exists() || copia.size() <= 0) {
        resultado.error = QStringLiteral("el respaldo quedó vacío");
        return resultado;
    }

    const QString sha = sha256De(copia.absoluteFilePath());
    if (sha.isEmpty()) {
        resultado.error = QStringLiteral("no se pudo calcular el checksum del respaldo");
        return resultado;
    }

    QFile checksum(copia.absoluteFilePath() + QStringLiteral(".sha256"));
    if (!checksum.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        resultado.error = QStringLiteral("no se pudo escribir el checksum");
        return resultado;
    }
    checksum.write(QStringLiteral("%1  %2\n")
                       .arg(sha, copia.fileName())
                       .toUtf8());
    checksum.close();

    QString errorVerificacion;
    if (!verificarRespaldo(copia.absoluteFilePath(), &errorVerificacion)) {
        resultado.error =
            QStringLiteral("el respaldo recién creado no es válido: %1")
                .arg(errorVerificacion);
        return resultado;
    }

    EstadoOperador estado;
    {
        auto db = abrir(ruta_, QStringLiteral("manifiesto"));
        const auto clinica = db->query(
            QStringLiteral("SELECT uuid, nombre FROM clinicas ORDER BY id LIMIT 1"));
        if (!clinica.empty()) {
            estado.clinicaUuid = textoDe(clinica.front(), 0);
            estado.clinicaNombre = textoDe(clinica.front(), 1);
        }
        estado.versionEsquema = versionDe(*db);
    }

    QJsonObject manifiesto;
    manifiesto.insert(QStringLiteral("formato"), 1);
    manifiesto.insert(QStringLiteral("ruta"), copia.absoluteFilePath());
    manifiesto.insert(QStringLiteral("sha256"), sha);
    manifiesto.insert(QStringLiteral("bytes"), static_cast<double>(copia.size()));
    manifiesto.insert(QStringLiteral("creado_at"), static_cast<double>(ahoraMs()));
    manifiesto.insert(QStringLiteral("version_esquema"), estado.versionEsquema);
    manifiesto.insert(QStringLiteral("clinica_uuid"), estado.clinicaUuid);
    manifiesto.insert(QStringLiteral("clinica"), estado.clinicaNombre);

    QFile archivoManifiesto(copia.absoluteFilePath() + QStringLiteral(".json"));
    if (!archivoManifiesto.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        resultado.error = QStringLiteral("no se pudo escribir el manifiesto");
        return resultado;
    }
    archivoManifiesto.write(QJsonDocument(manifiesto).toJson(QJsonDocument::Indented));
    archivoManifiesto.close();

    resultado.ok = true;
    resultado.ruta = copia.absoluteFilePath();
    resultado.sha256 = sha;
    resultado.manifiesto = archivoManifiesto.fileName();
    resultado.bytes = copia.size();
    resultado.creadoAt = ahoraMs();
    return resultado;
}

bool OperadorService::verificarRespaldo(const QString& archivo, QString* error) const
{
    const QFileInfo info(QFileInfo(archivo).absoluteFilePath());
    if (!info.exists() || !info.isFile()) {
        if (error != nullptr) {
            *error = QStringLiteral("no existe el respaldo: %1").arg(archivo);
        }
        return false;
    }

    QFile checksum(info.absoluteFilePath() + QStringLiteral(".sha256"));
    if (!checksum.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error != nullptr) {
            *error = QStringLiteral("falta el checksum del respaldo");
        }
        return false;
    }
    const QString linea = QString::fromUtf8(checksum.readLine()).trimmed();
    checksum.close();
    const QString esperado = linea.section(QLatin1Char(' '), 0, 0);
    if (esperado.size() != 64) {
        if (error != nullptr) {
            *error = QStringLiteral("el checksum del respaldo no es válido");
        }
        return false;
    }

    const QString real = sha256De(info.absoluteFilePath());
    if (real.compare(esperado, Qt::CaseInsensitive) != 0) {
        if (error != nullptr) {
            *error = QStringLiteral("el checksum del respaldo no coincide");
        }
        return false;
    }

    auto db = abrir(info.absoluteFilePath(), QStringLiteral("verificar_respaldo"));
    const QString integridad = integridadDe(*db, true, error);
    if (integridad.compare(QStringLiteral("ok"), Qt::CaseInsensitive) != 0) {
        if (error != nullptr && error->isEmpty()) {
            *error = QStringLiteral("el respaldo no pasó la verificación de integridad");
        }
        return false;
    }
    return true;
}

ResultadoRestauracion OperadorService::restaurar(const QString& origen, bool confirmado)
{
    ResultadoRestauracion resultado;

    if (!confirmado) {
        resultado.error =
            QStringLiteral("la restauración exige --confirmar y detener el escritorio");
        return resultado;
    }

    const QFileInfo fuente(QFileInfo(origen).absoluteFilePath());
    if (!fuente.exists() || !fuente.isFile()) {
        resultado.error = QStringLiteral("no existe el respaldo: %1").arg(origen);
        return resultado;
    }
    if (fuente.absoluteFilePath() == QFileInfo(ruta_).absoluteFilePath()) {
        resultado.error = QStringLiteral("el respaldo y la base son el mismo archivo");
        return resultado;
    }

    QString errorVerificacion;
    if (!verificarRespaldo(fuente.absoluteFilePath(), &errorVerificacion)) {
        resultado.error =
            QStringLiteral("el respaldo no es válido: %1").arg(errorVerificacion);
        return resultado;
    }

    if (QFileInfo::exists(ruta_)) {
        resultado.respaldoPrevio =
            QStringLiteral("%1.antes-de-restaurar-%2-%3")
                .arg(ruta_,
                     QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss")),
                     uuidNuevo().left(8));
        if (!QFile::copy(ruta_, resultado.respaldoPrevio)) {
            resultado.error =
                QStringLiteral("no se pudo guardar la base anterior antes de restaurar");
            resultado.respaldoPrevio.clear();
            return resultado;
        }
    }

    QFile::remove(ruta_);
    QFile::remove(ruta_ + QStringLiteral("-wal"));
    QFile::remove(ruta_ + QStringLiteral("-shm"));

    const QString temporal =
        QStringLiteral("%1.restaurando-%2").arg(ruta_, uuidNuevo().left(8));
    if (!QFile::copy(fuente.absoluteFilePath(), temporal)) {
        resultado.error = QStringLiteral("no se pudo copiar el respaldo");
        if (!resultado.respaldoPrevio.isEmpty()) {
            QFile::copy(resultado.respaldoPrevio, ruta_);
        }
        return resultado;
    }
    if (!QFile::rename(temporal, ruta_)) {
        QFile::remove(temporal);
        resultado.error = QStringLiteral("no se pudo reemplazar la base");
        if (!resultado.respaldoPrevio.isEmpty()) {
            QFile::copy(resultado.respaldoPrevio, ruta_);
        }
        return resultado;
    }

    QString errorIntegridad;
    QString integridad;
    {
        auto db = abrir(ruta_, QStringLiteral("restaurado"));
        integridad = integridadDe(*db, true, &errorIntegridad);
    }
    if (integridad.compare(QStringLiteral("ok"), Qt::CaseInsensitive) != 0) {
        resultado.error = QStringLiteral("la base restaurada no es válida: %1")
                              .arg(errorIntegridad);
        QFile::remove(ruta_);
        if (!resultado.respaldoPrevio.isEmpty()) {
            QFile::copy(resultado.respaldoPrevio, ruta_);
        }
        return resultado;
    }

    resultado.ok = true;
    resultado.ruta = ruta_;
    return resultado;
}

ResultadoRecuperacionAdmin OperadorService::recuperarAdmin(const QString& correo)
{
    ResultadoRecuperacionAdmin resultado;
    const QString correoLimpio = correo.trimmed().toLower();
    resultado.correo = correoLimpio;

    if (correoLimpio.isEmpty()) {
        resultado.error = QStringLiteral("falta el correo del administrador");
        return resultado;
    }
    if (!QFileInfo::exists(ruta_)) {
        resultado.error = QStringLiteral("no existe la base: %1").arg(ruta_);
        return resultado;
    }

    auto db = abrir(ruta_, QStringLiteral("recuperar_admin"));
    const auto filas = db->query(
        QStringLiteral("SELECT u.id, u.uuid, u.correo, u.activo, r.nombre_rol "
                       "FROM usuarios u JOIN roles r ON r.id = u.rol_id "
                       "WHERE lower(u.correo) = ? AND u.deleted_at IS NULL LIMIT 1"),
        {QVariant(correoLimpio)});
    if (filas.size() != 1) {
        resultado.error = QStringLiteral("no existe una cuenta con ese correo");
        return resultado;
    }

    const auto& fila = filas.front();
    const QString rol = textoDe(fila, 4);
    if (rol.compare(QStringLiteral("Admin"), Qt::CaseInsensitive) != 0) {
        resultado.error = QStringLiteral("la cuenta no tiene rol Admin");
        return resultado;
    }

    const qint64 usuarioId = fila.values.at(0).toLongLong();
    const QString usuarioUuid = textoDe(fila, 1);
    const QString password = PasswordHasher::passwordTemporal();
    const qint64 ahora = ahoraMs();

    if (!db->execute(QStringLiteral("BEGIN")).ok) {
        resultado.error = QStringLiteral("no se pudo iniciar la recuperación");
        return resultado;
    }

    const auto cambio = db->execute(
        QStringLiteral("UPDATE usuarios SET password_hash = ?, "
                       "debe_cambiar_password = 1, password_expira_at = ?, "
                       "activo = 1, updated_at = ? WHERE id = ?"),
        {QVariant(PasswordHasher::generar(password)),
         QVariant::fromValue(ahora + kHorasPasswordTemporal * 60 * 60 * 1000),
         QVariant::fromValue(ahora), QVariant::fromValue(usuarioId)});
    if (!cambio.ok) {
        db->execute(QStringLiteral("ROLLBACK"));
        resultado.error = QStringLiteral("no se pudo restablecer la contraseña");
        return resultado;
    }

    const auto auditoria = db->execute(
        QStringLiteral("INSERT INTO auditoria "
                       "(uuid, usuario_id, accion, entidad, entidad_uuid, detalle, "
                       " created_at, sync_state) "
                       "VALUES (?, NULL, 'recuperacion_admin_cli', 'usuarios', ?, ?, ?, "
                       "'pendiente')"),
        {QVariant(uuidNuevo()), QVariant(usuarioUuid),
         QVariant(QStringLiteral("Contraseña temporal emitida por la CLI de operador")),
         QVariant::fromValue(ahora)});
    if (!auditoria.ok) {
        db->execute(QStringLiteral("ROLLBACK"));
        resultado.error = QStringLiteral("no se pudo auditar la recuperación");
        return resultado;
    }

    if (!db->execute(QStringLiteral("COMMIT")).ok) {
        db->execute(QStringLiteral("ROLLBACK"));
        resultado.error = QStringLiteral("no se pudo confirmar la recuperación");
        return resultado;
    }

    resultado.ok = true;
    resultado.usuarioUuid = usuarioUuid;
    resultado.passwordTemporal = password;
    return resultado;
}

bool OperadorService::migrar(QString* error) const
{
    if (ruta_.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("falta la ruta de la base");
        }
        return false;
    }
    if (!QDir().mkpath(QFileInfo(ruta_).absolutePath())) {
        if (error != nullptr) {
            *error = QStringLiteral("no se pudo crear el directorio de la base");
        }
        return false;
    }

    const MigrationRunner runner = MigrationRunner::clinic();
    if (!runner.valido()) {
        if (error != nullptr) {
            *error = QStringLiteral("no hay migraciones de clínica disponibles");
        }
        return false;
    }

    auto db = abrir(ruta_, QStringLiteral("migrar"));
    if (!runner.aplicar(*db)) {
        if (error != nullptr) {
            *error = QStringLiteral("no se pudieron aplicar las migraciones");
        }
        return false;
    }
    return true;
}

} // namespace fuzzy::core
