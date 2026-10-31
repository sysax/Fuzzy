#include "fuzzy/core/EmparejamientoService.h"

#include "fuzzy/core/MovilStore.h"
#include "fuzzy/core/PortalTokenService.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QVariant>

#include <algorithm>

namespace fuzzy::core {

namespace {

qint64 msAhora()
{
    return QDateTime::currentMSecsSinceEpoch();
}

QString texto(const QJsonObject& objeto, const QString& clave)
{
    return objeto.value(clave).toString().trimmed();
}

bool mismoContacto(const QVariantMap& local, const QVariantMap& remoto)
{
    const auto normalizar = [](const QString& valor) {
        return valor.trimmed().toCaseFolded();
    };
    const auto digitos = [](const QString& valor) {
        QString salida;
        for (const QChar caracter : valor) {
            if (caracter.isDigit()) {
                salida.append(caracter);
            }
        }
        return salida;
    };

    const QString correoLocal = normalizar(local.value(QStringLiteral("correo")).toString());
    const QString correoRemoto = normalizar(remoto.value(QStringLiteral("correo")).toString());
    if (!correoLocal.isEmpty() && correoLocal == correoRemoto) {
        return true;
    }
    const QString dni = normalizar(local.value(QStringLiteral("dni")).toString());
    if (!dni.isEmpty() && dni == normalizar(remoto.value(QStringLiteral("dni")).toString())) {
        return true;
    }
    const QString telLocal = digitos(local.value(QStringLiteral("telefono")).toString());
    const QString telRemoto = digitos(remoto.value(QStringLiteral("telefono")).toString());
    return !telLocal.isEmpty() && telLocal == telRemoto;
}

bool mismaMascota(const QVariantMap& local, const QVariantMap& remoto)
{
    const QString nombre = local.value(QStringLiteral("nombre")).toString().trimmed();
    if (nombre.isEmpty()
        || nombre.compare(remoto.value(QStringLiteral("nombre")).toString().trimmed(),
                          Qt::CaseInsensitive)
            != 0) {
        return false;
    }
    const QString especie = local.value(QStringLiteral("especie")).toString().trimmed();
    return especie.isEmpty()
        || especie.compare(remoto.value(QStringLiteral("especie")).toString().trimmed(),
                           Qt::CaseInsensitive)
            == 0;
}

} // namespace

EmparejamientoService::EmparejamientoService(IQueryExecutor& portal, MovilStore& store,
                                             PortalCliente::Sonda sonda)
    : portal_(portal), store_(store), sonda_(std::move(sonda))
{
}

bool EmparejamientoService::leerQr(const QByteArray& json, DatosEmparejamiento* datos,
                                   QString* error)
{
    const auto fallar = [error](const QString& mensaje) {
        if (error != nullptr) {
            *error = mensaje;
        }
        return false;
    };
    if (datos == nullptr) {
        return fallar(QStringLiteral("Destino inválido"));
    }

    QJsonParseError fallo;
    const QJsonDocument documento = QJsonDocument::fromJson(json, &fallo);
    if (fallo.error != QJsonParseError::NoError || !documento.isObject()) {
        return fallar(QStringLiteral("El código no es un QR de FuzzyCloud"));
    }
    const QJsonObject objeto = documento.object();

    DatosEmparejamiento leido;
    leido.v = objeto.value(QStringLiteral("v")).toInt();
    leido.url = texto(objeto, QStringLiteral("url"));
    leido.token = texto(objeto, QStringLiteral("token"));
    leido.portal_uuid = texto(objeto, QStringLiteral("portal_uuid"));
    leido.clinica_uuid = texto(objeto, QStringLiteral("clinica_uuid"));
    leido.clinica_nombre = texto(objeto, QStringLiteral("clinica_nombre"));
    leido.huella = texto(objeto, QStringLiteral("huella")).toLower();

    // El plan original llamaba `clinica` al objeto; se acepta durante la
    // transición para no romper QRs ya impresos.
    const QJsonValue clinica = objeto.value(QStringLiteral("clinica"));
    if (clinica.isObject()) {
        const QJsonObject detalle = clinica.toObject();
        if (leido.clinica_uuid.isEmpty()) {
            leido.clinica_uuid = texto(detalle, QStringLiteral("uuid"));
        }
        if (leido.clinica_nombre.isEmpty()) {
            leido.clinica_nombre = texto(detalle, QStringLiteral("nombre"));
        }
    }

    if (leido.v != 1) {
        return fallar(QStringLiteral("Esta versión de la app no entiende ese código"));
    }
    if (leido.url.isEmpty() || leido.token.isEmpty() || leido.portal_uuid.isEmpty()
        || leido.clinica_uuid.isEmpty()) {
        return fallar(QStringLiteral("Al código le faltan datos del portal"));
    }
    if (leido.clinica_nombre.isEmpty()) {
        leido.clinica_nombre = QStringLiteral("Clínica");
    }
    if (leido.huella.size() != 6) {
        return fallar(QStringLiteral("Falta la huella de verificación del portal"));
    }
    const QString esperada = PortalTokenService::huella(leido.token).toLower();
    if (esperada != leido.huella) {
        // Un QR con token cambiado y huella vieja no debe emparejar: el
        // operador confirma justamente esta huella.
        return fallar(QStringLiteral("La huella del código no coincide; "
                                     "pide un QR nuevo en la clínica"));
    }

    *datos = leido;
    return true;
}

ResultadoEmparejamiento EmparejamientoService::emparejar(const QByteArray& qr)
{
    ResultadoEmparejamiento resultado;
    DatosEmparejamiento datos;
    QString error;
    if (!leerQr(qr, &datos, &error)) {
        resultado.error = error;
        return resultado;
    }
    resultado.portal_uuid = datos.portal_uuid;
    resultado.clinica_nombre = datos.clinica_nombre;

    if (!store_.desbloqueado()) {
        resultado.error = QStringLiteral("Desbloquea la app antes de emparejar");
        return resultado;
    }

    // Un portal por clínica (D26). Re-emparejar el mismo portal es una
    // rotación de token; emparejar otro portal de la misma sede es un error.
    bool existia = false;
    for (const PortalConocido& conocido : store_.portales()) {
        if (conocido.portal_uuid == datos.portal_uuid) {
            existia = true;
            if (conocido.clinica_uuid != datos.clinica_uuid) {
                resultado.error = QStringLiteral("Ese portal ya está emparejado "
                                                 "con otra clínica");
                return resultado;
            }
        } else if (conocido.clinica_uuid == datos.clinica_uuid) {
            resultado.error = QStringLiteral("Ya tienes un portal de esta clínica. "
                                             "Olvídalo antes de emparejar otro.");
            return resultado;
        }
    }

    PortalConocido portal;
    portal.portal_uuid = datos.portal_uuid;
    portal.clinica_uuid = datos.clinica_uuid;
    portal.clinica_nombre = datos.clinica_nombre;
    portal.url = datos.url;
    portal.huella = datos.huella;
    portal.emparejado_at = msAhora();
    portal.estado = QStringLiteral("activo");
    if (!store_.agregarPortal(portal, datos.token.toUtf8())) {
        resultado.error = QStringLiteral("No se pudo guardar el portal en el teléfono");
        return resultado;
    }

    PortalCliente cliente(portal_, sonda_);
    const ResultadoDescarga descarga = cliente.descargar(store_, datos.portal_uuid,
                                                         datos.clinica_uuid);
    if (!descarga.ok) {
        // Deshacer: un portal sin contenido dejaría la app mostrando una
        // clínica vacía y una cola que no puede subir.
        if (existia) {
            store_.marcarEstado(datos.portal_uuid, QStringLiteral("inaccesible"));
        } else {
            store_.olvidarPortal(datos.portal_uuid);
        }
        resultado.error = descarga.error;
        return resultado;
    }

    resultado.filas = descarga.filas;
    resultado.importados = importarRegistro(store_, datos.portal_uuid, &resultado.pendientes);
    store_.registrarSync(datos.portal_uuid, msAhora());
    resultado.ok = true;
    resultado.estado = QStringLiteral("activo");
    return resultado;
}

int EmparejamientoService::importarRegistro(MovilStore& store, const QString& portal_uuid,
                                            int* pendientes)
{
    const QList<QVariantMap> perfilesRemotos = store.filas(portal_uuid, QStringLiteral("perfil"));
    const QList<QVariantMap> mascotasRemotas = store.filas(portal_uuid, QStringLiteral("mascotas"));

    int importados = 0;
    int restantes = 0;

    for (const QVariantMap& local : store.registroLocal(QStringLiteral("perfil"))) {
        const bool coincide = std::any_of(
            perfilesRemotos.cbegin(), perfilesRemotos.cend(),
            [&local](const QVariantMap& remoto) { return mismoContacto(local, remoto); });
        if (coincide && store.marcarImportado(local.value(QStringLiteral("uuid")).toString())) {
            ++importados;
        } else {
            ++restantes;
        }
    }

    for (const QVariantMap& local : store.registroLocal(QStringLiteral("mascota"))) {
        const bool coincide = std::any_of(
            mascotasRemotas.cbegin(), mascotasRemotas.cend(),
            [&local](const QVariantMap& remoto) { return mismaMascota(local, remoto); });
        if (coincide && store.marcarImportado(local.value(QStringLiteral("uuid")).toString())) {
            ++importados;
        } else {
            ++restantes;
        }
    }

    if (pendientes != nullptr) {
        *pendientes = restantes;
    }
    return importados;
}

} // namespace fuzzy::core
