#include "fuzzy/core/PortalBaseStore.h"

#include "fuzzy/core/SqliteExecutor.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>

namespace fuzzy::core {

namespace {

bool esUuid(const QString& valor)
{
    static const QRegularExpression patron(
        QStringLiteral("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-"
                       "[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"));
    return patron.match(valor).hasMatch();
}

// Varias capas (proyector, recolector, provisioner) pueden abrir la misma
// base a la vez. Cada ejecutor necesita su propia conexión: reutilizar el
// nombre hace que QSqlDatabase reemplace la anterior y deje colgado al que
// todavía la estaba usando.
QString nombreConexion(const QString& etiqueta, const QString& portal_uuid)
{
    static int siguiente = 0;
    return QStringLiteral("fuzzy_portal_%1_%2_%3")
        .arg(etiqueta, portal_uuid)
        .arg(++siguiente);
}

} // namespace

LocalPortalBaseStore::LocalPortalBaseStore(QString directorio)
    : directorio_(std::move(directorio))
{
}

QString LocalPortalBaseStore::rutaDe(const QString& portal_uuid) const
{
    if (directorio_.isEmpty() || !esUuid(portal_uuid)) {
        return {};
    }
    return QDir(directorio_).filePath(
        QStringLiteral("fuzzy-portal-%1.db").arg(portal_uuid));
}

bool LocalPortalBaseStore::crear(const QString& portal_uuid)
{
    const QString ruta = rutaDe(portal_uuid);
    if (ruta.isEmpty()) {
        return false;
    }
    if (QFile::exists(ruta)) {
        return false;
    }
    if (!QDir().mkpath(directorio_)) {
        return false;
    }

    // Abrir con QSQLITE crea el archivo; el ejecutor se cierra al salir.
    SqliteExecutor creador(nombreConexion(QStringLiteral("crear"), portal_uuid), ruta);
    return QFile::exists(ruta);
}

std::unique_ptr<IQueryExecutor> LocalPortalBaseStore::abrir(const QString& portal_uuid)
{
    const QString ruta = rutaDe(portal_uuid);
    if (ruta.isEmpty() || !QFile::exists(ruta)) {
        return nullptr;
    }
    return std::make_unique<SqliteExecutor>(
        nombreConexion(QStringLiteral("abrir"), portal_uuid), ruta);
}

bool LocalPortalBaseStore::existe(const QString& portal_uuid) const
{
    const QString ruta = rutaDe(portal_uuid);
    return !ruta.isEmpty() && QFile::exists(ruta);
}

bool LocalPortalBaseStore::eliminar(const QString& portal_uuid)
{
    const QString ruta = rutaDe(portal_uuid);
    if (ruta.isEmpty()) {
        return false;
    }

    bool ok = true;
    const QStringList sufijos = {QString(), QStringLiteral("-wal"), QStringLiteral("-shm")};
    for (const QString& sufijo : sufijos) {
        QFile archivo(ruta + sufijo);
        if (archivo.exists() && !archivo.remove()) {
            ok = false;
        }
    }

    return ok && !QFile::exists(ruta);
}

} // namespace fuzzy::core
