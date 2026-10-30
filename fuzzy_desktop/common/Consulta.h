#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QDate>
#include <QDateTime>
#include <QVariantList>
#include <QVariantMap>

namespace fuzzy::desktop {

QVariantMap mapaDeFila(const fuzzy::core::QueryRow& fila,
                       const QStringList& claves);
QString textoDeFila(const fuzzy::core::QueryRow& fila, int indice);
qint64 enteroDeFila(const fuzzy::core::QueryRow& fila, int indice);
double realDeFila(const fuzzy::core::QueryRow& fila, int indice);
QVariantList listaDeFilas(const std::vector<fuzzy::core::QueryRow>& filas,
                          const QStringList& claves);

// Fecha del nodo. Las pruebas visuales la fijan con FUZZY_HOY=yyyy-MM-dd
// para que las capturas no dependan del día en que se ejecutan.
QDate fechaHoy();

QString fechaHoraTexto(qint64 ms);
QString fechaTexto(qint64 ms);
QString horaTexto(qint64 ms);
QString moneda(double valor);
QString numero(double valor, int decimales = 0);
qint64 inicioDiaMs(const QDate& fecha);
qint64 finDiaMs(const QDate& fecha);
qint64 msDesdeFechaHora(const QDateTime& fechaHora);

QString uuidClienteDe(fuzzy::core::IQueryExecutor& db, const QString& cliente_uuid);
QString uuidMascotaDe(fuzzy::core::IQueryExecutor& db, const QString& mascota_uuid);

} // namespace fuzzy::desktop
