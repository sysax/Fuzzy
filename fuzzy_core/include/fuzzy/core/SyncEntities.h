#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace fuzzy::core {

// Una columna entera que apunta a otra tabla. El payload no transporta ids
// locales (pueden diferir entre nodos): transporta el uuid del padre y al
// aplicar se resuelve el id local.
struct ReferenciaSync
{
    QString columna;
    QString tabla;
    QString clave;
    bool obligatoria = false;
};

// Entidad replicable y sus columnas de dominio. `id` y `sync_state` quedan
// fuera del payload: el primero es local y el segundo lo gobierna el motor.
struct EntidadSync
{
    QString nombre;
    QString tabla;
    QStringList columnas;
    QList<ReferenciaSync> referencias;
    int prioridad = 100;
};

// Entidades clínicas que participan del sync. El catálogo no viaja: las
// migraciones lo siembran igual en todos los nodos.
const QList<EntidadSync>& entidadesSync();
const EntidadSync* entidadSync(const QString& nombre);

// Serializa una fila a JSON. Los ids locales se sustituyen por uuid de padre.
bool serializarEntidad(IQueryExecutor& local, const EntidadSync& entidad,
                       const QString& uuid, QJsonObject& payload, QString* error = nullptr);

// Aplica un upsert con LWW (`updated_at` mayor o igual gana). Devuelve false
// si falta una referencia obligatoria (el evento queda pendiente para el
// siguiente pull) o si la escritura falla.
bool aplicarUpsert(IQueryExecutor& local, const EntidadSync& entidad,
                   const QJsonObject& payload, QString* error = nullptr);

// Aplica un tombstone. Si la fila no existe se considera aplicado; si la
// copia local es más nueva, LWW la conserva.
bool aplicarDelete(IQueryExecutor& local, const EntidadSync& entidad,
                   const QString& uuid, qint64 updated_at, QString* error = nullptr);

} // namespace fuzzy::core
