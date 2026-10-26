#include "fuzzy/core/TursoHttpExecutor.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

using fuzzy::core::TursoHttpExecutor;

namespace {

// Servidor HTTP mínimo para verificar el contrato de `/v1/execute` sin red
// real ni credenciales de Turso.
class ServidorFalso : public QObject
{
    Q_OBJECT

public:
    explicit ServidorFalso(QObject* parent = nullptr)
        : QObject(parent)
    {
        connect(&servidor_, &QTcpServer::newConnection, this, &ServidorFalso::aceptar);
        servidor_.listen(QHostAddress::LocalHost, 0);
    }

    QString url() const
    {
        return QStringLiteral("http://127.0.0.1:%1").arg(servidor_.serverPort());
    }

    void responder(const QByteArray& cuerpo) { respuesta_ = cuerpo; }

    QJsonObject ultimaSentencia() const
    {
        return QJsonDocument::fromJson(peticion_).object();
    }

private:
    void aceptar()
    {
        QTcpSocket* socket = servidor_.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, socket, [this, socket] { leer(socket); });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        leer(socket);
    }

    void leer(QTcpSocket* socket)
    {
        datos_ += socket->readAll();
        const int fin_cabeceras = datos_.indexOf("\r\n\r\n");
        if (fin_cabeceras < 0) {
            return;
        }

        int longitud = 0;
        const QList<QByteArray> lineas = datos_.left(fin_cabeceras).split('\n');
        for (const QByteArray& linea : lineas) {
            if (linea.toLower().startsWith("content-length:")) {
                longitud = linea.mid(15).trimmed().toInt();
            }
        }

        const int inicio_cuerpo = fin_cabeceras + 4;
        if (datos_.size() < inicio_cuerpo + longitud) {
            return;
        }
        peticion_ = datos_.mid(inicio_cuerpo, longitud);

        QByteArray respuesta = "HTTP/1.1 200 OK\r\n"
                               "Content-Type: application/json\r\n"
                               "Connection: close\r\n"
                               "Content-Length: "
            + QByteArray::number(respuesta_.size()) + "\r\n\r\n" + respuesta_;
        socket->write(respuesta);
        socket->disconnectFromHost();
    }

    QTcpServer servidor_;
    QByteArray datos_;
    QByteArray peticion_;
    QByteArray respuesta_;
};

} // namespace

class TstTursoHttpExecutor : public QObject
{
    Q_OBJECT

private slots:
    void ejecutaConArgumentosTipados();
    void consultaFilasTipadas();
    void servidorNoDisponibleNoRompe();
};

void TstTursoHttpExecutor::ejecutaConArgumentosTipados()
{
    ServidorFalso servidor;
    servidor.responder(QByteArrayLiteral(
        R"({"result":{"affected_row_count":1,"last_insert_rowid":42}})"));

    TursoHttpExecutor executor(servidor.url(), QStringLiteral("token-de-prueba"));
    const fuzzy::core::ExecResult resultado = executor.execute(
        QStringLiteral("INSERT INTO prueba VALUES (?, ?, ?, ?, ?)"),
        {QVariant(QStringLiteral("Luna")), QVariant::fromValue<qint64>(7),
         QVariant(true), QVariant(2.5), QVariant()});

    QCOMPARE(resultado.affected, 1LL);
    QCOMPARE(resultado.last_insert_id.toLongLong(), 42LL);

    const QJsonObject cuerpo = servidor.ultimaSentencia();
    const QJsonObject stmt = cuerpo.value(QStringLiteral("stmt")).toObject();
    QCOMPARE(stmt.value(QStringLiteral("sql")).toString(),
             QStringLiteral("INSERT INTO prueba VALUES (?, ?, ?, ?, ?)"));

    const QJsonArray args = stmt.value(QStringLiteral("args")).toArray();
    QCOMPARE(static_cast<int>(args.size()), 5);
    QCOMPARE(args.at(0).toObject().value(QStringLiteral("type")).toString(),
             QStringLiteral("text"));
    QCOMPARE(args.at(0).toObject().value(QStringLiteral("value")).toString(),
             QStringLiteral("Luna"));
    QCOMPARE(args.at(1).toObject().value(QStringLiteral("type")).toString(),
             QStringLiteral("integer"));
    QCOMPARE(args.at(1).toObject().value(QStringLiteral("value")).toString(),
             QStringLiteral("7"));
    QCOMPARE(args.at(2).toObject().value(QStringLiteral("value")).toString(),
             QStringLiteral("1"));
    QCOMPARE(args.at(3).toObject().value(QStringLiteral("type")).toString(),
             QStringLiteral("float"));
    QCOMPARE(args.at(3).toObject().value(QStringLiteral("value")).toDouble(), 2.5);
    QCOMPARE(args.at(4).toObject().value(QStringLiteral("type")).toString(),
             QStringLiteral("null"));
}

void TstTursoHttpExecutor::consultaFilasTipadas()
{
    ServidorFalso servidor;
    servidor.responder(QByteArrayLiteral(
        R"({"result":{"cols":[{"name":"nombre"},{"name":"edad"},{"name":"nota"},{"name":"peso"}],)"
        R"("rows":[[{"type":"text","value":"Luna"},{"type":"integer","value":"3"},)"
        R"({"type":"null","value":null},{"type":"float","value":2.5}]]}})"));

    TursoHttpExecutor executor(servidor.url(), QStringLiteral("token-de-prueba"));
    const auto filas = executor.query(
        QStringLiteral("SELECT nombre, edad, nota, peso FROM mascotas WHERE nombre = ?"),
        {QVariant(QStringLiteral("Luna"))});

    QCOMPARE(static_cast<int>(filas.size()), 1);
    QCOMPARE(static_cast<int>(filas.front().values.size()), 4);
    QCOMPARE(filas.front().values.at(0).toString(), QStringLiteral("Luna"));
    QCOMPARE(filas.front().values.at(1).toLongLong(), 3LL);
    QVERIFY(filas.front().values.at(2).isNull());
    QCOMPARE(filas.front().values.at(3).toDouble(), 2.5);
}

void TstTursoHttpExecutor::servidorNoDisponibleNoRompe()
{
    TursoHttpExecutor executor(QStringLiteral("http://127.0.0.1:1"),
                               QStringLiteral("token-de-prueba"));
    const fuzzy::core::ExecResult resultado = executor.execute(QStringLiteral("SELECT 1"));
    QCOMPARE(resultado.affected, 0LL);
    QVERIFY(executor.query(QStringLiteral("SELECT 1")).empty());
}

QTEST_MAIN(TstTursoHttpExecutor)

#include "tst_turso_http_executor.moc"
