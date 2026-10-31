#include "fuzzy/core/BuzonCliente.h"

#include "fuzzy/core/Uuid.h"

#include <QDateTime>

namespace fuzzy::core {

namespace {

const int kNombreMin = 3;
const int kNombreMax = 120;
const int kTelefonoMin = 7;
const int kTelefonoMax = 20;
const int kCorreoMin = 5;
const int kCorreoMax = 254;
const int kMascotaMax = 120;
const int kMotivoMax = 500;
const int kFranjaMax = 40;

QString limpio(const QVariantMap& datos, const QString& clave)
{
    return datos.value(clave).toString().trimmed();
}

QString errorDe(const QString& mensaje)
{
    return mensaje;
}

bool longitudEntre(const QString& valor, int minimo, int maximo)
{
    const int largo = valor.size();
    return largo >= minimo && largo <= maximo;
}

} // namespace

BuzonCliente::BuzonCliente(IQueryExecutor& inbox)
    : inbox_(inbox)
{
}

QVariantMap BuzonCliente::normalizar(const QVariantMap& datos, QString uuid,
                                     const QString& clinica_uuid, QString* error)
{
    const auto fallar = [error](const QString& mensaje) {
        if (error != nullptr) {
            *error = mensaje;
        }
        return QVariantMap();
    };

    const QString nombre = limpio(datos, QStringLiteral("nombre"));
    if (!longitudEntre(nombre, kNombreMin, kNombreMax)) {
        return fallar(QStringLiteral("El nombre debe tener entre 3 y 120 caracteres"));
    }

    const QString telefono = limpio(datos, QStringLiteral("telefono"));
    if (!longitudEntre(telefono, kTelefonoMin, kTelefonoMax)) {
        return fallar(QStringLiteral("El teléfono debe tener entre 7 y 20 caracteres"));
    }

    const QString correo = limpio(datos, QStringLiteral("correo"));
    if (!correo.isEmpty() && !longitudEntre(correo, kCorreoMin, kCorreoMax)) {
        return fallar(QStringLiteral("El correo debe tener entre 5 y 254 caracteres"));
    }
    if (!correo.isEmpty() && (!correo.contains(QLatin1Char('@'))
                              || correo.startsWith(QLatin1Char('@'))
                              || correo.endsWith(QLatin1Char('@')))) {
        return fallar(QStringLiteral("El correo no es válido"));
    }

    const QString mascota = limpio(datos, QStringLiteral("mascota"));
    if (mascota.size() > kMascotaMax) {
        return fallar(QStringLiteral("El nombre de la mascota no puede pasar de 120 caracteres"));
    }

    const QString motivo = limpio(datos, QStringLiteral("motivo"));
    if (motivo.size() > kMotivoMax) {
        return fallar(QStringLiteral("El motivo no puede pasar de 500 caracteres"));
    }

    const QString franja = limpio(datos, QStringLiteral("franja"));
    if (franja.size() > kFranjaMax) {
        return fallar(QStringLiteral("La franja no puede pasar de 40 caracteres"));
    }

    if (clinica_uuid.trimmed().isEmpty()) {
        return fallar(QStringLiteral("Falta la clínica de destino"));
    }

    // `uuid` vacío se genera aquí para que el llamador pueda reintentar el
    // mismo alta sin duplicarla; también sirve para importar un alta ya creada.
    QVariantMap alta;
    alta.insert(QStringLiteral("uuid"),
                uuid.trimmed().isEmpty() ? uuidNuevo() : uuid.trimmed());
    alta.insert(QStringLiteral("clinica_uuid"), clinica_uuid.trimmed());
    alta.insert(QStringLiteral("nombre"), nombre);
    alta.insert(QStringLiteral("correo"), correo.isEmpty() ? QVariant() : QVariant(correo));
    alta.insert(QStringLiteral("telefono"), telefono);
    alta.insert(QStringLiteral("mascota"), mascota.isEmpty() ? QVariant() : QVariant(mascota));
    alta.insert(QStringLiteral("motivo"), motivo.isEmpty() ? QVariant() : QVariant(motivo));
    alta.insert(QStringLiteral("franja"), franja.isEmpty() ? QVariant() : QVariant(franja));
    alta.insert(QStringLiteral("creada_at"), QDateTime::currentMSecsSinceEpoch());
    return alta;
}

ResultadoAlta BuzonCliente::enviar(const QVariantMap& alta)
{
    ResultadoAlta resultado;
    const QString uuid = alta.value(QStringLiteral("uuid")).toString();
    const QString clinica = alta.value(QStringLiteral("clinica_uuid")).toString();
    if (uuid.isEmpty() || clinica.isEmpty()) {
        resultado.error = QStringLiteral("El alta no tiene uuid o clínica");
        return resultado;
    }
    resultado.uuid = uuid;

    // Idempotente por uuid: el índice UNIQUE convierte un reenvío en un no-op
    // silencioso, que es justo lo que necesita un teléfono que perdió la red
    // justo después de insertar.
    const ExecResult ejecucion = inbox_.execute(
        QStringLiteral("INSERT INTO altas "
                       "(uuid, clinica_uuid, nombre, correo, telefono, mascota, motivo, "
                       "franja, creada_at) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
                       "ON CONFLICT (uuid) DO NOTHING"),
        {QVariant(uuid), QVariant(clinica),
         QVariant(alta.value(QStringLiteral("nombre"))),
         QVariant(alta.value(QStringLiteral("correo"))),
         QVariant(alta.value(QStringLiteral("telefono"))),
         QVariant(alta.value(QStringLiteral("mascota"))),
         QVariant(alta.value(QStringLiteral("motivo"))),
         QVariant(alta.value(QStringLiteral("franja"))),
         QVariant(alta.value(QStringLiteral("creada_at")))});

    resultado.ok = ejecucion.ok;
    if (!resultado.ok) {
        resultado.error = QStringLiteral("No se pudo enviar la solicitud; "
                                         "revisa la conexión e inténtalo de nuevo");
    }
    return resultado;
}

ResultadoAlta BuzonCliente::enviar(const QVariantMap& datos, const QString& clinica_uuid,
                                   QString uuid)
{
    QString error;
    const QVariantMap alta = normalizar(datos, std::move(uuid), clinica_uuid, &error);
    if (alta.isEmpty()) {
        ResultadoAlta resultado;
        resultado.error = errorDe(error);
        return resultado;
    }
    return enviar(alta);
}

} // namespace fuzzy::core
