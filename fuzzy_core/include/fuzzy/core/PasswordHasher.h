#pragma once

#include <QByteArray>
#include <QString>

namespace fuzzy::core {

// Hash de contraseñas del personal y de las cuentas del portal (D19).
//
// PBKDF2-HMAC-SHA256 con 600 000 iteraciones, salt aleatorio por usuario y
// comparación en tiempo constante. El formato almacenado es
//
//     pbkdf2_sha256$<iteraciones>$<salt-hex>$<hash-hex>
//
// para poder subir el número de iteraciones sin invalidar los hashes previos.
class PasswordHasher
{
public:
    static constexpr int kIteraciones = 600'000;
    static constexpr int kBytesSalt = 16;
    static constexpr int kBytesHash = 32;

    static QString generar(const QString& password,
                           int iteraciones = kIteraciones);
    static bool verificar(const QString& password, const QString& almacenado);

    // Contraseña temporal autogenerada de 12 caracteres (D8), sin caracteres
    // ambiguos para que se pueda dictar o copiar sin errores.
    static QString passwordTemporal(int longitud = 12);

    static bool esFormatoValido(const QString& almacenado);
};

} // namespace fuzzy::core
