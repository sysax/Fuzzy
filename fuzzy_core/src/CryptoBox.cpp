#include "fuzzy/core/CryptoBox.h"

#include <QCryptographicHash>
#include <QPasswordDigestor>
#include <QRandomGenerator>

#include <algorithm>
#include <array>
#include <cstring>

namespace fuzzy::core {

namespace {

constexpr int kBloque = 16;
constexpr int kRondas = 14; // AES-256
constexpr int kPalabras = 4 * (kRondas + 1);

uint8_t xtime(const uint8_t valor)
{
    return static_cast<uint8_t>((valor << 1) ^ ((valor & 0x80) ? 0x1B : 0x00));
}

// Producto en GF(2^8) con el polinomio 0x11B. Se usa solo para construir la
// caja S al arrancar, así que no hace falta una tabla de logaritmos.
uint8_t multiplicar(uint8_t a, uint8_t b)
{
    uint8_t producto = 0;
    for (int bit = 0; bit < 8; ++bit) {
        if (b & 1) {
            producto ^= a;
        }
        a = xtime(a);
        b >>= 1;
    }
    return producto;
}

// Inverso multiplicativo por exponenciación: a^254 = a^-1 en GF(2^8).
uint8_t inverso(const uint8_t valor)
{
    uint8_t resultado = 1;
    for (int i = 0; i < 254; ++i) {
        resultado = multiplicar(resultado, valor);
    }
    return resultado;
}

uint8_t rotarIzquierda(const uint8_t valor, const int bits)
{
    return static_cast<uint8_t>((valor << bits) | (valor >> (8 - bits)));
}

const std::array<uint8_t, 256>& cajaS()
{
    static const std::array<uint8_t, 256> tabla = [] {
        std::array<uint8_t, 256> valores{};
        for (int i = 0; i < 256; ++i) {
            const uint8_t inv = inverso(static_cast<uint8_t>(i));
            valores.at(i) = static_cast<uint8_t>(inv ^ rotarIzquierda(inv, 1)
                                                 ^ rotarIzquierda(inv, 2)
                                                 ^ rotarIzquierda(inv, 3)
                                                 ^ rotarIzquierda(inv, 4) ^ 0x63);
        }
        return valores;
    }();
    return tabla;
}

std::array<uint8_t, kBloque * (kRondas + 1)> expandirClave(const QByteArray& clave)
{
    std::array<uint8_t, kBloque * (kRondas + 1)> claves{};
    std::memcpy(claves.data(), clave.constData(), 32);

    const auto& sb = cajaS();
    int generados = 32;
    uint8_t rcon = 0x01;
    while (generados < static_cast<int>(claves.size())) {
        uint8_t temp[4] = {claves.at(generados - 4), claves.at(generados - 3),
                           claves.at(generados - 2), claves.at(generados - 1)};
        if (generados % 32 == 0) {
            const uint8_t primero = temp[0];
            temp[0] = static_cast<uint8_t>(sb.at(temp[1]) ^ rcon);
            temp[1] = sb.at(temp[2]);
            temp[2] = sb.at(temp[3]);
            temp[3] = sb.at(primero);
            rcon = xtime(rcon);
        } else if (generados % 32 == 16) {
            for (int i = 0; i < 4; ++i) {
                temp[i] = sb.at(temp[i]);
            }
        }
        for (int i = 0; i < 4; ++i) {
            claves.at(generados) = static_cast<uint8_t>(
                claves.at(generados - 32) ^ temp[i]);
            ++generados;
        }
    }
    return claves;
}

// Cifra un bloque. GCM solo necesita la dirección de cifrado: el descifrado es
// XOR con el mismo keystream.
void cifrarBloque(const std::array<uint8_t, kBloque * (kRondas + 1)>& claves,
                  const uint8_t entrada[kBloque], uint8_t salida[kBloque])
{
    const auto& sb = cajaS();
    uint8_t estado[kBloque];
    for (int i = 0; i < kBloque; ++i) {
        estado[i] = static_cast<uint8_t>(entrada[i] ^ claves.at(i));
    }

    for (int ronda = 1; ronda <= kRondas; ++ronda) {
        uint8_t mezcla[kBloque];
        // SubBytes + ShiftRows en una pasada.
        for (int columna = 0; columna < 4; ++columna) {
            for (int fila = 0; fila < 4; ++fila) {
                mezcla[fila + 4 * columna] =
                    sb.at(estado[fila + 4 * ((columna + fila) & 3)]);
            }
        }
        if (ronda != kRondas) {
            for (int columna = 0; columna < 4; ++columna) {
                uint8_t* col = mezcla + 4 * columna;
                const uint8_t a0 = col[0];
                const uint8_t a1 = col[1];
                const uint8_t a2 = col[2];
                const uint8_t a3 = col[3];
                col[0] = static_cast<uint8_t>(xtime(a0) ^ xtime(a1) ^ a1 ^ a2 ^ a3);
                col[1] = static_cast<uint8_t>(a0 ^ xtime(a1) ^ xtime(a2) ^ a2 ^ a3);
                col[2] = static_cast<uint8_t>(a0 ^ a1 ^ xtime(a2) ^ xtime(a3) ^ a3);
                col[3] = static_cast<uint8_t>(xtime(a0) ^ a0 ^ a1 ^ a2 ^ xtime(a3));
            }
        }
        for (int i = 0; i < kBloque; ++i) {
            estado[i] = static_cast<uint8_t>(mezcla[i] ^ claves.at(16 * ronda + i));
        }
    }
    std::memcpy(salida, estado, kBloque);
}

// Producto en GF(2^128) según SP 800-38D: bits de X de izquierda a derecha.
void gfMul(const uint8_t x[kBloque], const uint8_t h[kBloque], uint8_t z[kBloque])
{
    uint8_t v[kBloque];
    std::memcpy(v, h, kBloque);
    std::memset(z, 0, kBloque);
    for (int bit = 0; bit < 128; ++bit) {
        if ((x[bit >> 3] >> (7 - (bit & 7))) & 1) {
            for (int i = 0; i < kBloque; ++i) {
                z[i] ^= v[i];
            }
        }
        const bool ultimo = (v[kBloque - 1] & 1) != 0;
        for (int i = kBloque - 1; i > 0; --i) {
            v[i] = static_cast<uint8_t>((v[i] >> 1) | (v[i - 1] << 7));
        }
        v[0] >>= 1;
        if (ultimo) {
            v[0] ^= 0xE1;
        }
    }
}

void ghashBloques(const uint8_t h[kBloque], const uint8_t* datos, size_t largo,
                  uint8_t y[kBloque])
{
    size_t desplazamiento = 0;
    while (desplazamiento < largo) {
        uint8_t bloque[kBloque] = {};
        const size_t tomar = std::min<size_t>(kBloque, largo - desplazamiento);
        std::memcpy(bloque, datos + desplazamiento, tomar);
        for (int i = 0; i < kBloque; ++i) {
            y[i] ^= bloque[i];
        }
        uint8_t producto[kBloque];
        gfMul(y, h, producto);
        std::memcpy(y, producto, kBloque);
        desplazamiento += tomar;
    }
}

void escribir64(uint8_t* destino, const quint64 valor)
{
    for (int i = 0; i < 8; ++i) {
        destino[i] = static_cast<uint8_t>((valor >> (56 - 8 * i)) & 0xFF);
    }
}

void ghash(const uint8_t h[kBloque], const QByteArray& aad, const QByteArray& cifrado,
           uint8_t salida[kBloque])
{
    uint8_t y[kBloque] = {};
    ghashBloques(h, reinterpret_cast<const uint8_t*>(aad.constData()),
                 static_cast<size_t>(aad.size()), y);
    ghashBloques(h, reinterpret_cast<const uint8_t*>(cifrado.constData()),
                 static_cast<size_t>(cifrado.size()), y);

    uint8_t longitudes[kBloque] = {};
    escribir64(longitudes, static_cast<quint64>(aad.size()) * 8);
    escribir64(longitudes + 8, static_cast<quint64>(cifrado.size()) * 8);
    for (int i = 0; i < kBloque; ++i) {
        y[i] ^= longitudes[i];
    }
    gfMul(y, h, salida);
}

void inc32(uint8_t contador[kBloque])
{
    for (int i = kBloque - 1; i >= 12; --i) {
        contador[i] = static_cast<uint8_t>(contador[i] + 1);
        if (contador[i] != 0) {
            break;
        }
    }
}

QByteArray transformar(const std::array<uint8_t, kBloque * (kRondas + 1)>& claves,
                       const QByteArray& nonce, const QByteArray& datos)
{
    uint8_t j0[kBloque] = {};
    std::memcpy(j0, nonce.constData(), CryptoBox::kBytesNonce);
    j0[15] = 1;

    QByteArray salida(datos.size(), Qt::Uninitialized);
    uint8_t contador[kBloque];
    std::memcpy(contador, j0, kBloque);

    const auto* entrada = reinterpret_cast<const uint8_t*>(datos.constData());
    auto* destino = reinterpret_cast<uint8_t*>(salida.data());
    int procesado = 0;
    while (procesado < datos.size()) {
        inc32(contador);
        uint8_t keystream[kBloque];
        cifrarBloque(claves, contador, keystream);
        const int tomar = std::min<int>(kBloque, datos.size() - procesado);
        for (int i = 0; i < tomar; ++i) {
            destino[procesado + i] =
                static_cast<uint8_t>(entrada[procesado + i] ^ keystream[i]);
        }
        procesado += tomar;
    }
    return salida;
}

QByteArray etiqueta(const std::array<uint8_t, kBloque * (kRondas + 1)>& claves,
                    const QByteArray& nonce, const QByteArray& aad,
                    const QByteArray& cifrado)
{
    uint8_t h[kBloque] = {};
    const uint8_t cero[kBloque] = {};
    cifrarBloque(claves, cero, h);

    uint8_t s[kBloque];
    ghash(h, aad, cifrado, s);

    uint8_t j0[kBloque] = {};
    std::memcpy(j0, nonce.constData(), CryptoBox::kBytesNonce);
    j0[15] = 1;
    uint8_t mascara[kBloque];
    cifrarBloque(claves, j0, mascara);

    QByteArray salida(CryptoBox::kBytesTag, Qt::Uninitialized);
    for (int i = 0; i < CryptoBox::kBytesTag; ++i) {
        salida[i] = static_cast<char>(s[i] ^ mascara[i]);
    }
    return salida;
}

} // namespace

QByteArray CryptoBox::derivarClave(const QString& password, const QByteArray& salt,
                                   const int iteraciones)
{
    if (password.isEmpty() || salt.isEmpty() || iteraciones < 1) {
        return {};
    }
    return QPasswordDigestor::deriveKeyPbkdf2(
        QCryptographicHash::Sha256, password.toUtf8(), salt, iteraciones, kBytesClave);
}

QByteArray CryptoBox::cifrar(const QByteArray& clave, const QByteArray& datos,
                             const QByteArray& aad)
{
    return cifrarConNonce(clave, aleatorio(kBytesNonce), datos, aad);
}

QByteArray CryptoBox::cifrarConNonce(const QByteArray& clave, const QByteArray& nonce,
                                     const QByteArray& datos, const QByteArray& aad)
{
    if (clave.size() != kBytesClave || nonce.size() != kBytesNonce) {
        return {};
    }
    const auto claves = expandirClave(clave);
    const QByteArray cifrado = transformar(claves, nonce, datos);
    const QByteArray tag = etiqueta(claves, nonce, aad, cifrado);

    QByteArray sobre;
    sobre.reserve(1 + kBytesNonce + kBytesTag + cifrado.size());
    sobre.append(static_cast<char>(kVersion));
    sobre.append(nonce);
    sobre.append(tag);
    sobre.append(cifrado);
    return sobre;
}

bool CryptoBox::descifrar(const QByteArray& clave, const QByteArray& sobre,
                          QByteArray* datos, const QByteArray& aad)
{
    if (datos == nullptr || clave.size() != kBytesClave
        || sobre.size() < 1 + kBytesNonce + kBytesTag) {
        return false;
    }
    if (static_cast<quint8>(sobre.at(0)) != kVersion) {
        return false;
    }
    const QByteArray nonce = sobre.mid(1, kBytesNonce);
    const QByteArray tag = sobre.mid(1 + kBytesNonce, kBytesTag);
    const QByteArray cifrado = sobre.mid(1 + kBytesNonce + kBytesTag);

    const auto claves = expandirClave(clave);
    const QByteArray esperada = etiqueta(claves, nonce, aad, cifrado);
    if (!igualesEnTiempoConstante(tag, esperada)) {
        return false;
    }
    *datos = transformar(claves, nonce, cifrado);
    return true;
}

QByteArray CryptoBox::aleatorio(const int bytes)
{
    QByteArray salida;
    salida.reserve(bytes);
    while (salida.size() < bytes) {
        const quint32 valor = QRandomGenerator::system()->generate();
        for (int i = 0; i < 4 && salida.size() < bytes; ++i) {
            salida.append(static_cast<char>((valor >> (8 * i)) & 0xFF));
        }
    }
    return salida;
}

bool CryptoBox::igualesEnTiempoConstante(const QByteArray& a, const QByteArray& b)
{
    if (a.size() != b.size()) {
        return false;
    }
    unsigned char diferencia = 0;
    for (int i = 0; i < a.size(); ++i) {
        diferencia |= static_cast<unsigned char>(a.at(i))
            ^ static_cast<unsigned char>(b.at(i));
    }
    return diferencia == 0;
}

} // namespace fuzzy::core
