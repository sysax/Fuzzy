#include "fuzzy/core/SqliteExecutor.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

namespace fuzzy::core {

SqliteExecutor::SqliteExecutor(const QString& connection_name, const QString& database_path)
    : connection_name_(connection_name), database_path_(database_path)
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection_name_);
    if (!database_path_.isEmpty()) {
        db.setDatabaseName(database_path_);
    }
    db.open();
}

ExecResult SqliteExecutor::execute(const QString& sql, const std::vector<QVariant>& params)
{
    ExecResult result;

    QSqlDatabase db = QSqlDatabase::database(connection_name_, true);
    if (!db.isOpen()) {
        return result;
    }

    QSqlQuery query(db);
    if (params.empty()) {
        if (!query.exec(sql)) {
            return result;
        }
    } else {
        query.prepare(sql);
        for (int i = 0; i < static_cast<int>(params.size()); ++i) {
            // QSqlQuery numera los parámetros posicionales desde 0.
            query.bindValue(i, params[static_cast<size_t>(i)]);
        }
        if (!query.exec()) {
            return result;
        }
    }

    result.affected = query.numRowsAffected();
    result.last_insert_id = query.lastInsertId();
    return result;
}

std::vector<QueryRow> SqliteExecutor::query(const QString& sql, const std::vector<QVariant>& params)
{
    std::vector<QueryRow> filas;

    QSqlDatabase db = QSqlDatabase::database(connection_name_, true);
    if (!db.isOpen()) {
        return filas;
    }

    QSqlQuery query(db);
    if (!params.empty()) {
        query.prepare(sql);
        for (int i = 0; i < static_cast<int>(params.size()); ++i) {
            // QSqlQuery numera los parámetros posicionales desde 0.
            query.bindValue(i, params[static_cast<size_t>(i)]);
        }
        if (!query.exec()) {
            return filas;
        }
    } else {
        if (!query.exec(sql)) {
            return filas;
        }
    }

    while (query.next()) {
        QueryRow fila;
        const int columnas = query.record().count();
        for (int i = 0; i < columnas; ++i) {
            fila.values.push_back(query.value(i));
        }
        filas.push_back(std::move(fila));
    }

    return filas;
}

} // namespace fuzzy::core
