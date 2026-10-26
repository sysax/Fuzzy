#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QString>
#include <QtGlobal>

namespace fuzzy::core {

class PortalBaseStore;

// Ciclo de vida del portal y de su credencial.
//
// `portales.uuid` es la identidad estable del portal (nombre de la base y
// `portal_uuid` del QR). El token nunca se guarda en la base: solo su
// SHA-256 (D27). En Turso el token real lo emite la base y solo abre esa base
// (D18); el secreto vive en el keyring del sistema (nodo) o cifrado con la
// contraseña del dueño (teléfono, D23).
class PortalTokenService
{
public:
    explicit PortalTokenService(IQueryExecutor& executor, PortalBaseStore* bases = nullptr);

    // Registra un portal con estado 'activo' y devuelve el `uuid` estable del
    // portal; vacío si falla. Exige que la clínica sea la de este ejecutor y
    // que la cuenta pertenezca a la ficha indicada; así un portal no puede
    // crearse contra la base de otra sede. Del token solo persiste la huella.
    QString emitir(const QString& clinica_uuid, qint64 cuenta_id, qint64 cliente_id,
                   const QString& url, const QString& token);

    // Verifica el token dentro de la clínica. Denegar si el portal no existe,
    // si está revocado/borrado o si el ejecutor pertenece a otra clínica.
    bool verificar(const QString& token, const QString& clinica_uuid) const;

    // Rota el token en la misma fila: el portal conserva uuid, cuenta, ficha
    // y base. El token anterior deja de verificar de inmediato.
    bool rotar(const QString& token_actual, const QString& token_nuevo);

    // Revoca el token: `portales.estado` -> 'revocado'.
    bool revocar(const QString& token);

    // Borrado lógico y eliminación de la base. Tras esta llamada el token
    // deniega y la base del portal deja de existir.
    bool borrar(const QString& token);

    // Huella corta (6 hex) del token para el QR de emparejamiento.
    static QString huella(const QString& token);

private:
    IQueryExecutor& executor_;
    PortalBaseStore* bases_ = nullptr;
};

} // namespace fuzzy::core
