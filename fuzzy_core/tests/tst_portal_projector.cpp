#include "TestSupport.h"

#include "fuzzy/core/PortalBaseStore.h"
#include "fuzzy/core/PortalProjector.h"
#include "fuzzy/core/PortalProvisioner.h"
#include "fuzzy/core/PortalTokenService.h"

#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QVariant>
#include <QtTest>

using fuzzy::core::LocalPortalBaseStore;
using fuzzy::core::PortalProjector;
using fuzzy::core::PortalProvisioner;
using fuzzy::core::PortalTokenService;
using fuzzy::core::SqliteExecutor;
using namespace fuzzy::test;

namespace {

const QString kClinica = QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa1");

QString uuidFila(int grupo, int indice)
{
    return QStringLiteral("%1%2")
               .arg(grupo, 2, 10, QLatin1Char('0'))
               .arg(indice, 6, 10, QLatin1Char('0'))
        + QStringLiteral("-0000-4000-8000-000000000000");
}

struct Datos
{
    QString cliente;
    QString cuenta;
    QString mascota;
    QString consulta;
    QString receta;
    QString recetaItem;
    QString vacuna;
    QString signo;
    QString cita;

    QStringList todos() const
    {
        return {cliente, cuenta, mascota, consulta, receta,
                recetaItem, vacuna, signo, cita};
    }
};

Datos datosDe(int indice)
{
    Datos datos;
    datos.cliente = uuidFila(1, indice);
    datos.cuenta = uuidFila(2, indice);
    datos.mascota = uuidFila(3, indice);
    datos.consulta = uuidFila(4, indice);
    datos.receta = uuidFila(5, indice);
    datos.recetaItem = uuidFila(6, indice);
    datos.vacuna = uuidFila(7, indice);
    datos.signo = uuidFila(8, indice);
    datos.cita = uuidFila(9, indice);
    return datos;
}

void sembrarClinica(IQueryExecutor& executor)
{
    QVERIFY(aplicarEsquema(executor, QStringLiteral("clinic")));
    executor.execute(
        QStringLiteral("INSERT INTO clinicas (uuid, nombre, zona_horaria, created_at, "
                       "updated_at, activa, sync_state) "
                       "VALUES (?, 'Clínica Norte', 'America/Bogota', 1, 1, 1, 'pendiente')"),
        {QVariant(kClinica)});

    for (int i = 1; i <= 3; ++i) {
        const Datos d = datosDe(i);
        executor.execute(
            QStringLiteral("INSERT INTO clientes (id, uuid, nombre_completo, telefono, "
                           "correo, direccion, activo, created_at, updated_at, sync_state) "
                           "VALUES (?, ?, ?, ?, ?, ?, 1, ?, ?, 'pendiente')"),
            {QVariant::fromValue<qint64>(i), QVariant(d.cliente),
             QVariant(QStringLiteral("Dueño %1").arg(i)),
             QVariant(QStringLiteral("300000000%1").arg(i)),
             QVariant(QStringLiteral("dueno%1@example.com").arg(i)),
             QVariant(QStringLiteral("Calle %1").arg(i)),
             QVariant::fromValue<qint64>(1000 + i), QVariant::fromValue<qint64>(1000 + i)});
        executor.execute(
            QStringLiteral("INSERT INTO cuentas (id, uuid, cliente_id, correo, password_hash, "
                           "estado, activo, created_at, updated_at, sync_state) "
                           "VALUES (?, ?, ?, ?, 'hash', 'Activa', 1, ?, ?, 'pendiente')"),
            {QVariant::fromValue<qint64>(i), QVariant(d.cuenta),
             QVariant::fromValue<qint64>(i),
             QVariant(QStringLiteral("dueno%1@example.com").arg(i)),
             QVariant::fromValue<qint64>(2000 + i), QVariant::fromValue<qint64>(2000 + i)});
        executor.execute(
            QStringLiteral("INSERT INTO mascotas (id, uuid, cliente_id, nombre, especie, raza, "
                           "sexo, peso_kg, activo, castrado, created_at, updated_at, "
                           "sync_state) "
                           "VALUES (?, ?, ?, ?, 'Perro', 'Mestizo', 'Macho', ?, 1, 0, ?, ?, "
                           "'pendiente')"),
            {QVariant::fromValue<qint64>(i), QVariant(d.mascota),
             QVariant::fromValue<qint64>(i),
             QVariant(QStringLiteral("Mascota %1").arg(i)),
             QVariant(10.0 + i), QVariant::fromValue<qint64>(3000 + i),
             QVariant::fromValue<qint64>(3000 + i)});
        executor.execute(
            QStringLiteral("INSERT INTO consultas_medicas (id, uuid, mascota_id, "
                           "fecha_consulta, motivo_visita, diagnostico, estado, triage_notas, "
                           "created_at, updated_at, sync_state) "
                           "VALUES (?, ?, ?, ?, ?, ?, 'Cerrada', ?, ?, ?, 'pendiente')"),
            {QVariant::fromValue<qint64>(i), QVariant(d.consulta),
             QVariant::fromValue<qint64>(i), QVariant::fromValue<qint64>(4000 + i),
             QVariant(QStringLiteral("Motivo %1").arg(i)),
             QVariant(QStringLiteral("Diagnóstico %1").arg(i)),
             QVariant(QStringLiteral("NOTA-INTERNA-%1").arg(i)),
             QVariant::fromValue<qint64>(4000 + i), QVariant::fromValue<qint64>(4000 + i)});
        executor.execute(
            QStringLiteral("INSERT INTO recetas (id, uuid, consulta_id, mascota_id, "
                           "fecha_emision, indicaciones, created_at, updated_at, sync_state) "
                           "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 'pendiente')"),
            {QVariant::fromValue<qint64>(i), QVariant(d.receta),
             QVariant::fromValue<qint64>(i), QVariant::fromValue<qint64>(i),
             QVariant::fromValue<qint64>(5000 + i),
             QVariant(QStringLiteral("Indicaciones %1").arg(i)),
             QVariant::fromValue<qint64>(5000 + i), QVariant::fromValue<qint64>(5000 + i)});
        executor.execute(
            QStringLiteral("INSERT INTO receta_detalles (id, uuid, receta_id, medicamento, "
                           "dosis, frecuencia, duracion, cantidad, precio_unitario, created_at, "
                           "updated_at, sync_state) "
                           "VALUES (?, ?, ?, ?, '1 tableta', 'cada 8 h', '3 días', 3, 10.5, "
                           "?, ?, 'pendiente')"),
            {QVariant::fromValue<qint64>(i), QVariant(d.recetaItem),
             QVariant::fromValue<qint64>(i),
             QVariant(QStringLiteral("Medicamento %1").arg(i)),
             QVariant::fromValue<qint64>(6000 + i), QVariant::fromValue<qint64>(6000 + i)});
        executor.execute(
            QStringLiteral("INSERT INTO vacunas (id, uuid, mascota_id, nombre_vacuna, "
                           "fecha_aplicacion, created_at, updated_at, sync_state) "
                           "VALUES (?, ?, ?, ?, '2026-01-01', ?, ?, 'pendiente')"),
            {QVariant::fromValue<qint64>(i), QVariant(d.vacuna),
             QVariant::fromValue<qint64>(i),
             QVariant(QStringLiteral("Rabia %1").arg(i)),
             QVariant::fromValue<qint64>(7000 + i), QVariant::fromValue<qint64>(7000 + i)});
        executor.execute(
            QStringLiteral("INSERT INTO signos_vitales (id, uuid, mascota_id, fecha, peso_kg, "
                           "created_at, updated_at, sync_state) "
                           "VALUES (?, ?, ?, ?, ?, ?, ?, 'pendiente')"),
            {QVariant::fromValue<qint64>(i), QVariant(d.signo),
             QVariant::fromValue<qint64>(i), QVariant::fromValue<qint64>(8000 + i),
             QVariant(10.0 + i), QVariant::fromValue<qint64>(8000 + i),
             QVariant::fromValue<qint64>(8000 + i)});
        executor.execute(
            QStringLiteral("INSERT INTO citas (id, uuid, mascota_id, tipo_servicio_id, "
                           "estado_cita_id, fecha_hora_programada, origen, notas_cliente, "
                           "fecha_creacion, fecha_actualizacion, created_at, updated_at, "
                           "sync_state) "
                           "VALUES (?, ?, ?, 1, 1, ?, 'mostrador', ?, ?, ?, ?, ?, "
                           "'pendiente')"),
            {QVariant::fromValue<qint64>(i), QVariant(d.cita),
             QVariant::fromValue<qint64>(i), QVariant::fromValue<qint64>(9000 + i),
             QVariant(QStringLiteral("Nota %1").arg(i)),
             QVariant::fromValue<qint64>(9000 + i), QVariant::fromValue<qint64>(9000 + i),
             QVariant::fromValue<qint64>(9000 + i), QVariant::fromValue<qint64>(9000 + i)});
    }
}

QSet<QString> uuidsEnPortal(IQueryExecutor& portal)
{
    QSet<QString> encontrados;
    const QRegularExpression patron(
        QStringLiteral("[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-"
                       "[0-9a-fA-F]{4}-[0-9a-fA-F]{12}"));

    const auto tablas = portal.query(
        QStringLiteral("SELECT name FROM sqlite_master WHERE type = 'table' "
                       "AND name NOT LIKE 'sqlite_%' "
                       "AND name <> '_fuzzy_migraciones'"));
    for (const auto& tabla : tablas) {
        const QString nombre = tabla.values.front().toString();
        const auto filas = portal.query(QStringLiteral("SELECT * FROM %1").arg(nombre));
        for (const auto& fila : filas) {
            for (const QVariant& valor : fila.values) {
                if (!valor.isValid() || valor.isNull()) {
                    continue;
                }
                const QString texto = valor.toString();
                auto coincidencias = patron.globalMatch(texto);
                while (coincidencias.hasNext()) {
                    encontrados.insert(coincidencias.next().captured(0).toLower());
                }
            }
        }
    }
    return encontrados;
}

} // namespace

