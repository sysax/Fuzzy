#include "fuzzy/core/OperadorService.h"

#include <QCoreApplication>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include <QTextStream>

using fuzzy::core::EstadoOperador;
using fuzzy::core::OperadorService;

namespace {

QTextStream& salida()
{
    static QTextStream flujo(stdout);
    return flujo;
}

QTextStream& error()
{
    static QTextStream flujo(stderr);
    return flujo;
}

void ayuda()
{
    salida()
        << "fuzzy-admin — operaciones locales del nodo (fase 10)\n"
        << "\n"
        << "Uso:\n"
        << "  fuzzy-admin estado --db <ruta> [--json]\n"
        << "  fuzzy-admin verificar --db <ruta> [--respaldo <archivo>]\n"
        << "  fuzzy-admin respaldar --db <ruta> --salida <archivo>\n"
        << "  fuzzy-admin restaurar --db <ruta> --desde <archivo> --confirmar\n"
        << "  fuzzy-admin recuperar-admin --db <ruta> --correo <correo>\n"
        << "  fuzzy-admin migrar --db <ruta>\n"
        << "  fuzzy-admin --version\n"
        << "\n"
        << "Notas:\n"
        << "  · Detén el escritorio antes de restaurar una base.\n"
        << "  · La contraseña temporal se muestra una sola vez.\n"
        << "  · Los respaldos incluyen .sha256 y .json junto al archivo .db.\n"
        << "\n"
        << "Códigos de salida: 0 correcto, 1 uso incorrecto, 2 operación fallida.\n";
}

void imprimirEstado(const EstadoOperador& estado, bool json)
{
    if (json) {
        QJsonObject objeto;
        objeto.insert(QStringLiteral("ok"), estado.ok);
        objeto.insert(QStringLiteral("ruta"), estado.ruta);
        objeto.insert(QStringLiteral("clinica_uuid"), estado.clinicaUuid);
        objeto.insert(QStringLiteral("clinica"), estado.clinicaNombre);
        objeto.insert(QStringLiteral("version_esquema"), estado.versionEsquema);
        objeto.insert(QStringLiteral("integridad"), estado.integridad);
        objeto.insert(QStringLiteral("integridad_ok"), estado.integridadOk);
        objeto.insert(QStringLiteral("bytes"), static_cast<double>(estado.bytes));
        objeto.insert(QStringLiteral("usuarios"), static_cast<double>(estado.usuarios));
        objeto.insert(QStringLiteral("clientes"), static_cast<double>(estado.clientes));
        objeto.insert(QStringLiteral("mascotas"), static_cast<double>(estado.mascotas));
        objeto.insert(QStringLiteral("portales"), static_cast<double>(estado.portales));
        if (!estado.error.isEmpty()) {
            objeto.insert(QStringLiteral("error"), estado.error);
        }
        salida() << QString::fromUtf8(
                        QJsonDocument(objeto).toJson(QJsonDocument::Compact))
                 << "\n";
        return;
    }

    salida() << "Base:        " << estado.ruta << "\n";
    salida() << "Clínica:     " << estado.clinicaNombre << " (" << estado.clinicaUuid
             << ")\n";
    salida() << "Esquema:     " << estado.versionEsquema << "\n";
    salida() << "Integridad:  " << estado.integridad << "\n";
    salida() << "Tamaño:      " << estado.bytes << " bytes\n";
    salida() << "Usuarios:    " << estado.usuarios << "\n";
    salida() << "Clientes:    " << estado.clientes << "\n";
    salida() << "Mascotas:    " << estado.mascotas << "\n";
    salida() << "Portales:    " << estado.portales << "\n";
}

QString opcion(const QHash<QString, QString>& opciones, const QString& nombre)
{
    return opciones.value(nombre);
}

int fallo(const QString& mensaje)
{
    error() << "fuzzy-admin: " << mensaje << "\n";
    return 2;
}

int usoIncorrecto(const QString& mensaje)
{
    error() << "fuzzy-admin: " << mensaje << "\n\n";
    ayuda();
    return 1;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("fuzzy-admin"));
    QCoreApplication::setApplicationVersion(QStringLiteral(FUZZY_VERSION));

    QStringList argumentos = app.arguments();
    argumentos.removeFirst();

    if (argumentos.isEmpty()) {
        ayuda();
        return 1;
    }
    if (argumentos.contains(QStringLiteral("--help"))
        || argumentos.contains(QStringLiteral("-h"))) {
        ayuda();
        return 0;
    }
    if (argumentos.first() == QStringLiteral("--version")
        || argumentos.first() == QStringLiteral("version")) {
        salida() << "fuzzy-admin " << QCoreApplication::applicationVersion() << "\n";
        return 0;
    }

