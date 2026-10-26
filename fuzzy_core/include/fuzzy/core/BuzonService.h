#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace fuzzy::core {

struct AltaRecolectada
{
    QString alta_uuid;
    QString cliente_uuid;
    QString mascota_uuid;
    QString cita_uuid;
};

struct ResultadoRecoleccion
{
    bool ok = false;
    int procesadas = 0;
    int rechazadas = 0;
    QStringList errores;
    QList<AltaRecolectada> altas;
};

// Buzón de altas de primera visita (`fuzzy-inbox-<clinica>.altas`).
// La app escribe aquí antes de tener portal; el nodo consume y borra la fila.
// El consumo es idempotente por `uuid` y está acotado a la clínica: una fila
// se procesa una sola vez y ninguna sede consume altas de otra.
class BuzonService
{
public:
    // `clinica` es el ejecutor de la base de la sede; solo hace falta para
    // `recolectar`, no para `consumir`.
    explicit BuzonService(IQueryExecutor& executor, IQueryExecutor* clinica = nullptr);

    // Consume (borra) un alta de la clínica. Devuelve true solo la primera vez;
    // las llamadas siguientes son no-ops y devuelven false.
    bool consumir(const QString& clinica_uuid, const QString& uuid);

    // Convierte cada alta en ficha provisional + mascota + cita de primera
    // visita y borra la fila del buzón. Es idempotente: los uuid se derivan
    // del uuid del alta, así que reintentar tras un corte no duplica nada.
    ResultadoRecoleccion recolectar(const QString& clinica_uuid, qint64 ahora = 0);

private:
    IQueryExecutor& executor_;
    IQueryExecutor* clinica_ = nullptr;
};

} // namespace fuzzy::core
