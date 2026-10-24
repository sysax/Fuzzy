#include "fuzzy/core/SqliteExecutor.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

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
            query.bindValue(i + 1, params[static_cast<size_t>(i)]);
        }
        if (!query.exec()) {
            return result;
        }
    }

    result.affected = query.numRowsAffected();
    result.last_insert_id = query.lastInsertId();
    return result;
}

} // namespace fuzzy::core
