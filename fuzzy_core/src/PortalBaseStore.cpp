#include "fuzzy/core/PortalBaseStore.h"

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
