#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QList>
#include <QString>

namespace fuzzy::core {

class PortalBaseStore;

struct ResultadoSolicitud
{
    QString solicitud_uuid;
    QString estado;
    QString cita_uuid;
    QString respuesta;
};

struct ResultadoSolicitudes
{
    bool ok = false;
    QList<ResultadoSolicitud> procesadas;
    QString error;
};

// Ida y vuelta de las solicitudes de cita del portal (D17, §4.4).
//
// La app inserta en `solicitudes_cita`; el nodo valida fecha, mascota y
// agenda, crea la cita en la clínica y escribe el resultado en la misma fila
// del portal. El uuid de la cita se deriva del uuid de la solicitud, así que
// reintentar tras un corte no duplica la cita.
class SolicitudCitaService
{
public:
    SolicitudCitaService(IQueryExecutor& clinica, PortalBaseStore& bases);

    ResultadoSolicitudes procesar(const QString& portal_uuid, qint64 ahora = 0);

private:
    IQueryExecutor& clinica_;
    PortalBaseStore& bases_;
};

} // namespace fuzzy::core
