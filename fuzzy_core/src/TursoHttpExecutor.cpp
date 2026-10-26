#include "fuzzy/core/TursoHttpExecutor.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaType>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>

namespace fuzzy::core {

namespace {

// Valor en el formato de argumentos de Hrana (`SQL over HTTP`).
QJsonObject valorHrana(const QVariant& parametro)
{
    QJsonObject valor;

    switch (parametro.typeId()) {
    case QMetaType::UnknownType:
    case QMetaType::Nullptr:
        valor.insert(QStringLiteral("type"), QStringLiteral("null"));
        valor.insert(QStringLiteral("value"), QJsonValue::Null);
        break;
    case QMetaType::Bool:
        // SQLite no tiene booleano: viaja como entero 0/1.
        valor.insert(QStringLiteral("type"), QStringLiteral("integer"));
        valor.insert(QStringLiteral("value"), parametro.toBool() ? QStringLiteral("1")
                                                                 : QStringLiteral("0"));
        break;
    case QMetaType::Int:
    case QMetaType::LongLong:
        // Hrana envía los enteros como texto para no perder precisión en JSON.
        valor.insert(QStringLiteral("type"), QStringLiteral("integer"));
        valor.insert(QStringLiteral("value"), QString::number(parametro.toLongLong()));
        break;
    case QMetaType::Float:
    case QMetaType::Double:
        valor.insert(QStringLiteral("type"), QStringLiteral("float"));
        valor.insert(QStringLiteral("value"), parametro.toDouble());
        break;
    case QMetaType::QByteArray:
        valor.insert(QStringLiteral("type"), QStringLiteral("blob"));
        valor.insert(QStringLiteral("value"),
                     QString::fromLatin1(parametro.toByteArray().toBase64()));
        break;
    default:
        valor.insert(QStringLiteral("type"), QStringLiteral("text"));
        valor.insert(QStringLiteral("value"), parametro.toString());
        break;
    }

    return valor;
}

// Resultado de una sentencia. Acepta la forma v1 (`{"result": ...}`) y las
// variantes anidadas que devuelven el pipeline v2 o versiones antiguas.
QJsonObject extraerResultado(const QJsonObject& respuesta)
{
    if (respuesta.contains(QStringLiteral("result"))) {
        return respuesta.value(QStringLiteral("result")).toObject();
    }

    const QJsonArray results = respuesta.value(QStringLiteral("results")).toArray();
    if (results.isEmpty()) {
        return {};
    }

    QJsonObject resultado = results.at(0).toObject();
    if (resultado.contains(QStringLiteral("response"))) {
        resultado = resultado.value(QStringLiteral("response")).toObject();
    }
    if (resultado.contains(QStringLiteral("result"))) {
        resultado = resultado.value(QStringLiteral("result")).toObject();
    }
    return resultado;
}

QVariant valorDesdeHrana(const QJsonObject& celda)
{
    const QString tipo = celda.value(QStringLiteral("type")).toString();
    const QJsonValue valor = celda.value(QStringLiteral("value"));

    if (tipo == QStringLiteral("null")) {
        return {};
    }
    if (tipo == QStringLiteral("integer")) {
        // Puede llegar como número o como texto; ambos se convierten.
        return QVariant::fromValue<qlonglong>(valor.toVariant().toLongLong());
    }
    if (tipo == QStringLiteral("float")) {
        return QVariant::fromValue<double>(valor.toDouble());
    }
    if (tipo == QStringLiteral("blob")) {
        return QVariant(QByteArray::fromBase64(valor.toString().toLatin1()));
    }
    return QVariant(valor.toString());
}

} // namespace

TursoHttpExecutor::TursoHttpExecutor(const QString& base_url, const QString& token, const QString& org)
    : base_url_(base_url), token_(token), org_(org)
{
}

bool TursoHttpExecutor::ejecutar(const QString& sql, const std::vector<QVariant>& params,
                                 QJsonObject& resultado)
{
    QString url = base_url_;
    while (url.endsWith(QLatin1Char('/'))) {
        url.chop(1);
    }
    url += QStringLiteral("/v1/execute");
    if (!org_.isEmpty()) {
        // La organización es un dato de URL: se codifica, no se decodifica.
        url += QStringLiteral("?org=")
            + QString::fromLatin1(QUrl::toPercentEncoding(org_));
    }

    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader,
                      QVariant(QStringLiteral("application/json")));
    request.setRawHeader(QByteArray("Authorization"), QByteArray("Bearer " + token_.toUtf8()));

    QJsonObject stmt;
    stmt.insert(QStringLiteral("sql"), sql);
    if (!params.empty()) {
        QJsonArray args;
        for (const QVariant& parametro : params) {
            args.append(valorHrana(parametro));
        }
        stmt.insert(QStringLiteral("args"), args);
    }

    QJsonObject body;
    body.insert(QStringLiteral("stmt"), stmt);

    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::JsonFormat::Compact);
    QNetworkReply* reply = manager_.post(request, payload);
    if (reply == nullptr) {
        return false;
    }

    // `/v1/execute` no mantiene conexión: la respuesta llega completa y se
    // cierra. El bucle espera el fin con un límite explícito.
    QEventLoop espera;
    QTimer limite;
    limite.setSingleShot(true);
    QObject::connect(&limite, &QTimer::timeout, &espera, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &espera, &QEventLoop::quit);
    limite.start(30000);
    if (!reply->isFinished()) {
        espera.exec();
    }
    if (!reply->isFinished()) {
        reply->abort();
    }

    const QNetworkReply::NetworkError error = reply->error();
    const QByteArray respuesta = reply->readAll();
    delete reply;

    if (error != QNetworkReply::NetworkError::NoError) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(respuesta);
    if (!doc.isObject()) {
        return false;
    }

    resultado = extraerResultado(doc.object());
    return true;
}

ExecResult TursoHttpExecutor::execute(const QString& sql, const std::vector<QVariant>& params)
{
    ExecResult result;

    QJsonObject respuesta;
    if (!ejecutar(sql, params, respuesta)) {
        return result;
    }

    result.ok = true;
    result.affected = respuesta.value(QStringLiteral("affected_row_count")).toInteger();
    const QJsonValue ultimo = respuesta.value(QStringLiteral("last_insert_rowid"));
    if (!ultimo.isNull() && !ultimo.isUndefined()) {
        result.last_insert_id = ultimo.toVariant();
    }
    return result;
}

std::vector<QueryRow> TursoHttpExecutor::query(const QString& sql,
                                               const std::vector<QVariant>& params)
{
    std::vector<QueryRow> filas;

    QJsonObject respuesta;
    if (!ejecutar(sql, params, respuesta)) {
        return filas;
    }

    const QJsonArray rows = respuesta.value(QStringLiteral("rows")).toArray();
    for (const QJsonValue& row : rows) {
        QueryRow fila;
        const QJsonArray valores = row.toArray();
        for (const QJsonValue& valor : valores) {
            fila.values.push_back(valorDesdeHrana(valor.toObject()));
        }
        filas.push_back(std::move(fila));
    }
    return filas;
}

} // namespace fuzzy::core
