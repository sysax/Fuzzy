#include "fuzzy/core/Uuid.h"

#include <QUuid>

namespace fuzzy::core {

QString uuidDeterminista(const QString& semilla)
{
    // Espacio de nombres propio del producto para no colisionar con UUID v5
    // de otros dominios (DNS, URL, etc.).
    static const QUuid espacio = QUuid::fromString(
        QStringLiteral("6f757a7a-1d1a-5e5f-9a2b-7c1d0e5f0001"));
    return QUuid::createUuidV5(espacio, semilla).toString(QUuid::WithoutBraces);
}

QString uuidNuevo()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

} // namespace fuzzy::core
