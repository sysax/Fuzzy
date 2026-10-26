#pragma once

#include <QString>

namespace fuzzy::core {

// UUID determinista (v5) para operaciones idempotentes: reintentar la misma
// semilla produce siempre el mismo uuid y no duplica filas.
QString uuidDeterminista(const QString& semilla);

} // namespace fuzzy::core
