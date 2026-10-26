#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace fuzzy::core {

struct MigracionSql
{
    QString version;
    QString nombre;
    QString sql;
};

// Runner de migraciones embebido (D14). Aplica los archivos *.sql de un
// directorio (o recurso Qt) en orden y registra cada versión en
// `_fuzzy_migraciones`. Es idempotente: una versión aplicada no se repite.
class MigrationRunner
{
public:
    explicit MigrationRunner(QString directorio);

    // Migraciones del portal, incrustadas en el binario como recurso Qt.
    static MigrationRunner portal();

    bool valido() const { return !migraciones_.isEmpty(); }
    QString directorio() const { return directorio_; }
    QString versionActual() const;
    QStringList versiones() const;
    QList<MigracionSql> migraciones() const { return migraciones_; }

    // Crea el historial si falta y aplica solo las versiones pendientes.
    bool aplicar(IQueryExecutor& executor) const;

private:
    void cargar();

    QString directorio_;
    QList<MigracionSql> migraciones_;
};

} // namespace fuzzy::core
