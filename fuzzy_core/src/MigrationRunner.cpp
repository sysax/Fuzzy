#include "fuzzy/core/MigrationRunner.h"

#include "fuzzy/core/SqlScript.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSet>

// Debe estar en el espacio de nombres global: Q_INIT_RESOURCE declara la
// función generada por rcc con enlace externo.
static bool fuzzyInicializarRecursosMigraciones()
{
    Q_INIT_RESOURCE(migraciones);
    return true;
}

namespace fuzzy::core {

namespace {

const QString kTablaHistorial = QStringLiteral("_fuzzy_migraciones");

QString leerArchivo(const QString& ruta)
{
    QFile archivo(ruta);
    if (!archivo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(archivo.readAll());
}

QString ahoraMs()
{
    return QStringLiteral("CAST(strftime('%s', 'now') AS INTEGER) * 1000");
}

} // namespace

MigrationRunner::MigrationRunner(QString directorio)
    : directorio_(std::move(directorio))
{
    cargar();
}

MigrationRunner MigrationRunner::portal()
{
    // Fuerza la inclusión del objeto de recursos en la biblioteca estática.
    static const bool recursos = fuzzyInicializarRecursosMigraciones();
    Q_UNUSED(recursos);

    QString directorio = QStringLiteral(":/fuzzy/migrations/portal");
    if (!QDir(directorio).exists()) {
        const QByteArray alterno = qgetenv("FUZZY_MIGRATIONS_DIR");
        if (!alterno.isEmpty()) {
            directorio = QString::fromLocal8Bit(alterno) + QStringLiteral("/portal");
        }
    }
    return MigrationRunner(directorio);
}

void MigrationRunner::cargar()
{
    migraciones_.clear();
    const QDir dir(directorio_);
    if (!dir.exists()) {
        return;
    }

    const QStringList archivos = dir.entryList({QStringLiteral("*.sql")},
                                               QDir::Files, QDir::Name);
    for (const QString& archivo : archivos) {
        const QString ruta = dir.filePath(archivo);
        const QString sql = leerArchivo(ruta);
        if (sql.trimmed().isEmpty()) {
            continue;
        }
        MigracionSql migracion;
        migracion.version = archivo.section(QLatin1Char('_'), 0, 0);
        migracion.nombre = archivo;
        migracion.sql = sql;
        migraciones_.append(migracion);
    }
}

QString MigrationRunner::versionActual() const
{
    return migraciones_.isEmpty() ? QString() : migraciones_.last().version;
}

QStringList MigrationRunner::versiones() const
{
    QStringList versiones;
    for (const MigracionSql& migracion : migraciones_) {
        versiones.append(migracion.version);
    }
    return versiones;
}

bool MigrationRunner::aplicar(IQueryExecutor& executor) const
{
    if (migraciones_.isEmpty()) {
        return false;
    }

    ExecResult historial = executor.execute(
        QStringLiteral("CREATE TABLE IF NOT EXISTS %1 ("
                       "version TEXT PRIMARY KEY, "
                       "nombre TEXT NOT NULL, "
                       "aplicada_at INTEGER NOT NULL)")
            .arg(kTablaHistorial));
    if (!historial.ok) {
        return false;
    }

    const auto aplicadas = executor.query(
        QStringLiteral("SELECT version FROM %1").arg(kTablaHistorial));
    QSet<QString> versionesAplicadas;
    for (const QueryRow& fila : aplicadas) {
        if (!fila.values.empty()) {
            versionesAplicadas.insert(fila.values.front().toString());
        }
    }

    for (const MigracionSql& migracion : migraciones_) {
        if (versionesAplicadas.contains(migracion.version)) {
            continue;
        }

        if (!executor.execute(QStringLiteral("BEGIN")).ok) {
            return false;
        }

        bool ok = true;
        for (const QString& sentencia : sentenciasSql(migracion.sql)) {
            if (!executor.execute(sentencia).ok) {
                ok = false;
                break;
            }
        }
        if (!ok) {
            executor.execute(QStringLiteral("ROLLBACK"));
            return false;
        }

        const ExecResult registrada = executor.execute(
            QStringLiteral("INSERT INTO %1 (version, nombre, aplicada_at) "
                           "VALUES (?, ?, %2)")
                .arg(kTablaHistorial, ahoraMs()),
            {QVariant(migracion.version), QVariant(migracion.nombre)});
        if (!registrada.ok) {
            executor.execute(QStringLiteral("ROLLBACK"));
            return false;
        }
        if (!executor.execute(QStringLiteral("COMMIT")).ok) {
            executor.execute(QStringLiteral("ROLLBACK"));
            return false;
        }
    }

    return true;
}

} // namespace fuzzy::core
