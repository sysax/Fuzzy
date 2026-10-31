#include "app/MobileController.h"
#include "fuzzy/core/MigrationRunner.h"
#include "fuzzy/core/MovilStore.h"
#include "fuzzy/core/SqliteExecutor.h"
#include "fuzzy/core/TursoHttpExecutor.h"
#include "fuzzy/core/Version.h"
#include "style/Style.h"

#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>

#include <cstdio>
#include <memory>

namespace {

// El teléfono abre cada portal con su propio token descifrado. La sonda usa el
// estado del transporte para distinguir "sin red" de "token revocado".
fuzzy::mobile::ConexionPortal abrirPortal(const fuzzy::core::PortalConocido& portal,
                                          const QString& token)
{
    fuzzy::mobile::ConexionPortal conexion;
    auto ejecutor = std::make_unique<fuzzy::core::TursoHttpExecutor>(portal.url, token);
    auto* turso = ejecutor.get();
    conexion.sonda = [turso] { return turso->salud(); };
    conexion.ejecutor = std::move(ejecutor);
    return conexion;
}

QByteArray leerDirectorio()
{
    QFile archivo(QStringLiteral(":/fuzzy/directory/clinicas.json"));
    if (!archivo.open(QIODevice::ReadOnly)) {
        return {};
    }
    return archivo.readAll();
}

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("FuzzyCloud Móvil"));
    app.setApplicationVersion(fuzzy::core::version());
    app.setOrganizationName(QStringLiteral("FuzzyCloud"));
    app.setOrganizationDomain(QStringLiteral("fuzzycloud.local"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    const int fuente = QFontDatabase::addApplicationFont(
        QStringLiteral(":/fonts/DejaVuSans.ttf"));
    if (fuente >= 0) {
        const QStringList familias = QFontDatabase::applicationFontFamilies(fuente);
        if (!familias.isEmpty()) {
            QGuiApplication::setFont(QFont(familias.front()));
        }
    }

    const QString datos = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (datos.isEmpty() || !QDir().mkpath(datos)) {
        std::fprintf(stderr, "No se pudo preparar el directorio de datos.\n");
        return 1;
    }

    fuzzy::core::SqliteExecutor local(QStringLiteral("fuzzy_mobile"),
                                      datos + QStringLiteral("/movil.db"));
    const fuzzy::core::MigrationRunner migraciones = fuzzy::core::MigrationRunner::mobile();
    if (!migraciones.valido() || !migraciones.aplicar(local)) {
        std::fprintf(stderr, "No se pudo aplicar el esquema local del teléfono.\n");
        return 1;
    }

    fuzzy::core::MovilStore store(local);
    fuzzy::mobile::Style estilo;
    fuzzy::mobile::MobileController movil(store, abrirPortal);

    const QByteArray directorio = leerDirectorio();
    if (!directorio.isEmpty()) {
        movil.cargarDirectorio(QString::fromUtf8(directorio));
    }

    QQmlApplicationEngine engine;
    QQmlContext* contexto = engine.rootContext();
    contexto->setContextProperty(QStringLiteral("versionApp"), fuzzy::core::version());
    contexto->setContextProperty(QStringLiteral("estilo"), &estilo);
    contexto->setContextProperty(QStringLiteral("movil"), &movil);

    engine.loadFromModule(QStringLiteral("FuzzyMovil"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    return app.exec();
}
