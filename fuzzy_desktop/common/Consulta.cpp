#include "common/Consulta.h"

#include <QLocale>
#include <QVariant>

namespace fuzzy::desktop {

using fuzzy::core::QueryRow;

QVariantMap mapaDeFila(const QueryRow& fila, const QStringList& claves)
{
    QVariantMap mapa;
    for (int i = 0; i < claves.size() && i < static_cast<int>(fila.values.size()); ++i) {
        mapa.insert(claves.at(i), fila.values[static_cast<size_t>(i)]);
    }
    return mapa;
}

QString textoDeFila(const QueryRow& fila, int indice)
{
    if (indice < 0 || indice >= static_cast<int>(fila.values.size())) {
        return {};
    }
    return fila.values[static_cast<size_t>(indice)].toString();
}

qint64 enteroDeFila(const QueryRow& fila, int indice)
{
    if (indice < 0 || indice >= static_cast<int>(fila.values.size())) {
        return 0;
    }
    return fila.values[static_cast<size_t>(indice)].toLongLong();
}

double realDeFila(const QueryRow& fila, int indice)
{
    if (indice < 0 || indice >= static_cast<int>(fila.values.size())) {
        return 0.0;
    }
    return fila.values[static_cast<size_t>(indice)].toDouble();
}

QVariantList listaDeFilas(const std::vector<QueryRow>& filas, const QStringList& claves)
{
    QVariantList lista;
    lista.reserve(static_cast<qsizetype>(filas.size()));
    for (const auto& fila : filas) {
        lista.append(mapaDeFila(fila, claves));
    }
    return lista;
}

QDate fechaHoy()
{
    static const QDate fijada = [] {
        const QString texto = qEnvironmentVariable("FUZZY_HOY").trimmed();
        const QDate fecha = QDate::fromString(texto, QStringLiteral("yyyy-MM-dd"));
        return fecha.isValid() ? fecha : QDate::currentDate();
    }();
    return fijada;
}

QString fechaHoraTexto(qint64 ms)
{
    if (ms <= 0) {
        return {};
    }
    return QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("dd/MM/yyyy HH:mm"));
}

QString fechaTexto(qint64 ms)
{
    if (ms <= 0) {
        return {};
    }
    return QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("dd/MM/yyyy"));
}

QString horaTexto(qint64 ms)
{
    if (ms <= 0) {
        return {};
    }
    return QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("HH:mm"));
}

QString moneda(double valor)
{
    return QLocale(QLocale::Spanish, QLocale::Colombia)
        .toCurrencyString(valor, QStringLiteral("$"));
}

QString numero(double valor, int decimales)
{
    return QLocale(QLocale::Spanish, QLocale::Colombia)
        .toString(valor, 'f', decimales);
}

qint64 inicioDiaMs(const QDate& fecha)
{
    return QDateTime(fecha, QTime(0, 0)).toMSecsSinceEpoch();
}

qint64 finDiaMs(const QDate& fecha)
{
    return QDateTime(fecha, QTime(23, 59, 59, 999)).toMSecsSinceEpoch();
}

qint64 msDesdeFechaHora(const QDateTime& fechaHora)
{
    return fechaHora.toMSecsSinceEpoch();
}

QString uuidClienteDe(fuzzy::core::IQueryExecutor& db, const QString& cliente_uuid)
{
    const auto filas = db.query(
        QStringLiteral("SELECT uuid FROM clientes WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(cliente_uuid)});
    return filas.size() == 1 ? textoDeFila(filas.front(), 0) : QString();
}

QString uuidMascotaDe(fuzzy::core::IQueryExecutor& db, const QString& mascota_uuid)
{
    const auto filas = db.query(
        QStringLiteral("SELECT uuid FROM mascotas WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(mascota_uuid)});
    return filas.size() == 1 ? textoDeFila(filas.front(), 0) : QString();
}

} // namespace fuzzy::desktop
