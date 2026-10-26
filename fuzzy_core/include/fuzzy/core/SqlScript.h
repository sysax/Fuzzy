#pragma once

#include <QString>
#include <QStringList>

namespace fuzzy::core {

// Divide un script SQL en sentencias ejecutables. Respeta cadenas entre
// comillas simples, comentarios de línea (--) y de bloque (/* ... */), y no
// parte por los puntos y coma que aparecen dentro de ellos.
QStringList sentenciasSql(const QString& script);

} // namespace fuzzy::core
