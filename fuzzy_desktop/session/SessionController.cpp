#include "session/SessionController.h"

namespace fuzzy::desktop {

using fuzzy::core::Accion;
using fuzzy::core::ContextoPermiso;
using fuzzy::core::Permiso;
using fuzzy::core::ResultadoAuth;
using fuzzy::core::ResultadoOperacion;

namespace {

struct PermisoNombre
{
    Permiso permiso;
    const char* nombre;
};

const PermisoNombre kPermisos[] = {
    {Permiso::VerPanel, "VerPanel"},
    {Permiso::GestionarCitas, "GestionarCitas"},
    {Permiso::RegistrarConsulta, "RegistrarConsulta"},
    {Permiso::RegistrarCliente, "RegistrarCliente"},
    {Permiso::EmitirFactura, "EmitirFactura"},
    {Permiso::CerrarCaja, "CerrarCaja"},
    {Permiso::GestionarInventario, "GestionarInventario"},
    {Permiso::SuministrarMedicamento, "SuministrarMedicamento"},
    {Permiso::GestionarUsuarios, "GestionarUsuarios"},
    {Permiso::ConfigurarClinica, "ConfigurarClinica"},
    {Permiso::AprobarVinculo, "AprobarVinculo"},
    {Permiso::VerAuditoria, "VerAuditoria"},
    {Permiso::RegistrarEstetica, "RegistrarEstetica"},
};

Accion accionPara(Permiso permiso)
{
    switch (permiso) {
    case Permiso::VerPanel:
    case Permiso::GestionarCitas:
    case Permiso::GestionarInventario:
    case Permiso::GestionarUsuarios:
    case Permiso::VerAuditoria:
        return Accion::Leer;
    case Permiso::ConfigurarClinica:
        return Accion::Actualizar;
    default:
        return Accion::Crear;
    }
}

} // namespace

SessionController::SessionController(fuzzy::core::AuthService& auth, QObject* parent)
    : QObject(parent), auth_(auth)
{
    hayInstalacion_ = auth_.hayInstalacion();
    refrescarClinica();
}

void SessionController::refrescarClinica()
{
    const auto datos = auth_.clinica();
    nombreClinica_ = datos.nombre;
}

void SessionController::aplicar(const ResultadoAuth& resultado)
{
    if (!resultado.ok) {
        error_ = resultado.error;
        emit errorCambiado();
        return;
    }

    sesion_ = resultado.sesion;
    nombre_ = resultado.nombre;
    correo_ = resultado.correo;
    debeCambiarPassword_ = resultado.debeCambiarPassword;
    hayInstalacion_ = true;
    error_.clear();
    mensaje_.clear();
    refrescarClinica();
    emit instalacionCambiada();
    emit errorCambiado();
    emit mensajeCambiado();
    emit sesionCambiada();
}

void SessionController::bootstrap(const QString& nombreClinica, const QString& ciudad,
                                  const QString& nombreAdmin, const QString& correo,
                                  const QString& password)
{
    aplicar(auth_.bootstrap(nombreClinica, ciudad, nombreAdmin, correo, password));
}

void SessionController::iniciarSesion(const QString& correo, const QString& password)
{
    aplicar(auth_.iniciarSesion(correo, password));
}

void SessionController::cambiarPassword(const QString& actual, const QString& nueva)
{
    const ResultadoOperacion resultado =
        auth_.cambiarPassword(sesion_.cuenta_id, actual, nueva);
    if (!resultado.ok) {
        error_ = resultado.error;
        emit errorCambiado();
        return;
    }

    debeCambiarPassword_ = false;
    error_.clear();
    mensaje_ = QStringLiteral("Contraseña actualizada.");
    emit errorCambiado();
    emit mensajeCambiado();
    emit sesionCambiada();
}

void SessionController::cerrarSesion()
{
    sesion_ = {};
    nombre_.clear();
    correo_.clear();
    debeCambiarPassword_ = false;
    error_.clear();
    mensaje_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
    emit sesionCambiada();
}

void SessionController::limpiarMensajes()
{
    error_.clear();
    mensaje_.clear();
    emit errorCambiado();
    emit mensajeCambiado();
}

bool SessionController::puede(const QString& permiso) const
{
    if (!autenticado()) {
        return false;
    }
    bool encontrado = false;
    const Permiso valor = permisoDesdeNombre(permiso, &encontrado);
    if (!encontrado) {
        return false;
    }
    ContextoPermiso contexto;
    contexto.recurso = QStringLiteral("ui");
    contexto.accion = accionPara(valor);
    contexto.clinica_id = sesion_.clinica_id;
    return politicas_.exigir(sesion_, valor, contexto);
}

Permiso SessionController::permisoDesdeNombre(const QString& nombre, bool* encontrado)
{
    for (const auto& entrada : kPermisos) {
        if (nombre == QLatin1String(entrada.nombre)) {
            *encontrado = true;
            return entrada.permiso;
        }
    }
    *encontrado = false;
    return Permiso::VerPanel;
}

} // namespace fuzzy::desktop
