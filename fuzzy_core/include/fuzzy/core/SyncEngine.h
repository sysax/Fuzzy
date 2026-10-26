#pragma once

#include "fuzzy/core/IQueryExecutor.h"
#include "fuzzy/core/SyncOutbox.h"

#include <QString>

namespace fuzzy::core {

struct ResultadoSync
{
    bool ok = false;
    int detectados = 0;
    int empujados = 0;
    int aplicados = 0;
    int aplazados = 0;
    QString error;
};

// Motor de sincronización entre un nodo y la base compartida de la clínica.
//
// Empujar es idempotente por `event_uuid`: si el proceso muere después de
// publicar y antes de marcar el outbox, el siguiente lote vuelve a insertar
// el mismo evento y el change log lo descarta. Jalar avanza un marcador de
// agua que nunca deja atrás un evento aplazado (por ejemplo, un hijo cuyo
// padre aún no llegó).
class SyncEngine
{
public:
    SyncEngine(IQueryExecutor& local, IQueryExecutor& remoto, QString nodo_uuid);

    // Verifica que las tablas de sync existan en ambos extremos y registra la
    // identidad del nodo en `sync_meta`.
    bool preparar(QString* error = nullptr);

    // detectarPendientes + empujar + jalar.
    ResultadoSync sincronizar(int limite = 500);

    ResultadoSync empujar(int limite = 500);
    ResultadoSync jalar(int limite = 500);

    // Encola explícitamente una fila (usado por los servicios tras escribir).
    bool registrarCambio(const QString& entidad, const QString& uuid);

    SyncOutbox& outbox() { return outbox_; }

private:
    IQueryExecutor& local_;
    IQueryExecutor& remoto_;
    QString nodo_uuid_;
    SyncOutbox outbox_;
};

} // namespace fuzzy::core
