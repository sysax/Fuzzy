#include "panel/PanelController.h"

#include "common/Consulta.h"

#include <QLocale>
#include <QVariant>

namespace fuzzy::desktop {

namespace {

QVariantList citasEntre(fuzzy::core::IQueryExecutor& db, qint64 desde, qint64 hasta,
                        bool soloActivas, int limite)
{
    QString sql = QStringLiteral(
        "SELECT c.uuid, c.fecha_hora_programada, m.nombre, cl.nombre_completo, "
        "ts.nombre_servicio, ec.nombre_estado, c.prioridad, c.es_emergencia, m.uuid "
        "FROM citas c "
        "JOIN mascotas m ON m.id = c.mascota_id "
        "JOIN clientes cl ON cl.id = m.cliente_id "
        "JOIN tipos_servicio_cita ts ON ts.id = c.tipo_servicio_id "
        "JOIN estados_cita ec ON ec.id = c.estado_cita_id "
        "WHERE c.deleted_at IS NULL AND c.fecha_hora_programada BETWEEN ? AND ? ");
    if (soloActivas) {
        sql += QStringLiteral("AND ec.nombre_estado IN ('En espera', 'En curso') ");
    }
    sql += QStringLiteral("ORDER BY c.fecha_hora_programada LIMIT ?");

    const auto filas = db.query(sql, {QVariant::fromValue(desde), QVariant::fromValue(hasta),
                                      QVariant::fromValue(limite)});
    QVariantList lista;
    for (const auto& fila : filas) {
        const qint64 ms = enteroDeFila(fila, 1);
        QVariantMap cita;
        cita.insert(QStringLiteral("uuid"), textoDeFila(fila, 0));
        cita.insert(QStringLiteral("hora"), horaTexto(ms));
        cita.insert(QStringLiteral("fechaHora"), fechaHoraTexto(ms));
        cita.insert(QStringLiteral("mascota"), textoDeFila(fila, 2));
        cita.insert(QStringLiteral("propietario"), textoDeFila(fila, 3));
        cita.insert(QStringLiteral("tipo"), textoDeFila(fila, 4));
        cita.insert(QStringLiteral("estado"), textoDeFila(fila, 5));
        cita.insert(QStringLiteral("prioridad"), textoDeFila(fila, 6));
        cita.insert(QStringLiteral("emergencia"), enteroDeFila(fila, 7) == 1);
        cita.insert(QStringLiteral("mascotaUuid"), textoDeFila(fila, 8));
        lista.append(cita);
    }
    return lista;
}

int contar(fuzzy::core::IQueryExecutor& db, const QString& sql,
           const std::vector<QVariant>& params = {})
{
    const auto filas = db.query(sql, params);
    return filas.empty() ? 0 : static_cast<int>(filas.front().values.front().toLongLong());
}

double sumar(fuzzy::core::IQueryExecutor& db, const QString& sql,
             const std::vector<QVariant>& params = {})
{
    const auto filas = db.query(sql, params);
    return filas.empty() ? 0.0 : filas.front().values.front().toDouble();
}

} // namespace

PanelController::PanelController(fuzzy::core::IQueryExecutor& db, QObject* parent)
    : QObject(parent), db_(db)
{
    cargar();
}

QString PanelController::ingresosHoyTexto() const
{
    return moneda(ingresosHoy_);
}

QString PanelController::fechaHoy() const
{
    const QLocale locale(QLocale::Spanish, QLocale::Colombia);
    return locale.toString(fuzzy::desktop::fechaHoy(), QStringLiteral("dddd d 'de' MMMM"));
}

void PanelController::cargar()
{
    const QDate hoy = fuzzy::desktop::fechaHoy();
    const qint64 desde = inicioDiaMs(hoy);
    const qint64 hasta = finDiaMs(hoy);

    citasHoy_ = contar(
        db_,
        QStringLiteral("SELECT count(*) FROM citas c "
                       "JOIN estados_cita e ON e.id = c.estado_cita_id "
                       "WHERE c.deleted_at IS NULL AND c.fecha_hora_programada BETWEEN ? AND ? "
                       "AND e.nombre_estado <> 'Cancelada'"),
        {QVariant::fromValue(desde), QVariant::fromValue(hasta)});
    enEspera_ = contar(
        db_,
        QStringLiteral("SELECT count(*) FROM citas c "
                       "JOIN estados_cita e ON e.id = c.estado_cita_id "
                       "WHERE c.deleted_at IS NULL AND c.fecha_hora_programada BETWEEN ? AND ? "
                       "AND e.nombre_estado = 'En espera'"),
        {QVariant::fromValue(desde), QVariant::fromValue(hasta)});
    enCurso_ = contar(
        db_,
        QStringLiteral("SELECT count(*) FROM citas c "
                       "JOIN estados_cita e ON e.id = c.estado_cita_id "
                       "WHERE c.deleted_at IS NULL AND c.fecha_hora_programada BETWEEN ? AND ? "
                       "AND e.nombre_estado = 'En curso'"),
        {QVariant::fromValue(desde), QVariant::fromValue(hasta)});
    pacientesActivos_ = contar(
        db_, QStringLiteral("SELECT count(*) FROM mascotas "
                            "WHERE deleted_at IS NULL AND activo = 1"));
    stockCritico_ = contar(
        db_, QStringLiteral("SELECT count(*) FROM productos_insumos "
                            "WHERE deleted_at IS NULL AND stock_actual <= stock_minimo"));
    ingresosHoy_ = sumar(
        db_,
        QStringLiteral("SELECT coalesce(sum(monto_total), 0) FROM facturas "
                       "WHERE deleted_at IS NULL AND estado = 'Pagada' "
                       "AND fecha_emision BETWEEN ? AND ?"),
        {QVariant::fromValue(desde), QVariant::fromValue(hasta)});

    agendaHoy_ = citasEntre(db_, desde, hasta, false, 12);
    salaEspera_ = citasEntre(db_, desde, hasta, true, 8);

    // El gráfico cubre la semana en curso (lunes a domingo), igual que la
    // agenda semanal. Antes mostraba los últimos siete días, que con la
    // semana empezando en lunes dejaba el día actual al final y el resto
    // como días pasados.
    graficoSemana_.clear();
    int maximo = 1;
    QList<int> totales;
    QList<QDate> fechas;
    const QDate lunes = hoy.addDays(1 - hoy.dayOfWeek());
    for (int dias = 0; dias < 7; ++dias) {
        const QDate fecha = lunes.addDays(dias);
        const int total = contar(
            db_,
            QStringLiteral("SELECT count(*) FROM citas "
                           "WHERE deleted_at IS NULL AND fecha_hora_programada BETWEEN ? AND ?"),
            {QVariant::fromValue(inicioDiaMs(fecha)),
             QVariant::fromValue(finDiaMs(fecha))});
        totales.append(total);
        fechas.append(fecha);
        maximo = qMax(maximo, total);
    }
    const QLocale locale(QLocale::Spanish, QLocale::Colombia);
    for (int i = 0; i < totales.size(); ++i) {
        QVariantMap punto;
        punto.insert(QStringLiteral("dia"),
                     locale.toString(fechas.at(i), QStringLiteral("ddd")));
        punto.insert(QStringLiteral("fecha"),
                     locale.toString(fechas.at(i), QStringLiteral("dd/MM")));
        punto.insert(QStringLiteral("total"), totales.at(i));
        punto.insert(QStringLiteral("porcentaje"),
                     static_cast<int>(100.0 * totales.at(i) / maximo));
        graficoSemana_.append(punto);
    }

    const auto alertas = db_.query(
        QStringLiteral("SELECT uuid, nombre_comercial, stock_actual, stock_minimo "
                       "FROM productos_insumos "
                       "WHERE deleted_at IS NULL AND stock_actual <= stock_minimo "
                       "ORDER BY (stock_actual - stock_minimo), nombre_comercial LIMIT 5"));
    alertasInventario_ = listaDeFilas(
        alertas, {QStringLiteral("uuid"), QStringLiteral("nombre"),
                  QStringLiteral("stock"), QStringLiteral("minimo")});

    emit datosCambiados();
}

} // namespace fuzzy::desktop
