#pragma once

#include <QString>

namespace fuzzy::core {

// Estado de una base de clínica visto por el operador. No requiere sesión: la
// CLI se ejecuta en el equipo del nodo con acceso al archivo.
struct EstadoOperador
{
    bool ok = false;
    QString error;
    QString ruta;
    QString clinicaUuid;
    QString clinicaNombre;
    QString versionEsquema;
    QString integridad;
    bool integridadOk = false;
    qint64 bytes = 0;
    qint64 usuarios = 0;
    qint64 clientes = 0;
    qint64 mascotas = 0;
    qint64 portales = 0;
};

struct ResultadoRespaldo
{
    bool ok = false;
    QString error;
    QString ruta;
    QString sha256;
    QString manifiesto;
    qint64 bytes = 0;
    qint64 creadoAt = 0;
};

struct ResultadoRestauracion
{
    bool ok = false;
    QString error;
    QString ruta;
    // Copia de seguridad del archivo que se reemplazó, para poder deshacer la
    // restauración si el resultado no es el esperado.
    QString respaldoPrevio;
};

struct ResultadoRecuperacionAdmin
{
    bool ok = false;
    QString error;
    QString correo;
    QString usuarioUuid;
    QString passwordTemporal;
};

// Operaciones locales de la CLI `fuzzy-admin` (fase 10).
//
// D7: el último Admin se recupera desde aquí, nunca reabriendo el bootstrap.
// Los respaldos son archivos SQLite completos (VACUUM INTO) con checksum y
// manifiesto; la restauración exige confirmación explícita y deja una copia
// del archivo anterior.
class OperadorService
{
public:
    explicit OperadorService(QString rutaBase);

    QString rutaBase() const { return ruta_; }

    EstadoOperador estado() const;

    // Crea un respaldo consistente en `destino` y escribe `destino.sha256` y
    // `destino.json`. Falla si el destino ya existe.
    ResultadoRespaldo respaldar(const QString& destino) const;

    // Comprueba el checksum del respaldo y la integridad de la base copiada.
    bool verificarRespaldo(const QString& archivo, QString* error = nullptr) const;

    // Reemplaza la base actual por un respaldo verificado. Solo con
    // `confirmado = true`. Deja la base anterior en `<ruta>.antes-de-restaurar-<ts>`.
    ResultadoRestauracion restaurar(const QString& origen, bool confirmado);

    // Restablece la contraseña de una cuenta Admin, la reactiva y emite una
    // contraseña temporal de 72 h con cambio obligatorio. No exige sesión.
    ResultadoRecuperacionAdmin recuperarAdmin(const QString& correo);

    // Aplica las migraciones embebidas de la clínica (crea la base si falta).
    bool migrar(QString* error = nullptr) const;

private:
    QString ruta_;
};

} // namespace fuzzy::core
