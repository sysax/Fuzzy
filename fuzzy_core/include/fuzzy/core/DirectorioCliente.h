#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QByteArray>
#include <QList>
#include <QString>

namespace fuzzy::core {

// Clínica tal como aparece en `fuzzy-directory`: solo datos comerciales y el
// token público de inserción del buzón. Nunca identidades ni tokens de clínica
// (D22).
struct ClinicaPublica
{
    QString uuid;
    QString nombre;
    QString ciudad;
    QString direccion;
    QString telefono;
    QString inbox_url;
    QString inbox_token;
    bool activa = true;
    qint64 updated_at = 0;
};

// Lectura del directorio. Acepta el JSON que publica CI y la tabla espejo de
// `fuzzy-directory`.
class DirectorioCliente
{
public:
    // Formato: {"clinicas": [ ... ]} o directamente un array.
    static QList<ClinicaPublica> desdeJson(const QByteArray& json, QString* error = nullptr);

    // Filas de la base del directorio. El ejecutor suele ser un
    // TursoHttpExecutor con el token público de solo lectura.
    static QList<ClinicaPublica> desdeExecutor(IQueryExecutor& directorio);

    // Filtra por nombre, ciudad o dirección sin distinguir mayúsculas.
    static QList<ClinicaPublica> buscar(const QList<ClinicaPublica>& clinicas,
                                        const QString& texto);
};

} // namespace fuzzy::core
