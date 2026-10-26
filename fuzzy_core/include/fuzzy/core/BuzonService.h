#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QString>

namespace fuzzy::core {

// Buzón de altas de primera visita (`fuzzy-inbox-<clinica>.altas`).
// La app escribe aquí antes de tener portal; el nodo consume y borra la fila.
// El consumo es idempotente por `uuid` y está acotado a la clínica: una fila
// se procesa una sola vez y ninguna sede consume altas de otra.
class BuzonService
{
public:
    explicit BuzonService(IQueryExecutor& executor);

    // Consume (borra) un alta de la clínica. Devuelve true solo la primera vez;
    // las llamadas siguientes son no-ops y devuelven false.
    bool consumir(const QString& clinica_uuid, const QString& uuid);

private:
    IQueryExecutor& executor_;
};

} // namespace fuzzy::core
