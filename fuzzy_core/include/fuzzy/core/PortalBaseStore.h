#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QString>

#include <memory>

namespace fuzzy::core {

// Almacén de las bases de portal. En Turso la base se destruye con el CLI
// (`turso db destroy fuzzy-portal-<cuenta>`); en local es un archivo SQLite.
// El servicio de portales usa esta abstracción para que el borrado lógico
// también elimine la base, sin acoplar el dominio al transporte.
class PortalBaseStore
{
public:
    virtual ~PortalBaseStore() = default;

    // Crea el archivo de la base (sin esquema) si no existe.
    virtual bool crear(const QString& portal_uuid) = 0;

    // Elimina la base del portal. Devuelve true si al terminar no queda base.
    virtual bool eliminar(const QString& portal_uuid) = 0;

    // Abre la base para aplicar esquema, proyectar o recolectar solicitudes.
    virtual std::unique_ptr<IQueryExecutor> abrir(const QString& portal_uuid) = 0;

    virtual bool existe(const QString& portal_uuid) const = 0;
};

// Implementación local: `fuzzy-portal-<uuid>.db` dentro de un directorio.
// Solo acepta UUIDs, de modo que un token no pueda escapar del directorio.
class LocalPortalBaseStore final : public PortalBaseStore
{
public:
    explicit LocalPortalBaseStore(QString directorio);

    bool crear(const QString& portal_uuid) override;
    bool eliminar(const QString& portal_uuid) override;
    std::unique_ptr<IQueryExecutor> abrir(const QString& portal_uuid) override;
    bool existe(const QString& portal_uuid) const override;

    // Ruta de la base local del portal; vacía si el uuid no es válido.
    QString rutaDe(const QString& portal_uuid) const;

private:
    QString directorio_;
};

} // namespace fuzzy::core
