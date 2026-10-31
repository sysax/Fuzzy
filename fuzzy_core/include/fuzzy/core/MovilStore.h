#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVariantMap>

namespace fuzzy::core {

// Portal emparejado en este teléfono. Uno por clínica (D25, D26).
struct PortalConocido
{
    QString portal_uuid;
    QString clinica_uuid;
    QString clinica_nombre;
    QString url;
    QString huella;
    QString estado = QStringLiteral("activo"); // activo | revocado | inaccesible
    qint64 emparejado_at = 0;
    qint64 ultima_sync = 0;
};

struct EntradaOutbox
{
    qint64 id = 0;
    QString portal_uuid;
    QString uuid;
    QString tabla;
    QVariantMap datos;
    qint64 creado_at = 0;
    int intentos = 0;
    QString ultimo_error;
};

struct AltaLocal
{
    QString uuid;
    QString clinica_uuid;
    QString clinica_nombre;
    QString estado = QStringLiteral("pendiente"); // pendiente | enviada | rechazada
    QVariantMap datos;
    qint64 creado_at = 0;
    qint64 enviado_at = 0;
    QString error;
};

// Almacén local del teléfono: portales, caché cifrada, cola de escrituras,
// altas del buzón y registro previo al emparejamiento.
//
// La clave sale de la contraseña del dueño (PBKDF2-HMAC-SHA256 + salt por
// instalación) y vive solo en memoria mientras la app está desbloqueada. Cada
// fila de caché y cada token se guardan en un sobre AES-256-GCM atado a su
// portal, tabla y uuid, de modo que mover un sobre de sitio no descifra.
class MovilStore
{
public:
    static constexpr qint64 kDiasCache = 30;
    static constexpr qint64 kMsDia = 24 * 60 * 60 * 1000;

    explicit MovilStore(IQueryExecutor& db);

    // ── Clave de la caché ────────────────────────────────────
    // Permite bajar las iteraciones en pruebas; en producción no se toca.
    void setIteraciones(int iteraciones) { iteraciones_ = iteraciones; }
    int iteraciones() const { return iteraciones_; }

    bool tieneClave() const;
    bool configurarClave(const QString& password, qint64 ahora = 0);
    bool desbloquear(const QString& password);
    void bloquear();
    bool desbloqueado() const { return clave_.size() == 32; }

    // ── Portales ─────────────────────────────────────────────
    QList<PortalConocido> portales() const;
    bool portal(const QString& portal_uuid, PortalConocido* salida) const;
    bool agregarPortal(const PortalConocido& portal, const QByteArray& token);
    QByteArray tokenDe(const QString& portal_uuid) const;
    bool marcarEstado(const QString& portal_uuid, const QString& estado);
    bool registrarSync(const QString& portal_uuid, qint64 ahora);
    bool olvidarPortal(const QString& portal_uuid);

    // ── Caché cifrada ────────────────────────────────────────
    // Upsert idempotente por (portal, tabla, uuid) con LWW por `updated_at`.
    // Una fila local pendiente no se pisa con una descarga que no la incluya;
    // cuando el nodo la devuelve, `pendiente` se limpia.
    bool guardarFila(const QString& portal_uuid, const QString& tabla,
                     const QVariantMap& fila, bool pendiente = false,
                     bool desdeRemoto = false);
    int guardarFilas(const QString& portal_uuid, const QString& tabla,
                     const QList<QVariantMap>& filas, int* descartadas = nullptr);
    QList<QVariantMap> filas(const QString& portal_uuid, const QString& tabla,
                             bool incluirBorradas = false) const;
    QVariantMap fila(const QString& portal_uuid, const QString& tabla,
                     const QString& uuid) const;
    int contar(const QString& portal_uuid, const QString& tabla) const;
    QVariantMap portalMeta(const QString& portal_uuid) const;

    // ── Outbox ───────────────────────────────────────────────
    bool encolar(const QString& portal_uuid, const QString& tabla, const QString& uuid,
                 const QVariantMap& datos, qint64 ahora = 0);
    QList<EntradaOutbox> pendientes(const QString& portal_uuid) const;
    int pendientesTotales(const QString& portal_uuid) const;
    bool quitarPendiente(qint64 id);
    bool registrarFallo(qint64 id, const QString& error);

    // ── Altas del buzón ──────────────────────────────────────
    bool guardarAlta(const AltaLocal& alta);
    QList<AltaLocal> altas(const QString& estado = QString()) const;
    bool marcarAlta(const QString& uuid, const QString& estado, const QString& error,
                    qint64 ahora = 0);

    // ── Registro local antes de emparejar ────────────────────
    bool guardarRegistroLocal(const QString& tipo, const QString& uuid,
                              const QVariantMap& datos, qint64 ahora = 0);
    QList<QVariantMap> registroLocal(const QString& tipo, bool importado = false) const;
    bool marcarImportado(const QString& uuid);

    // ── Caducidad y borrado ──────────────────────────────────
    // La caché descargada caduca a los 30 días desde `ultima_sync` (§4.6).
    int purgarVencida(qint64 ahora = 0);
    int limpiarCache();

    // Borrado local completo: portales, caché, cola, altas, registro y la
    // clave derivada. Tras esto hay que crear clave y volver a emparejar.
    bool borrarTodo();

private:
    QVariantMap descifrarFila(const QVariantMap& crudo) const;
    QByteArray sobre(const QString& portal_uuid, const QString& tabla,
                     const QString& uuid, const QByteArray& datos) const;
    static QByteArray jsonDe(const QVariantMap& datos);
    static QVariantMap datosDe(const QByteArray& json);
    static qint64 fechaDe(const QString& tabla, const QVariantMap& fila);
    static qint64 msAhora();

    IQueryExecutor& db_;
    QByteArray clave_;
    int iteraciones_ = 600'000;
};

} // namespace fuzzy::core
