#include "fuzzy/core/BuzonService.h"

#include <QVariant>

namespace fuzzy::core {

BuzonService::BuzonService(IQueryExecutor& executor)
    : executor_(executor)
{
}

bool BuzonService::consumir(const QString& clinica_uuid, const QString& uuid)
{
    if (clinica_uuid.isEmpty() || uuid.isEmpty()) {
        return false;
    }

    // El DELETE acotado por clínica y uuid es la operación atómica: solo la
    // primera llamada afecta una fila; las siguientes devuelven 0.
    const ExecResult consumido = executor_.execute(
        QStringLiteral("DELETE FROM altas WHERE uuid = ? AND clinica_uuid = ?"),
        {QVariant(uuid), QVariant(clinica_uuid)});
    return consumido.affected == 1;
}

} // namespace fuzzy::core