    const QString comando = argumentos.takeFirst();
    QHash<QString, QString> opciones;
    QSet<QString> banderas;
    for (int i = 0; i < argumentos.size(); ++i) {
        const QString argumento = argumentos.at(i);
        if (argumento == QStringLiteral("--json")
            || argumento == QStringLiteral("--confirmar")) {
            banderas.insert(argumento);
            continue;
        }
        if (!argumento.startsWith(QStringLiteral("--"))) {
            return usoIncorrecto(QStringLiteral("argumento inesperado: %1").arg(argumento));
        }
        if (i + 1 >= argumentos.size()) {
            return usoIncorrecto(
                QStringLiteral("falta el valor de %1").arg(argumento));
        }
        opciones.insert(argumento, argumentos.at(++i));
    }

    const QString ruta = opcion(opciones, QStringLiteral("--db"));
    if (ruta.isEmpty()) {
        return usoIncorrecto(QStringLiteral("falta --db"));
    }

    OperadorService operador(ruta);

    if (comando == QStringLiteral("estado")) {
        const EstadoOperador estado = operador.estado();
        if (!estado.ok) {
            return fallo(estado.error);
        }
        imprimirEstado(estado, banderas.contains(QStringLiteral("--json")));
        return 0;
    }

    if (comando == QStringLiteral("verificar")) {
        const QString respaldo = opcion(opciones, QStringLiteral("--respaldo"));
        if (!respaldo.isEmpty()) {
            QString detalle;
            if (!operador.verificarRespaldo(respaldo, &detalle)) {
                return fallo(detalle);
            }
            salida() << "Respaldo válido: " << respaldo << "\n";
            return 0;
        }

        const EstadoOperador estado = operador.estado();
        if (!estado.ok) {
            return fallo(estado.error);
        }
        salida() << "Base válida: " << estado.ruta << " (esquema "
                 << estado.versionEsquema << ")\n";
        return 0;
    }

    if (comando == QStringLiteral("respaldar")) {
        const QString destino = opcion(opciones, QStringLiteral("--salida"));
        if (destino.isEmpty()) {
            return usoIncorrecto(QStringLiteral("falta --salida"));
        }
        const auto resultado = operador.respaldar(destino);
        if (!resultado.ok) {
            return fallo(resultado.error);
        }
        salida() << "Respaldo creado: " << resultado.ruta << "\n";
        salida() << "SHA-256:         " << resultado.sha256 << "\n";
        salida() << "Bytes:           " << resultado.bytes << "\n";
        salida() << "Manifiesto:      " << resultado.manifiesto << "\n";
        return 0;
    }

    if (comando == QStringLiteral("restaurar")) {
        const QString origen = opcion(opciones, QStringLiteral("--desde"));
        if (origen.isEmpty()) {
            return usoIncorrecto(QStringLiteral("falta --desde"));
        }
        if (!banderas.contains(QStringLiteral("--confirmar"))) {
            return usoIncorrecto(
                QStringLiteral("la restauración exige --confirmar"));
        }
        const auto resultado = operador.restaurar(
            origen, banderas.contains(QStringLiteral("--confirmar")));
        if (!resultado.ok) {
            return fallo(resultado.error);
        }
        salida() << "Base restaurada: " << resultado.ruta << "\n";
        if (!resultado.respaldoPrevio.isEmpty()) {
            salida() << "Copia previa:    " << resultado.respaldoPrevio << "\n";
        }
        return 0;
    }

    if (comando == QStringLiteral("recuperar-admin")) {
        const QString correo = opcion(opciones, QStringLiteral("--correo"));
        if (correo.isEmpty()) {
            return usoIncorrecto(QStringLiteral("falta --correo"));
        }
        const auto resultado = operador.recuperarAdmin(correo);
        if (!resultado.ok) {
            return fallo(resultado.error);
        }
        salida() << "Recuperación emitida para " << resultado.correo << "\n";
        salida() << "Contraseña temporal: " << resultado.passwordTemporal << "\n";
        salida() << "Válida 72 h; el usuario debe cambiarla al entrar.\n";
        error() << "fuzzy-admin: guarda la contraseña; no se vuelve a mostrar.\n";
        return 0;
    }

    if (comando == QStringLiteral("migrar")) {
        QString detalle;
        if (!operador.migrar(&detalle)) {
            return fallo(detalle);
        }
        const EstadoOperador estado = operador.estado();
        if (!estado.ok) {
            return fallo(estado.error);
        }
        salida() << "Esquema aplicado en " << estado.ruta << " (versión "
                 << estado.versionEsquema << ")\n";
        return 0;
    }

    return usoIncorrecto(QStringLiteral("comando desconocido: %1").arg(comando));
}
