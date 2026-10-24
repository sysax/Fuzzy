#include "fuzzy/core/Version.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("FuzzyCloud"));
    app.setApplicationVersion(fuzzy::core::version());
    app.setOrganizationName(QStringLiteral("FuzzyCloud"));

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("appVersion"), fuzzy::core::version()}});
    engine.loadFromModule(QStringLiteral("Fuzzy"), QStringLiteral("Main"));

    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
