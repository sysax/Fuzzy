#pragma once

#include <QString>

namespace fuzzy::core {

// Almacén de las bases de portal. En Turso la base se destruye con el CLI
// (`turso db destroy fuzzy-portal-<cuenta>`); en local es un archivo SQLite.
// El servicio de portales usa esta abstracción para que el borrado lógico
// también elimine la base, sin acoplar el dominio al transporte.
class PortalBaseStore
{
public:
    virtual ~PortalBaseStore() = default;

    // Elimina la base del portal. Devuelve true si al terminar no queda base.
    virtual bool eliminar(const QString& portal_uuid) = 0;
};

// Implementación local: `fuzzy-portal-<uuid>.db` dentro de un directorio.
// Solo acepta UUIDs, de modo que un token no pueda escapar del directorio.
class LocalPortalBaseStore final : public PortalBaseStore
{
public:
    explicit LocalPortalBaseStore(QString directorio);

    bool eliminar(const QString& portal_uuid) override;

    // Ruta de la base local del portal; vacía si el uuid no es válido.
    QString rutaDe(const QString& portal_uuid) const;

private:
    QString directorio_;
};

} // namespace fuzzy::core
