#include "TestSupport.h"

#include "fuzzy/core/FileStore.h"
#include "fuzzy/core/GaleriaClinicaService.h"
#include "fuzzy/core/MascotaFotoService.h"
#include "fuzzy/core/PortalBaseStore.h"
#include "fuzzy/core/PortalProjector.h"
#include "fuzzy/core/SyncEngine.h"
#include "fuzzy/core/SyncEntities.h"

#include <QBuffer>
#include <QColor>
#include <QFileInfo>
#include <QImage>
#include <QMetaType>
#include <QTemporaryDir>
#include <QVariant>
#include <QtTest>

using fuzzy::core::FileStore;
using fuzzy::core::GaleriaClinicaService;
using fuzzy::core::LocalPortalBaseStore;
using fuzzy::core::MascotaFotoService;
using fuzzy::core::PortalProjector;
using fuzzy::core::SqliteExecutor;
using fuzzy::core::SyncEngine;
using namespace fuzzy::test;

namespace {

const QString kClinica = QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa1");
const QString kCliente = QStringLiteral("10000000-0000-4000-8000-000000000001");
const QString kCuenta = QStringLiteral("20000000-0000-4000-8000-000000000001");
const QString kMascota = QStringLiteral("30000000-0000-4000-8000-000000000001");
const QString kConsulta = QStringLiteral("40000000-0000-4000-8000-000000000001");
const QString kPortal = QStringLiteral("bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbb1");

void sembrarClinica(IQueryExecutor& clinica)
{
    QVERIFY(aplicarEsquema(clinica, QStringLiteral("clinic")));
    QVERIFY(clinica.execute(
                        QStringLiteral("INSERT INTO clinicas "
                                       "(id, uuid, nombre, zona_horaria, created_at, updated_at, "
                                       " activa, sync_state) "
                                       "VALUES (1, ?, 'Clínica Norte', 'America/Bogota', 1, 1, "
                                       "1, 'pendiente')"),
                        {QVariant(kClinica)})
                .ok);
    QVERIFY(clinica.execute(
                        QStringLiteral("INSERT INTO clientes "
                                       "(id, uuid, nombre_completo, telefono, correo, "
                                       " activo, created_at, updated_at, sync_state) "
                                       "VALUES (1, ?, 'Ana Uno', '3001111111', "
                                       "'ana@example.com', 1, 1, 1, 'pendiente')"),
                        {QVariant(kCliente)})
                .ok);
    QVERIFY(clinica.execute(
                        QStringLiteral("INSERT INTO cuentas "
                                       "(id, uuid, cliente_id, correo, password_hash, "
                                       " estado, activo, created_at, updated_at, sync_state) "
                                       "VALUES (1, ?, 1, 'ana@example.com', 'hash', "
                                       "'Activa', 1, 1, 1, 'pendiente')"),
                        {QVariant(kCuenta)})
                .ok);
    QVERIFY(clinica.execute(
                        QStringLiteral("INSERT INTO mascotas "
                                       "(id, uuid, cliente_id, nombre, especie, raza, sexo, "
                                       " peso_kg, activo, castrado, created_at, updated_at, "
                                       " sync_state) "
                                       "VALUES (1, ?, 1, 'Luna', 'Perro', 'Mestizo', 'Hembra', "
                                       "12, 1, 0, 1, 1, 'pendiente')"),
                        {QVariant(kMascota)})
                .ok);
    QVERIFY(clinica.execute(
                        QStringLiteral("INSERT INTO consultas_medicas "
                                       "(id, uuid, mascota_id, fecha_consulta, motivo_visita, "
                                       " diagnostico, estado, created_at, updated_at, "
                                       " sync_state) "
                                       "VALUES (1, ?, 1, 1, 'Control', 'Sano', 'Cerrada', 1, 1, "
                                       "'pendiente')"),
                        {QVariant(kConsulta)})
                .ok);
}

void sembrarPortal(LocalPortalBaseStore& bases)
{
    QVERIFY(bases.crear(kPortal));
    auto portal = bases.abrir(kPortal);
    QVERIFY(portal != nullptr);
    QVERIFY(aplicarEsquema(*portal, QStringLiteral("portal")));
    QVERIFY(portal->execute(
                       QStringLiteral("INSERT INTO portal_meta "
                                      "(id, esquema_version, cuenta_uuid, clinica_uuid, "
                                      " generado_at, clinica_nombre, proyeccion_version) "
                                      "VALUES (1, 2, ?, ?, 1, 'Clínica Norte', 0)"),
                       {QVariant(kCuenta), QVariant(kClinica)})
                .ok);
    QVERIFY(portal->execute(
                       QStringLiteral("INSERT INTO mascotas "
                                      "(uuid, nombre, especie, created_at, updated_at) "
                                      "VALUES (?, 'Luna', 'Perro', 1, 1)"),
                       {QVariant(kMascota)})
                .ok);
}

QByteArray imagenPng(int ancho, int alto, const QColor& color)
{
    QImage imagen(ancho, alto, QImage::Format_RGB32);
    imagen.fill(color);
    QByteArray datos;
    QBuffer buffer(&datos);
    buffer.open(QIODevice::WriteOnly);
    imagen.save(&buffer, "PNG");
    return datos;
}

QStringList tablasDe(IQueryExecutor& executor)
{
    QStringList tablas;
    const auto filas = executor.query(
        QStringLiteral("SELECT name FROM sqlite_master WHERE type = 'table' "
                       "AND name NOT LIKE 'sqlite_%'"));
    for (const auto& fila : filas) {
        tablas.append(fila.values.front().toString());
    }
    return tablas;
}

QStringList columnasDe(IQueryExecutor& executor, const QString& tabla)
{
    QStringList columnas;
    const auto filas = executor.query(
        QStringLiteral("SELECT name FROM pragma_table_info(?)"), {QVariant(tabla)});
    for (const auto& fila : filas) {
        columnas.append(fila.values.front().toString());
    }
    return columnas;
}

bool tieneColumnaBlob(IQueryExecutor& executor, const QString& tabla)
{
    const auto filas = executor.query(
        QStringLiteral("SELECT name, type FROM pragma_table_info(?)"), {QVariant(tabla)});
    for (const auto& fila : filas) {
        if (fila.values.size() >= 2
            && fila.values.at(1).toString().toUpper().contains(QStringLiteral("BLOB"))) {
            return true;
        }
    }
    return false;
}

// Busca el binario, su base64 y su hex en todas las tablas y columnas. Es el
// corazón del gate: si un byte clínico llega a Turso, esto lo encuentra.
QString binarioEnBase(IQueryExecutor& executor, const QByteArray& buscado)
{
    const QByteArray base64 = buscado.toBase64();
    const QByteArray base64Url =
        buscado.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    const QByteArray hex = buscado.toHex();
    const QString base64Texto = QString::fromLatin1(base64);
    const QString base64UrlTexto = QString::fromLatin1(base64Url);
    const QString hexTexto = QString::fromLatin1(hex);

    for (const QString& tabla : tablasDe(executor)) {
        const QStringList columnas = columnasDe(executor, tabla);
        const auto filas = executor.query(QStringLiteral("SELECT * FROM %1").arg(tabla));
        for (const auto& fila : filas) {
            for (int i = 0; i < static_cast<int>(fila.values.size()); ++i) {
                const QVariant& valor = fila.values[static_cast<size_t>(i)];
                const QString columna =
                    i < columnas.size() ? columnas.at(i) : QString::number(i);

                if (valor.typeId() == QMetaType::QByteArray) {
                    if (valor.toByteArray().contains(buscado)) {
                        return QStringLiteral("%1.%2").arg(tabla, columna);
                    }
                    continue;
                }
                if (valor.typeId() != QMetaType::QString) {
                    continue;
                }
                const QString texto = valor.toString();
                if (texto.contains(base64Texto) || texto.contains(base64UrlTexto)
                    || texto.contains(hexTexto)) {
                    return QStringLiteral("%1.%2").arg(tabla, columna);
                }
            }
        }
    }
    return {};
}

QString textoEnBase(IQueryExecutor& executor, const QString& buscado)
{
    if (buscado.isEmpty()) {
        return {};
    }
    for (const QString& tabla : tablasDe(executor)) {
        const QStringList columnas = columnasDe(executor, tabla);
        const auto filas = executor.query(QStringLiteral("SELECT * FROM %1").arg(tabla));
        for (const auto& fila : filas) {
            for (int i = 0; i < static_cast<int>(fila.values.size()); ++i) {
                const QVariant& valor = fila.values[static_cast<size_t>(i)];
                if (valor.typeId() != QMetaType::QString) {
                    continue;
                }
                if (valor.toString().contains(buscado)) {
                    const QString columna =
                        i < columnas.size() ? columnas.at(i) : QString::number(i);
                    return QStringLiteral("%1.%2").arg(tabla, columna);
                }
            }
        }
    }
    return {};
}

} // namespace

