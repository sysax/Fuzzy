#include "fuzzy/core/TursoHttpExecutor.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>

namespace fuzzy::core {

TursoHttpExecutor::TursoHttpExecutor(const QString& base_url, const QString& token, const QString& org)
    : base_url_(base_url), token_(token), org_(org)
{
}

ExecResult TursoHttpExecutor::execute(const QString& sql, const std::vector<QVariant>& params)
{
    ExecResult result;

    QString url = base_url_ + QStringLiteral("/v1/execute");
    if (!org_.isEmpty()) {
        url += QStringLiteral("?org=") + QUrl::fromPercentEncoding(org_.toUtf8());
    }

    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader,
                      QVariant(QStringLiteral("application/json")));
    request.setRawHeader(QByteArray("Authorization"), QByteArray("Bearer " + token_.toUtf8()));

    QJsonObject body;
    body.insert(QStringLiteral("query"), QJsonValue(sql));

    QJsonArray args;
    for (const QVariant& p : params) {
        QJsonValue v;
        if (p.typeId() == QVariant::Int || p.typeId() == QVariant::LongLong) {
            v = QJsonValue(p.toLongLong());
        } else if (p.typeId() == QVariant::Double) {
            v = QJsonValue(p.toDouble());
        } else if (p.typeId() == QVariant::Bool) {
            v = QJsonValue(p.toBool());
        } else {
            v = QJsonValue(p.toString());
        }
        args.append(v);
    }
    body.insert(QStringLiteral("args"), args);

    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::JsonFormat::Compact);
    QNetworkReply* reply = manager_.post(request, payload);
    if (reply == nullptr) {
        return result;
    }

    reply->waitForReadyRead(30000);
    const QNetworkReply::NetworkError error = reply->error();
    const QByteArray response_payload = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(response_payload);
    QJsonObject obj = doc.object();

    if (error == QNetworkReply::NetworkError::NoError && obj.contains(QStringLiteral("results"))) {
        const QJsonValue results_value = obj[QStringLiteral("results")];
        const QJsonArray results = results_value.toArray();
        if (!results.isEmpty()) {
            const QJsonObject res = results.at(0).toObject();
            if (res.contains(QStringLiteral("changes"))) {
                result.affected = res[QStringLiteral("changes")].toInteger();
            }
        }
    }

    delete reply;
    return result;
}

} // namespace fuzzy::core
