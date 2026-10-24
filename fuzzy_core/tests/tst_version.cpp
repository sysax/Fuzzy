#include "fuzzy/core/Version.h"

#include <QtTest>

class TstVersion : public QObject
{
    Q_OBJECT

private slots:
    void devuelveLaVersionDelProyecto();
};

void TstVersion::devuelveLaVersionDelProyecto()
{
    QCOMPARE(fuzzy::core::version(), QStringLiteral("0.1.0"));
}

QTEST_APPLESS_MAIN(TstVersion)

#include "tst_version.moc"
