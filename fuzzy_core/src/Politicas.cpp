#include "fuzzy/core/Politicas.h"

#include <algorithm>
#include <map>

namespace fuzzy::core {

bool Politicas::exigir(const Sesion& sesion, const Permiso permiso, const ContextoPermiso& contexto) const
{
    const std::vector<Permiso> permisos = permisosDe(sesion.rol);
    if (std::find(permisos.begin(), permisos.end(), permiso) == permisos.end()) {
        return false;
    }
    if (!accionPermitida(permiso, contexto.accion)) {
        return false;
    }
    // Aislamiento por clínica: el contexto solo opera dentro de la clínica
    // de la sesión. Un Admin de la clínica A no ve la clínica B.
    if (!sesion.clinica_id.isEmpty() && !contexto.clinica_id.isEmpty()
        && contexto.clinica_id != sesion.clinica_id) {
        return false;
    }
    return true;
}

std::vector<Permiso> Politicas::permisosDe(const Rol rol) const
{
    static const std::map<Rol, std::vector<Permiso>> matriz = {
        {Rol::Admin, {Permiso::VerPanel,           Permiso::GestionarCitas,
                     Permiso::RegistrarConsulta, Permiso::RegistrarCliente, Permiso::EmitirFactura,
                     Permiso::CerrarCaja,        Permiso::GestionarInventario, Permiso::SuministrarMedicamento,
                     Permiso::GestionarUsuarios, Permiso::ConfigurarClinica, Permiso::AprobarVinculo,
                     Permiso::VerAuditoria,     Permiso::RegistrarEstetica}},
        {Rol::Veterinario, {Permiso::VerPanel, Permiso::GestionarCitas, Permiso::RegistrarConsulta,
                            Permiso::GestionarInventario, Permiso::SuministrarMedicamento}},
        {Rol::Recepcion, {Permiso::VerPanel, Permiso::GestionarCitas, Permiso::RegistrarCliente,
                          Permiso::EmitirFactura, Permiso::CerrarCaja, Permiso::GestionarInventario,
                          Permiso::AprobarVinculo}},
        {Rol::Estilista, {Permiso::VerPanel, Permiso::GestionarCitas, Permiso::GestionarInventario}},
        {Rol::Cliente,  {}},
        {Rol::Desconocido, {}},
    };

    const auto entrada = matriz.find(rol);
    if (entrada == matriz.end()) {
        return {};
    }
    return entrada->second;
}

bool Politicas::accionPermitida(const Permiso permiso, const Accion accion) const
{
    switch (permiso) {
    case Permiso::VerPanel:
        return accion == Accion::Leer;
    case Permiso::GestionarCitas:
        return accion == Accion::Crear || accion == Accion::Leer || accion == Accion::Actualizar
            || accion == Accion::Borrar;
    case Permiso::RegistrarConsulta:
        return accion == Accion::Crear;
    case Permiso::RegistrarCliente:
        return accion == Accion::Crear;
    case Permiso::EmitirFactura:
        return accion == Accion::Crear;
    case Permiso::CerrarCaja:
        return accion == Accion::Crear;
    case Permiso::GestionarInventario:
        return accion == Accion::Crear || accion == Accion::Leer || accion == Accion::Actualizar
            || accion == Accion::Borrar;
    case Permiso::SuministrarMedicamento:
        return accion == Accion::Crear;
    case Permiso::GestionarUsuarios:
        return accion == Accion::Crear || accion == Accion::Leer || accion == Accion::Actualizar
            || accion == Accion::Borrar;
    case Permiso::ConfigurarClinica:
        return accion == Accion::Crear || accion == Accion::Actualizar;
    case Permiso::AprobarVinculo:
        return accion == Accion::Crear;
    case Permiso::VerAuditoria:
        return accion == Accion::Leer;
    case Permiso::RegistrarEstetica:
        return accion == Accion::Crear;
    }
    return false;
}

} // namespace fuzzy::core
