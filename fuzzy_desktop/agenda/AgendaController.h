#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QDate>
#include <QObject>
#include <QVariantList>

namespace fuzzy::desktop {

class AgendaController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString modo READ modo NOTIFY datosCambiados)
    Q_PROPERTY(QString etiquetaPeriodo READ etiquetaPeriodo NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList citas READ citas NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList tipos READ tipos NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList estados READ estados NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList mascotas READ mascotas NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList dias READ dias NOTIFY datosCambiados)
    Q_PROPERTY(QString fechaActual READ fechaActual NOTIFY datosCambiados)
    Q_PROPERTY(QString error READ error NOTIFY errorCambiado)
    Q_PROPERTY(QString mensaje READ mensaje NOTIFY mensajeCambiado)

public:
    explicit AgendaController(fuzzy::core::IQueryExecutor& db, QObject* parent = nullptr);

    QString modo() const { return modo_; }
    QString etiquetaPeriodo() const;
    QVariantList citas() const { return citas_; }
    QVariantList tipos() const { return tipos_; }
    QVariantList estados() const { return estados_; }
    QVariantList mascotas() const { return mascotas_; }
    QVariantList dias() const { return dias_; }
    QString fechaActual() const { return fecha_.toString(QStringLiteral("yyyy-MM-dd")); }
    QString error() const { return error_; }
    QString mensaje() const { return mensaje_; }

    Q_INVOKABLE void cargar();
    Q_INVOKABLE void irHoy();
    Q_INVOKABLE void anterior();
    Q_INVOKABLE void siguiente();
    Q_INVOKABLE void cambiarModo(const QString& modo);
    Q_INVOKABLE void agendar(const QString& mascotaUuid, const QString& tipoUuid,
                             const QString& fecha, const QString& hora,
                             const QString& notas, bool emergencia);
    Q_INVOKABLE void cambiarEstado(const QString& citaUuid, const QString& estadoUuid);
    Q_INVOKABLE void mover(const QString& citaUuid, const QString& fecha,
                           const QString& hora);
    Q_INVOKABLE void cancelar(const QString& citaUuid);
    Q_INVOKABLE void limpiarMensajes();

signals:
    void datosCambiados();
    void errorCambiado();
    void mensajeCambiado();

private:
    qint64 inicioPeriodo() const;
    qint64 finPeriodo() const;
    void recargar();
    void fallo(const QString& texto);
    void exito(const QString& texto);
    bool resolverCita(const QString& citaUuid, qint64* id, qint64* estadoActual);

    fuzzy::core::IQueryExecutor& db_;
    QDate fecha_;
    QString modo_ = QStringLiteral("semana");
    QVariantList citas_;
    QVariantList tipos_;
    QVariantList estados_;
    QVariantList mascotas_;
    QVariantList dias_;
    QString error_;
    QString mensaje_;
};

} // namespace fuzzy::desktop
