#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QString>
#include <QVector>

#include <vector>

namespace fuzzy::core {

struct EventoSalida
{
    qint64 id = 0;
    QString event_uuid;
    QString entidad;
    QString entidad_uuid;
    QString operacion;
    QString payload;
    qint64 updated_at = 0;
};

// Cola local de cambios pendientes de publicar. La clave única
// (entidad, entidad_uuid) hace que varias ediciones seguidas de la misma fila
// solo conserven la última; el change log conserva el historial.
class SyncOutbox
{
public:
    SyncOutbox(IQueryExecutor& local, QString nodo_uuid);

    QString nodoUuid() const { return nodo_uuid_; }

    // Serializa la fila actual y la encola. Devuelve false si la fila no
    // existe o si el esquema de sync no está aplicado.
    bool registrar(const QString& entidad, const QString& uuid);

    // Recorre las entidades replicables y encola las filas marcadas
    // `sync_state = 'pendiente'`. Devuelve cuántas se encolaron.
    int detectarPendientes(int limite = 1000);

    std::vector<EventoSalida> pendientes(int limite = 500) const;

    // Marca el evento como publicado. Si la fila local cambió mientras tanto
    // (updated_at distinto), la deja pendiente para el siguiente lote.
    bool marcarEnviado(const EventoSalida& evento);

    bool marcarError(const EventoSalida& evento, const QString& error);

private:
    IQueryExecutor& local_;
    QString nodo_uuid_;
};

} // namespace fuzzy::core
