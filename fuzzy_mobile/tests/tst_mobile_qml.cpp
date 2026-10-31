#include "app/MobileController.h"
#include "fuzzy/core/MigrationRunner.h"
#include "fuzzy/core/MovilStore.h"
#include "fuzzy/core/SqliteExecutor.h"
#include "fuzzy/core/Version.h"
#include "style/Style.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>

using fuzzy::core::MigrationRunner;
using fuzzy::core::MovilStore;
using fuzzy::core::SqliteExecutor;
using fuzzy::mobile::MobileController;
using fuzzy::mobile::Style;

namespace {

int contadorConexiones()
{
    static int siguiente = 0;
    return ++siguiente;
}

QString conexion(const QString& etiqueta)
{
    return QStringLiteral("fase8_qml_%1_%2").arg(etiqueta).arg(contadorConexiones());
}

} // namespace

// Carga real del módulo QML del teléfono. Las pruebas de controlador cubren la
// lógica; esta vigila que Main.qml, las vistas y los diálogos compilen y se
// instancien con el contexto que arma `main.cpp`.
class MobileQmlTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void cargaLaPantallaDeBloqueo();
    void cargaElShellDesbloqueado();
    void directorioEmbebidoSoloTieneTokensDeBuzon();

private:
    bool cargar(QQmlApplicationEngine& engine);

    std::unique_ptr<QTemporaryDir> dir_;
    std::unique_ptr<SqliteExecutor> localDb_;
    std::unique_ptr<MovilStore> store_;
    std::unique_ptr<MobileController> movil_;
    std::unique_ptr<Style> estilo_;
};

void MobileQmlTest::init()
{
    dir_ = std::make_unique<QTemporaryDir>();
    QVERIFY(dir_->isValid());
    localDb_ = std::make_unique<SqliteExecutor>(
        conexion(QStringLiteral("local")), dir_->filePath(QStringLiteral("movil.db")));
    QVERIFY(MigrationRunner::mobile().aplicar(*localDb_));
    store_ = std::make_unique<MovilStore>(*localDb_);
    store_->setIteraciones(1000);
    movil_ = std::make_unique<MobileController>(*store_);
    estilo_ = std::make_unique<Style>();
}

void MobileQmlTest::cleanup()
{
    movil_.reset();
    store_.reset();
    localDb_.reset();
    dir_.reset();
    estilo_.reset();
}

bool MobileQmlTest::cargar(QQmlApplicationEngine& engine)
{
    QQmlContext* contexto = engine.rootContext();
    contexto->setContextProperty(QStringLiteral("versionApp"), fuzzy::core::version());
    contexto->setContextProperty(QStringLiteral("estilo"), estilo_.get());
    contexto->setContextProperty(QStringLiteral("movil"), movil_.get());
    engine.loadFromModule(QStringLiteral("FuzzyMovil"), QStringLiteral("Main"));
    return !engine.rootObjects().isEmpty();
}

void MobileQmlTest::cargaLaPantallaDeBloqueo()
{
    QQmlApplicationEngine engine;
    QVERIFY(cargar(engine));
    QCOMPARE(movil_->configurada(), false);
    QCOMPARE(engine.rootObjects().size(), 1);
    // La ventana arranca bloqueada: el StackLayout muestra BloqueoView.
    QObject* raiz = engine.rootObjects().front();
    QVERIFY(raiz->property("width").toInt() >= 360);
}

void MobileQmlTest::cargaElShellDesbloqueado()
{
    QVERIFY(store_->configurarClave(QStringLiteral("Aurora2026!")));
    QVERIFY(store_->desbloqueado());

    QQmlApplicationEngine engine;
    QVERIFY(cargar(engine));
    QVERIFY(movil_->desbloqueada());
    QCOMPARE(engine.rootObjects().size(), 1);

    // Sin portales, las vistas muestran estados vacíos en lugar de fallar por
    // modelos nulos.
    QCOMPARE(movil_->portales().size(), 0);
    QCOMPARE(movil_->mascotas().size(), 0);
    QCOMPARE(movil_->citas().size(), 0);
    QCOMPARE(movil_->historial().size(), 0);
}

void MobileQmlTest::directorioEmbebidoSoloTieneTokensDeBuzon()
{
    QFile archivo(QStringLiteral(":/fuzzy/directory/clinicas.json"));
    QVERIFY2(archivo.open(QIODevice::ReadOnly), "El directorio debe viajar en el binario");
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(archivo.readAll(), &error);
    QVERIFY2(doc.isObject(), qPrintable(error.errorString()));

    const QJsonArray clinicas = doc.object().value(QStringLiteral("clinicas")).toArray();
    QVERIFY(!clinicas.isEmpty());

    // D22: el directorio es público. Solo puede llevar datos comerciales y el
    // token de inserción del buzón; nunca un token de portal ni una credencial
    // de lectura de la base de la clínica.
    const QSet<QString> permitidas = {
        QStringLiteral("uuid"),           QStringLiteral("nombre"),
        QStringLiteral("ciudad"),         QStringLiteral("direccion"),
        QStringLiteral("telefono"),       QStringLiteral("inbox_url"),
        QStringLiteral("inbox_token"),    QStringLiteral("activa"),
        QStringLiteral("updated_at"),
    };

    for (const QJsonValue& valor : clinicas) {
        const QJsonObject clinica = valor.toObject();
        QVERIFY(!clinica.isEmpty());
        for (auto it = clinica.constBegin(); it != clinica.constEnd(); ++it) {
            QVERIFY2(permitidas.contains(it.key()),
                     qPrintable(QStringLiteral("Clave inesperada en el directorio: %1")
                                    .arg(it.key())));
        }
        const QString token = clinica.value(QStringLiteral("inbox_token")).toString();
        QVERIFY(!token.contains(QStringLiteral("portal")));
        QVERIFY(!clinica.contains(QStringLiteral("token_portal")));
        QVERIFY(!clinica.contains(QStringLiteral("portal_token")));
    }
}

QTEST_MAIN(MobileQmlTest)

#include "tst_mobile_qml.moc"
