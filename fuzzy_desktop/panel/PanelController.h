#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QObject>
#include <QVariantList>

namespace fuzzy::desktop {

class PanelController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int citasHoy READ citasHoy NOTIFY datosCambiados)
    Q_PROPERTY(int enEspera READ enEspera NOTIFY datosCambiados)
    Q_PROPERTY(int enCurso READ enCurso NOTIFY datosCambiados)
    Q_PROPERTY(int pacientesActivos READ pacientesActivos NOTIFY datosCambiados)
    Q_PROPERTY(int stockCritico READ stockCritico NOTIFY datosCambiados)
    Q_PROPERTY(double ingresosHoy READ ingresosHoy NOTIFY datosCambiados)
    Q_PROPERTY(QString ingresosHoyTexto READ ingresosHoyTexto NOTIFY datosCambiados)
    Q_PROPERTY(QString fechaHoy READ fechaHoy NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList agendaHoy READ agendaHoy NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList salaEspera READ salaEspera NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList graficoSemana READ graficoSemana NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList alertasInventario READ alertasInventario NOTIFY datosCambiados)

public:
    explicit PanelController(fuzzy::core::IQueryExecutor& db, QObject* parent = nullptr);

    int citasHoy() const { return citasHoy_; }
    int enEspera() const { return enEspera_; }
    int enCurso() const { return enCurso_; }
    int pacientesActivos() const { return pacientesActivos_; }
    int stockCritico() const { return stockCritico_; }
    double ingresosHoy() const { return ingresosHoy_; }
    QString ingresosHoyTexto() const;
    QString fechaHoy() const;
    QVariantList agendaHoy() const { return agendaHoy_; }
    QVariantList salaEspera() const { return salaEspera_; }
    QVariantList graficoSemana() const { return graficoSemana_; }
    QVariantList alertasInventario() const { return alertasInventario_; }

    Q_INVOKABLE void cargar();

signals:
    void datosCambiados();

private:
    fuzzy::core::IQueryExecutor& db_;
    int citasHoy_ = 0;
    int enEspera_ = 0;
    int enCurso_ = 0;
    int pacientesActivos_ = 0;
    int stockCritico_ = 0;
    double ingresosHoy_ = 0.0;
    QVariantList agendaHoy_;
    QVariantList salaEspera_;
    QVariantList graficoSemana_;
    QVariantList alertasInventario_;
};

} // namespace fuzzy::desktop
