#pragma once

#include "fuzzy/core/PortalTokenService.h"

#include <QJsonObject>
#include <QString>

#include <memory>
#include <optional>

namespace fuzzy::core {

class PortalBaseStore;

// Crea una base de portal completa: registro en `portales`, archivo SQLite,
// esquema del portal y fila única de `portal_meta`. Devuelve también el
// contenido del QR de emparejamiento y su huella corta.
class PortalProvisioner
{
public:
    struct Creado
    {
        QString portal_uuid;
        QString token;
        QString url;
        QString huella;
        QJsonObject qr;
    };

    PortalProvisioner(IQueryExecutor& clinica, PortalTokenService& tokens,
                      PortalBaseStore& bases);

    // Si `token` está vacío se genera uno local. En Turso el llamador pasa el
    // token emitido por la base.
    std::optional<Creado> crear(const QString& clinica_uuid, qint64 cuenta_id,
                                qint64 cliente_id, const QString& url,
                                const QString& token = QString());

    // Migración perezosa antes de proyectar (fase 5B). Actualiza
    // `portal_meta.esquema_version` sin tocar la identidad del portal.
    bool migrar(const QString& portal_uuid);

private:
    IQueryExecutor& clinica_;
    PortalTokenService& tokens_;
    PortalBaseStore& bases_;
};

} // namespace fuzzy::core
