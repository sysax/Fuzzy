#pragma once

#include "fuzzy/core/IQueryExecutor.h"
#include "fuzzy/core/PortalCliente.h"

#include <QByteArray>
#include <QString>

namespace fuzzy::core {

class MovilStore;

struct DatosEmparejamiento
{
    int v = 0;
    QString url;
    QString token;
    QString portal_uuid;
    QString clinica_uuid;
    QString clinica_nombre;
    QString huella;
};

struct ResultadoEmparejamiento
{
    bool ok = false;
    QString estado;
    QString portal_uuid;
    QString clinica_nombre;
    int importados = 0;
    int pendientes = 0;
    int filas = 0;
    QString error;
};

// Emparejamiento del teléfono con el portal de una clínica (D23, §4.3).
//
// El QR que muestra el nodo lleva `{v, url, token, portal_uuid, clinica_uuid,
// clinica_nombre, huella}`. La huella de 6 hex se muestra de vuelta y el
// operador confirma que coincide: así un QR manipulado en una foto no pasa
// desapercibido. El token se guarda cifrado y la descarga inicial verifica que
// la base del portal pertenezca a la clínica del QR.
class EmparejamientoService
{
public:
    EmparejamientoService(IQueryExecutor& portal, MovilStore& store,
                          PortalCliente::Sonda sonda = {});

    // Parsea y valida el QR sin tocar la base. Devuelve false y explica el
    // motivo si falta un campo, la versión no es 1 o la huella no corresponde.
    static bool leerQr(const QByteArray& json, DatosEmparejamiento* datos,
                       QString* error = nullptr);

    // Empareja: guarda el portal, descarga su contenido e importa el registro
    // local. Si la descarga falla, deshace el alta para no dejar un portal a
    // medio emparejar.
    ResultadoEmparejamiento emparejar(const QByteArray& qr);

    // Marca como importadas las filas de `registro_local` que ya existen en el
    // portal descargado. Las que no coinciden quedan pendientes: la app las
    // mostrará como "aún no registradas en la clínica". Devuelve cuántas se
    // importaron y deja en `pendientes` las que siguen sin coincidir.
    static int importarRegistro(MovilStore& store, const QString& portal_uuid,
                                int* pendientes = nullptr);

private:
    IQueryExecutor& portal_;
    MovilStore& store_;
    PortalCliente::Sonda sonda_;
};

} // namespace fuzzy::core