class TstAdjuntosClinicos : public QObject
{
    Q_OBJECT

private slots:
    void metadatosSinBinarioEnTurso();
    void eliminarDejaSinArchivoNiFila();
    void rechazaFormatosYLimites();
};

void TstAdjuntosClinicos::metadatosSinBinarioEnTurso()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor clinica(nuevaConexion(QStringLiteral("adjuntos_clinica")),
                           dir.filePath(QStringLiteral("clinic.db")));
    sembrarClinica(clinica);

    LocalPortalBaseStore bases(dir.filePath(QStringLiteral("portales")));
    sembrarPortal(bases);

    FileStore almacen(dir.filePath(QStringLiteral("imagenes")));
    GaleriaClinicaService galeria(clinica, almacen);

    const QByteArray radiografia = imagenPng(96, 64, QColor(210, 190, 40));
    const auto adjunto = galeria.guardar(kMascota, QStringLiteral("Radiografía de tórax"),
                                         QStringLiteral("Radiografia"), radiografia, kConsulta);
    QVERIFY2(adjunto.ok, qPrintable(adjunto.error));
    QCOMPARE(adjunto.mime, QStringLiteral("image/png"));
    QVERIFY(almacen.existe(adjunto.storagePath));

    QByteArray leidos;
    QVERIFY2(galeria.leer(adjunto.uuid, leidos, nullptr), "no se pudo leer el adjunto");
    QCOMPARE(leidos, radiografia);

    const auto fila = clinica.query(
        QStringLiteral("SELECT storage_path, mime, sha256, tamano, sync_state "
                       "FROM mascota_galeria WHERE uuid = ?"),
        {QVariant(adjunto.uuid)});
    QCOMPARE(fila.size(), size_t(1));
    QCOMPARE(fila.front().values.at(0).toString(), adjunto.storagePath);
    QCOMPARE(fila.front().values.at(1).toString(), QStringLiteral("image/png"));
    QCOMPARE(fila.front().values.at(2).toString(), adjunto.sha256);
    QCOMPARE(fila.front().values.at(3).toLongLong(), static_cast<qint64>(radiografia.size()));
    QCOMPARE(fila.front().values.at(4).toString(), QStringLiteral("sincronizado"));

    // La galería clínica no es una entidad replicable; la foto del portal
    // tampoco viaja por el change log de la clínica.
    QVERIFY(fuzzy::core::entidadSync(QStringLiteral("mascota_galeria")) == nullptr);
    QVERIFY(fuzzy::core::entidadSync(QStringLiteral("mascota_fotos")) == nullptr);
    QVERIFY(fuzzy::core::entidadSync(QStringLiteral("adjuntos")) == nullptr);

    // El binario no está en la base local: solo su ruta y su hash.
    QVERIFY2(binarioEnBase(clinica, radiografia).isEmpty(),
             qPrintable(QStringLiteral("el binario clínico está en la base del nodo: %1")
                            .arg(binarioEnBase(clinica, radiografia))));

    // La foto del dueño sí vive en el portal; se usa para comprobar que el
    // escaneo no confunde un binario permitido con el material clínico.
    auto portal = bases.abrir(kPortal);
    QVERIFY(portal != nullptr);
    MascotaFotoService fotos(*portal);
    const auto foto = fotos.reemplazar(kMascota, imagenPng(240, 180, QColor(20, 80, 200)));
    QVERIFY2(foto.ok, qPrintable(foto.error));
    portal.reset();

    // Sync real: la clínica empuja sus entidades y el change log remoto queda
    // sin ninguna fila de imagen.
    SqliteExecutor remoto(nuevaConexion(QStringLiteral("adjuntos_remoto")),
                          dir.filePath(QStringLiteral("remote.db")));
    QVERIFY(aplicarEsquema(remoto, QStringLiteral("clinic")));
    SyncEngine motor(clinica, remoto, QStringLiteral("nodo-a"));
    const auto sync = motor.sincronizar();
    QVERIFY2(sync.ok, qPrintable(sync.error));
    QCOMPARE(sync.empujados, 3);

    const auto eventosImagen = remoto.query(
        QStringLiteral("SELECT count(*) FROM sync_change_log "
                       "WHERE entidad IN ('mascota_galeria', 'mascota_fotos', 'adjuntos')"));
    QCOMPARE(eventosImagen.front().values.front().toLongLong(), 0LL);

    const QString binarioRemoto = binarioEnBase(remoto, radiografia);
    QVERIFY2(binarioRemoto.isEmpty(),
             qPrintable(QStringLiteral("Turso contiene el binario clínico en %1")
                            .arg(binarioRemoto)));
    QVERIFY2(textoEnBase(remoto, adjunto.storagePath).isEmpty(),
             "la ruta del disco del nodo viajó a Turso");
    QVERIFY2(textoEnBase(remoto, QFileInfo(adjunto.storagePath).fileName()).isEmpty(),
             "el nombre del archivo del nodo viajó a Turso");

    // Proyección: al portal llega solo la fila de metadatos.
    PortalProjector proyector(clinica, bases);
    const auto proyeccion = proyector.proyectar(kPortal);
    QVERIFY2(proyeccion.ok, qPrintable(proyeccion.error));

    portal = bases.abrir(kPortal);
    QVERIFY(portal != nullptr);
    QCOMPARE(contar(*portal, QStringLiteral("adjuntos")), 1LL);

    const auto metadata = portal->query(
        QStringLiteral("SELECT uuid, mascota_uuid, visita_uuid, nombre, mime, sha256, "
                       "tamano, deleted_at FROM adjuntos"));
    QCOMPARE(metadata.size(), size_t(1));
    const auto& v = metadata.front().values;
    QCOMPARE(v.at(0).toString(), adjunto.uuid);
    QCOMPARE(v.at(1).toString(), kMascota);
    QCOMPARE(v.at(2).toString(), kConsulta);
    QCOMPARE(v.at(3).toString(), QStringLiteral("Radiografía de tórax"));
    QCOMPARE(v.at(4).toString(), QStringLiteral("image/png"));
    QCOMPARE(v.at(5).toString(), adjunto.sha256);
    QCOMPARE(v.at(6).toLongLong(), static_cast<qint64>(radiografia.size()));
    QVERIFY(v.at(7).isNull());

    QVERIFY(!tieneColumnaBlob(*portal, QStringLiteral("adjuntos")));
    QVERIFY(!existeTabla(*portal, QStringLiteral("mascota_galeria")));

    const QString binarioPortal = binarioEnBase(*portal, radiografia);
    QVERIFY2(binarioPortal.isEmpty(),
             qPrintable(QStringLiteral("el portal contiene el binario clínico en %1")
                            .arg(binarioPortal)));
    QVERIFY2(textoEnBase(*portal, adjunto.storagePath).isEmpty(),
             "la ruta del disco del nodo viajó al portal");

    // La foto permitida sigue ahí y es distinta del material clínico.
    const auto fotoPortal = portal->query(
        QStringLiteral("SELECT sha256, length(foto) FROM mascota_fotos WHERE mascota_uuid = ?"),
        {QVariant(kMascota)});
    QCOMPARE(fotoPortal.size(), size_t(1));
    QCOMPARE(fotoPortal.front().values.at(0).toString(), foto.sha256);
    QVERIFY(fotoPortal.front().values.at(1).toLongLong() > 0);
}

