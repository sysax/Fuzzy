#include "app/MobileController.h"

#include "fuzzy/core/BuzonCliente.h"
#include "fuzzy/core/EmparejamientoService.h"
#include "fuzzy/core/MovilStore.h"
#include "fuzzy/core/PortalTokenService.h"
#include "fuzzy/core/TursoHttpExecutor.h"
#include "fuzzy/core/Uuid.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include <QVariant>

#include <algorithm>

namespace fuzzy::mobile {

namespace {

const QString kFormatoFecha = QStringLiteral("dd MMM yyyy");
const QString kFormatoHora = QStringLiteral("HH:mm");

QVariantList aLista(const QList<QVariantMap>& filas)
{
    QVariantList salida;
    salida.reserve(filas.size());
    for (const QVariantMap& fila : filas) {
        salida.append(fila);
    }
    return salida;
}

QString fechaTexto(qint64 ms)
{
    if (ms <= 0) {
        return {};
    }
    return QDateTime::fromMSecsSinceEpoch(ms).toString(kFormatoFecha);
}

QString horaTexto(qint64 ms)
{
    if (ms <= 0) {
        return {};
    }
    return QDateTime::fromMSecsSinceEpoch(ms).toString(kFormatoHora);
}

qint64 msDeFecha(const QString& texto)
{
    const QDate fecha = QDate::fromString(texto.trimmed(), Qt::ISODate);
    if (!fecha.isValid()) {
        return 0;
    }
    return QDateTime(fecha, QTime(12, 0)).toMSecsSinceEpoch();
}

qint64 numero(const QVariantMap& fila, const QString& clave)
{
    return fila.value(clave).toLongLong();
}

QString cadena(const QVariantMap& fila, const QString& clave)
{
    return fila.value(clave).toString();
}

} // namespace

MobileController::MobileController(fuzzy::core::MovilStore& store,
                                   AbridorPortal abridorPortal, AbridorBuzon abridorBuzon,
                                   QObject* parent)
    : QObject(parent),
      store_(store),
      abridor_portal_(std::move(abridorPortal)),
      abridor_buzon_(std::move(abridorBuzon))
{
}

bool MobileController::hayPortales() const
{
    return !store_.portales().isEmpty();
}

QString MobileController::nombrePortal() const
{
    fuzzy::core::PortalConocido portal;
    if (portal_activo_.isEmpty() || !store_.portal(portal_activo_, &portal)) {
        return {};
    }
    return portal.clinica_nombre;
}

int MobileController::pendientes() const
{
    int total = 0;
    for (const fuzzy::core::PortalConocido& portal : store_.portales()) {
        total += store_.pendientesTotales(portal.portal_uuid);
    }
    return total;
}

QVariantMap MobileController::portalMap(const fuzzy::core::PortalConocido& portal) const
{
    QVariantMap mapa;
    mapa.insert(QStringLiteral("uuid"), portal.portal_uuid);
    mapa.insert(QStringLiteral("clinica"), portal.clinica_nombre);
    mapa.insert(QStringLiteral("clinica_uuid"), portal.clinica_uuid);
    mapa.insert(QStringLiteral("estado"), portal.estado);
    mapa.insert(QStringLiteral("ultima_sync"), fechaTexto(portal.ultima_sync));
    mapa.insert(QStringLiteral("pendientes"), store_.pendientesTotales(portal.portal_uuid));
    mapa.insert(QStringLiteral("activo"), portal.portal_uuid == portal_activo_);
    return mapa;
}

QVariantList MobileController::portales() const
{
    QVariantList salida;
    for (const fuzzy::core::PortalConocido& portal : store_.portales()) {
        salida.append(portalMap(portal));
    }
    return salida;
}

QVariantList MobileController::perfil() const
{
    const QList<QVariantMap> filas = store_.filas(portal_activo_, QStringLiteral("perfil"));
    return aLista(filas);
}

QVariantList MobileController::mascotas() const
{
    QVariantList salida;
    for (const fuzzy::core::PortalConocido& portal : store_.portales()) {
        const QList<QVariantMap> fotos = store_.filas(portal.portal_uuid,
                                                      QStringLiteral("mascota_fotos"));
        QHash<QString, QVariantMap> fotoPorMascota;
        for (const QVariantMap& foto : fotos) {
            fotoPorMascota.insert(cadena(foto, QStringLiteral("mascota_uuid")), foto);
        }

        for (QVariantMap mascota : store_.filas(portal.portal_uuid, QStringLiteral("mascotas"))) {
            const QString uuid = cadena(mascota, QStringLiteral("uuid"));
            mascota.insert(QStringLiteral("portal_uuid"), portal.portal_uuid);
            mascota.insert(QStringLiteral("clinica"), portal.clinica_nombre);
            mascota.insert(QStringLiteral("portal_estado"), portal.estado);

            const QVariantMap foto = fotoPorMascota.value(uuid);
            const QString base64 = cadena(foto, QStringLiteral("foto_b64"));
            if (!base64.isEmpty()) {
                const QString mime = cadena(foto, QStringLiteral("mime"));
                mascota.insert(QStringLiteral("foto"),
                               QStringLiteral("data:%1;base64,%2")
                                   .arg(mime.isEmpty() ? QStringLiteral("image/jpeg") : mime,
                                        base64));
            }
            salida.append(mascota);
        }
    }
    return salida;
}

QString MobileController::nombreMascota(const QString& portal_uuid,
                                        const QString& mascota_uuid) const
{
    const QList<QVariantMap> filas = store_.filas(portal_uuid, QStringLiteral("mascotas"));
    for (const QVariantMap& fila : filas) {
        if (cadena(fila, QStringLiteral("uuid")) == mascota_uuid) {
            return cadena(fila, QStringLiteral("nombre"));
        }
    }
    return QStringLiteral("Mascota");
}

QVariantList MobileController::citas() const
{
    QVariantList salida;
    for (QVariantMap cita : store_.filas(portal_activo_, QStringLiteral("citas"))) {
        const QString mascotaUuid = cadena(cita, QStringLiteral("mascota_uuid"));
        cita.insert(QStringLiteral("mascota"),
                    nombreMascota(portal_activo_, mascotaUuid));
        const qint64 cuando = numero(cita, QStringLiteral("fecha_hora"));
        cita.insert(QStringLiteral("fecha"), fechaTexto(cuando));
        cita.insert(QStringLiteral("hora"), horaTexto(cuando));
        cita.insert(QStringLiteral("_orden"), cuando);
        salida.append(cita);
    }
    std::sort(salida.begin(), salida.end(), [](const QVariant& a, const QVariant& b) {
        return a.toMap().value(QStringLiteral("_orden")).toLongLong()
            < b.toMap().value(QStringLiteral("_orden")).toLongLong();
    });
    return salida;
}

QVariantList MobileController::solicitudes() const
{
    QVariantList salida;
    for (QVariantMap solicitud :
         store_.filas(portal_activo_, QStringLiteral("solicitudes_cita"))) {
        const QString mascotaUuid = cadena(solicitud, QStringLiteral("mascota_uuid"));
        solicitud.insert(QStringLiteral("mascota"),
                         nombreMascota(portal_activo_, mascotaUuid));
        solicitud.insert(QStringLiteral("fecha"),
                         cadena(solicitud, QStringLiteral("fecha_preferida")));
        solicitud.insert(QStringLiteral("hora"),
                         cadena(solicitud, QStringLiteral("hora_preferida")));
        salida.append(solicitud);
    }
    return salida;
}

QVariantList MobileController::historial() const
{
    QVariantList eventos;
    if (portal_activo_.isEmpty()) {
        return eventos;
    }
    const auto incluir = [this](const QString& mascotaUuid) {
        return mascota_activa_.isEmpty() || mascota_activa_ == mascotaUuid;
    };
    const auto agregar = [&eventos](const QString& tipo, qint64 orden, const QString& titulo,
                                    const QString& detalle, const QString& mascota,
                                    const QString& mascotaUuid, const QString& portal_uuid) {
        QVariantMap evento;
        evento.insert(QStringLiteral("tipo"), tipo);
        evento.insert(QStringLiteral("fecha"), fechaTexto(orden));
        evento.insert(QStringLiteral("_orden"), orden);
        evento.insert(QStringLiteral("titulo"), titulo);
        evento.insert(QStringLiteral("detalle"), detalle);
        evento.insert(QStringLiteral("mascota"), mascota);
        evento.insert(QStringLiteral("mascota_uuid"), mascotaUuid);
        evento.insert(QStringLiteral("portal_uuid"), portal_uuid);
        eventos.append(evento);
    };

    for (const QVariantMap& visita : store_.filas(portal_activo_, QStringLiteral("visitas"))) {
        const QString mascotaUuid = cadena(visita, QStringLiteral("mascota_uuid"));
        if (!incluir(mascotaUuid)) {
            continue;
        }
        QString titulo = cadena(visita, QStringLiteral("motivo"));
        if (titulo.isEmpty()) {
            titulo = QStringLiteral("Consulta");
        }
        QString detalle = cadena(visita, QStringLiteral("diagnostico"));
        const QString tratamiento = cadena(visita, QStringLiteral("tratamiento"));
        if (!tratamiento.isEmpty()) {
            detalle = detalle.isEmpty() ? tratamiento
                                        : detalle + QStringLiteral(" · ") + tratamiento;
        }
        agregar(QStringLiteral("Visita"), numero(visita, QStringLiteral("fecha")), titulo,
                detalle, nombreMascota(portal_activo_, mascotaUuid), mascotaUuid,
                portal_activo_);
    }

    for (const QVariantMap& vacuna : store_.filas(portal_activo_, QStringLiteral("vacunas"))) {
        const QString mascotaUuid = cadena(vacuna, QStringLiteral("mascota_uuid"));
        if (!incluir(mascotaUuid)) {
            continue;
        }
        const QString proxima = cadena(vacuna, QStringLiteral("proxima_dosis"));
        agregar(QStringLiteral("Vacuna"),
                msDeFecha(cadena(vacuna, QStringLiteral("fecha_aplicacion"))),
                cadena(vacuna, QStringLiteral("nombre_vacuna")),
                proxima.isEmpty() ? QString() : QStringLiteral("Próxima dosis: %1").arg(proxima),
                nombreMascota(portal_activo_, mascotaUuid), mascotaUuid, portal_activo_);
    }

    for (const QVariantMap& peso : store_.filas(portal_activo_, QStringLiteral("pesos"))) {
        const QString mascotaUuid = cadena(peso, QStringLiteral("mascota_uuid"));
        if (!incluir(mascotaUuid)) {
            continue;
        }
        agregar(QStringLiteral("Peso"), numero(peso, QStringLiteral("fecha")),
                QStringLiteral("%1 kg").arg(peso.value(QStringLiteral("peso_kg")).toDouble(),
                                            0, 'f', 1),
                QString(), nombreMascota(portal_activo_, mascotaUuid), mascotaUuid,
                portal_activo_);
    }

    for (const QVariantMap& examen :
         store_.filas(portal_activo_, QStringLiteral("laboratorio"))) {
        const QString mascotaUuid = cadena(examen, QStringLiteral("mascota_uuid"));
        if (!incluir(mascotaUuid)) {
            continue;
        }
        agregar(QStringLiteral("Laboratorio"),
                numero(examen, QStringLiteral("fecha_solicitud")),
                cadena(examen, QStringLiteral("tipo_examen")),
                cadena(examen, QStringLiteral("estado")),
                nombreMascota(portal_activo_, mascotaUuid), mascotaUuid, portal_activo_);
    }

    for (const QVariantMap& receta : store_.filas(portal_activo_, QStringLiteral("recetas"))) {
        const QString mascotaUuid = cadena(receta, QStringLiteral("mascota_uuid"));
        if (!incluir(mascotaUuid)) {
            continue;
        }
        agregar(QStringLiteral("Receta"),
                numero(receta, QStringLiteral("fecha_emision")),
                QStringLiteral("Formula médica"),
                cadena(receta, QStringLiteral("indicaciones")),
                nombreMascota(portal_activo_, mascotaUuid), mascotaUuid, portal_activo_);
    }

    std::sort(eventos.begin(), eventos.end(), [](const QVariant& a, const QVariant& b) {
        return a.toMap().value(QStringLiteral("_orden")).toLongLong()
            > b.toMap().value(QStringLiteral("_orden")).toLongLong();
    });
    return eventos;
}

QVariantList MobileController::directorio() const
{
    QVariantList salida;
    for (const fuzzy::core::ClinicaPublica& clinica :
         fuzzy::core::DirectorioCliente::buscar(directorio_, busqueda_)) {
        QVariantMap mapa;
        mapa.insert(QStringLiteral("uuid"), clinica.uuid);
        mapa.insert(QStringLiteral("nombre"), clinica.nombre);
        mapa.insert(QStringLiteral("ciudad"), clinica.ciudad);
        mapa.insert(QStringLiteral("direccion"), clinica.direccion);
        mapa.insert(QStringLiteral("telefono"), clinica.telefono);
        salida.append(mapa);
    }
    return salida;
}

QVariantList MobileController::altas() const
{
    QVariantList salida;
    for (const fuzzy::core::AltaLocal& alta : store_.altas()) {
        QVariantMap mapa = alta.datos;
        mapa.insert(QStringLiteral("uuid"), alta.uuid);
        mapa.insert(QStringLiteral("clinica_uuid"), alta.clinica_uuid);
        mapa.insert(QStringLiteral("clinica"), alta.clinica_nombre);
        mapa.insert(QStringLiteral("estado"), alta.estado);
        mapa.insert(QStringLiteral("fecha"), fechaTexto(alta.creado_at));
        mapa.insert(QStringLiteral("error"), alta.error);
        salida.append(mapa);
    }
    return salida;
}

QVariantList MobileController::registroPendiente() const
{
    QVariantList salida;
    for (QVariantMap fila : store_.registroLocal(QStringLiteral("perfil"))) {
        fila.insert(QStringLiteral("tipo"), QStringLiteral("perfil"));
        salida.append(fila);
    }
    for (QVariantMap fila : store_.registroLocal(QStringLiteral("mascota"))) {
        fila.insert(QStringLiteral("tipo"), QStringLiteral("mascota"));
        salida.append(fila);
    }
    return salida;
}

const fuzzy::core::ClinicaPublica* MobileController::clinica(const QString& uuid) const
{
    for (const fuzzy::core::ClinicaPublica& candidata : directorio_) {
        if (candidata.uuid == uuid) {
            return &candidata;
        }
    }
    return nullptr;
}

ConexionPortal MobileController::abrirPortal(const fuzzy::core::PortalConocido& portal) const
{
    const QString token = QString::fromUtf8(store_.tokenDe(portal.portal_uuid));
    if (token.isEmpty()) {
        return {};
    }
    if (abridor_portal_) {
        return abridor_portal_(portal, token);
    }
    ConexionPortal conexion;
    auto ejecutor = std::make_unique<fuzzy::core::TursoHttpExecutor>(portal.url, token);
    auto* turso = ejecutor.get();
    conexion.sonda = [turso] { return turso->salud(); };
    conexion.ejecutor = std::move(ejecutor);
    return conexion;
}

void MobileController::elegirPortalSiFalta()
{
    const QList<fuzzy::core::PortalConocido> disponibles = store_.portales();
    bool sigue = false;
    for (const fuzzy::core::PortalConocido& portal : disponibles) {
        if (portal.portal_uuid == portal_activo_) {
            sigue = true;
            break;
        }
    }
    if (!sigue) {
        portal_activo_ = disponibles.isEmpty() ? QString() : disponibles.front().portal_uuid;
        mascota_activa_.clear();
    }
}

void MobileController::refrescarTodo()
{
    elegirPortalSiFalta();
    emit portalesCambiados();
    emit portalCambiado();
    emit datosCambiados();
}

void MobileController::configurarClave(const QString& password)
{
    error_.clear();
    mensaje_.clear();
    if (password.size() < 8) {
        error_ = QStringLiteral("La clave debe tener al menos 8 caracteres.");
        emit errorCambiado();
        return;
    }
    if (!store_.configurarClave(password)) {
        error_ = QStringLiteral("No se pudo crear la clave. Usa al menos 8 caracteres.");
        emit errorCambiado();
        return;
    }
    mensaje_ = QStringLiteral("Clave creada. Tu información queda cifrada en este teléfono.");
    emit mensajeCambiado();
    emit bloqueoCambiado();
    refrescarTodo();
}

void MobileController::desbloquear(const QString& password)
{
    error_.clear();
    mensaje_.clear();
    if (!store_.desbloquear(password)) {
        error_ = QStringLiteral("Clave incorrecta.");
        emit errorCambiado();
        return;
    }
    emit bloqueoCambiado();
    refrescarTodo();
}

void MobileController::bloquear()
{
    store_.bloquear();
    emit bloqueoCambiado();
    emit datosCambiados();
}

void MobileController::cerrarSesion()
{
    // §4.6: al cerrar sesión se borra la caché local; lo ya descargado no se
    // puede borrar a distancia, pero aquí sí desaparece del teléfono.
    store_.borrarTodo();
    store_.bloquear();
    portal_activo_.clear();
    mascota_activa_.clear();
    estado_sync_ = QStringLiteral("sin_sincronizar");
    sin_red_ = false;
    mensaje_ = QStringLiteral("Sesión cerrada y caché borrada.");
    emit mensajeCambiado();
    emit bloqueoCambiado();
    refrescarTodo();
    emit syncCambiado();
}

void MobileController::setModoAvion(bool activo)
{
    if (modo_avion_ == activo) {
        return;
    }
    modo_avion_ = activo;
    mensaje_ = activo
        ? QStringLiteral("Modo avión: la app usará solo lo descargado.")
        : QStringLiteral("Modo avión desactivado.");
    emit mensajeCambiado();
    emit modoAvionCambiado();
}

void MobileController::limpiarMensajes()
{
    if (!error_.isEmpty()) {
        error_.clear();
        emit errorCambiado();
    }
    if (!mensaje_.isEmpty()) {
        mensaje_.clear();
        emit mensajeCambiado();
    }
}

void MobileController::seleccionarPortal(const QString& portal_uuid)
{
    fuzzy::core::PortalConocido portal;
    if (!store_.portal(portal_uuid, &portal)) {
        return;
    }
    portal_activo_ = portal_uuid;
    mascota_activa_.clear();
    emit portalCambiado();
    emit mascotaCambiada();
    emit datosCambiados();
}

void MobileController::olvidarPortal(const QString& portal_uuid)
{
    if (!store_.olvidarPortal(portal_uuid)) {
        error_ = QStringLiteral("No se pudo olvidar el portal.");
        emit errorCambiado();
        return;
    }
    if (portal_activo_ == portal_uuid) {
        portal_activo_.clear();
    }
    mensaje_ = QStringLiteral("Portal olvidado. La caché de esa clínica se borró.");
    emit mensajeCambiado();
    refrescarTodo();
}

void MobileController::abrirMascota(const QString& portal_uuid, const QString& mascota_uuid)
{
    seleccionarPortal(portal_uuid);
    mascota_activa_ = mascota_uuid;
    emit mascotaCambiada();
    emit datosCambiados();
}

void MobileController::cerrarMascota()
{
    mascota_activa_.clear();
    emit mascotaCambiada();
    emit datosCambiados();
}

bool MobileController::leerQr(const QString& contenido)
{
    error_.clear();
    fuzzy::core::DatosEmparejamiento datos;
    QString motivo;
    if (!fuzzy::core::EmparejamientoService::leerQr(contenido.toUtf8(), &datos, &motivo)) {
        error_ = motivo;
        emit errorCambiado();
        return false;
    }
    qr_pendiente_ = contenido;
    huella_pendiente_ = datos.huella.toUpper();
    clinica_pendiente_ = datos.clinica_nombre;
    emit emparejamientoCambiado();
    return true;
}

void MobileController::cancelarEmparejamiento()
{
    qr_pendiente_.clear();
    huella_pendiente_.clear();
    clinica_pendiente_.clear();
    emit emparejamientoCambiado();
}

void MobileController::aplicarEmparejamiento(const QByteArray& contenido)
{
    fuzzy::core::DatosEmparejamiento datos;
    QString motivo;
    if (!fuzzy::core::EmparejamientoService::leerQr(contenido, &datos, &motivo)) {
        error_ = motivo;
        emit errorCambiado();
        return;
    }

    fuzzy::core::PortalConocido provisional;
    provisional.portal_uuid = datos.portal_uuid;
    provisional.clinica_uuid = datos.clinica_uuid;
    provisional.clinica_nombre = datos.clinica_nombre;
    provisional.url = datos.url;
    provisional.huella = datos.huella;
    ConexionPortal conexion = abrirPortal(provisional);
    if (!conexion.ejecutor) {
        // El portal todavía no está emparejado: el token del QR es la única
        // credencial disponible en este momento.
        if (abridor_portal_) {
            conexion = abridor_portal_(provisional, datos.token);
        } else {
            auto ejecutor = std::make_unique<fuzzy::core::TursoHttpExecutor>(datos.url,
                                                                             datos.token);
            auto* turso = ejecutor.get();
            conexion.sonda = [turso] { return turso->salud(); };
            conexion.ejecutor = std::move(ejecutor);
        }
    }
    if (!conexion.ejecutor) {
        error_ = QStringLiteral("No se pudo abrir el portal.");
        emit errorCambiado();
        return;
    }

    fuzzy::core::EmparejamientoService servicio(*conexion.ejecutor, store_, conexion.sonda);
    const auto resultado = servicio.emparejar(contenido);
    if (!resultado.ok) {
        error_ = resultado.error;
        emit errorCambiado();
        return;
    }

    portal_activo_ = resultado.portal_uuid;
    qr_pendiente_.clear();
    huella_pendiente_.clear();
    clinica_pendiente_.clear();
    error_.clear();
    mensaje_ = resultado.pendientes > 0
        ? QStringLiteral("Portal de %1 emparejado. %2 datos locales aún no están "
                         "en la clínica; llévalos a recepción.")
              .arg(resultado.clinica_nombre)
              .arg(resultado.pendientes)
        : QStringLiteral("Portal de %1 emparejado.").arg(resultado.clinica_nombre);
    emit mensajeCambiado();
    emit emparejamientoCambiado();
    refrescarTodo();
}

void MobileController::confirmarEmparejamiento()
{
    if (qr_pendiente_.isEmpty()) {
        error_ = QStringLiteral("Primero lee el código de la clínica.");
        emit errorCambiado();
        return;
    }
    aplicarEmparejamiento(qr_pendiente_.toUtf8());
}

void MobileController::sincronizar()
{
    if (modo_avion_) {
        sin_red_ = true;
        estado_sync_ = QStringLiteral("sin_red");
        mensaje_ = QStringLiteral("Modo avión activo: no se intentó sincronizar.");
        emit mensajeCambiado();
        emit syncCambiado();
        return;
    }
    if (!store_.desbloqueado()) {
        estado_sync_ = QStringLiteral("sin_clave");
        emit syncCambiado();
        return;
    }

    const QList<fuzzy::core::PortalConocido> lista = store_.portales();
    if (lista.isEmpty()) {
        estado_sync_ = QStringLiteral("sin_portal");
        emit syncCambiado();
        return;
    }

    int exitos = 0;
    int fallos = 0;
    sin_red_ = false;
    QString primerError;
    for (const fuzzy::core::PortalConocido& portal : lista) {
        ConexionPortal conexion = abrirPortal(portal);
        if (!conexion.ejecutor) {
            ++fallos;
            primerError = QStringLiteral("No se pudo descifrar el token de %1")
                              .arg(portal.clinica_nombre);
            continue;
        }
        fuzzy::core::PortalCliente cliente(*conexion.ejecutor, conexion.sonda);
        const auto resultado = cliente.sincronizar(store_, portal.portal_uuid);
        if (resultado.ok) {
            ++exitos;
            continue;
        }
        ++fallos;
        if (primerError.isEmpty()) {
            primerError = resultado.error;
        }
        if (resultado.estado == QLatin1String("sin_red")) {
            sin_red_ = true;
        }
        if (resultado.estado == QLatin1String("sin_acceso")) {
            sin_red_ = false;
        }
    }

    if (fallos == 0) {
        estado_sync_ = QStringLiteral("ok");
        mensaje_ = QStringLiteral("Sincronizado con %1 portal(es).").arg(exitos);
    } else if (sin_red_) {
        estado_sync_ = QStringLiteral("sin_red");
        mensaje_ = QStringLiteral("Sin conexión. Tus cambios quedan en cola.");
    } else {
        estado_sync_ = QStringLiteral("error");
        error_ = primerError;
        emit errorCambiado();
    }
    emit mensajeCambiado();
    emit syncCambiado();
    refrescarTodo();
}

void MobileController::limpiarCache()
{
    const int borradas = store_.limpiarCache();
    mensaje_ = QStringLiteral("Caché local borrada (%1 filas). Vuelve a sincronizar.")
                   .arg(borradas);
    emit mensajeCambiado();
    refrescarTodo();
}

void MobileController::cargarDirectorio(const QString& json)
{
    QString motivo;
    const QList<fuzzy::core::ClinicaPublica> clinicas =
        fuzzy::core::DirectorioCliente::desdeJson(json.toUtf8(), &motivo);
    if (clinicas.isEmpty()) {
        error_ = motivo;
        emit errorCambiado();
        return;
    }
    directorio_ = clinicas;
    error_.clear();
    emit directorioCambiado();
}

void MobileController::buscarDirectorio(const QString& texto)
{
    busqueda_ = texto;
    emit directorioCambiado();
}

void MobileController::enviarAlta(const QString& clinica_uuid, const QString& nombre,
                                  const QString& correo, const QString& telefono,
                                  const QString& mascota, const QString& motivo,
                                  const QString& franja)
{
    if (!store_.desbloqueado()) {
        error_ = QStringLiteral("Desbloquea la app para enviar la solicitud.");
        emit errorCambiado();
        return;
    }
    const fuzzy::core::ClinicaPublica* destino = clinica(clinica_uuid);
    if (destino == nullptr) {
        error_ = QStringLiteral("Elige una clínica del directorio.");
        emit errorCambiado();
        return;
    }

    QVariantMap datos;
    datos.insert(QStringLiteral("nombre"), nombre);
    datos.insert(QStringLiteral("correo"), correo);
    datos.insert(QStringLiteral("telefono"), telefono);
    datos.insert(QStringLiteral("mascota"), mascota);
    datos.insert(QStringLiteral("motivo"), motivo);
    datos.insert(QStringLiteral("franja"), franja);

    QString motivoError;
    const QVariantMap normalizada =
        fuzzy::core::BuzonCliente::normalizar(datos, fuzzy::core::uuidNuevo(),
                                              destino->uuid, &motivoError);
    if (normalizada.isEmpty()) {
        error_ = motivoError;
        emit errorCambiado();
        return;
    }

    fuzzy::core::AltaLocal alta;
    alta.uuid = normalizada.value(QStringLiteral("uuid")).toString();
    alta.clinica_uuid = destino->uuid;
    alta.clinica_nombre = destino->nombre;
    alta.datos = normalizada;
    alta.creado_at = QDateTime::currentMSecsSinceEpoch();
    if (!store_.guardarAlta(alta)) {
        error_ = QStringLiteral("No se pudo guardar la solicitud en el teléfono.");
        emit errorCambiado();
        return;
    }

    if (modo_avion_) {
        mensaje_ = QStringLiteral("Solicitud guardada. Se enviará al recuperar la red.");
        emit mensajeCambiado();
        emit datosCambiados();
        return;
    }
    enviarAltaPendiente(alta);
    emit datosCambiados();
}

void MobileController::enviarAltaPendiente(const fuzzy::core::AltaLocal& alta)
{
    const fuzzy::core::ClinicaPublica* destino = clinica(alta.clinica_uuid);
    if (destino == nullptr) {
        store_.marcarAlta(alta.uuid, QStringLiteral("pendiente"),
                          QStringLiteral("La clínica no está en el directorio"));
        return;
    }

    std::unique_ptr<fuzzy::core::IQueryExecutor> ejecutor;
    if (abridor_buzon_) {
        ejecutor = abridor_buzon_(*destino);
    } else {
        ejecutor = std::make_unique<fuzzy::core::TursoHttpExecutor>(destino->inbox_url,
                                                                    destino->inbox_token);
    }
    if (!ejecutor) {
        store_.marcarAlta(alta.uuid, QStringLiteral("pendiente"),
                          QStringLiteral("No se pudo abrir el buzón"));
        return;
    }

    fuzzy::core::BuzonCliente buzon(*ejecutor);
    const auto resultado = buzon.enviar(alta.datos);
    if (resultado.ok) {
        store_.marcarAlta(alta.uuid, QStringLiteral("enviada"), {});
        mensaje_ = QStringLiteral("Solicitud enviada a %1. La clínica te contactará "
                                  "al %2.")
                       .arg(destino->nombre, alta.datos.value(QStringLiteral("telefono"))
                                                .toString());
    } else {
        store_.marcarAlta(alta.uuid, QStringLiteral("pendiente"), resultado.error);
        error_ = resultado.error;
        emit errorCambiado();
    }
    emit mensajeCambiado();
}

void MobileController::reintentarAltas()
{
    const QList<fuzzy::core::AltaLocal> pendientesAlta =
        store_.altas(QStringLiteral("pendiente"));
    for (const fuzzy::core::AltaLocal& alta : pendientesAlta) {
        enviarAltaPendiente(alta);
    }
    emit datosCambiados();
}

void MobileController::registrarPerfil(const QString& nombre, const QString& correo,
                                       const QString& telefono, const QString& direccion,
                                       const QString& dni)
{
    if (!store_.desbloqueado()) {
        error_ = QStringLiteral("Desbloquea la app para guardar tus datos.");
        emit errorCambiado();
        return;
    }
    if (nombre.trimmed().size() < 3) {
        error_ = QStringLiteral("Escribe tu nombre completo.");
        emit errorCambiado();
        return;
    }
    QVariantMap datos;
    datos.insert(QStringLiteral("nombre_completo"), nombre.trimmed());
    datos.insert(QStringLiteral("correo"), correo.trimmed());
    datos.insert(QStringLiteral("telefono"), telefono.trimmed());
    datos.insert(QStringLiteral("direccion"), direccion.trimmed());
    datos.insert(QStringLiteral("dni"), dni.trimmed());
    if (!store_.guardarRegistroLocal(QStringLiteral("perfil"), fuzzy::core::uuidNuevo(),
                                     datos)) {
        error_ = QStringLiteral("No se pudieron guardar tus datos.");
        emit errorCambiado();
        return;
    }
    mensaje_ = QStringLiteral("Datos guardados en el teléfono. Se importarán al emparejar.");
    emit mensajeCambiado();
    emit datosCambiados();
}

void MobileController::registrarMascota(const QString& nombre, const QString& especie,
                                        const QString& raza, const QString& sexo, double peso)
{
    if (!store_.desbloqueado()) {
        error_ = QStringLiteral("Desbloquea la app para guardar tu mascota.");
        emit errorCambiado();
        return;
    }
    if (nombre.trimmed().isEmpty() || especie.trimmed().isEmpty()) {
        error_ = QStringLiteral("Escribe el nombre y la especie de tu mascota.");
        emit errorCambiado();
        return;
    }
    QVariantMap datos;
    datos.insert(QStringLiteral("nombre"), nombre.trimmed());
    datos.insert(QStringLiteral("especie"), especie.trimmed());
    datos.insert(QStringLiteral("raza"), raza.trimmed());
    datos.insert(QStringLiteral("sexo"), sexo.trimmed().isEmpty()
                                            ? QStringLiteral("Desconocido")
                                            : sexo.trimmed());
    datos.insert(QStringLiteral("peso_kg"), peso);
    if (!store_.guardarRegistroLocal(QStringLiteral("mascota"), fuzzy::core::uuidNuevo(),
                                     datos)) {
        error_ = QStringLiteral("No se pudo guardar la mascota.");
        emit errorCambiado();
        return;
    }
    mensaje_ = QStringLiteral("Mascota guardada en el teléfono. "
                              "Se importará al emparejar con la clínica.");
    emit mensajeCambiado();
    emit datosCambiados();
}

void MobileController::solicitarCita(const QString& mascota_uuid,
                                     const QString& tipo_servicio, const QString& fecha,
                                     const QString& hora, const QString& motivo)
{
    if (portal_activo_.isEmpty()) {
        error_ = QStringLiteral("Empareja un portal para pedir una cita.");
        emit errorCambiado();
        return;
    }
    bool pertenece = false;
    for (const QVariantMap& mascota :
         store_.filas(portal_activo_, QStringLiteral("mascotas"))) {
        if (mascota.value(QStringLiteral("uuid")).toString() == mascota_uuid) {
            pertenece = true;
            break;
        }
    }
    if (!pertenece) {
        error_ = QStringLiteral("Esa mascota no pertenece al portal activo.");
        emit errorCambiado();
        return;
    }
    fuzzy::core::PortalConocido portal;
    store_.portal(portal_activo_, &portal);
    ConexionPortal conexion = abrirPortal(portal);
    if (!conexion.ejecutor) {
        error_ = QStringLiteral("No se pudo abrir el portal.");
        emit errorCambiado();
        return;
    }
    fuzzy::core::PortalCliente cliente(*conexion.ejecutor, conexion.sonda);
    if (!cliente.solicitarCita(store_, portal_activo_, mascota_uuid, tipo_servicio, fecha,
                               hora, motivo)) {
        error_ = cliente.ultimoError();
        emit errorCambiado();
        return;
    }
    mensaje_ = QStringLiteral("Solicitud guardada. Se enviará a la clínica al sincronizar.");
    emit mensajeCambiado();
    emit datosCambiados();
    if (!modo_avion_) {
        sincronizar();
    }
}

void MobileController::actualizarPerfil(const QString& nombre, const QString& correo,
                                        const QString& telefono, const QString& direccion,
                                        const QString& dni)
{
    if (portal_activo_.isEmpty()) {
        error_ = QStringLiteral("Empareja un portal para actualizar tu perfil.");
        emit errorCambiado();
        return;
    }
    fuzzy::core::PortalConocido portal;
    store_.portal(portal_activo_, &portal);
    ConexionPortal conexion = abrirPortal(portal);
    if (!conexion.ejecutor) {
        error_ = QStringLiteral("No se pudo abrir el portal.");
        emit errorCambiado();
        return;
    }
    QVariantMap campos;
    campos.insert(QStringLiteral("nombre_completo"), nombre.trimmed());
    campos.insert(QStringLiteral("correo"), correo.trimmed());
    campos.insert(QStringLiteral("telefono"), telefono.trimmed());
    campos.insert(QStringLiteral("direccion"), direccion.trimmed());
    campos.insert(QStringLiteral("dni"), dni.trimmed());
    fuzzy::core::PortalCliente cliente(*conexion.ejecutor, conexion.sonda);
    if (!cliente.actualizarPerfil(store_, portal_activo_, campos)) {
        error_ = cliente.ultimoError();
        emit errorCambiado();
        return;
    }
    mensaje_ = QStringLiteral("Perfil guardado. Se enviará a la clínica al sincronizar.");
    emit mensajeCambiado();
    emit datosCambiados();
    if (!modo_avion_) {
        sincronizar();
    }
}

} // namespace fuzzy::mobile
