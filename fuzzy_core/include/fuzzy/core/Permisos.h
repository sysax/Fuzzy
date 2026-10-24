#pragma once

#include <QString>

namespace fuzzy::core {

// Roles del sistema (RBAC). El rol y el ámbito salen de la sesión,
// nunca del cuerpo de la petición (D25, docs/matriz-autorizacion.md).
enum class Rol {
    Desconocido = 0,
    Admin,
    Veterinario,
    Recepcion,
    Estilista,
    Cliente, // dueño de mascota: sesión emitida desde el portal
};

// Acciones sobre recursos: crear, leer, actualizar, borrar.
enum class Accion {
    Crear = 0,
    Leer,
    Actualizar,
    Borrar,
};

// Permisos del sistema. Conversión del enum Accion de FuzzyApp y de las
// políticas RLS de Supabase al modelo de clínica única (sin identidad global,
// sin base de control, aislamiento por clínica y por portal).
enum class Permiso {
    VerPanel = 0,
    GestionarCitas,
    RegistrarConsulta,
    RegistrarCliente,
    EmitirFactura,
    CerrarCaja,
    GestionarInventario,
    SuministrarMedicamento,
    GestionarUsuarios,
    ConfigurarClinica,
    AprobarVinculo,
    VerAuditoria,
    RegistrarEstetica,
};

// Sesión verificada: el rol y el ámbito (clínica) salen de la sesión.
struct Sesion {
    Rol rol = Rol::Desconocido;
    QString clinica_id;
    QString cuenta_id; // persona dueña de los recursos de alcance "propio"
};

// Contexto de la operación pedida: recurso, acción y ámbito.
struct ContextoPermiso {
    QString recurso;
    Accion accion = Accion::Leer;
    QString recurso_id;
    QString clinica_id;
};

} // namespace fuzzy::core
