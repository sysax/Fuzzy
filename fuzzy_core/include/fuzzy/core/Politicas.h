#pragma once

#include "fuzzy/core/Permisos.h"

#include <string>
#include <vector>

namespace fuzzy::core {

// Políticas de autorización (RBAC), convertidas desde las políticas RLS.
// Una sola función, exigir, usada por los servicios locales y por el módulo
// servidor. El rol y el ámbito salen de la sesión; el recurso, de la operación.
class Politicas
{
public:
    // ¿La sesión puede realizar la acción sobre el recurso?
    // Reglas: (1) el rol debe tener el permiso; (2) la acción debe estar
    // permitida para el permiso; (3) el contexto debe pertenecer a la clínica
    // de la sesión (aislamiento por clínica).
    bool exigir(const Sesion& sesion, const Permiso permiso, const ContextoPermiso& contexto) const;

    // Permisos efectivos de un rol: matriz rol × permiso.
    std::vector<Permiso> permisosDe(const Rol rol) const;

private:
    bool accionPermitida(const Permiso permiso, const Accion accion) const;
};

} // namespace fuzzy::core
