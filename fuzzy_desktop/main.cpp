#include "agenda/AgendaController.h"
#include "ajustes/AjustesController.h"
#include "facturacion/FacturacionController.h"
#include "fuzzy/core/AuthService.h"
#include "fuzzy/core/MigrationRunner.h"
#include "fuzzy/core/PortalBaseStore.h"
#include "fuzzy/core/PortalProvisioner.h"
#include "fuzzy/core/PortalTokenService.h"
#include "fuzzy/core/SqliteExecutor.h"
#include "fuzzy/core/Version.h"
#include "inventario/InventarioController.h"
#include "pacientes/PacientesController.h"
#include "panel/PanelController.h"
#include "propietarios/PropietariosController.h"
#include "session/SessionController.h"
#include "style/Style.h"
#include "vinculos/VinculosController.h"

#include <QDir>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>

#include <cstdio>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("FuzzyCloud"));
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

    // El nodo guarda su base en el directorio de datos de la aplicación. No se
    // escribe nada junto al binario: en una instalación de escritorio esa ruta
    // puede ser de solo lectura.
    const QString datos = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    if (datos.isEmpty() || !QDir().mkpath(datos)) {
        std::fprintf(stderr, "No se pudo preparar el directorio de datos.\n");
        return 1;
    }

    fuzzy::core::SqliteExecutor clinica(
        QStringLiteral("fuzzy_desktop_clinic"),
        datos + QStringLiteral("/clinica.db"));

    const fuzzy::core::MigrationRunner migraciones = fuzzy::core::MigrationRunner::clinic();
    if (!migraciones.valido() || !migraciones.aplicar(clinica)) {
        std::fprintf(stderr, "No se pudo aplicar el esquema de la clínica.\n");
        return 1;
    }

    fuzzy::core::AuthService auth(clinica);

    const QString portalesDir = datos + QStringLiteral("/portales");
    QDir().mkpath(portalesDir);
    fuzzy::core::LocalPortalBaseStore bases(portalesDir);
    fuzzy::core::PortalTokenService tokens(clinica, &bases);
    fuzzy::core::PortalProvisioner provisioner(clinica, tokens, bases);

    fuzzy::desktop::Style estilo;

    fuzzy::desktop::SessionController sesion(auth);
    fuzzy::desktop::PanelController panel(clinica);
    fuzzy::desktop::AgendaController agenda(clinica);
    fuzzy::desktop::PacientesController pacientes(clinica);
    fuzzy::desktop::PropietariosController propietarios(clinica);
    fuzzy::desktop::InventarioController inventario(clinica);
    fuzzy::desktop::FacturacionController facturacion(clinica);
    fuzzy::desktop::AjustesController ajustes(auth);
    fuzzy::desktop::VinculosController vinculos(clinica, provisioner, bases);

    QQmlApplicationEngine engine;
    QQmlContext* contexto = engine.rootContext();
    contexto->setContextProperty(QStringLiteral("appVersion"), fuzzy::core::version());
    contexto->setContextProperty(QStringLiteral("estilo"), &estilo);
    contexto->setContextProperty(QStringLiteral("sesion"), &sesion);
    contexto->setContextProperty(QStringLiteral("panel"), &panel);
    contexto->setContextProperty(QStringLiteral("agenda"), &agenda);
    contexto->setContextProperty(QStringLiteral("pacientes"), &pacientes);
    contexto->setContextProperty(QStringLiteral("propietarios"), &propietarios);
    contexto->setContextProperty(QStringLiteral("inventario"), &inventario);
    contexto->setContextProperty(QStringLiteral("facturacion"), &facturacion);
    contexto->setContextProperty(QStringLiteral("ajustes"), &ajustes);
    contexto->setContextProperty(QStringLiteral("vinculos"), &vinculos);

    engine.setInitialProperties(
        {{QStringLiteral("appVersion"), fuzzy::core::version()}});
    engine.loadFromModule(QStringLiteral("Fuzzy"), QStringLiteral("Main"));

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    return app.exec();
}
