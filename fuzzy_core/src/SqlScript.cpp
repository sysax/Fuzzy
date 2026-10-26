#include "fuzzy/core/SqlScript.h"

namespace fuzzy::core {

QStringList sentenciasSql(const QString& script)
{
    QStringList sentencias;
    QString actual;
    bool enCadena = false;
    bool enLinea = false;
    bool enBloque = false;

    const int total = script.size();
    for (int i = 0; i < total; ++i) {
        const QChar c = script.at(i);
        const QChar siguiente = (i + 1 < total) ? script.at(i + 1) : QChar();

        if (enLinea) {
            if (c == QLatin1Char('\n')) {
                enLinea = false;
                actual.append(c);
            }
            continue;
        }
        if (enBloque) {
            if (c == QLatin1Char('*') && siguiente == QLatin1Char('/')) {
                enBloque = false;
                ++i;
            }
            continue;
        }
        if (enCadena) {
            actual.append(c);
            if (c == QLatin1Char('\'')) {
                if (siguiente == QLatin1Char('\'')) {
                    // Comilla escapada dentro de la cadena.
                    actual.append(siguiente);
                    ++i;
                } else {
                    enCadena = false;
                }
            }
            continue;
        }

        if (c == QLatin1Char('-') && siguiente == QLatin1Char('-')) {
            enLinea = true;
            ++i;
            continue;
        }
        if (c == QLatin1Char('/') && siguiente == QLatin1Char('*')) {
            enBloque = true;
            ++i;
            continue;
        }
        if (c == QLatin1Char('\'')) {
            enCadena = true;
            actual.append(c);
            continue;
        }
        if (c == QLatin1Char(';')) {
            const QString sentencia = actual.trimmed();
            if (!sentencia.isEmpty()) {
                sentencias.append(sentencia);
            }
            actual.clear();
            continue;
        }
        actual.append(c);
    }

    const QString ultima = actual.trimmed();
    if (!ultima.isEmpty()) {
        sentencias.append(ultima);
    }
    return sentencias;
}

} // namespace fuzzy::core
