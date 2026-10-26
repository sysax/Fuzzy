#include "TestSupport.h"

#include "fuzzy/core/PortalBaseStore.h"
#include "fuzzy/core/PortalProvisioner.h"
#include "fuzzy/core/PortalTokenService.h"

#include <QSet>
#include <QTemporaryDir>
#include <QtTest>

using fuzzy::core::LocalPortalBaseStore;
using fuzzy::core::PortalProvisioner;
using fuzzy::core::PortalTokenService;
using fuzzy::core::SqliteExecutor;
using namespace fuzzy::test;

namespace {

const QString kClinica = QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa1");

QStringList tablasDe(IQueryExecutor& executor)
{
    QStringList tablas;
    const auto filas = executor.query(
        QStringLiteral("SELECT name FROM sqlite_master WHERE type = 'table' "
                       "ORDER BY name"));
    for (const auto& fila : filas) {
        tablas.append(fila.values.front().toString());
    }
    return tablas;
}

} // namespace

class TstPortalContract : public QObject
{
    Q_OBJECT

private slots:
    void esquemaDelPortalEsElContrato();
};

void TstPortalContract::esquemaDelPortalEsElContrato()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SqliteExecutor clinica(nuevaConexion(QStringLiteral("contrato")),
                           dir.filePath(QStringLiteral("clinic.db")));
    QVERIFY(aplicarEsquema(clinica, QStringLiteral("clinic")));
    clinica.execute(
        QStringLiteral("INSERT INTO clinicas (uuid, nombre, zona_horaria, created_at, "
                       "updated_at, activa, sync_state) "
                       "VALUES (?, 'Clínica Norte', 'America/Bogota', 1, 1, 1, 'pendiente')"),
        {QVariant(kClinica)});
    clinica.execute(
        QStringLiteral("INSERT INTO clientes (id, uuid, nombre_completo, activo, created_at, "
                       "updated_at, sync_state) "
                       "VALUES (1, 'cccccccc-cccc-4ccc-8ccc-ccccccccccc1', 'Ana', 1, 1, 1, "
                       "'pendiente')"));
    clinica.execute(
        QStringLiteral("INSERT INTO cuentas (id, uuid, cliente_id, correo, password_hash, "
                       "estado, activo, created_at, updated_at, sync_state) "
                       "VALUES (1, 'dddddddd-dddd-4ddd-8ddd-ddddddddddd1', 1, "
                       "'ana@example.com', 'hash', 'Activa', 1, 1, 1, 'pendiente')"));

    LocalPortalBaseStore bases(dir.filePath(QStringLiteral("portales")));
    PortalTokenService tokens(clinica, &bases);
    PortalProvisioner provisioner(clinica, tokens, bases);
    const auto creado = provisioner.crear(kClinica, 1, 1, QString(), QStringLiteral("token"));
    QVERIFY(creado.has_value());

    auto portal = bases.abrir(creado->portal_uuid);
    QVERIFY(portal != nullptr);

    const QSet<QString> esperadas = {
        QStringLiteral("_fuzzy_migraciones"), QStringLiteral("adjuntos"),
        QStringLiteral("citas"),              QStringLiteral("laboratorio"),
        QStringLiteral("mascota_fotos"),      QStringLiteral("mascotas"),
        QStringLiteral("perfil"),             QStringLiteral("pesos"),
        QStringLiteral("portal_meta"),        QStringLiteral("receta_items"),
        QStringLiteral("recetas"),            QStringLiteral("solicitudes_cita"),
        QStringLiteral("vacunas"),            QStringLiteral("visitas"),
    };
    const QStringList tablas = tablasDe(*portal);
    QCOMPARE(QSet<QString>(tablas.begin(), tablas.end()), esperadas);

    // La defensa es que el dato no exista, no filtrarlo al proyectar.
    const QSet<QString> internas = {
        QStringLiteral("auditoria"),      QStringLiteral("categorias_inventario"),
        QStringLiteral("cierres_caja"),   QStringLiteral("clientes"),
        QStringLiteral("clinicas"),       QStringLiteral("consultas_medicas"),
        QStringLiteral("cuentas"),        QStringLiteral("detalles_factura"),
        QStringLiteral("estados_cita"),   QStringLiteral("facturas"),
        QStringLiteral("historial_cambios_cita"),
        QStringLiteral("mascota_galeria"),
        QStringLiteral("portales"),       QStringLiteral("productos_insumos"),
        QStringLiteral("receta_detalles"), QStringLiteral("roles"),
        QStringLiteral("signos_vitales"), QStringLiteral("sync_change_log"),
        QStringLiteral("sync_outbox"),    QStringLiteral("tipos_servicio_cita"),
        QStringLiteral("usuarios"),
    };
    for (const QString& interna : internas) {
        QVERIFY2(!tablas.contains(interna),
                 qPrintable(QStringLiteral("el portal contiene la tabla interna %1")
                                .arg(interna)));
    }

    // Toda tabla con uuid debe tener timestamps y borrado lógico (D12).
    for (const QString& tabla : tablas) {
        if (tabla == QStringLiteral("_fuzzy_migraciones")
            || tabla == QStringLiteral("portal_meta")) {
            continue;
        }
        QVERIFY(existeColumna(*portal, tabla, QStringLiteral("uuid")));
        QVERIFY(existeColumna(*portal, tabla, QStringLiteral("created_at")));
        QVERIFY(existeColumna(*portal, tabla, QStringLiteral("updated_at")));
        QVERIFY(existeColumna(*portal, tabla, QStringLiteral("deleted_at")));
    }

    // `mascota_fotos` no está en `mascotas`: Turso autoriza por tabla.
    QVERIFY(!existeColumna(*portal, QStringLiteral("mascotas"), QStringLiteral("foto")));
    QVERIFY(!existeColumna(*portal, QStringLiteral("visitas"),
                           QStringLiteral("notas_internas")));

    const auto meta = portal->query(
        QStringLiteral("SELECT count(*), max(esquema_version) FROM portal_meta"));
    QCOMPARE(meta.front().values.at(0).toLongLong(), 1LL);
    QCOMPARE(meta.front().values.at(1).toInt(), 2);
}

QTEST_MAIN(TstPortalContract)
#include "tst_portal_contract.moc"
