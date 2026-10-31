#pragma once

#include <QObject>
#include <QString>

namespace fuzzy::mobile {

// Tokens visuales del teléfono. Se exponen a QML como `estilo`, igual que en
// el escritorio, para no acoplar las vistas a un singleton.
class Style : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString ink READ ink CONSTANT)
    Q_PROPERTY(QString surface READ surface CONSTANT)
    Q_PROPERTY(QString background READ background CONSTANT)
    Q_PROPERTY(QString border READ border CONSTANT)
    Q_PROPERTY(QString secondary READ secondary CONSTANT)
    Q_PROPERTY(QString muted READ muted CONSTANT)
    Q_PROPERTY(QString accent READ accent CONSTANT)
    Q_PROPERTY(QString accentSoft READ accentSoft CONSTANT)
    Q_PROPERTY(QString danger READ danger CONSTANT)
    Q_PROPERTY(QString dangerSoft READ dangerSoft CONSTANT)
    Q_PROPERTY(QString success READ success CONSTANT)
    Q_PROPERTY(QString successSoft READ successSoft CONSTANT)
    Q_PROPERTY(QString warning READ warning CONSTANT)
    Q_PROPERTY(QString warningSoft READ warningSoft CONSTANT)
    Q_PROPERTY(int radius READ radius CONSTANT)
    Q_PROPERTY(int spacing READ spacing CONSTANT)
    Q_PROPERTY(int touch READ touch CONSTANT)

public:
    using QObject::QObject;

    QString ink() const { return QStringLiteral("#111827"); }
    QString surface() const { return QStringLiteral("#FFFFFF"); }
    QString background() const { return QStringLiteral("#F4F5F7"); }
    QString border() const { return QStringLiteral("#E5E7EB"); }
    QString secondary() const { return QStringLiteral("#6B7280"); }
    QString muted() const { return QStringLiteral("#9CA3AF"); }
    QString accent() const { return QStringLiteral("#2563EB"); }
    QString accentSoft() const { return QStringLiteral("#DBEAFE"); }
    QString danger() const { return QStringLiteral("#DC2626"); }
    QString dangerSoft() const { return QStringLiteral("#FEE2E2"); }
    QString success() const { return QStringLiteral("#15803D"); }
    QString successSoft() const { return QStringLiteral("#DCFCE7"); }
    QString warning() const { return QStringLiteral("#B45309"); }
    QString warningSoft() const { return QStringLiteral("#FEF3C7"); }
    int radius() const { return 16; }
    int spacing() const { return 16; }
    int touch() const { return 48; }
};

} // namespace fuzzy::mobile
