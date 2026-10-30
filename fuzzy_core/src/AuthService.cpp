#include "fuzzy/core/AuthService.h"

#include "fuzzy/core/PasswordHasher.h"
#include "fuzzy/core/Uuid.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QVariant>

namespace fuzzy::core {

namespace {

const QString kRolAdmin = QStringLiteral("Admin");

void asignarError(ResultadoAuth& resultado, const QString& texto)
{
    resultado.ok = false;
    resultado.error = texto;
}

void asignarError(ResultadoOperacion& resultado, const QString& texto)
{
    resultado.ok = false;
    resultado.error = texto;
}

bool correoValido(const QString& correo)
{
    static const QRegularExpression patron(
        QStringLiteral("^[^\\s@]+@[^\\s@]+\\.[^\\s@]+$"));
    return patron.match(correo).hasMatch();
}

QString normalizarCorreo(const QString& correo)
{
    return correo.trimmed().toLower();
}

qint64 ahoraMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

QString textoDe(const QueryRow& fila, int indice)
{
    if (indice < 0 || indice >= static_cast<int>(fila.values.size())) {
        return {};
    }
    return fila.values[static_cast<size_t>(indice)].toString();
}

} // namespace

AuthService::AuthService(IQueryExecutor& clinica) : clinica_(clinica)
{
}

bool AuthService::hayInstalacion() const
{
    const auto filas = clinica_.query(
        QStringLiteral("SELECT (SELECT count(*) FROM instalacion) "
                       "+ (SELECT count(*) FROM clinicas)"));
    return !filas.empty() && !filas.front().values.empty()
        && filas.front().values.front().toLongLong() > 0;
}

DatosClinica AuthService::clinica() const
{
    DatosClinica datos;
    const auto filas = clinica_.query(
        QStringLiteral("SELECT uuid, nombre, ciudad, direccion, telefono "
                       "FROM clinicas ORDER BY id LIMIT 1"));
    if (filas.empty()) {
        return datos;
    }
    const auto& fila = filas.front();
    datos.uuid = textoDe(fila, 0);
    datos.nombre = textoDe(fila, 1);
    datos.ciudad = textoDe(fila, 2);
    datos.direccion = textoDe(fila, 3);
    datos.telefono = textoDe(fila, 4);
    return datos;
}

ResultadoAuth AuthService::bootstrap(const QString& nombreClinica, const QString& ciudad,
                                     const QString& nombreAdmin, const QString& correo,
                                     const QString& password)
{
    ResultadoAuth resultado;

    if (hayInstalacion()) {
        asignarError(resultado, QStringLiteral("la instalación ya existe"));
        return resultado;
    }

    const QString clinicaLimpia = nombreClinica.trimmed();
    const QString adminLimpio = nombreAdmin.trimmed();
    const QString correoLimpio = normalizarCorreo(correo);
    if (clinicaLimpia.size() < 2) {
        asignarError(resultado, QStringLiteral("el nombre de la clínica es obligatorio"));
        return resultado;
    }
    if (adminLimpio.size() < 3) {
        asignarError(resultado, QStringLiteral("el nombre del administrador es obligatorio"));
        return resultado;
    }
    if (!correoValido(correoLimpio)) {
        asignarError(resultado, QStringLiteral("el correo no es válido"));
        return resultado;
    }
    if (password.size() < kMinPassword) {
        asignarError(resultado,
                     QStringLiteral("la contraseña debe tener al menos 8 caracteres"));
        return resultado;
    }

    const auto rol = clinica_.query(
        QStringLiteral("SELECT id FROM roles WHERE lower(nombre_rol) = 'admin'"));
    if (rol.size() != 1) {
        asignarError(resultado, QStringLiteral("falta el rol Admin en el esquema"));
        return resultado;
    }

    if (!clinica_.execute(QStringLiteral("BEGIN")).ok) {
        asignarError(resultado, QStringLiteral("no se pudo iniciar el alta"));
        return resultado;
    }

    const QString clinicaUuid = uuidNuevo();
    const QString usuarioUuid = uuidNuevo();
    const qint64 ahora = ahoraMs();

    const auto altaClinica = clinica_.execute(
        QStringLiteral("INSERT INTO clinicas "
                       "(uuid, nombre, ciudad, zona_horaria, created_at, updated_at, "
                       " activa, sync_state) "
                       "VALUES (?, ?, ?, 'America/Bogota', ?, ?, 1, 'pendiente')"),
        {QVariant(clinicaUuid), QVariant(clinicaLimpia),
         ciudad.trimmed().isEmpty() ? QVariant() : QVariant(ciudad.trimmed()),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!altaClinica.ok) {
        clinica_.execute(QStringLiteral("ROLLBACK"));
        asignarError(resultado, QStringLiteral("no se pudo crear la clínica"));
        return resultado;
    }

    const auto altaUsuario = clinica_.execute(
        QStringLiteral("INSERT INTO usuarios "
                       "(uuid, nombre_completo, correo, password_hash, rol_id, "
                       " fecha_registro, activo, debe_cambiar_password, created_at, "
                       " updated_at, sync_state) "
                       "VALUES (?, ?, ?, ?, ?, ?, 1, 0, ?, ?, 'pendiente')"),
        {QVariant(usuarioUuid), QVariant(adminLimpio), QVariant(correoLimpio),
         QVariant(PasswordHasher::generar(password)),
         rol.front().values.front(), QVariant::fromValue(ahora),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!altaUsuario.ok) {
        clinica_.execute(QStringLiteral("ROLLBACK"));
        asignarError(resultado, QStringLiteral("no se pudo crear el administrador"));
        return resultado;
    }

    const auto altaInstalacion = clinica_.execute(
        QStringLiteral("INSERT INTO instalacion "
                       "(id, clinica_uuid, bootstrap_at, bootstrap_por, version) "
                       "VALUES (1, ?, ?, ?, 1)"),
        {QVariant(clinicaUuid), QVariant::fromValue(ahora), QVariant(usuarioUuid)});
    if (!altaInstalacion.ok) {
        clinica_.execute(QStringLiteral("ROLLBACK"));
        asignarError(resultado, QStringLiteral("no se pudo registrar la instalación"));
        return resultado;
    }

    if (!clinica_.execute(QStringLiteral("COMMIT")).ok) {
        clinica_.execute(QStringLiteral("ROLLBACK"));
        asignarError(resultado, QStringLiteral("no se pudo confirmar el alta"));
        return resultado;
    }

    auditar(usuarioUuid, QStringLiteral("bootstrap"), QStringLiteral("clinicas"),
            clinicaUuid, QStringLiteral("Alta inicial de la clínica"));

    resultado.ok = true;
    resultado.sesion = Sesion{Rol::Admin, clinicaUuid, usuarioUuid};
    resultado.usuario_uuid = usuarioUuid;
    resultado.nombre = adminLimpio;
    resultado.correo = correoLimpio;
    return resultado;
}

ResultadoAuth AuthService::iniciarSesion(const QString& correo, const QString& password)
{
    ResultadoAuth resultado;
    const QString correoLimpio = normalizarCorreo(correo);
    if (correoLimpio.isEmpty() || password.isEmpty()) {
        asignarError(resultado, QStringLiteral("escribe tu correo y contraseña"));
        return resultado;
    }

    const auto filas = clinica_.query(
        QStringLiteral("SELECT u.uuid, u.nombre_completo, u.correo, u.password_hash, "
                       "u.debe_cambiar_password, u.password_expira_at, u.activo, "
                       "u.deleted_at, r.nombre_rol "
                       "FROM usuarios u JOIN roles r ON r.id = u.rol_id "
                       "WHERE lower(u.correo) = ? LIMIT 1"),
        {QVariant(correoLimpio)});
    if (filas.size() != 1) {
        asignarError(resultado, QStringLiteral("correo o contraseña incorrectos"));
        return resultado;
    }

    const auto& fila = filas.front();
    const QString hash = textoDe(fila, 3);
    if (textoDe(fila, 6) != QStringLiteral("1") || !textoDe(fila, 7).isEmpty()) {
        asignarError(resultado, QStringLiteral("la cuenta está desactivada"));
        return resultado;
    }
    if (!PasswordHasher::verificar(password, hash)) {
        asignarError(resultado, QStringLiteral("correo o contraseña incorrectos"));
        return resultado;
    }

    const bool debeCambiar = textoDe(fila, 4) == QStringLiteral("1");
    const qint64 expira = fila.values.at(5).toLongLong();
    if (debeCambiar && expira > 0 && expira < ahoraMs()) {
        asignarError(resultado,
                     QStringLiteral("la contraseña temporal venció; pide una nueva"));
        return resultado;
    }

    const QString usuarioUuid = textoDe(fila, 0);
    const QString rolNombre = textoDe(fila, 8);
    const Rol rol = rolDesdeNombre(rolNombre);
    if (rol == Rol::Desconocido) {
        asignarError(resultado, QStringLiteral("el rol de la cuenta no es válido"));
        return resultado;
    }

    const qint64 ahora = ahoraMs();
    clinica_.execute(
        QStringLiteral("UPDATE usuarios SET ultimo_acceso = ?, updated_at = ? WHERE uuid = ?"),
        {QVariant::fromValue(ahora), QVariant::fromValue(ahora), QVariant(usuarioUuid)});

    resultado.ok = true;
    resultado.sesion = Sesion{rol, clinicaUuid(), usuarioUuid};
    resultado.usuario_uuid = usuarioUuid;
    resultado.nombre = textoDe(fila, 1);
    resultado.correo = textoDe(fila, 2);
    resultado.debeCambiarPassword = debeCambiar;
    resultado.passwordExpiraAt = expira;
    return resultado;
}

ResultadoOperacion AuthService::cambiarPassword(const QString& usuario_uuid,
                                                const QString& passwordActual,
                                                const QString& passwordNueva)
{
    ResultadoOperacion resultado;
    if (usuario_uuid.isEmpty()) {
        asignarError(resultado, QStringLiteral("falta el usuario"));
        return resultado;
    }
    if (passwordNueva.size() < kMinPassword) {
        asignarError(resultado,
                     QStringLiteral("la nueva contraseña debe tener al menos 8 caracteres"));
        return resultado;
    }
    if (passwordNueva == passwordActual) {
        asignarError(resultado, QStringLiteral("la nueva contraseña debe ser distinta"));
        return resultado;
    }

    const auto filas = clinica_.query(
        QStringLiteral("SELECT id, password_hash FROM usuarios "
                       "WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(usuario_uuid)});
    if (filas.size() != 1) {
        asignarError(resultado, QStringLiteral("el usuario no existe"));
        return resultado;
    }
    if (!PasswordHasher::verificar(passwordActual, textoDe(filas.front(), 1))) {
        asignarError(resultado, QStringLiteral("la contraseña actual no coincide"));
        return resultado;
    }

    const qint64 ahora = ahoraMs();
    const auto cambio = clinica_.execute(
        QStringLiteral("UPDATE usuarios SET password_hash = ?, debe_cambiar_password = 0, "
                       "password_expira_at = NULL, updated_at = ? WHERE uuid = ?"),
        {QVariant(PasswordHasher::generar(passwordNueva)), QVariant::fromValue(ahora),
         QVariant(usuario_uuid)});
    if (!cambio.ok) {
        asignarError(resultado, QStringLiteral("no se pudo cambiar la contraseña"));
        return resultado;
    }

    auditar(usuario_uuid, QStringLiteral("cambio_password"), QStringLiteral("usuarios"),
            usuario_uuid, QStringLiteral("Contraseña actualizada por el usuario"));
    resultado.ok = true;
    resultado.uuid = usuario_uuid;
    return resultado;
}

QList<UsuarioEquipo> AuthService::equipo() const
{
    QList<UsuarioEquipo> equipo;
    const auto filas = clinica_.query(
        QStringLiteral("SELECT u.uuid, u.nombre_completo, u.correo, r.nombre_rol, "
                       "u.activo, u.debe_cambiar_password, u.ultimo_acceso "
                       "FROM usuarios u JOIN roles r ON r.id = u.rol_id "
                       "WHERE u.deleted_at IS NULL "
                       "ORDER BY CASE r.nombre_rol WHEN 'Admin' THEN 1 "
                       "WHEN 'Veterinario' THEN 2 WHEN 'Recepcion' THEN 3 ELSE 4 END, "
                       "u.nombre_completo"));
    for (const auto& fila : filas) {
        UsuarioEquipo usuario;
        usuario.uuid = textoDe(fila, 0);
        usuario.nombre = textoDe(fila, 1);
        usuario.correo = textoDe(fila, 2);
        usuario.rol = textoDe(fila, 3);
        usuario.activo = textoDe(fila, 4) == QStringLiteral("1");
        usuario.debeCambiarPassword = textoDe(fila, 5) == QStringLiteral("1");
        usuario.ultimoAcceso = fila.values.at(6).toLongLong();
        equipo.append(usuario);
    }
    return equipo;
}

ResultadoOperacion AuthService::crearUsuario(const QString& admin_uuid,
                                             const QString& nombre,
                                             const QString& correo,
                                             const QString& rol)
{
    ResultadoOperacion resultado;
    if (!esAdminActivo(admin_uuid)) {
        asignarError(resultado, QStringLiteral("solo un administrador puede crear personal"));
        return resultado;
    }

    const QString nombreLimpio = nombre.trimmed();
    const QString correoLimpio = normalizarCorreo(correo);
    if (nombreLimpio.size() < 3) {
        asignarError(resultado, QStringLiteral("el nombre es obligatorio"));
        return resultado;
    }
    if (!correoValido(correoLimpio)) {
        asignarError(resultado, QStringLiteral("el correo no es válido"));
        return resultado;
    }
    if (rolDesdeNombre(rol) == Rol::Desconocido || rolDesdeNombre(rol) == Rol::Cliente) {
        asignarError(resultado, QStringLiteral("el rol no es válido"));
        return resultado;
    }
    if (correoEnUso(correoLimpio)) {
        asignarError(resultado, QStringLiteral("ya existe una cuenta con ese correo"));
        return resultado;
    }

    const auto rolFila = clinica_.query(
        QStringLiteral("SELECT id FROM roles WHERE nombre_rol = ?"), {QVariant(rol)});
    if (rolFila.size() != 1) {
        asignarError(resultado, QStringLiteral("el rol no existe"));
        return resultado;
    }

    const QString password = PasswordHasher::passwordTemporal();
    const QString usuarioUuid = uuidNuevo();
    const qint64 ahora = ahoraMs();
    const qint64 expira = ahora + kHorasPasswordTemporal * 60 * 60 * 1000;

    const auto alta = clinica_.execute(
        QStringLiteral("INSERT INTO usuarios "
                       "(uuid, nombre_completo, correo, password_hash, rol_id, "
                       " fecha_registro, activo, debe_cambiar_password, password_expira_at, "
                       " created_at, updated_at, sync_state) "
                       "VALUES (?, ?, ?, ?, ?, ?, 1, 1, ?, ?, ?, 'pendiente')"),
        {QVariant(usuarioUuid), QVariant(nombreLimpio), QVariant(correoLimpio),
         QVariant(PasswordHasher::generar(password)), rolFila.front().values.front(),
         QVariant::fromValue(ahora), QVariant::fromValue(expira),
         QVariant::fromValue(ahora), QVariant::fromValue(ahora)});
    if (!alta.ok) {
        asignarError(resultado, QStringLiteral("no se pudo crear la cuenta"));
        return resultado;
    }

    auditar(admin_uuid, QStringLiteral("alta_usuario"), QStringLiteral("usuarios"),
            usuarioUuid, QStringLiteral("Alta con rol %1").arg(rol));
    resultado.ok = true;
    resultado.uuid = usuarioUuid;
    resultado.passwordTemporal = password;
    return resultado;
}

ResultadoOperacion AuthService::actualizarUsuario(const QString& admin_uuid,
                                                  const QString& usuario_uuid,
                                                  const QString& nombre,
                                                  const QString& correo,
                                                  const QString& rol, bool activo)
{
    ResultadoOperacion resultado;
    if (!esAdminActivo(admin_uuid)) {
        asignarError(resultado, QStringLiteral("solo un administrador puede modificar personal"));
        return resultado;
    }
    if (usuario_uuid.isEmpty()) {
        asignarError(resultado, QStringLiteral("falta el usuario"));
        return resultado;
    }

    const QString nombreLimpio = nombre.trimmed();
    const QString correoLimpio = normalizarCorreo(correo);
    if (nombreLimpio.size() < 3) {
        asignarError(resultado, QStringLiteral("el nombre es obligatorio"));
        return resultado;
    }
    if (!correoValido(correoLimpio)) {
        asignarError(resultado, QStringLiteral("el correo no es válido"));
        return resultado;
    }
    const Rol nuevoRol = rolDesdeNombre(rol);
    if (nuevoRol == Rol::Desconocido || nuevoRol == Rol::Cliente) {
        asignarError(resultado, QStringLiteral("el rol no es válido"));
        return resultado;
    }
    if (correoEnUso(correoLimpio, usuario_uuid)) {
        asignarError(resultado, QStringLiteral("ya existe una cuenta con ese correo"));
        return resultado;
    }

    const auto objetivo = clinica_.query(
        QStringLiteral("SELECT u.id, r.nombre_rol FROM usuarios u "
                       "JOIN roles r ON r.id = u.rol_id "
                       "WHERE u.uuid = ? AND u.deleted_at IS NULL"),
        {QVariant(usuario_uuid)});
    if (objetivo.size() != 1) {
        asignarError(resultado, QStringLiteral("el usuario no existe"));
        return resultado;
    }
    const QString rolActual = textoDe(objetivo.front(), 1);

    if (usuario_uuid == admin_uuid && (rol != rolActual || !activo)) {
        asignarError(resultado,
                     QStringLiteral("no puedes cambiar tu propio rol ni desactivarte"));
        return resultado;
    }

    if (rolActual == kRolAdmin && (nuevoRol != Rol::Admin || !activo)) {
        const auto admins = clinica_.query(
            QStringLiteral("SELECT count(*) FROM usuarios u JOIN roles r ON r.id = u.rol_id "
                           "WHERE r.nombre_rol = 'Admin' AND u.activo = 1 "
                           "AND u.deleted_at IS NULL AND u.uuid <> ?"),
            {QVariant(usuario_uuid)});
        if (admins.empty() || admins.front().values.front().toLongLong() == 0) {
            asignarError(resultado,
                         QStringLiteral("no se puede dejar la clínica sin administrador"));
            return resultado;
        }
    }

    const auto rolFila = clinica_.query(
        QStringLiteral("SELECT id FROM roles WHERE nombre_rol = ?"), {QVariant(rol)});
    if (rolFila.size() != 1) {
        asignarError(resultado, QStringLiteral("el rol no existe"));
        return resultado;
    }

    const auto cambio = clinica_.execute(
        QStringLiteral("UPDATE usuarios SET nombre_completo = ?, correo = ?, rol_id = ?, "
                       "activo = ?, updated_at = ? WHERE uuid = ?"),
        {QVariant(nombreLimpio), QVariant(correoLimpio), rolFila.front().values.front(),
         QVariant::fromValue(activo ? 1 : 0),
         QVariant::fromValue(ahoraMs()), QVariant(usuario_uuid)});
    if (!cambio.ok) {
        asignarError(resultado, QStringLiteral("no se pudo actualizar la cuenta"));
        return resultado;
    }

    auditar(admin_uuid, QStringLiteral("edicion_usuario"), QStringLiteral("usuarios"),
            usuario_uuid, QStringLiteral("Rol %1, activo %2").arg(rol).arg(activo));
    resultado.ok = true;
    resultado.uuid = usuario_uuid;
    return resultado;
}

ResultadoOperacion AuthService::restablecerPassword(const QString& admin_uuid,
                                                    const QString& usuario_uuid)
{
    ResultadoOperacion resultado;
    if (!esAdminActivo(admin_uuid)) {
        asignarError(resultado, QStringLiteral("solo un administrador puede restablecer claves"));
        return resultado;
    }

    const auto objetivo = clinica_.query(
        QStringLiteral("SELECT id FROM usuarios WHERE uuid = ? AND deleted_at IS NULL"),
        {QVariant(usuario_uuid)});
    if (objetivo.size() != 1) {
        asignarError(resultado, QStringLiteral("el usuario no existe"));
        return resultado;
    }

    const QString password = PasswordHasher::passwordTemporal();
    const qint64 ahora = ahoraMs();
    const auto cambio = clinica_.execute(
        QStringLiteral("UPDATE usuarios SET password_hash = ?, debe_cambiar_password = 1, "
                       "password_expira_at = ?, updated_at = ? WHERE uuid = ?"),
        {QVariant(PasswordHasher::generar(password)),
         QVariant::fromValue(ahora + kHorasPasswordTemporal * 60 * 60 * 1000),
         QVariant::fromValue(ahora), QVariant(usuario_uuid)});
    if (!cambio.ok) {
        asignarError(resultado, QStringLiteral("no se pudo restablecer la contraseña"));
        return resultado;
    }

    auditar(admin_uuid, QStringLiteral("reset_password"), QStringLiteral("usuarios"),
            usuario_uuid, QStringLiteral("Contraseña temporal emitida"));
    resultado.ok = true;
    resultado.uuid = usuario_uuid;
    resultado.passwordTemporal = password;
    return resultado;
}

ResultadoOperacion AuthService::actualizarClinica(const QString& admin_uuid,
                                                  const QString& nombre,
                                                  const QString& ciudad,
                                                  const QString& direccion,
                                                  const QString& telefono)
{
    ResultadoOperacion resultado;
    if (!esAdminActivo(admin_uuid)) {
        asignarError(resultado, QStringLiteral("solo un administrador puede configurar la clínica"));
        return resultado;
    }
    const QString nombreLimpio = nombre.trimmed();
    if (nombreLimpio.size() < 2) {
        asignarError(resultado, QStringLiteral("el nombre de la clínica es obligatorio"));
        return resultado;
    }

    const QString uuid = clinicaUuid();
    const auto cambio = clinica_.execute(
        QStringLiteral("UPDATE clinicas SET nombre = ?, ciudad = ?, direccion = ?, "
                       "telefono = ?, updated_at = ? WHERE uuid = ?"),
        {QVariant(nombreLimpio),
         ciudad.trimmed().isEmpty() ? QVariant() : QVariant(ciudad.trimmed()),
         direccion.trimmed().isEmpty() ? QVariant()
                                       : QVariant(direccion.trimmed()),
         telefono.trimmed().isEmpty() ? QVariant()
                                      : QVariant(telefono.trimmed()),
         QVariant::fromValue(ahoraMs()), QVariant(uuid)});
    if (!cambio.ok) {
        asignarError(resultado, QStringLiteral("no se pudo actualizar la clínica"));
        return resultado;
    }

    auditar(admin_uuid, QStringLiteral("edicion_clinica"), QStringLiteral("clinicas"),
            uuid, QStringLiteral("Datos de la clínica actualizados"));
    resultado.ok = true;
    resultado.uuid = uuid;
    return resultado;
}

Rol AuthService::rolDesdeNombre(const QString& nombre)
{
    if (nombre == QStringLiteral("Admin")) {
        return Rol::Admin;
    }
    if (nombre == QStringLiteral("Veterinario")) {
        return Rol::Veterinario;
    }
    if (nombre == QStringLiteral("Recepcion")) {
        return Rol::Recepcion;
    }
    if (nombre == QStringLiteral("Estilista")) {
        return Rol::Estilista;
    }
    if (nombre == QStringLiteral("Cliente")) {
        return Rol::Cliente;
    }
    return Rol::Desconocido;
}

QString AuthService::nombreRol(Rol rol)
{
    switch (rol) {
    case Rol::Admin:
        return QStringLiteral("Admin");
    case Rol::Veterinario:
        return QStringLiteral("Veterinario");
    case Rol::Recepcion:
        return QStringLiteral("Recepcion");
    case Rol::Estilista:
        return QStringLiteral("Estilista");
    case Rol::Cliente:
        return QStringLiteral("Cliente");
    case Rol::Desconocido:
        break;
    }
    return {};
}

bool AuthService::esAdminActivo(const QString& usuario_uuid) const
{
    if (usuario_uuid.isEmpty()) {
        return false;
    }
    const auto filas = clinica_.query(
        QStringLiteral("SELECT count(*) FROM usuarios u JOIN roles r ON r.id = u.rol_id "
                       "WHERE u.uuid = ? AND u.activo = 1 AND u.deleted_at IS NULL "
                       "AND r.nombre_rol = 'Admin'"),
        {QVariant(usuario_uuid)});
    return !filas.empty() && !filas.front().values.empty()
        && filas.front().values.front().toLongLong() == 1;
}

bool AuthService::correoEnUso(const QString& correo, const QString& excepto_uuid) const
{
    const auto filas = clinica_.query(
        QStringLiteral("SELECT count(*) FROM usuarios "
                       "WHERE lower(correo) = ? AND deleted_at IS NULL AND uuid <> ?"),
        {QVariant(correo), QVariant(excepto_uuid)});
    return !filas.empty() && !filas.front().values.empty()
        && filas.front().values.front().toLongLong() > 0;
}

qint64 AuthService::idDeUsuario(const QString& uuid) const
{
    if (uuid.isEmpty()) {
        return 0;
    }
    const auto filas = clinica_.query(
        QStringLiteral("SELECT id FROM usuarios WHERE uuid = ?"), {QVariant(uuid)});
    return filas.empty() ? 0 : filas.front().values.front().toLongLong();
}

QString AuthService::clinicaUuid() const
{
    return clinica().uuid;
}

void AuthService::auditar(const QString& usuario_uuid, const QString& accion,
                          const QString& entidad, const QString& entidad_uuid,
                          const QString& detalle)
{
    const qint64 usuarioId = idDeUsuario(usuario_uuid);
    clinica_.execute(
        QStringLiteral("INSERT INTO auditoria "
                       "(uuid, usuario_id, accion, entidad, entidad_uuid, detalle, "
                       " created_at, sync_state) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, 'pendiente')"),
        {QVariant(uuidNuevo()), usuarioId > 0 ? QVariant::fromValue(usuarioId)
                                              : QVariant(),
         QVariant(accion), QVariant(entidad),
         entidad_uuid.isEmpty() ? QVariant() : QVariant(entidad_uuid),
         QVariant(detalle), QVariant::fromValue(ahoraMs())});
}

} // namespace fuzzy::core