class TstPortalProjector : public QObject
{
    Q_OBJECT

private slots:
    void cadaPortalSoloTieneUuidsDelDueno();
    void proyectaTombstonesYRespetaEscriturasDeLaApp();
    void reconstruirEsIdempotente();
};

void TstPortalProjector::cadaPortalSoloTieneUuidsDelDueno()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor clinica(nuevaConexion(QStringLiteral("proyector")),
                           dir.filePath(QStringLiteral("clinic.db")));
    sembrarClinica(clinica);

    LocalPortalBaseStore bases(dir.filePath(QStringLiteral("portales")));
    PortalTokenService tokens(clinica, &bases);
    PortalProvisioner provisioner(clinica, tokens, bases);
    PortalProjector proyector(clinica, bases);

    for (int i = 1; i <= 3; ++i) {
        const Datos d = datosDe(i);
        const auto creado = provisioner.crear(
            kClinica, i, i, QString(), QStringLiteral("token-%1").arg(i));
        QVERIFY(creado.has_value());

        const auto resultado = proyector.proyectar(creado->portal_uuid);
        QVERIFY2(resultado.ok, qPrintable(resultado.error));
        QVERIFY(resultado.filas >= 8);

        auto portal = bases.abrir(creado->portal_uuid);
        QVERIFY(portal != nullptr);
        QCOMPARE(contar(*portal, QStringLiteral("perfil")), 1LL);
        QCOMPARE(contar(*portal, QStringLiteral("mascotas")), 1LL);
        QCOMPARE(contar(*portal, QStringLiteral("visitas")), 1LL);
        QCOMPARE(contar(*portal, QStringLiteral("recetas")), 1LL);
        QCOMPARE(contar(*portal, QStringLiteral("receta_items")), 1LL);
        QCOMPARE(contar(*portal, QStringLiteral("vacunas")), 1LL);
        QCOMPARE(contar(*portal, QStringLiteral("pesos")), 1LL);
        QCOMPARE(contar(*portal, QStringLiteral("citas")), 1LL);

        QSet<QString> permitidos;
        permitidos.insert(kClinica);
        for (const QString& uuid : d.todos()) {
            permitidos.insert(uuid);
        }

        QSet<QString> ajenos;
        for (int j = 1; j <= 3; ++j) {
            if (j == i) {
                continue;
            }
            for (const QString& uuid : datosDe(j).todos()) {
                ajenos.insert(uuid);
            }
        }
        QVERIFY(!ajenos.isEmpty());

        const QSet<QString> encontrados = uuidsEnPortal(*portal);
        QVERIFY(!encontrados.isEmpty());

        // Gate de la fase 5: ningún uuid de otro dueño puede aparecer en el
        // portal. Si esto falla, el build se rompe.
        QSet<QString> interseccion = encontrados;
        interseccion.intersect(ajenos);
        QVERIFY2(interseccion.isEmpty(),
                 qPrintable(QStringLiteral("portal %1 contiene uuid ajenos: %2")
                                .arg(creado->portal_uuid,
                                     interseccion.values().join(QStringLiteral(", ")))));

        QSet<QString> desconocidos = encontrados;
        desconocidos.subtract(permitidos);
        QVERIFY2(desconocidos.isEmpty(),
                 qPrintable(QStringLiteral("portal %1 contiene uuid no proyectados: %2")
                                .arg(creado->portal_uuid,
                                     desconocidos.values().join(QStringLiteral(", ")))));
    }
}

