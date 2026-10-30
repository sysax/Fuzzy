#pragma once

#include <QObject>
#include <QString>

namespace fuzzy::desktop {

// Tokens del sistema de diseño (docs/PLAN.md). Se exponen a QML como una
// propiedad de contexto (`estilo`) para no depender de singletons QML.
class Style : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString ink READ ink CONSTANT)
    Q_PROPERTY(QString surface READ surface CONSTANT)
    Q_PROPERTY(QString background READ background CONSTANT)
    Q_PROPERTY(QString border READ border CONSTANT)
    Q_PROPERTY(QString textSecondary READ textSecondary CONSTANT)
    Q_PROPERTY(QString textMuted READ textMuted CONSTANT)
    Q_PROPERTY(QString danger READ danger CONSTANT)
    Q_PROPERTY(QString dangerBg READ dangerBg CONSTANT)
    Q_PROPERTY(QString warning READ warning CONSTANT)
    Q_PROPERTY(QString warningBg READ warningBg CONSTANT)
    Q_PROPERTY(QString success READ success CONSTANT)
    Q_PROPERTY(QString successBg READ successBg CONSTANT)
    Q_PROPERTY(QString info READ info CONSTANT)
    Q_PROPERTY(QString infoBg READ infoBg CONSTANT)
    Q_PROPERTY(QString chip READ chip CONSTANT)
    Q_PROPERTY(int radiusCard READ radiusCard CONSTANT)
    Q_PROPERTY(int radiusPill READ radiusPill CONSTANT)
    Q_PROPERTY(int spacing READ spacing CONSTANT)

public:
    using QObject::QObject;

    QString ink() const { return QStringLiteral("#141414"); }
    QString surface() const { return QStringLiteral("#FFFFFF"); }
    QString background() const { return QStringLiteral("#F5F5F3"); }
    QString border() const { return QStringLiteral("#E6E6E4"); }
    QString textSecondary() const { return QStringLiteral("#6F6F6F"); }
    QString textMuted() const { return QStringLiteral("#A0A0A0"); }
    QString danger() const { return QStringLiteral("#E5484D"); }
    QString dangerBg() const { return QStringLiteral("#FDECEC"); }
    QString warning() const { return QStringLiteral("#B7791F"); }
    QString warningBg() const { return QStringLiteral("#FEF3C7"); }
    QString success() const { return QStringLiteral("#2F9E44"); }
    QString successBg() const { return QStringLiteral("#E7F5EC"); }
    QString info() const { return QStringLiteral("#2F6FED"); }
    QString infoBg() const { return QStringLiteral("#E8F0FE"); }
    QString chip() const { return QStringLiteral("#EDEDEB"); }
    int radiusCard() const { return 18; }
    int radiusPill() const { return 999; }
    int spacing() const { return 16; }
};

} // namespace fuzzy::desktop