void TstAdjuntosClinicos::eliminarDejaSinArchivoNiFila()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor clinica(nuevaConexion(QStringLiteral("adjuntos_borrado")),
                           dir.filePath(QStringLiteral("clinic.db")));
    sembrarClinica(clinica);

    FileStore almacen(dir.filePath(QStringLiteral("imagenes")));
    GaleriaClinicaService galeria(clinica, almacen);
    const auto adjunto = galeria.guardar(kMascota, QStringLiteral("Ecografía"),
                                         QStringLiteral("Ecografia"),
                                         imagenPng(80, 80, QColor(10, 10, 10)));
    QVERIFY2(adjunto.ok, qPrintable(adjunto.error));
    QVERIFY(almacen.existe(adjunto.storagePath));

    QString error;
    QVERIFY2(galeria.eliminar(adjunto.uuid, &error), qPrintable(error));
    QVERIFY(!almacen.existe(adjunto.storagePath));
    QCOMPARE(contar(clinica, QStringLiteral("mascota_galeria")), 0LL);
    QVERIFY(!galeria.eliminar(adjunto.uuid));
}

void TstAdjuntosClinicos::rechazaFormatosYLimites()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor clinica(nuevaConexion(QStringLiteral("adjuntos_limites")),
                           dir.filePath(QStringLiteral("clinic.db")));
    sembrarClinica(clinica);

    FileStore almacen(dir.filePath(QStringLiteral("imagenes")));
    GaleriaClinicaService galeria(clinica, almacen);

    const auto desconocido = galeria.guardar(
        kMascota, QStringLiteral("Archivo"), QStringLiteral("Otro"),
        QByteArrayLiteral("MZ ejecutable disfrazado"));
    QVERIFY(!desconocido.ok);

    const QByteArray grande(GaleriaClinicaService::kBytesMaximos + 1, 'x');
    const auto excedido = galeria.guardar(kMascota, QStringLiteral("Grande"),
                                          QStringLiteral("Otro"), grande);
    QVERIFY(!excedido.ok);
    QCOMPARE(contar(clinica, QStringLiteral("mascota_galeria")), 0LL);

    const auto tituloVacio = galeria.guardar(kMascota, QStringLiteral("   "),
                                             QStringLiteral("Foto"),
                                             imagenPng(20, 20, QColor(1, 1, 1)));
    QVERIFY(!tituloVacio.ok);
}

QTEST_GUILESS_MAIN(TstAdjuntosClinicos)
#include "tst_adjuntos_clinicos.moc"