void TstPortalProjector::proyectaTombstonesYRespetaEscriturasDeLaApp()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor clinica(nuevaConexion(QStringLiteral("proyector_tombstone")),
                           dir.filePath(QStringLiteral("clinic.db")));
    sembrarClinica(clinica);

    LocalPortalBaseStore bases(dir.filePath(QStringLiteral("portales")));
    PortalTokenService tokens(clinica, &bases);
    PortalProvisioner provisioner(clinica, tokens, bases);
    PortalProjector proyector(clinica, bases);

    const auto creado = provisioner.crear(kClinica, 1, 1, QString(), QStringLiteral("token-1"));
    QVERIFY(creado.has_value());
    QVERIFY(proyector.proyectar(creado->portal_uuid).ok);

    const Datos d = datosDe(1);
    auto portal = bases.abrir(creado->portal_uuid);
    QVERIFY(portal != nullptr);

    const QString solicitud = uuidFila(10, 1);
    const QString foto = uuidFila(11, 1);
    QVERIFY(portal->execute(
                       QStringLiteral("INSERT INTO solicitudes_cita "
                                      "(uuid, mascota_uuid, tipo_servicio, fecha_preferida, "
                                      " hora_preferida, motivo, estado, created_at, updated_at) "
                                      "VALUES (?, ?, 'Médica', '2026-12-01', '09:00', "
                                      "'Control', 'Pendiente', 1, 1)"),
                       {QVariant(solicitud), QVariant(d.mascota)})
                .ok);
    QVERIFY(portal->execute(
                       QStringLiteral("INSERT INTO mascota_fotos "
                                      "(uuid, mascota_uuid, foto, mime, actualizada_at, "
                                      " created_at, updated_at) "
                                      "VALUES (?, ?, NULL, 'image/jpeg', 1, 1, 1)"),
                       {QVariant(foto), QVariant(d.mascota)})
                .ok);

    // Borrado físico en la clínica: debe llegar como tombstone al portal.
    clinica.execute(QStringLiteral("DELETE FROM consultas_medicas WHERE id = 1"));
    clinica.execute(
        QStringLiteral("UPDATE mascotas SET deleted_at = 999999, updated_at = 999999 "
                       "WHERE id = 1"));

    const auto segunda = proyector.proyectar(creado->portal_uuid);
    QVERIFY2(segunda.ok, qPrintable(segunda.error));

    const auto mascota = portal->query(
        QStringLiteral("SELECT deleted_at FROM mascotas WHERE uuid = ?"),
        {QVariant(d.mascota)});
    QCOMPARE(mascota.size(), size_t(1));
    QVERIFY(!mascota.front().values.front().isNull());

    const auto visita = portal->query(
        QStringLiteral("SELECT deleted_at FROM visitas WHERE uuid = ?"),
        {QVariant(d.consulta)});
    QCOMPARE(visita.size(), size_t(1));
    QVERIFY(!visita.front().values.front().isNull());

    // Las tablas que escribe la app jamás se tocan en la proyección.
    const auto solicitudes = portal->query(
        QStringLiteral("SELECT uuid, estado FROM solicitudes_cita"));
    QCOMPARE(solicitudes.size(), size_t(1));
    QCOMPARE(solicitudes.front().values.at(0).toString(), solicitud);
    QCOMPARE(solicitudes.front().values.at(1).toString(), QStringLiteral("Pendiente"));

    const auto fotos = portal->query(QStringLiteral("SELECT uuid FROM mascota_fotos"));
    QCOMPARE(fotos.size(), size_t(1));
    QCOMPARE(fotos.front().values.at(0).toString(), foto);
}

