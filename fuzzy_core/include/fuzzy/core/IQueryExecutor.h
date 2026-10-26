#pragma once

#include <QString>
#include <vector>

#include <QVariant>

namespace fuzzy::core {

// Resultado de una ejecución de sentencia SQL.
struct ExecResult
{
    // La sentencia se ejecutó sin error en el transporte. `affected` puede ser
    // 0 en un UPDATE que no cambió filas o en un ON CONFLICT que no aplicó.
    bool ok = false;
    // Filas afectadas por la última operación (INSERT/UPDATE/DELETE).
    long long affected = 0;
    // Id de inserción generado (INSERT), si aplica.
    QVariant last_insert_id;
};

// Fila devuelta por una consulta de lectura.
struct QueryRow
{
    // Valores de la fila; el índice es la posición de la columna.
    std::vector<QVariant> values;
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

    // Ejecuta una sentencia de lectura (SELECT) con parámetros posicionales (?).
    // Devuelve las filas resultantes.
    virtual std::vector<QueryRow> query(const QString& sql, const std::vector<QVariant>& params = {}) = 0;
};

} // namespace fuzzy::core
