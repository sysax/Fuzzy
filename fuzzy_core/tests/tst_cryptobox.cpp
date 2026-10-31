#include "fuzzy/core/CryptoBox.h"

#include <QTest>

using fuzzy::core::CryptoBox;

namespace {

QByteArray deHex(const char* texto)
{
    return QByteArray::fromHex(QByteArray(texto));
}

} // namespace

// Vectores generados con OpenSSL (`AES-256-GCM`) para claves, nonces, AAD y
// longitudes distintas. Si el cifrado se rompe, la caché del teléfono deja de
// ser ilegible y el fallo se ve aquí, no en un teléfono.
class CryptoBoxTest : public QObject
{
    Q_OBJECT

private slots:
    void vectorVacio();
    void vectorTexto();
    void vectorConAad();
    void vectorLargo();
    void idaYVueltaConNonceAleatorio();
    void rechazaEtiquetaAlterada();
    void rechazaCiphertextAlterado();
    void rechazaAadDistinto();
    void rechazaSobreTruncado();
    void rechazaClaveIncorrecta();
    void derivaClaveConSaltYIteraciones();
    void nonceAleatorioNoSeRepite();
};

void CryptoBoxTest::vectorVacio()
{
    const QByteArray clave(32, '\0');
    const QByteArray nonce(12, '\0');
    const QByteArray sobre = CryptoBox::cifrarConNonce(clave, nonce, {});
    QCOMPARE(sobre.size(), 1 + 12 + 16);
    QCOMPARE(sobre.mid(1 + 12).toHex(),
             deHex("530f8afbc74536b9a963b4f1c4cb738b").toHex());
}

void CryptoBoxTest::vectorTexto()
{
    QByteArray clave;
    for (int i = 0; i < 32; ++i) {
        clave.append(static_cast<char>(i));
    }
    QByteArray nonce;
    for (int i = 0; i < 12; ++i) {
        nonce.append(static_cast<char>(i));
    }
    const QByteArray sobre = CryptoBox::cifrarConNonce(clave, nonce, "FuzzyCloud");
    QCOMPARE(sobre.mid(1 + 12, 16).toHex(),
             deHex("ed51f50b3d42581a72c015dc72aab2cc").toHex());
    QCOMPARE(sobre.mid(1 + 12 + 16).toHex(),
             deHex("0177ac61bca6ae74f825").toHex());
}

void CryptoBoxTest::vectorConAad()
{
    const QByteArray clave(32, 0x11);
    const QByteArray nonce(12, 0x22);
    const QByteArray aad = "portal/0001/mascotas/luna";
    const QByteArray sobre = CryptoBox::cifrarConNonce(clave, nonce, "alergias: ninguna", aad);
    QCOMPARE(sobre.mid(1 + 12, 16).toHex(),
             deHex("f3fdf0b94108473cbecc27dcb9df6f1a").toHex());
    QCOMPARE(sobre.mid(1 + 12 + 16).toHex(),
             deHex("769b623ba7a6fe2cdf1fb05526c19cbb7e").toHex());
}

void CryptoBoxTest::vectorLargo()
{
    QByteArray clave;
    for (int i = 0; i < 32; ++i) {
        clave.append(static_cast<char>(i));
    }
    const QByteArray nonce(12, static_cast<char>(0xAB));
    QByteArray datos;
    for (int i = 1; i <= 100; ++i) {
        datos.append(static_cast<char>(i));
    }
    const QByteArray sobre = CryptoBox::cifrarConNonce(clave, nonce, datos, "fila");
    QCOMPARE(sobre.mid(1 + 12, 16).toHex(),
             deHex("95256340ee6270fddbbe2cf21502ec2e").toHex());
    QCOMPARE(sobre.mid(1 + 12 + 16).toHex(),
             deHex("cddab74da11e16f32b554d272e9c3bf4a443933f8c221e2c88aa55f0ba296688"
                   "5458fbebd379fd7c486a3c4c64e2793735a6351a7c135e7c9130281ad87b3db2"
                   "c93569a7eb899f1973f9914338b9dc3c10c3bc0dcab205179f36a8283b7aac3d"
                   "66fbe171").toHex());
}

