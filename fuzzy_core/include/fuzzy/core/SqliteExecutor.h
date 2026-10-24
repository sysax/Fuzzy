#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QSqlDatabase>
#include <QString>

namespace fuzzy::core {

// Executor sobre SQLite local (QSqlDatabase). Lo usan escritorio y móvil.
class SqliteExecutor final : public IQueryExecutor
{
public:
    SqliteExecutor(const QString& connection_name = QStringLiteral("fuzzy_core"),
                   const QString& database_path = QString());
    ~SqliteExecutor() override = default;

    ExecResult execute(const QString& sql, const std::vector<QVariant>& params = {}) override;

private:
    QString connection_name_;
    QString database_path_;
};

} // namespace fuzzy::core
