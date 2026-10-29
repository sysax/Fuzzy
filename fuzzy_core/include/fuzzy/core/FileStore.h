#pragma once

#include <QByteArray>
#include <QString>

namespace fuzzy::core {

// Almacén de binarios en el disco del nodo. Solo acepta rutas relativas
// formadas por segmentos seguros (`[A-Za-z0-9][A-Za-z0-9._-]*`): ni rutas
// absolutas, ni `..`, ni separadores de Windows. Escribe con `QSaveFile`
// (temporal + rename) y deja los archivos con permisos de dueño.
//
// El binario de la galería clínica vive aquí, nunca en SQLite ni en Turso.
class FileStore
{
public:
    explicit FileStore(QString raiz);

    QString raiz() const { return raiz_; }

    // Ruta absoluta resultante, o vacía si la relativa no es segura.
    QString rutaAbsoluta(const QString& rutaRelativa) const;

    static bool rutaRelativaValida(const QString& rutaRelativa);

    bool guardar(const QString& rutaRelativa, const QByteArray& datos,
                 QString* error = nullptr);

    bool leer(const QString& rutaRelativa, QByteArray& datos,
              QString* error = nullptr) const;

    bool existe(const QString& rutaRelativa) const;

    // Borra el archivo. Devuelve true si al terminar ya no existe.
    bool eliminar(const QString& rutaRelativa, QString* error = nullptr);

private:
    QString raiz_;
};

} // namespace fuzzy::core
