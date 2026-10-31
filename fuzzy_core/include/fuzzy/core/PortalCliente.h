#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <functional>

namespace fuzzy::core {

class MovilStore;

// Tablas del portal que el teléfono puede leer, con sus columnas en orden.
// Es una lista blanca cerrada: el SQL de descarga y de subida se construye solo
// con estos nombres, nunca con texto que venga del QR o de la interfaz.
struct TablaPortal
{
    QString nombre;
    QStringList columnas;
};

struct ResultadoDescarga
{
    bool ok = false;
    int filas = 0;
    QString error;
};

struct ResultadoSync
{
    bool ok = false;
    // ok | sin_red | sin_acceso | sin_clave | sin_portal
    QString estado = QStringLiteral("ok");
    int enviadas = 0;
    int fallidas = 0;
    int descargadas = 0;
    QString error;
};

// Cliente del portal de un dueño (D18, D23, D26).
//
// El portal es la unidad de aislamiento: este objeto se construye con el
// ejecutor de UNA base de portal y nunca recibe el `clinica_uuid` de otra. Antes
// de aplicar una descarga se verifica que `portal_meta.clinica_uuid` sea el de
// la clínica del emparejamiento; si no coincide, no se escribe nada.
//
// La caché local se escribe cifrada y con resolución de conflictos LWW
// (`MovilStore::guardarFila`). Las escrituras del dueño van primero a la caché
// y a la cola local, así que la app funciona en modo avión y sincroniza al
// recuperar la red.
class PortalCliente
{
public:
    // Sonda de salud del portal. El contrato `IQueryExecutor` no distingue
    // "sin red" de "token revocado" (una consulta fallida devuelve vacío), así
    // que el transporte aporta esa información. Debe devolver
    // "ok" | "sin_red" | "sin_acceso".
    using Sonda = std::function<QString()>;

    explicit PortalCliente(IQueryExecutor& portal, Sonda sonda = {});

    static const QList<TablaPortal>& tablas();
    static bool tablaConocida(const QString& tabla);
    static QStringList columnasDe(const QString& tabla);

    // Descarga todas las tablas del portal a la caché cifrada. Devuelve fallo
    // sin tocar la caché si la clínica no coincide con `clinica_esperada` o si
    // `portal_meta` no se puede leer.
    ResultadoDescarga descargar(MovilStore& store, const QString& portal_uuid,
                                const QString& clinica_esperada = {});

    // Sube la cola pendiente y luego descarga. Es idempotente: un corte de red
    // deja la cola intacta y el siguiente intento reenvía lo mismo.
    ResultadoSync sincronizar(MovilStore& store, const QString& portal_uuid);

    // Encola una solicitud de cita (D17). La fila local queda `pendiente` y se
    // libera cuando el nodo la devuelve en la descarga.
    bool solicitarCita(MovilStore& store, const QString& portal_uuid,
                       const QString& mascota_uuid, const QString& tipo_servicio,
                       const QString& fecha_preferida, const QString& hora_preferida,
                       const QString& motivo = {}, QString uuid = {}, qint64 ahora = 0);

    // Encola un cambio de perfil sobre la fila ya conocida. El uuid del perfil
    // sale de la caché: la app no inventa identidades para el dueño.
    bool actualizarPerfil(MovilStore& store, const QString& portal_uuid,
                          const QVariantMap& campos, qint64 ahora = 0);

    QString ultimoError() const { return ultimo_error_; }

private:
    bool empujar(MovilStore& store, const QString& portal_uuid, ResultadoSync& resultado);
    bool subirPendiente(const QString& tabla, const QVariantMap& datos);

    IQueryExecutor& portal_;
    Sonda sonda_;
    QString ultimo_error_;
};

} // namespace fuzzy::core