void TstPortalProjector::reconstruirEsIdempotente()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor clinica(nuevaConexion(QStringLiteral("proyector_idem")),
                           dir.filePath(QStringLiteral("clinic.db")));
    sembrarClinica(clinica);

    LocalPortalBaseStore bases(dir.filePath(QStringLiteral("portales")));
    PortalTokenService tokens(clinica, &bases);
    PortalProvisioner provisioner(clinica, tokens, bases);
    PortalProjector proyector(clinica, bases);

    const auto creado = provisioner.crear(kClinica, 1, 1, QString(), QStringLiteral("token-1"));
    QVERIFY(creado.has_value());
    QVERIFY(proyector.proyectar(creado->portal_uuid).ok);
    QVERIFY(proyector.proyectar(creado->portal_uuid).ok);

    auto portal = bases.abrir(creado->portal_uuid);
    QVERIFY(portal != nullptr);
    QCOMPARE(contar(*portal, QStringLiteral("mascotas")), 1LL);
    QCOMPARE(contar(*portal, QStringLiteral("visitas")), 1LL);
    QCOMPARE(contar(*portal, QStringLiteral("recetas")), 1LL);
    QCOMPARE(contar(*portal, QStringLiteral("citas")), 1LL);

    const auto meta = portal->query(
        QStringLiteral("SELECT proyeccion_version FROM portal_meta WHERE id = 1"));
    QCOMPARE(meta.front().values.front().toInt(), 1);
}

QTEST_MAIN(TstPortalProjector)
#include "tst_portal_projector.moc"
