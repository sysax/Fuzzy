#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QByteArray>
#include <QString>

namespace fuzzy::core {

class FileStore;

struct ResultadoImagen
{
    bool ok = false;
    QString error;
    QString uuid;
    QString storagePath;
    QString mime;
    QString sha256;
    int bytes = 0;
    bool existente = false;
};

// Galería clínica de una mascota (radiografías, ecografías, laboratorios).
//
// El binario se guarda en el disco del nodo a través de `FileStore`; la base
// de la clínica solo conserva metadatos verificables. La entidad no participa
// del sync: el binario y su ruta nunca viajan. El proyector publica únicamente
// la fila de metadatos en `adjuntos` del portal.
class GaleriaClinicaService
{
public:
    static constexpr int kBytesMaximos = 10 * 1024 * 1024;

    GaleriaClinicaService(IQueryExecutor& clinica, FileStore& almacen);

    // Guarda el binario y registra la fila. El uuid es determinista a partir
    // del contenido y del destino, de modo que reintentar no duplica nada.
    // `mime` se deduce del contenido; no se aceptan formatos desconocidos.
    ResultadoImagen guardar(const QString& mascota_uuid, const QString& titulo,
                            const QString& tipo, const QByteArray& datos,
                            const QString& visita_uuid = QString(),
                            const QString& usuario_uuid = QString());

    // Borra la fila y el archivo. No deja metadatos apuntando a un binario
    // ausente ni un archivo huérfano.
    bool eliminar(const QString& uuid, QString* error = nullptr);

    bool leer(const QString& uuid, QByteArray& datos, QString* error = nullptr) const;

private:
    IQueryExecutor& clinica_;
    FileStore& almacen_;
};

} // namespace fuzzy::core
