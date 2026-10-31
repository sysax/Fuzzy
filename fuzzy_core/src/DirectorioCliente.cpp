#include "fuzzy/core/DirectorioCliente.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QVariant>

namespace fuzzy::core {

namespace {

ClinicaPublica desdeObjeto(const QJsonObject& objeto)
{
    ClinicaPublica clinica;
    clinica.uuid = objeto.value(QStringLiteral("uuid")).toString();
    clinica.nombre = objeto.value(QStringLiteral("nombre")).toString();
    clinica.ciudad = objeto.value(QStringLiteral("ciudad")).toString();
    clinica.direccion = objeto.value(QStringLiteral("direccion")).toString();
    clinica.telefono = objeto.value(QStringLiteral("telefono")).toString();
    clinica.inbox_url = objeto.value(QStringLiteral("inbox_url")).toString();
    clinica.inbox_token = objeto.value(QStringLiteral("inbox_token")).toString();
    clinica.activa = objeto.value(QStringLiteral("activa")).toBool(true);
    clinica.updated_at = objeto.value(QStringLiteral("updated_at")).toVariant().toLongLong();
    return clinica;
}

} // namespace

QList<ClinicaPublica> DirectorioCliente::desdeJson(const QByteArray& json, QString* error)
{
    QList<ClinicaPublica> salida;
    QJsonParseError fallo;
    const QJsonDocument documento = QJsonDocument::fromJson(json, &fallo);
    if (fallo.error != QJsonParseError::NoError) {
        if (error != nullptr) {
            *error = fallo.errorString();
        }
        return salida;
    }

    QJsonArray lista;
    if (documento.isArray()) {
        lista = documento.array();
    } else if (documento.isObject()) {
        lista = documento.object().value(QStringLiteral("clinicas")).toArray();
    }

    for (const QJsonValue& valor : lista) {
        if (!valor.isObject()) {
            continue;
        }
        const ClinicaPublica clinica = desdeObjeto(valor.toObject());
        // Una entrada sin identidad o sin nombre no se puede mostrar ni usar
        // para enviar un alta.
        if (clinica.uuid.isEmpty() || clinica.nombre.isEmpty()) {
            continue;
        }
        salida.append(clinica);
    }
    if (salida.isEmpty() && error != nullptr) {
        *error = QStringLiteral("El directorio no tiene clínicas válidas");
    }
    return salida;
}

QList<ClinicaPublica> DirectorioCliente::desdeExecutor(IQueryExecutor& directorio)
{
    QList<ClinicaPublica> salida;
    const auto filas = directorio.query(
        QStringLiteral("SELECT uuid, nombre, ciudad, direccion, telefono, inbox_url, "
                       "inbox_token, activa, updated_at FROM clinicas_publicas "
                       "WHERE activa = 1 ORDER BY nombre"));
    for (const auto& fila : filas) {
        ClinicaPublica clinica;
        clinica.uuid = fila.values.at(0).toString();
        clinica.nombre = fila.values.at(1).toString();
        clinica.ciudad = fila.values.at(2).toString();
        clinica.direccion = fila.values.at(3).toString();
        clinica.telefono = fila.values.at(4).toString();
        clinica.inbox_url = fila.values.at(5).toString();
        clinica.inbox_token = fila.values.at(6).toString();
        clinica.activa = fila.values.at(7).toInt() != 0;
        clinica.updated_at = fila.values.at(8).toLongLong();
        if (!clinica.uuid.isEmpty() && !clinica.nombre.isEmpty()) {
            salida.append(clinica);
        }
    }
    return salida;
}

QList<ClinicaPublica> DirectorioCliente::buscar(const QList<ClinicaPublica>& clinicas,
                                                const QString& texto)
{
    const QString buscado = texto.trimmed();
    if (buscado.isEmpty()) {
        return clinicas;
    }
    QList<ClinicaPublica> salida;
    for (const ClinicaPublica& clinica : clinicas) {
        if (clinica.nombre.contains(buscado, Qt::CaseInsensitive)
            || clinica.ciudad.contains(buscado, Qt::CaseInsensitive)
            || clinica.direccion.contains(buscado, Qt::CaseInsensitive)) {
            salida.append(clinica);
        }
    }
    return salida;
}

} // namespace fuzzy::core
