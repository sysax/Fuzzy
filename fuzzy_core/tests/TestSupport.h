#pragma once

#include "fuzzy/core/IQueryExecutor.h"
#include "fuzzy/core/MigrationRunner.h"
#include "fuzzy/core/SqliteExecutor.h"

#include <QString>
#include <QVariant>

// Los tests usan el contrato sin calificar en cada archivo.
using fuzzy::core::IQueryExecutor;
using fuzzy::core::MigrationRunner;

namespace fuzzy::test {

// Ruta absoluta definida por CMake; los tests aplican el esquema real.
inline QString migracionesDir()
{
    return QStringLiteral(FUZZY_MIGRATIONS_DIR);
}

inline bool aplicarEsquema(IQueryExecutor& executor, const QString& base)
{
    const MigrationRunner runner(migracionesDir() + QStringLiteral("/") + base);
    return runner.valido() && runner.aplicar(executor);
}

inline QString nuevaConexion(const QString& etiqueta)
{
    static int siguiente = 0;
    return QStringLiteral("fase5_%1_%2").arg(etiqueta).arg(++siguiente);
}

inline qint64 contar(IQueryExecutor& executor, const QString& tabla)
{
    const auto filas =
        executor.query(QStringLiteral("SELECT count(*) FROM %1").arg(tabla));
    if (filas.empty() || filas.front().values.empty()) {
        return -1;
    }
    return filas.front().values.front().toLongLong();
}

inline bool existeTabla(IQueryExecutor& executor, const QString& tabla)
{
    const auto filas = executor.query(
        QStringLiteral("SELECT name FROM sqlite_master WHERE type = 'table' AND name = ?"),
        {QVariant(tabla)});
    return filas.size() == 1;
}

inline bool existeColumna(IQueryExecutor& executor, const QString& tabla,
                          const QString& columna)
{
    const auto filas = executor.query(
        QStringLiteral("SELECT name FROM pragma_table_info(?)"), {QVariant(tabla)});
    for (const auto& fila : filas) {
        if (!fila.values.empty() && fila.values.front().toString() == columna) {
            return true;
        }
    }
    return false;
}

} // namespace fuzzy::test
