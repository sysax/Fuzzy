#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QByteArray>
#include <QString>

namespace fuzzy::core {

struct ResultadoFoto
{
    bool ok = false;
    QString error;
    QString uuid;
    QString sha256;
    int ancho = 0;
    int alto = 0;
    int bytes = 0;
};

// Foto de perfil de una mascota en el portal del dueño (D2).
//
// Valida y reencoda la imagen en el nodo: máximo 1024 px de lado mayor y
// 2 MB tanto de entrada como de salida, siempre JPEG. La fila de
// `mascota_fotos` es única por mascota y se reemplaza con un UPSERT sobre la
// misma fila: no quedan versiones, filas huérfanas ni temporales.
class MascotaFotoService
{
public:
    static constexpr int kLadoMaximo = 1024;
    static constexpr int kBytesMaximos = 2 * 1024 * 1024;
    static constexpr qint64 kPixelesMaximos = 40'000'000;

    explicit MascotaFotoService(IQueryExecutor& portal);

    // Decodifica, orienta según EXIF, escala y codifica a JPEG. Devuelve el
    // JPEG normalizado y sus dimensiones finales.
    static bool reencodar(const QByteArray& original, QByteArray& jpeg,
                          int& ancho, int& alto, QString* error = nullptr);

    ResultadoFoto reemplazar(const QString& mascota_uuid, const QByteArray& original);

private:
    IQueryExecutor& portal_;
};

} // namespace fuzzy::core
