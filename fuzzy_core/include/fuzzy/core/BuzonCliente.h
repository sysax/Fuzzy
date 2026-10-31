#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QString>
#include <QVariantMap>

namespace fuzzy::core {

// Resultado de una alta enviada al buzón de la clínica.
struct ResultadoAlta
{
    bool ok = false;
    QString uuid;
    QString error;
};

// Escritura en el buzón de primera visita (`fuzzy-inbox-<clinica>.altas`).
//
// El buzón es de un solo sentido: la app inserta y nunca lee. La validación
// local reproduce los CHECK de la migración para que un alta inválida no salga
// del teléfono con un error opaco del servidor. El `uuid` es la clave de
// idempotencia: reenviar tras un corte no duplica la solicitud.
class BuzonCliente
{
public:
    explicit BuzonCliente(IQueryExecutor& inbox);

    // Valida y normaliza los campos de un alta. Devuelve un mapa con
    // `uuid, clinica_uuid, nombre, correo, telefono, mascota, motivo, franja,
    // creada_at`. Un campo opcional vacío se guarda como nulo.
    static QVariantMap normalizar(const QVariantMap& datos, QString uuid,
                                  const QString& clinica_uuid, QString* error);

    // Inserta el alta ya normalizada. Idempotente por `uuid`.
    ResultadoAlta enviar(const QVariantMap& alta);

    // Genera el uuid si falta, valida y envía en una sola llamada.
    ResultadoAlta enviar(const QVariantMap& datos, const QString& clinica_uuid,
                         QString uuid = {});

private:
    IQueryExecutor& inbox_;
};

} // namespace fuzzy::core
