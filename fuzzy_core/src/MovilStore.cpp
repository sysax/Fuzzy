#include "fuzzy/core/MovilStore.h"

#include "fuzzy/core/CryptoBox.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>

namespace fuzzy::core {

namespace {

const QString kClaveSalt = QStringLiteral("kdf_salt");
const QString kClaveIteraciones = QStringLiteral("kdf_iteraciones");
const QString kClaveCheck = QStringLiteral("clave_check");
const QByteArray kTestigo = QByteArrayLiteral("fuzzy-mobile-cache-v1");

QByteArray sobreDe(const QByteArray& clave, const QByteArray& datos, const QByteArray& aad)
{
    return CryptoBox::cifrar(clave, datos, aad);
}

bool abrirSobre(const QByteArray& clave, const QByteArray& sobre, QByteArray* datos,
                const QByteArray& aad)
{
    return CryptoBox::descifrar(clave, sobre, datos, aad);
}

QByteArray aadDe(const QString& portal_uuid, const QString& tabla, const QString& uuid)
{
    return QStringLiteral("%1/%2/%3").arg(portal_uuid, tabla, uuid).toUtf8();
}

} // namespace

MovilStore::MovilStore(IQueryExecutor& db)
    : db_(db)
{
}

qint64 MovilStore::msAhora()
{
    return QDateTime::currentMSecsSinceEpoch();
}

// ── Clave ────────────────────────────────────────────────────

bool MovilStore::tieneClave() const
{
    const auto filas = db_.query(
        QStringLiteral("SELECT 1 FROM ajustes WHERE clave = ?"), {QVariant(kClaveSalt)});
    return filas.size() == 1;
}

bool MovilStore::configurarClave(const QString& password, qint64 ahora)
{
    if (password.isEmpty() || tieneClave()) {
        return false;
    }
    const qint64 momento = ahora > 0 ? ahora : msAhora();
    const QByteArray salt = CryptoBox::aleatorio(16);
    const QByteArray clave = CryptoBox::derivarClave(password, salt, iteraciones_);
    if (clave.size() != CryptoBox::kBytesClave) {
        return false;
    }
    const QByteArray check = sobreDe(clave, kTestigo, salt);

    const auto guardar = [this, momento](const QString& claveAjuste, const QString& valor) {
        return db_.execute(
                   QStringLiteral("INSERT INTO ajustes (clave, valor, updated_at) "
                                  "VALUES (?, ?, ?) "
                                  "ON CONFLICT (clave) DO UPDATE SET valor = excluded.valor, "
                                  "updated_at = excluded.updated_at"),
                   {QVariant(claveAjuste), QVariant(valor), QVariant::fromValue(momento)})
            .ok;
    };

    if (!guardar(kClaveSalt, QString::fromLatin1(salt.toHex()))
        || !guardar(kClaveIteraciones, QString::number(iteraciones_))
        || !guardar(kClaveCheck, QString::fromLatin1(check.toHex()))) {
        return false;
    }
    clave_ = clave;
    return true;
}

bool MovilStore::desbloquear(const QString& password)
{
    if (password.isEmpty()) {
        return false;
    }
    const auto filas = db_.query(
        QStringLiteral("SELECT clave, valor FROM ajustes "
                       "WHERE clave IN (?, ?, ?)"),
        {QVariant(kClaveSalt), QVariant(kClaveIteraciones), QVariant(kClaveCheck)});
    QByteArray salt;
    QByteArray check;
    int iteraciones = iteraciones_;
    for (const auto& fila : filas) {
        const QString clave = fila.values.at(0).toString();
        const QString valor = fila.values.at(1).toString();
        if (clave == kClaveSalt) {
            salt = QByteArray::fromHex(valor.toLatin1());
        } else if (clave == kClaveIteraciones) {
            const int valorIteraciones = valor.toInt();
            if (valorIteraciones > 0) {
                iteraciones = valorIteraciones;
            }
        } else if (clave == kClaveCheck) {
            check = QByteArray::fromHex(valor.toLatin1());
        }
    }
    if (salt.isEmpty() || check.isEmpty()) {
        return false;
    }

    const QByteArray clave = CryptoBox::derivarClave(password, salt, iteraciones);
    QByteArray testigo;
    if (!abrirSobre(clave, check, &testigo, salt) || testigo != kTestigo) {
        return false;
    }
    clave_ = clave;
    return true;
}

void MovilStore::bloquear()
{
    clave_.fill('\0');
    clave_.clear();
}

// ── Portales ─────────────────────────────────────────────────

QList<PortalConocido> MovilStore::portales() const
{
    QList<PortalConocido> salida;
    const auto filas = db_.query(
        QStringLiteral("SELECT portal_uuid, clinica_uuid, clinica_nombre, url, huella, "
                       "estado, emparejado_at, coalesce(ultima_sync, 0) "
                       "FROM portales_conocidos ORDER BY clinica_nombre"));
    for (const auto& fila : filas) {
        PortalConocido portal;
        portal.portal_uuid = fila.values.at(0).toString();
        portal.clinica_uuid = fila.values.at(1).toString();
        portal.clinica_nombre = fila.values.at(2).toString();
        portal.url = fila.values.at(3).toString();
        portal.huella = fila.values.at(4).toString();
        portal.estado = fila.values.at(5).toString();
        portal.emparejado_at = fila.values.at(6).toLongLong();
        portal.ultima_sync = fila.values.at(7).toLongLong();
        salida.append(portal);
    }
    return salida;
}

bool MovilStore::portal(const QString& portal_uuid, PortalConocido* salida) const
{
    if (salida == nullptr) {
        return false;
    }
    const auto filas = db_.query(
        QStringLiteral("SELECT portal_uuid, clinica_uuid, clinica_nombre, url, huella, "
                       "estado, emparejado_at, coalesce(ultima_sync, 0) "
                       "FROM portales_conocidos WHERE portal_uuid = ?"),
        {QVariant(portal_uuid)});
    if (filas.size() != 1) {
        return false;
    }
    const auto& fila = filas.front();
    salida->portal_uuid = fila.values.at(0).toString();
    salida->clinica_uuid = fila.values.at(1).toString();
    salida->clinica_nombre = fila.values.at(2).toString();
    salida->url = fila.values.at(3).toString();
    salida->huella = fila.values.at(4).toString();
    salida->estado = fila.values.at(5).toString();
    salida->emparejado_at = fila.values.at(6).toLongLong();
    salida->ultima_sync = fila.values.at(7).toLongLong();
    return true;
}

bool MovilStore::agregarPortal(const PortalConocido& portal, const QByteArray& token)
{
    if (!desbloqueado() || portal.portal_uuid.isEmpty() || portal.clinica_uuid.isEmpty()
        || portal.url.isEmpty() || token.isEmpty() || portal.huella.size() != 6) {
        return false;
    }
    const qint64 momento = portal.emparejado_at > 0 ? portal.emparejado_at : msAhora();
    const QByteArray cifrado = sobreDe(
        clave_, token, aadDe(portal.portal_uuid, QStringLiteral("token"), portal.portal_uuid));
    if (cifrado.isEmpty()) {
        return false;
    }
    const ExecResult resultado = db_.execute(
        QStringLiteral("INSERT INTO portales_conocidos "
                       "(portal_uuid, clinica_uuid, clinica_nombre, url, token_cifrado, "
                       "huella, emparejado_at, estado) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
                       "ON CONFLICT (portal_uuid) DO UPDATE SET "
                       "clinica_nombre = excluded.clinica_nombre, url = excluded.url, "
                       "token_cifrado = excluded.token_cifrado, huella = excluded.huella"),
        {QVariant(portal.portal_uuid), QVariant(portal.clinica_uuid),
         QVariant(portal.clinica_nombre), QVariant(portal.url), QVariant(cifrado),
         QVariant(portal.huella), QVariant::fromValue(momento), QVariant(portal.estado)});
    return resultado.ok;
}

QByteArray MovilStore::tokenDe(const QString& portal_uuid) const
{
    if (!desbloqueado()) {
        return {};
    }
    const auto filas = db_.query(
        QStringLiteral("SELECT token_cifrado FROM portales_conocidos WHERE portal_uuid = ?"),
        {QVariant(portal_uuid)});
    if (filas.size() != 1) {
        return {};
    }
    QByteArray token;
    if (!abrirSobre(clave_, filas.front().values.at(0).toByteArray(), &token,
                    aadDe(portal_uuid, QStringLiteral("token"), portal_uuid))) {
        return {};
    }
    return token;
}

bool MovilStore::marcarEstado(const QString& portal_uuid, const QString& estado)
{
    return db_.execute(
               QStringLiteral("UPDATE portales_conocidos SET estado = ? WHERE portal_uuid = ?"),
               {QVariant(estado), QVariant(portal_uuid)})
        .ok;
}

bool MovilStore::registrarSync(const QString& portal_uuid, qint64 ahora)
{
    return db_.execute(
               QStringLiteral("UPDATE portales_conocidos SET ultima_sync = ?, estado = 'activo' "
                              "WHERE portal_uuid = ?"),
               {QVariant::fromValue(ahora > 0 ? ahora : msAhora()), QVariant(portal_uuid)})
        .ok;
}

bool MovilStore::olvidarPortal(const QString& portal_uuid)
{
    // Las claves foráneas no se fuerzan en la conexión: se borran los hijos a
    // mano para no dejar caché huérfana.
    db_.execute(QStringLiteral("DELETE FROM outbox WHERE portal_uuid = ?"),
                {QVariant(portal_uuid)});
    db_.execute(QStringLiteral("DELETE FROM cache_filas WHERE portal_uuid = ?"),
                {QVariant(portal_uuid)});
    return db_.execute(QStringLiteral("DELETE FROM portales_conocidos WHERE portal_uuid = ?"),
                       {QVariant(portal_uuid)})
        .ok;
}

// ── Caché ────────────────────────────────────────────────────

QByteArray MovilStore::jsonDe(const QVariantMap& datos)
{
    return QJsonDocument(QJsonObject::fromVariantMap(datos))
        .toJson(QJsonDocument::Compact);
}

QVariantMap MovilStore::datosDe(const QByteArray& json)
{
    const QJsonDocument documento = QJsonDocument::fromJson(json);
    return documento.isObject() ? documento.object().toVariantMap() : QVariantMap();
}

qint64 MovilStore::fechaDe(const QString& tabla, const QVariantMap& fila)
{
    if (tabla == QLatin1String("citas")) {
        return fila.value(QStringLiteral("fecha_hora")).toLongLong();
    }
    if (tabla == QLatin1String("visitas") || tabla == QLatin1String("pesos")) {
        return fila.value(QStringLiteral("fecha")).toLongLong();
    }
    if (tabla == QLatin1String("recetas")) {
        return fila.value(QStringLiteral("fecha_emision")).toLongLong();
    }
    if (tabla == QLatin1String("laboratorio")) {
        return fila.value(QStringLiteral("fecha_solicitud")).toLongLong();
    }
    if (tabla == QLatin1String("solicitudes_cita")) {
        return fila.value(QStringLiteral("created_at")).toLongLong();
    }
    return fila.value(QStringLiteral("updated_at")).toLongLong();
}

QByteArray MovilStore::sobre(const QString& portal_uuid, const QString& tabla,
                             const QString& uuid, const QByteArray& datos) const
{
    return sobreDe(clave_, datos, aadDe(portal_uuid, tabla, uuid));
}

bool MovilStore::guardarFila(const QString& portal_uuid, const QString& tabla,
                             const QVariantMap& fila, bool pendiente, bool desdeRemoto)
{
    if (!desbloqueado()) {
        return false;
    }
    const QString uuid = fila.value(QStringLiteral("uuid")).toString();
    if (uuid.isEmpty()) {
        return false;
    }
    const qint64 updatedAt = fila.value(QStringLiteral("updated_at")).toLongLong();
    if (updatedAt <= 0) {
        return false;
    }

    bool quedaPendiente = pendiente;
    const auto actuales = db_.query(
        QStringLiteral("SELECT updated_at, pendiente FROM cache_filas "
                       "WHERE portal_uuid = ? AND tabla = ? AND uuid = ?"),
        {QVariant(portal_uuid), QVariant(tabla), QVariant(uuid)});
    if (actuales.size() == 1) {
        const qint64 anterior = actuales.front().values.at(0).toLongLong();
        const bool pendienteAnterior = actuales.front().values.at(1).toInt() == 1;
        // LWW: gana la versión más reciente; a igualdad, la remota.
        if (updatedAt < anterior) {
            return false;
        }
        if (updatedAt == anterior && !desdeRemoto) {
            return false;
        }
        if (desdeRemoto && pendienteAnterior) {
            const auto enCola = db_.query(
                QStringLiteral("SELECT 1 FROM outbox WHERE portal_uuid = ? AND uuid = ?"),
                {QVariant(portal_uuid), QVariant(uuid)});
            quedaPendiente = enCola.size() == 1;
        } else if (desdeRemoto) {
            quedaPendiente = false;
        }
    }

    const QByteArray contenido = jsonDe(fila);
    const QByteArray cifrado = sobre(portal_uuid, tabla, uuid, contenido);
    if (cifrado.isEmpty()) {
        return false;
    }

    QVariant borrada;
    if (fila.contains(QStringLiteral("deleted_at"))
        && !fila.value(QStringLiteral("deleted_at")).isNull()) {
        borrada = fila.value(QStringLiteral("deleted_at"));
    }

    const ExecResult resultado = db_.execute(
        QStringLiteral("INSERT INTO cache_filas "
                       "(portal_uuid, tabla, uuid, fecha, updated_at, deleted_at, "
                       "pendiente, payload) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
                       "ON CONFLICT (portal_uuid, tabla, uuid) DO UPDATE SET "
                       "fecha = excluded.fecha, updated_at = excluded.updated_at, "
                       "deleted_at = excluded.deleted_at, pendiente = excluded.pendiente, "
                       "payload = excluded.payload"),
        {QVariant(portal_uuid), QVariant(tabla), QVariant(uuid),
         QVariant::fromValue(fechaDe(tabla, fila)), QVariant::fromValue(updatedAt), borrada,
         QVariant(quedaPendiente ? 1 : 0), QVariant(cifrado)});
    return resultado.ok;
}

int MovilStore::guardarFilas(const QString& portal_uuid, const QString& tabla,
                             const QList<QVariantMap>& filas, int* descartadas)
{
    int escritas = 0;
    int omitidas = 0;
    for (const QVariantMap& fila : filas) {
        if (guardarFila(portal_uuid, tabla, fila, false, true)) {
            ++escritas;
        } else {
            ++omitidas;
        }
    }
    if (descartadas != nullptr) {
        *descartadas = omitidas;
    }
    return escritas;
}

QList<QVariantMap> MovilStore::filas(const QString& portal_uuid, const QString& tabla,
                                     bool incluirBorradas) const
{
    QList<QVariantMap> salida;
    if (!desbloqueado()) {
        return salida;
    }
    QString sql = QStringLiteral(
        "SELECT uuid, fecha, updated_at, deleted_at, pendiente, payload FROM cache_filas "
        "WHERE portal_uuid = ? AND tabla = ?");
    if (!incluirBorradas) {
        sql += QStringLiteral(" AND deleted_at IS NULL");
    }
    sql += QStringLiteral(" ORDER BY fecha IS NULL, fecha, uuid");

    const auto filasDb = db_.query(sql, {QVariant(portal_uuid), QVariant(tabla)});
    for (const auto& cruda : filasDb) {
        QByteArray datos;
        if (!abrirSobre(clave_, cruda.values.at(5).toByteArray(), &datos,
                        aadDe(portal_uuid, tabla, cruda.values.at(0).toString()))) {
            continue;
        }
        QVariantMap fila = datosDe(datos);
        fila.insert(QStringLiteral("uuid"), cruda.values.at(0).toString());
        fila.insert(QStringLiteral("_pendiente"), cruda.values.at(4).toInt() == 1);
        salida.append(fila);
    }
    return salida;
}

QVariantMap MovilStore::fila(const QString& portal_uuid, const QString& tabla,
                             const QString& uuid) const
{
    QVariantMap salida;
    if (!desbloqueado()) {
        return salida;
    }
    const auto filasDb = db_.query(
        QStringLiteral("SELECT payload FROM cache_filas "
                       "WHERE portal_uuid = ? AND tabla = ? AND uuid = ?"),
        {QVariant(portal_uuid), QVariant(tabla), QVariant(uuid)});
    if (filasDb.size() != 1) {
        return salida;
    }
    QByteArray datos;
    if (!abrirSobre(clave_, filasDb.front().values.at(0).toByteArray(), &datos,
                    aadDe(portal_uuid, tabla, uuid))) {
        return salida;
    }
    salida = datosDe(datos);
    salida.insert(QStringLiteral("uuid"), uuid);
    return salida;
}

int MovilStore::contar(const QString& portal_uuid, const QString& tabla) const
{
    const auto filasDb = db_.query(
        QStringLiteral("SELECT count(*) FROM cache_filas "
                       "WHERE portal_uuid = ? AND tabla = ? AND deleted_at IS NULL"),
        {QVariant(portal_uuid), QVariant(tabla)});
    return filasDb.size() == 1 ? filasDb.front().values.at(0).toInt() : 0;
}

QVariantMap MovilStore::portalMeta(const QString& portal_uuid) const
{
    return fila(portal_uuid, QStringLiteral("portal_meta"), QStringLiteral("portal"));
}

// ── Outbox ───────────────────────────────────────────────────

bool MovilStore::encolar(const QString& portal_uuid, const QString& tabla,
                         const QString& uuid, const QVariantMap& datos, qint64 ahora)
{
    if (!desbloqueado() || uuid.isEmpty()) {
        return false;
    }
    const QByteArray cifrado = sobre(
        portal_uuid, QStringLiteral("outbox/") + tabla, uuid, jsonDe(datos));
    if (cifrado.isEmpty()) {
        return false;
    }
    const ExecResult resultado = db_.execute(
        QStringLiteral("INSERT INTO outbox (portal_uuid, uuid, tabla, payload, creado_at) "
                       "VALUES (?, ?, ?, ?, ?) "
                       "ON CONFLICT (portal_uuid, uuid) DO NOTHING"),
        {QVariant(portal_uuid), QVariant(uuid), QVariant(tabla), QVariant(cifrado),
         QVariant::fromValue(ahora > 0 ? ahora : msAhora())});
    return resultado.ok;
}

QList<EntradaOutbox> MovilStore::pendientes(const QString& portal_uuid) const
{
    QList<EntradaOutbox> salida;
    if (!desbloqueado()) {
        return salida;
    }
    const auto filasDb = db_.query(
        QStringLiteral("SELECT id, uuid, tabla, payload, creado_at, intentos, "
                       "coalesce(ultimo_error, '') FROM outbox "
                       "WHERE portal_uuid = ? ORDER BY id"),
        {QVariant(portal_uuid)});
    for (const auto& cruda : filasDb) {
        EntradaOutbox entrada;
        entrada.id = cruda.values.at(0).toLongLong();
        entrada.portal_uuid = portal_uuid;
        entrada.uuid = cruda.values.at(1).toString();
        entrada.tabla = cruda.values.at(2).toString();
        entrada.creado_at = cruda.values.at(4).toLongLong();
        entrada.intentos = cruda.values.at(5).toInt();
        entrada.ultimo_error = cruda.values.at(6).toString();
        QByteArray datos;
        if (!abrirSobre(clave_, cruda.values.at(3).toByteArray(), &datos,
                        aadDe(portal_uuid, QStringLiteral("outbox/") + entrada.tabla,
                              entrada.uuid))) {
            continue;
        }
        entrada.datos = datosDe(datos);
        salida.append(entrada);
    }
    return salida;
}

int MovilStore::pendientesTotales(const QString& portal_uuid) const
{
    const auto filasDb = db_.query(
        QStringLiteral("SELECT count(*) FROM outbox WHERE portal_uuid = ?"),
        {QVariant(portal_uuid)});
    return filasDb.size() == 1 ? filasDb.front().values.at(0).toInt() : 0;
}

bool MovilStore::quitarPendiente(qint64 id)
{
    return db_.execute(QStringLiteral("DELETE FROM outbox WHERE id = ?"),
                       {QVariant::fromValue(id)})
        .ok;
}

bool MovilStore::registrarFallo(qint64 id, const QString& error)
{
    return db_.execute(
               QStringLiteral("UPDATE outbox SET intentos = intentos + 1, ultimo_error = ? "
                              "WHERE id = ?"),
               {QVariant(error), QVariant::fromValue(id)})
        .ok;
}

// ── Altas ────────────────────────────────────────────────────

bool MovilStore::guardarAlta(const AltaLocal& alta)
{
    if (!desbloqueado() || alta.uuid.isEmpty() || alta.clinica_uuid.isEmpty()) {
        return false;
    }
    const QByteArray cifrado = sobreDe(
        clave_, jsonDe(alta.datos),
        aadDe(QStringLiteral("alta"), alta.clinica_uuid, alta.uuid));
    const ExecResult resultado = db_.execute(
        QStringLiteral("INSERT INTO altas "
                       "(uuid, clinica_uuid, clinica_nombre, payload, estado, creado_at) "
                       "VALUES (?, ?, ?, ?, ?, ?) "
                       "ON CONFLICT (uuid) DO UPDATE SET payload = excluded.payload, "
                       "estado = excluded.estado"),
        {QVariant(alta.uuid), QVariant(alta.clinica_uuid), QVariant(alta.clinica_nombre),
         QVariant(cifrado), QVariant(alta.estado),
         QVariant::fromValue(alta.creado_at > 0 ? alta.creado_at : msAhora())});
    return resultado.ok;
}

QList<AltaLocal> MovilStore::altas(const QString& estado) const
{
    QList<AltaLocal> salida;
    if (!desbloqueado()) {
        return salida;
    }
    QString sql = QStringLiteral(
        "SELECT uuid, clinica_uuid, clinica_nombre, payload, estado, creado_at, "
        "coalesce(enviado_at, 0), coalesce(error, '') FROM altas");
    std::vector<QVariant> parametros;
    if (!estado.isEmpty()) {
        sql += QStringLiteral(" WHERE estado = ?");
        parametros.push_back(QVariant(estado));
    }
    sql += QStringLiteral(" ORDER BY creado_at");

    for (const auto& cruda : db_.query(sql, parametros)) {
        AltaLocal alta;
        alta.uuid = cruda.values.at(0).toString();
        alta.clinica_uuid = cruda.values.at(1).toString();
        alta.clinica_nombre = cruda.values.at(2).toString();
        alta.estado = cruda.values.at(4).toString();
        alta.creado_at = cruda.values.at(5).toLongLong();
        alta.enviado_at = cruda.values.at(6).toLongLong();
        alta.error = cruda.values.at(7).toString();
        QByteArray datos;
        if (!abrirSobre(clave_, cruda.values.at(3).toByteArray(), &datos,
                        aadDe(QStringLiteral("alta"), alta.clinica_uuid, alta.uuid))) {
            continue;
        }
        alta.datos = datosDe(datos);
        salida.append(alta);
    }
    return salida;
}

bool MovilStore::marcarAlta(const QString& uuid, const QString& estado,
                            const QString& error, qint64 ahora)
{
    return db_.execute(
               QStringLiteral("UPDATE altas SET estado = ?, error = ?, enviado_at = ? "
                              "WHERE uuid = ?"),
               {QVariant(estado), QVariant(error),
                QVariant::fromValue(ahora > 0 ? ahora : msAhora()), QVariant(uuid)})
        .ok;
}

// ── Registro local ───────────────────────────────────────────

bool MovilStore::guardarRegistroLocal(const QString& tipo, const QString& uuid,
                                      const QVariantMap& datos, qint64 ahora)
{
    if (!desbloqueado() || uuid.isEmpty()) {
        return false;
    }
    const qint64 momento = ahora > 0 ? ahora : msAhora();
    const QByteArray cifrado = sobreDe(
        clave_, jsonDe(datos),
        aadDe(QStringLiteral("registro"), tipo, uuid));
    const ExecResult resultado = db_.execute(
        QStringLiteral("INSERT INTO registro_local (uuid, tipo, payload, created_at, updated_at) "
                       "VALUES (?, ?, ?, ?, ?) "
                       "ON CONFLICT (uuid) DO UPDATE SET payload = excluded.payload, "
                       "updated_at = excluded.updated_at"),
        {QVariant(uuid), QVariant(tipo), QVariant(cifrado),
         QVariant::fromValue(momento), QVariant::fromValue(momento)});
    return resultado.ok;
}

QList<QVariantMap> MovilStore::registroLocal(const QString& tipo, bool importado) const
{
    QList<QVariantMap> salida;
    if (!desbloqueado()) {
        return salida;
    }
    const auto filasDb = db_.query(
        QStringLiteral("SELECT uuid, payload FROM registro_local "
                       "WHERE tipo = ? AND importado = ? ORDER BY created_at"),
        {QVariant(tipo), QVariant(importado ? 1 : 0)});
    for (const auto& cruda : filasDb) {
        const QString uuid = cruda.values.at(0).toString();
        QByteArray datos;
        if (!abrirSobre(clave_, cruda.values.at(1).toByteArray(), &datos,
                        aadDe(QStringLiteral("registro"), tipo, uuid))) {
            continue;
        }
        QVariantMap fila = datosDe(datos);
        fila.insert(QStringLiteral("uuid"), uuid);
        salida.append(fila);
    }
    return salida;
}

bool MovilStore::marcarImportado(const QString& uuid)
{
    return db_.execute(QStringLiteral("UPDATE registro_local SET importado = 1 WHERE uuid = ?"),
                       {QVariant(uuid)})
        .ok;
}

// ── Caducidad ────────────────────────────────────────────────

int MovilStore::purgarVencida(qint64 ahora)
{
    const qint64 momento = ahora > 0 ? ahora : msAhora();
    const ExecResult resultado = db_.execute(
        QStringLiteral("DELETE FROM cache_filas WHERE portal_uuid IN ("
                       "SELECT portal_uuid FROM portales_conocidos "
                       "WHERE ultima_sync IS NOT NULL AND ultima_sync + ? < ?)"),
        {QVariant::fromValue(kDiasCache * kMsDia), QVariant::fromValue(momento)});
    return static_cast<int>(resultado.affected);
}

int MovilStore::limpiarCache()
{
    return static_cast<int>(
        db_.execute(QStringLiteral("DELETE FROM cache_filas")).affected);
}

bool MovilStore::borrarTodo()
{
    db_.execute(QStringLiteral("DELETE FROM outbox"));
    db_.execute(QStringLiteral("DELETE FROM cache_filas"));
    db_.execute(QStringLiteral("DELETE FROM altas"));
    db_.execute(QStringLiteral("DELETE FROM registro_local"));
    db_.execute(QStringLiteral("DELETE FROM portales_conocidos"));
    // Cerrar sesión en el teléfono incluye olvidar la clave: sin las filas de
    // `ajustes` el archivo no se puede volver a descifrar ni con la contraseña
    // correcta. Es el borrado local completo, no un simple bloqueo.
    const bool ajustes = db_.execute(QStringLiteral("DELETE FROM ajustes")).ok;
    clave_.fill('\0');
    clave_.clear();
    return ajustes;
}

} // namespace fuzzy::core
