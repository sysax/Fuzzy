#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QString>

namespace fuzzy::core {

class PortalBaseStore;

struct ResultadoProyeccion
{
    bool ok = false;
    int filas = 0;
    QString error;
};

// Proyecta en la base de un dueño únicamente los datos de su ficha. Es
// idempotente por uuid y deja tombstones para las filas desaparecidas; nunca
// toca `solicitudes_cita`, `mascota_fotos` ni datos escritos por la app.
class PortalProjector
{
public:
    PortalProjector(IQueryExecutor& clinica, PortalBaseStore& bases);

    ResultadoProyeccion proyectar(const QString& portal_uuid);

private:
    IQueryExecutor& clinica_;
    PortalBaseStore& bases_;
};

} // namespace fuzzy::core