void CryptoBoxTest::idaYVueltaConNonceAleatorio()
{
    const QByteArray clave(32, 'k');
    const QByteArray claro = "notas clínicas: sin alergias conocidas";
    const QByteArray aad = "portal-1/visitas/abc";

    const QByteArray primero = CryptoBox::cifrar(clave, claro, aad);
    const QByteArray segundo = CryptoBox::cifrar(clave, claro, aad);
    QVERIFY(!primero.isEmpty());
    QVERIFY(primero != segundo); // nonce distinto en cada sobre
    QVERIFY(!primero.contains(claro));

    QByteArray recuperado;
    QVERIFY(CryptoBox::descifrar(clave, primero, &recuperado, aad));
    QCOMPARE(recuperado, claro);
}

void CryptoBoxTest::rechazaEtiquetaAlterada()
{
    const QByteArray clave(32, 'k');
    const QByteArray aad = "aad";
    QByteArray sobre = CryptoBox::cifrar(clave, "dato", aad);
    sobre[1 + 12] = static_cast<char>(sobre.at(1 + 12) ^ 0x01);

    QByteArray recuperado;
    QVERIFY(!CryptoBox::descifrar(clave, sobre, &recuperado, aad));
}

void CryptoBoxTest::rechazaCiphertextAlterado()
{
    const QByteArray clave(32, 'k');
    QByteArray sobre = CryptoBox::cifrar(clave, "dato clínico");
    sobre[sobre.size() - 1] = static_cast<char>(sobre.at(sobre.size() - 1) ^ 0x40);

    QByteArray recuperado;
    QVERIFY(!CryptoBox::descifrar(clave, sobre, &recuperado));
}

void CryptoBoxTest::rechazaAadDistinto()
{
    const QByteArray clave(32, 'k');
    const QByteArray sobre = CryptoBox::cifrar(clave, "dato", "mascotas/luna");

    QByteArray recuperado;
    QVERIFY(!CryptoBox::descifrar(clave, sobre, &recuperado, "mascotas/rocky"));
    QVERIFY(CryptoBox::descifrar(clave, sobre, &recuperado, "mascotas/luna"));
}

void CryptoBoxTest::rechazaSobreTruncado()
{
    const QByteArray clave(32, 'k');
    const QByteArray sobre = CryptoBox::cifrar(clave, "dato");
    QByteArray recuperado;
    QVERIFY(!CryptoBox::descifrar(clave, sobre.left(10), &recuperado));
    QVERIFY(!CryptoBox::descifrar(clave, {}, &recuperado));
}

void CryptoBoxTest::rechazaClaveIncorrecta()
{
    const QByteArray sobre = CryptoBox::cifrar(QByteArray(32, 'a'), "dato");
    QByteArray recuperado;
    QVERIFY(!CryptoBox::descifrar(QByteArray(32, 'b'), sobre, &recuperado));
    QVERIFY(!CryptoBox::descifrar(QByteArray(16, 'a'), sobre, &recuperado));
}

void CryptoBoxTest::derivaClaveConSaltYIteraciones()
{
    const QByteArray salt = "salt-de-prueba";
    const QByteArray clave = CryptoBox::derivarClave("Aurora2026!", salt, 1000);
    QCOMPARE(clave.size(), 32);
    QCOMPARE(CryptoBox::derivarClave("Aurora2026!", salt, 1000), clave);
    QVERIFY(CryptoBox::derivarClave("otra", salt, 1000) != clave);
    QVERIFY(CryptoBox::derivarClave("Aurora2026!", "otro-salt", 1000) != clave);
    QVERIFY(CryptoBox::derivarClave("Aurora2026!", salt, 2000) != clave);
    QVERIFY(CryptoBox::derivarClave({}, salt, 1000).isEmpty());
}

void CryptoBoxTest::nonceAleatorioNoSeRepite()
{
    QCOMPARE(CryptoBox::aleatorio(12).size(), 12);
    QByteArray a = CryptoBox::aleatorio(12);
    QByteArray b = CryptoBox::aleatorio(12);
    QVERIFY(a != b);
}

QTEST_GUILESS_MAIN(CryptoBoxTest)

#include "tst_cryptobox.moc"
