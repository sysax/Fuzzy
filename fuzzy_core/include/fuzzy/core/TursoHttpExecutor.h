#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QNetworkAccessManager>
#include <QString>

class QJsonObject;

namespace fuzzy::core {

// Executor sobre Turso (libsql/HTTP) mediante QNetworkAccessManager. Lo usan
// el nodo (sync, proyección) y la app del dueño (portal directo).
class TursoHttpExecutor final : public IQueryExecutor
{
public:
    TursoHttpExecutor(const QString& base_url, const QString& token, const QString& org = QString());
    ~TursoHttpExecutor() override = default;

    ExecResult execute(const QString& sql, const std::vector<QVariant>& params = {}) override;

    std::vector<QueryRow> query(const QString& sql, const std::vector<QVariant>& params = {}) override;

private:
    // Envía una sentencia y devuelve su `result` ya desempaquetado.
    bool ejecutar(const QString& sql, const std::vector<QVariant>& params,
                  QJsonObject& resultado);

    QString base_url_;
    QString token_;
    QString org_;
    QNetworkAccessManager manager_;
};

} // namespace fuzzy::core
