#include "fuzzy/core/PasswordHasher.h"

#include <QCryptographicHash>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QStringList>

namespace fuzzy::core {

namespace {

const QString kPrefijo = QStringLiteral("pbkdf2_sha256");

QByteArray derivar(const QString& password, const QByteArray& salt, int iteraciones)
{
    return QPasswordDigestor::deriveKeyPbkdf2(
        QCryptographicHash::Sha256, password.toUtf8(), salt, iteraciones,
        PasswordHasher::kBytesHash);
}

bool igualdadConstante(const QByteArray& a, const QByteArray& b)
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

} // namespace

QString PasswordHasher::generar(const QString& password, int iteraciones)
{
    if (password.isEmpty() || iteraciones < 1) {
        return {};
    }
    QByteArray salt;
    salt.reserve(kBytesSalt);
    while (salt.size() < kBytesSalt) {
        const quint32 valor = QRandomGenerator::system()->generate();
        for (int byte = 0; byte < 4 && salt.size() < kBytesSalt; ++byte) {
            salt.append(static_cast<char>((valor >> (8 * byte)) & 0xFF));
        }
    }
    const QByteArray hash = derivar(password, salt, iteraciones);
    return QStringLiteral("%1$%2$%3$%4")
        .arg(kPrefijo)
        .arg(iteraciones)
        .arg(QString::fromLatin1(salt.toHex()), QString::fromLatin1(hash.toHex()));
}

bool PasswordHasher::verificar(const QString& password, const QString& almacenado)
{
    const QStringList partes = almacenado.split(QLatin1Char('$'));
    if (partes.size() != 4 || partes.at(0) != kPrefijo) {
        return false;
    }
    bool ok = false;
    const int iteraciones = partes.at(1).toInt(&ok);
    if (!ok || iteraciones < 1) {
        return false;
    }
    const QByteArray salt = QByteArray::fromHex(partes.at(2).toLatin1());
    const QByteArray esperado = QByteArray::fromHex(partes.at(3).toLatin1());
    if (salt.size() != kBytesSalt || esperado.size() != kBytesHash) {
        return false;
    }
    return igualdadConstante(derivar(password, salt, iteraciones), esperado);
}

QString PasswordHasher::passwordTemporal(int longitud)
{
    static const QString alfabeto =
        QStringLiteral("ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789");
    if (longitud < 8) {
        longitud = 8;
    }
    QString password;
    password.reserve(longitud);
    for (int i = 0; i < longitud; ++i) {
        password.append(alfabeto.at(
            QRandomGenerator::system()->bounded(alfabeto.size())));
    }
    return password;
}

bool PasswordHasher::esFormatoValido(const QString& almacenado)
{
    const QStringList partes = almacenado.split(QLatin1Char('$'));
    if (partes.size() != 4 || partes.at(0) != kPrefijo) {
        return false;
    }
    bool ok = false;
    partes.at(1).toInt(&ok);
    return ok && QByteArray::fromHex(partes.at(2).toLatin1()).size() == kBytesSalt
        && QByteArray::fromHex(partes.at(3).toLatin1()).size() == kBytesHash;
}

} // namespace fuzzy::core
