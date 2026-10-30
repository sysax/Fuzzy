#pragma once

#include "fuzzy/core/IQueryExecutor.h"
#include "fuzzy/core/Permisos.h"

#include <QList>
#include <QString>

namespace fuzzy::core {

struct ResultadoAuth
{
    bool ok = false;
    QString error;
    Sesion sesion;
    QString usuario_uuid;
    QString nombre;
    QString correo;
    bool debeCambiarPassword = false;
    qint64 passwordExpiraAt = 0;
    QString passwordTemporal;
};

struct UsuarioEquipo
{
    QString uuid;
    QString nombre;
    QString correo;
    QString rol;
    bool activo = false;
    bool debeCambiarPassword = false;
    qint64 ultimoAcceso = 0;
};

struct DatosClinica
{
    QString uuid;
    QString nombre;
    QString ciudad;
    QString direccion;
    QString telefono;
};

struct ResultadoOperacion
{
    bool ok = false;
    QString error;
    QString uuid;
    QString passwordTemporal;
};

// Autenticación local del personal (D4, D6, D8, D16, D19).
//
// No hay proveedor externo: el nodo guarda los usuarios en su SQLite y valida
// con PBKDF2. El bootstrap solo se ofrece cuando no existe instalación ni
// clínica; el último Admin no se puede desactivar ni degradar.
class AuthService
{
public:
    static constexpr int kMinPassword = 8;
    static constexpr qint64 kHorasPasswordTemporal = 72;

    explicit AuthService(IQueryExecutor& clinica);

    bool hayInstalacion() const;
    DatosClinica clinica() const;

    // Crea clínica + primer Admin + fila de instalación en una transacción.
    ResultadoAuth bootstrap(const QString& nombreClinica, const QString& ciudad,
                            const QString& nombreAdmin, const QString& correo,
                            const QString& password);

    ResultadoAuth iniciarSesion(const QString& correo, const QString& password);

    ResultadoOperacion cambiarPassword(const QString& usuario_uuid,
                                       const QString& passwordActual,
                                       const QString& passwordNueva);

    QList<UsuarioEquipo> equipo() const;

    // Solo un Admin activo puede crear o modificar personal.
    ResultadoOperacion crearUsuario(const QString& admin_uuid, const QString& nombre,
                                    const QString& correo, const QString& rol);

    ResultadoOperacion actualizarUsuario(const QString& admin_uuid,
                                         const QString& usuario_uuid,
                                         const QString& nombre, const QString& correo,
                                         const QString& rol, bool activo);

    ResultadoOperacion restablecerPassword(const QString& admin_uuid,
                                           const QString& usuario_uuid);

    ResultadoOperacion actualizarClinica(const QString& admin_uuid,
                                         const QString& nombre, const QString& ciudad,
                                         const QString& direccion,
                                         const QString& telefono);

    static Rol rolDesdeNombre(const QString& nombre);
    static QString nombreRol(Rol rol);

private:
    bool esAdminActivo(const QString& usuario_uuid) const;
    bool correoEnUso(const QString& correo, const QString& excepto_uuid = QString()) const;
    qint64 idDeUsuario(const QString& uuid) const;
    QString clinicaUuid() const;
    void auditar(const QString& usuario_uuid, const QString& accion,
                 const QString& entidad, const QString& entidad_uuid,
                 const QString& detalle);

    IQueryExecutor& clinica_;
};

} // namespace fuzzy::core
