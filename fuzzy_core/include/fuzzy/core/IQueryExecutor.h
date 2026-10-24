#pragma once

#include <QString>
#include <vector>

#include <QVariant>

namespace fuzzy::core {

// Resultado de una ejecución de sentencia SQL.
struct ExecResult
{
    // Filas afectadas por la última operación (INSERT/UPDATE/DELETE).
    long long affected = 0;
    // Id de inserción generado (INSERT), si aplica.
    QVariant last_insert_id;
};

// Contrato de persistencia del dominio. El dominio (servicios, DAOs, Politicas)
// depende de esta abstracción y no conoce el transporte: SQLite local
// (desktop/móvil) o Turso (sync/proyección).
class IQueryExecutor
{
public:
    virtual ~IQueryExecutor() = default;

    // Ejecuta una sentencia SQL (dialecto SQLite) con parámetros posicionales (?).
    virtual ExecResult execute(const QString& sql, const std::vector<QVariant>& params = {}) = 0;
};

} // namespace fuzzy::core
