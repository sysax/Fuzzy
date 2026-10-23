#include "fuzzy/core/Version.h"

namespace fuzzy::core {

QString version()
{
    return QStringLiteral(FUZZY_VERSION);
}

} // namespace fuzzy::core
