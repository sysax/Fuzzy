#pragma once

#include <QByteArray>
#include <QString>

namespace fuzzy::core {

// Cifrado autenticado de la caché del teléfono (D23, §4.3).
//
// AES-256-GCM con nonce aleatorio de 96 bits y etiqueta de 128 bits. La clave
// se deriva de la contraseña del dueño con PBKDF2-HMAC-SHA256 y un salt
// aleatorio por instalación, así que extraer el archivo SQLite no basta para
// leer la caché ni el token del portal.
//
// El sobre que devuelve `cifrar` tiene el formato
//
//     [versión:1][nonce:12][etiqueta:16][ciphertext:n]
//
// y `aad` se autentica sin cifrarse: se usa para atar cada fila a su portal,
// tabla y uuid, de modo que un sobre no se pueda mover de sitio.
class CryptoBox
{
public:
    static constexpr int kBytesClave = 32;
    static constexpr int kBytesNonce = 12;
    static constexpr int kBytesTag = 16;
    static constexpr quint8 kVersion = 1;

    // Deriva una clave de 32 bytes. `iteraciones` se guarda junto al salt
    // para poder subirlo sin invalidar lo ya cifrado.
    static QByteArray derivarClave(const QString& password, const QByteArray& salt,
                                   int iteraciones);

    // Devuelve el sobre cifrado, o vacío si la clave no mide lo esperado.
    static QByteArray cifrar(const QByteArray& clave, const QByteArray& datos,
                             const QByteArray& aad = {});

    // Igual que `cifrar` pero con un nonce dado. Solo para pruebas y vectores
    // conocidos: en producción el nonce debe ser aleatorio.
    static QByteArray cifrarConNonce(const QByteArray& clave, const QByteArray& nonce,
                                     const QByteArray& datos,
                                     const QByteArray& aad = {});

    // Descifra un sobre y verifica la etiqueta. Devuelve false si el sobre
    // está truncado, si la clave es incorrecta o si el AAD no coincide.
    static bool descifrar(const QByteArray& clave, const QByteArray& sobre,
                          QByteArray* datos, const QByteArray& aad = {});

    static QByteArray aleatorio(int bytes);

    // Utilidad para comparar etiquetas en tiempo constante.
    static bool igualesEnTiempoConstante(const QByteArray& a, const QByteArray& b);
};

} // namespace fuzzy::core
