#include "fuzzy/core/BuzonCliente.h"
#include "fuzzy/core/DirectorioCliente.h"
#include "fuzzy/core/EmparejamientoService.h"
#include "fuzzy/core/MigrationRunner.h"
#include "fuzzy/core/MovilStore.h"
#include "fuzzy/core/PortalCliente.h"
#include "fuzzy/core/PortalTokenService.h"
#include "fuzzy/core/SqliteExecutor.h"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QVariant>
#include <QtTest>

using fuzzy::core::AltaLocal;
using fuzzy::core::BuzonCliente;
using fuzzy::core::ClinicaPublica;
using fuzzy::core::DirectorioCliente;
using fuzzy::core::EmparejamientoService;
using fuzzy::core::IQueryExecutor;
using fuzzy::core::MigrationRunner;
using fuzzy::core::MovilStore;
using fuzzy::core::PortalCliente;
using fuzzy::core::PortalConocido;
using fuzzy::core::PortalTokenService;
using fuzzy::core::SqliteExecutor;

namespace {

const QString kClinicaA = QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa1");
const QString kClinicaB = QStringLiteral("bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbb2");
const QString kPortalA = QStringLiteral("11111111-1111-4111-8111-111111111111");
const QString kPortalB = QStringLiteral("22222222-2222-4222-8222-222222222222");
const QString kTokenA = QStringLiteral("token-clinica-a-0001");
const QString kTokenB = QStringLiteral("token-clinica-b-0002");
const QString kCuentaA = QStringLiteral("cuenta-a");
const QString kCuentaB = QStringLiteral("cuenta-b");
const QString kPerfilA = QStringLiteral("perfil-a");
const QString kPerfilB = QStringLiteral("perfil-b");
const QString kMascotaA = QStringLiteral("mascota-a");
const QString kMascotaB = QStringLiteral("mascota-b");

int contadorConexiones()
{
    static int siguiente = 0;
    return ++siguiente;
}

QString conexion(const QString& etiqueta)
{
    return QStringLiteral("fase8_%1_%2").arg(etiqueta).arg(contadorConexiones());
}

constexpr qint64 kAhora = 1760000000000LL;

void sembrarPortal(IQueryExecutor& portal, const QString& clinica, const QString& cuenta,
                   const QString& perfil_uuid, const QString& mascota_uuid,
                   const QString& nombreMascota, const QString& nombreDueno)
{
    portal.execute(
        QStringLiteral("INSERT INTO portal_meta "
                       "(id, esquema_version, cuenta_uuid, clinica_uuid, generado_at, "
                       "clinica_nombre, proyeccion_version, proyectado_at) "
                       "VALUES (1, 2, ?, ?, ?, 'Clínica de prueba', 1, ?)"),
        {QVariant(cuenta), QVariant(clinica), QVariant::fromValue(kAhora),
         QVariant::fromValue(kAhora)});
    portal.execute(
        QStringLiteral("INSERT INTO perfil "
                       "(uuid, nombre_completo, correo, telefono, activo, created_at, "
                       "updated_at) VALUES (?, ?, 'ana@example.com', '3001234567', 1, ?, ?)"),
        {QVariant(perfil_uuid), QVariant(nombreDueno), QVariant::fromValue(kAhora),
         QVariant::fromValue(kAhora)});
    portal.execute(
        QStringLiteral("INSERT INTO mascotas "
                       "(uuid, nombre, especie, raza, sexo, peso_kg, activo, castrado, "
                       "created_at, updated_at) VALUES (?, ?, 'Perro', 'Mestiza', 'Hembra', "
                       "12.5, 1, 0, ?, ?)"),
        {QVariant(mascota_uuid), QVariant(nombreMascota), QVariant::fromValue(kAhora),
         QVariant::fromValue(kAhora)});
    portal.execute(
        QStringLiteral("INSERT INTO citas "
                       "(uuid, mascota_uuid, fecha_hora, tipo_servicio, estado, origen, "
                       "created_at, updated_at) VALUES ('cita-1', ?, ?, 'Consulta', "
                       "'Agendada', 'app', ?, ?)"),
        {QVariant(mascota_uuid), QVariant::fromValue(kAhora + 86400000),
         QVariant::fromValue(kAhora), QVariant::fromValue(kAhora)});
}

QByteArray qrDe(const QString& portal_uuid, const QString& clinica_uuid,
                const QString& clinica_nombre, const QString& token,
                const QString& huella = {})
{
    QJsonObject objeto;
    objeto.insert(QStringLiteral("v"), 1);
    objeto.insert(QStringLiteral("url"), QStringLiteral("https://turso.example/%1")
                                            .arg(portal_uuid));
    objeto.insert(QStringLiteral("token"), token);
    objeto.insert(QStringLiteral("portal_uuid"), portal_uuid);
    objeto.insert(QStringLiteral("clinica_uuid"), clinica_uuid);
    objeto.insert(QStringLiteral("clinica_nombre"), clinica_nombre);
    objeto.insert(QStringLiteral("huella"),
                  huella.isEmpty() ? PortalTokenService::huella(token) : huella);
    return QJsonDocument(objeto).toJson(QJsonDocument::Compact);
}

QStringList filasComoTexto(const QList<QVariantMap>& filas)
{
    QStringList salida;
    for (const QVariantMap& fila : filas) {
        salida.append(fila.value(QStringLiteral("uuid")).toString());
    }
    return salida;
}

} // namespace

// Servicios de la fase 8: almacén local cifrado, buzón, directorio, portal y
// emparejamiento. Cada prueba usa bases SQLite reales; no hay dobles de
// persistencia, porque el valor del gate está en el flujo completo.
class MovilServiciosTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void desbloqueoYClave();
    void directorioDesdeJsonYBuscador();
    void altaInvalidaNoSaleDelTelefono();
    void altaIdempotentePorUuid();
    void emparejamientoVerificaHuella();
    void emparejamientoDescargaEImporta();
    void dosPortalesAisladosYRevocacionIndependiente();
    void modoAvionYColaSincroniza();
    void conflictoLwwDuplicadoYTombstone();
    void caducidadDeLaCache();
    void tokenYCamposClinicosNoEstanEnClaro();

private:
    MovilStore* abrirStore(const QString& etiqueta, const QString& ruta = {});
    QTemporaryDir dir_;
    std::vector<std::unique_ptr<SqliteExecutor>> ejecutores_;
    std::unique_ptr<MovilStore> store_;
};

MovilStore* MovilServiciosTest::abrirStore(const QString& etiqueta, const QString& ruta)
{
    auto db = std::make_unique<SqliteExecutor>(
        conexion(etiqueta), ruta.isEmpty() ? QStringLiteral(":memory:") : ruta);
    if (!MigrationRunner::mobile().aplicar(*db)) {
        qFatal("No se pudo aplicar el esquema móvil");
    }
    MovilStore* store = new MovilStore(*db);
    store->setIteraciones(1000);
    store_.reset(store);
    ejecutores_.push_back(std::move(db));
    return store;
}

void MovilServiciosTest::init()
{
    ejecutores_.clear();
    store_.reset();
}

void MovilServiciosTest::cleanup()
{
    store_.reset();
    ejecutores_.clear();
}

void MovilServiciosTest::desbloqueoYClave()
{
    MovilStore* store = abrirStore(QStringLiteral("clave"));
    QVERIFY(!store->tieneClave());
    QVERIFY(store->configurarClave(QStringLiteral("Aurora2026!")));
    QVERIFY(store->tieneClave());
    QVERIFY(store->desbloqueado());
    QVERIFY(!store->configurarClave(QStringLiteral("otra"))); // ya hay clave

    store->bloquear();
    QVERIFY(!store->desbloqueado());
    QVERIFY(!store->desbloquear(QStringLiteral("incorrecta")));
    QVERIFY(!store->desbloqueado());
    QVERIFY(store->desbloquear(QStringLiteral("Aurora2026!")));
    QVERIFY(store->desbloqueado());
}

void MovilServiciosTest::directorioDesdeJsonYBuscador()
{
    const QByteArray json = R"({"clinicas":[
        {"uuid":"c1","nombre":"Veterinaria Norte","ciudad":"Bogotá",
         "direccion":"Calle 1","telefono":"300","inbox_url":"https://inbox/1",
         "inbox_token":"publico-1","activa":true,"updated_at":10},
        {"uuid":"c2","nombre":"Clínica Sur","ciudad":"Medellín",
         "direccion":"Carrera 2","telefono":"301","inbox_url":"https://inbox/2",
         "inbox_token":"publico-2","activa":false,"updated_at":20},
        {"uuid":"","nombre":"Sin identidad"}
    ]})";
    QString error;
    const QList<ClinicaPublica> clinicas = DirectorioCliente::desdeJson(json, &error);
    QCOMPARE(clinicas.size(), 2);
    QCOMPARE(clinicas.front().inbox_token, QStringLiteral("publico-1"));
    QVERIFY(clinicas.at(1).updated_at == 20);

    const QList<ClinicaPublica> porCiudad = DirectorioCliente::buscar(clinicas,
                                                                     QStringLiteral("medell"));
    QCOMPARE(porCiudad.size(), 1);
    QCOMPARE(porCiudad.front().uuid, QStringLiteral("c2"));
    QCOMPARE(DirectorioCliente::buscar(clinicas, QString()).size(), 2);

    QString roto;
    QVERIFY(DirectorioCliente::desdeJson("{", &roto).isEmpty());
    QVERIFY(!roto.isEmpty());
}

void MovilServiciosTest::altaInvalidaNoSaleDelTelefono()
{
    auto inbox = std::make_unique<SqliteExecutor>(conexion(QStringLiteral("inbox_invalida")),
                                                  QStringLiteral(":memory:"));
    inbox->execute(QStringLiteral(
        "CREATE TABLE altas (id INTEGER PRIMARY KEY, uuid TEXT NOT NULL UNIQUE, "
        "clinica_uuid TEXT NOT NULL, nombre TEXT NOT NULL, correo TEXT, "
        "telefono TEXT NOT NULL, mascota TEXT, motivo TEXT, franja TEXT, "
        "creada_at INTEGER NOT NULL, "
        "CHECK (length(nombre) BETWEEN 3 AND 120), "
        "CHECK (length(telefono) BETWEEN 7 AND 20), "
        "CHECK (correo IS NULL OR length(correo) BETWEEN 5 AND 254), "
        "CHECK (mascota IS NULL OR length(mascota) <= 120), "
        "CHECK (motivo IS NULL OR length(motivo) <= 500), "
        "CHECK (franja IS NULL OR length(franja) <= 40))"));

    BuzonCliente cliente(*inbox);
    QVariantMap datos;
    datos.insert(QStringLiteral("nombre"), QStringLiteral("Al"));
    datos.insert(QStringLiteral("telefono"), QStringLiteral("3001234567"));

    QString error;
    QVERIFY(BuzonCliente::normalizar(datos, {}, kClinicaA, &error).isEmpty());
    QVERIFY(error.contains(QStringLiteral("3")));

    datos.insert(QStringLiteral("nombre"), QStringLiteral("Ana María"));
    datos.insert(QStringLiteral("telefono"), QStringLiteral("123"));
    QVERIFY(BuzonCliente::normalizar(datos, {}, kClinicaA, &error).isEmpty());
    QVERIFY(error.contains(QStringLiteral("7")));

    datos.insert(QStringLiteral("telefono"), QStringLiteral("3001234567"));
    datos.insert(QStringLiteral("correo"), QStringLiteral("sin-arroba"));
    QVERIFY(BuzonCliente::normalizar(datos, {}, kClinicaA, &error).isEmpty());
    QVERIFY(error.contains(QStringLiteral("correo")));

    const auto altas = inbox->query(QStringLiteral("SELECT count(*) FROM altas"));
    QCOMPARE(altas.front().values.front().toInt(), 0);

    datos.insert(QStringLiteral("correo"), QStringLiteral("ana@example.com"));
    datos.insert(QStringLiteral("mascota"), QStringLiteral("Luna"));
    datos.insert(QStringLiteral("motivo"), QStringLiteral("Vacunación"));
    datos.insert(QStringLiteral("franja"), QStringLiteral("mañana"));
    const auto resultado = cliente.enviar(datos, kClinicaA);
    QVERIFY2(resultado.ok, qPrintable(resultado.error));
    QCOMPARE(inbox->query(QStringLiteral("SELECT count(*) FROM altas"))
                 .front().values.front().toInt(),
             1);
}

void MovilServiciosTest::altaIdempotentePorUuid()
{
    auto inbox = std::make_unique<SqliteExecutor>(conexion(QStringLiteral("inbox_idem")),
                                                  QStringLiteral(":memory:"));
    inbox->execute(QStringLiteral(
        "CREATE TABLE altas (id INTEGER PRIMARY KEY, uuid TEXT NOT NULL UNIQUE, "
        "clinica_uuid TEXT NOT NULL, nombre TEXT NOT NULL, correo TEXT, "
        "telefono TEXT NOT NULL, mascota TEXT, motivo TEXT, franja TEXT, "
        "creada_at INTEGER NOT NULL)"));
    BuzonCliente cliente(*inbox);

    QVariantMap datos;
    datos.insert(QStringLiteral("nombre"), QStringLiteral("Ana María"));
    datos.insert(QStringLiteral("telefono"), QStringLiteral("3001234567"));
    const auto primera = cliente.enviar(datos, kClinicaA, QStringLiteral("alta-fija"));
    QVERIFY(primera.ok);
    const auto segunda = cliente.enviar(datos, kClinicaA, QStringLiteral("alta-fija"));
    QVERIFY(segunda.ok);
    QCOMPARE(inbox->query(QStringLiteral("SELECT count(*) FROM altas"))
                 .front().values.front().toInt(),
             1);
}

void MovilServiciosTest::emparejamientoVerificaHuella()
{
    auto portal = std::make_unique<SqliteExecutor>(conexion(QStringLiteral("portal_huella")),
                                                   QStringLiteral(":memory:"));
    QVERIFY(MigrationRunner::portal().aplicar(*portal));
    sembrarPortal(*portal, kClinicaA, kCuentaA, kPerfilA, kMascotaA,
                  QStringLiteral("Luna"), QStringLiteral("Ana"));
    MovilStore* store = abrirStore(QStringLiteral("store_huella"));
    QVERIFY(store->configurarClave(QStringLiteral("Aurora2026!")));

    EmparejamientoService servicio(*portal, *store);
    QString error;
    fuzzy::core::DatosEmparejamiento datos;
    QVERIFY(!EmparejamientoService::leerQr(
        qrDe(kPortalA, kClinicaA, QStringLiteral("Norte"), kTokenA,
              QStringLiteral("000000")),
        &datos, &error));
    QVERIFY(error.contains(QStringLiteral("huella")));

    QVERIFY(!EmparejamientoService::leerQr(R"({"v":2})", &datos, &error));
    QVERIFY(!error.isEmpty());

    const auto malo = servicio.emparejar(
        qrDe(kPortalA, kClinicaA, QStringLiteral("Norte"), kTokenA, QStringLiteral("abcdef")));
    QVERIFY(!malo.ok);
    QVERIFY(store->portales().isEmpty());
}

void MovilServiciosTest::emparejamientoDescargaEImporta()
{
    auto portal = std::make_unique<SqliteExecutor>(conexion(QStringLiteral("portal_alta")),
                                                   QStringLiteral(":memory:"));
    QVERIFY(MigrationRunner::portal().aplicar(*portal));
    sembrarPortal(*portal, kClinicaA, kCuentaA, kPerfilA, kMascotaA,
                  QStringLiteral("Luna"), QStringLiteral("Ana"));
    MovilStore* store = abrirStore(QStringLiteral("store_alta"));
    QVERIFY(store->configurarClave(QStringLiteral("Aurora2026!")));

    QVariantMap perfilLocal;
    perfilLocal.insert(QStringLiteral("correo"), QStringLiteral("ANA@example.com"));
    QVERIFY(store->guardarRegistroLocal(QStringLiteral("perfil"), QStringLiteral("reg-perfil"),
                                        perfilLocal));
    QVariantMap mascotaLocal;
    mascotaLocal.insert(QStringLiteral("nombre"), QStringLiteral("luna"));
    mascotaLocal.insert(QStringLiteral("especie"), QStringLiteral("Perro"));
    QVERIFY(store->guardarRegistroLocal(QStringLiteral("mascota"), QStringLiteral("reg-mascota"),
                                        mascotaLocal));
    QVariantMap otraMascota;
    otraMascota.insert(QStringLiteral("nombre"), QStringLiteral("Rocky"));
    QVERIFY(store->guardarRegistroLocal(QStringLiteral("mascota"), QStringLiteral("reg-otra"),
                                        otraMascota));

    EmparejamientoService servicio(*portal, *store);
    const auto resultado = servicio.emparejar(
        qrDe(kPortalA, kClinicaA, QStringLiteral("Veterinaria Norte"), kTokenA));
    QVERIFY2(resultado.ok, qPrintable(resultado.error));
    QCOMPARE(resultado.importados, 2);
    QCOMPARE(resultado.pendientes, 1);
    QCOMPARE(store->portales().size(), 1);
    QCOMPARE(store->portales().front().clinica_nombre, QStringLiteral("Veterinaria Norte"));
    QVERIFY(store->tokenDe(kPortalA) == kTokenA.toUtf8());
    QCOMPARE(store->contar(kPortalA, QStringLiteral("mascotas")), 1);
    QCOMPARE(store->contar(kPortalA, QStringLiteral("citas")), 1);
    QCOMPARE(store->registroLocal(QStringLiteral("mascota"), false).size(), 1);
}

void MovilServiciosTest::dosPortalesAisladosYRevocacionIndependiente()
{
    auto portalA = std::make_unique<SqliteExecutor>(conexion(QStringLiteral("portal_a")),
                                                    QStringLiteral(":memory:"));
    auto portalB = std::make_unique<SqliteExecutor>(conexion(QStringLiteral("portal_b")),
                                                    QStringLiteral(":memory:"));
    QVERIFY(MigrationRunner::portal().aplicar(*portalA));
    QVERIFY(MigrationRunner::portal().aplicar(*portalB));
    sembrarPortal(*portalA, kClinicaA, kCuentaA, kPerfilA, kMascotaA,
                  QStringLiteral("Luna"), QStringLiteral("Ana en A"));
    sembrarPortal(*portalB, kClinicaB, kCuentaB, kPerfilB, kMascotaB,
                  QStringLiteral("Rocky"), QStringLiteral("Ana en B"));

    MovilStore* store = abrirStore(QStringLiteral("store_dos"));
    QVERIFY(store->configurarClave(QStringLiteral("Aurora2026!")));

    EmparejamientoService servicioA(*portalA, *store);
    EmparejamientoService servicioB(*portalB, *store);
    QVERIFY2(servicioA.emparejar(qrDe(kPortalA, kClinicaA, QStringLiteral("Norte"), kTokenA)).ok,
             "portal A");
    QVERIFY2(servicioB.emparejar(qrDe(kPortalB, kClinicaB, QStringLiteral("Sur"), kTokenB)).ok,
             "portal B");

    QCOMPARE(store->portales().size(), 2);
    QCOMPARE(store->filas(kPortalA, QStringLiteral("mascotas")).front()
                 .value(QStringLiteral("nombre")).toString(),
             QStringLiteral("Luna"));
    QCOMPARE(store->filas(kPortalB, QStringLiteral("mascotas")).front()
                 .value(QStringLiteral("nombre")).toString(),
             QStringLiteral("Rocky"));
    QCOMPARE(store->portalMeta(kPortalA).value(QStringLiteral("clinica_uuid")).toString(),
             kClinicaA);
    QCOMPARE(store->portalMeta(kPortalB).value(QStringLiteral("clinica_uuid")).toString(),
             kClinicaB);

    // El mismo dueño tiene dos cuentas y dos portales: la cuenta y el perfil de
    // A no aparecen en B.
    QCOMPARE(store->portalMeta(kPortalA).value(QStringLiteral("cuenta_uuid")).toString(),
             kCuentaA);
    QCOMPARE(store->portalMeta(kPortalB).value(QStringLiteral("cuenta_uuid")).toString(),
             kCuentaB);

    // Revocar A no toca B: la sonda niega solo el portal A.
    PortalCliente clienteA(*portalA, [] { return QStringLiteral("sin_acceso"); });
    const auto syncA = clienteA.sincronizar(*store, kPortalA);
    QVERIFY(!syncA.ok);
    QCOMPARE(syncA.estado, QStringLiteral("sin_acceso"));

    PortalConocido estadoA;
    PortalConocido estadoB;
    QVERIFY(store->portal(kPortalA, &estadoA));
    QVERIFY(store->portal(kPortalB, &estadoB));
    QCOMPARE(estadoA.estado, QStringLiteral("revocado"));
    QCOMPARE(estadoB.estado, QStringLiteral("activo"));

    PortalCliente clienteB(*portalB);
    const auto syncB = clienteB.sincronizar(*store, kPortalB);
    QVERIFY2(syncB.ok, qPrintable(syncB.error));
    QCOMPARE(store->filas(kPortalB, QStringLiteral("mascotas")).size(), 1);

    // Un portal no puede escribir en la base de otra clínica: la descarga
    // compara `portal_meta.clinica_uuid` con la sede emparejada.
    const auto cruzado = clienteB.descargar(*store, kPortalB, kClinicaA);
    QVERIFY(!cruzado.ok);
    QVERIFY(cruzado.error.contains(QStringLiteral("otra clínica")));
}

void MovilServiciosTest::modoAvionYColaSincroniza()
{
    auto portal = std::make_unique<SqliteExecutor>(conexion(QStringLiteral("portal_avion")),
                                                   QStringLiteral(":memory:"));
    QVERIFY(MigrationRunner::portal().aplicar(*portal));
    sembrarPortal(*portal, kClinicaA, kCuentaA, kPerfilA, kMascotaA,
                  QStringLiteral("Luna"), QStringLiteral("Ana"));
    MovilStore* store = abrirStore(QStringLiteral("store_avion"));
    QVERIFY(store->configurarClave(QStringLiteral("Aurora2026!")));
    QVERIFY(EmparejamientoService(*portal, *store)
                .emparejar(qrDe(kPortalA, kClinicaA, QStringLiteral("Norte"), kTokenA))
                .ok);

    // Modo avión: la cita queda en el teléfono y en la cola.
    PortalCliente sinRed(*portal, [] { return QStringLiteral("sin_red"); });
    const auto syncSinRed = sinRed.sincronizar(*store, kPortalA);
    QVERIFY(!syncSinRed.ok);
    QCOMPARE(syncSinRed.estado, QStringLiteral("sin_red"));

    QVERIFY2(sinRed.solicitarCita(*store, kPortalA, kMascotaA, QStringLiteral("Vacunación"),
                                  QStringLiteral("2026-11-02"), QStringLiteral("10:00"),
                                  QStringLiteral("Refuerzo"), QStringLiteral("sol-1"),
                                  kAhora),
             qPrintable(sinRed.ultimoError()));
    QCOMPARE(store->pendientesTotales(kPortalA), 1);
    QCOMPARE(store->filas(kPortalA, QStringLiteral("solicitudes_cita")).size(), 1);
    QVERIFY(store->filas(kPortalA, QStringLiteral("solicitudes_cita")).front()
                .value(QStringLiteral("_pendiente")).toBool());
    QCOMPARE(portal->query(QStringLiteral("SELECT count(*) FROM solicitudes_cita"))
                 .front().values.front().toInt(),
             0);

    // Al volver la red, la cola se envía una sola vez y la caché se libera
    // cuando el portal devuelve la fila.
    PortalCliente conRed(*portal);
    const auto sync = conRed.sincronizar(*store, kPortalA);
    QVERIFY2(sync.ok, qPrintable(sync.error));
    QCOMPARE(sync.enviadas, 1);
    QCOMPARE(sync.fallidas, 0);
    QCOMPARE(store->pendientesTotales(kPortalA), 0);
    QCOMPARE(portal->query(QStringLiteral("SELECT count(*) FROM solicitudes_cita"))
                 .front().values.front().toInt(),
             1);
    QVERIFY(!store->filas(kPortalA, QStringLiteral("solicitudes_cita")).front()
                 .value(QStringLiteral("_pendiente")).toBool());

    // Reintentar la sincronización no duplica la solicitud (uuid idempotente).
    QVERIFY(conRed.sincronizar(*store, kPortalA).ok);
    QCOMPARE(portal->query(QStringLiteral("SELECT count(*) FROM solicitudes_cita"))
                 .front().values.front().toInt(),
             1);
}

void MovilServiciosTest::conflictoLwwDuplicadoYTombstone()
{
    auto portal = std::make_unique<SqliteExecutor>(conexion(QStringLiteral("portal_lww")),
                                                   QStringLiteral(":memory:"));
    QVERIFY(MigrationRunner::portal().aplicar(*portal));
    sembrarPortal(*portal, kClinicaA, kCuentaA, kPerfilA, kMascotaA,
                  QStringLiteral("Luna"), QStringLiteral("Ana"));
    MovilStore* store = abrirStore(QStringLiteral("store_lww"));
    QVERIFY(store->configurarClave(QStringLiteral("Aurora2026!")));
    QVERIFY(EmparejamientoService(*portal, *store)
                .emparejar(qrDe(kPortalA, kClinicaA, QStringLiteral("Norte"), kTokenA))
                .ok);

    // Una edición local más nueva gana sobre una descarga vieja.
    QVariantMap local = store->fila(kPortalA, QStringLiteral("mascotas"), kMascotaA);
    local.insert(QStringLiteral("alergias"), QStringLiteral("Ninguna"));
    local.insert(QStringLiteral("updated_at"), kAhora + 1000);
    QVERIFY(store->guardarFila(kPortalA, QStringLiteral("mascotas"), local, true));
    QVERIFY(store->encolar(kPortalA, QStringLiteral("mascotas"), kMascotaA, local));

    portal->execute(QStringLiteral("UPDATE mascotas SET alergias = 'Polen', "
                                   "updated_at = ? WHERE uuid = ?"),
                    {QVariant::fromValue(kAhora + 500), QVariant(kMascotaA)});
    PortalCliente cliente(*portal);
    QVERIFY(cliente.descargar(*store, kPortalA, kClinicaA).ok);
    QCOMPARE(store->fila(kPortalA, QStringLiteral("mascotas"), kMascotaA)
                 .value(QStringLiteral("alergias")).toString(),
             QStringLiteral("Ninguna"));

    // Una descarga más nueva sí gana (LWW remoto).
    portal->execute(QStringLiteral("UPDATE mascotas SET alergias = 'Ácaros', "
                                   "updated_at = ? WHERE uuid = ?"),
                    {QVariant::fromValue(kAhora + 2000), QVariant(kMascotaA)});
    QVERIFY(cliente.descargar(*store, kPortalA, kClinicaA).ok);
    QCOMPARE(store->fila(kPortalA, QStringLiteral("mascotas"), kMascotaA)
                 .value(QStringLiteral("alergias")).toString(),
             QStringLiteral("Ácaros"));

    // Descargar dos veces la misma versión es idempotente y no duplica filas.
    QVERIFY(cliente.descargar(*store, kPortalA, kClinicaA).ok);
    QCOMPARE(store->contar(kPortalA, QStringLiteral("mascotas")), 1);

    // Un tombstone remoto más nuevo oculta la fila sin borrarla del cache.
    portal->execute(QStringLiteral("UPDATE mascotas SET deleted_at = ?, updated_at = ? "
                                   "WHERE uuid = ?"),
                    {QVariant::fromValue(kAhora + 3000), QVariant::fromValue(kAhora + 3000),
                     QVariant(kMascotaA)});
    QVERIFY(cliente.descargar(*store, kPortalA, kClinicaA).ok);
    QCOMPARE(store->contar(kPortalA, QStringLiteral("mascotas")), 0);
    QCOMPARE(store->filas(kPortalA, QStringLiteral("mascotas"), true).size(), 1);

    // Y una fila pendiente no se pisa con una descarga que no la incluye.
    QCOMPARE(store->fila(kPortalA, QStringLiteral("mascotas"), kMascotaA)
                 .value(QStringLiteral("uuid")).toString(),
             kMascotaA);
}

void MovilServiciosTest::caducidadDeLaCache()
{
    auto portal = std::make_unique<SqliteExecutor>(conexion(QStringLiteral("portal_caduca")),
                                                   QStringLiteral(":memory:"));
    QVERIFY(MigrationRunner::portal().aplicar(*portal));
    sembrarPortal(*portal, kClinicaA, kCuentaA, kPerfilA, kMascotaA,
                  QStringLiteral("Luna"), QStringLiteral("Ana"));
    MovilStore* store = abrirStore(QStringLiteral("store_caduca"));
    QVERIFY(store->configurarClave(QStringLiteral("Aurora2026!")));
    QVERIFY(EmparejamientoService(*portal, *store)
                .emparejar(qrDe(kPortalA, kClinicaA, QStringLiteral("Norte"), kTokenA))
                .ok);

    const qint64 hace40Dias = kAhora - 40 * MovilStore::kMsDia;
    QVERIFY(store->registrarSync(kPortalA, hace40Dias));
    // La purga usa `ultima_sync + 30 días`; con 40 días de antigüedad la caché
    // se borra y el portal sigue emparejado para volver a descargar.
    QVERIFY(store->purgarVencida(kAhora) > 0);
    QCOMPARE(store->contar(kPortalA, QStringLiteral("mascotas")), 0);
    QVERIFY(!store->portales().isEmpty());
}

void MovilServiciosTest::tokenYCamposClinicosNoEstanEnClaro()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString ruta = dir.filePath(QStringLiteral("movil.db"));

    auto portal = std::make_unique<SqliteExecutor>(conexion(QStringLiteral("portal_claro")),
                                                   QStringLiteral(":memory:"));
    QVERIFY(MigrationRunner::portal().aplicar(*portal));
    sembrarPortal(*portal, kClinicaA, kCuentaA, kPerfilA, kMascotaA,
                  QStringLiteral("LunaSecreta"), QStringLiteral("Ana Secreta"));
    MovilStore* store = abrirStore(QStringLiteral("store_claro"), ruta);
    QVERIFY(store->configurarClave(QStringLiteral("Aurora2026!")));
    QVERIFY(EmparejamientoService(*portal, *store)
                .emparejar(qrDe(kPortalA, kClinicaA, QStringLiteral("Norte"), kTokenA))
                .ok);
    store->bloquear();
    ejecutores_.clear();
    store_.reset();

    QFile archivo(ruta);
    QVERIFY(archivo.open(QIODevice::ReadOnly));
    const QByteArray bytes = archivo.readAll();
    QVERIFY(!bytes.isEmpty());
    // El token de la clínica vive cifrado: sacar el archivo no da acceso.
    QVERIFY(!bytes.contains(kTokenA.toUtf8()));
    QVERIFY(!bytes.contains("token-clinica"));
    // Los datos de negocio (nombre del dueño y de la mascota) tampoco.
    QVERIFY(!bytes.contains("LunaSecreta"));
    QVERIFY(!bytes.contains("Ana Secreta"));

    // Y sin la contraseña el portal no se puede abrir.
    auto reabrir = std::make_unique<SqliteExecutor>(conexion(QStringLiteral("store_reabrir")),
                                                    ruta);
    MovilStore segundo(*reabrir);
    QVERIFY(!segundo.desbloquear(QStringLiteral("otra")));
    QVERIFY(segundo.tokenDe(kPortalA).isEmpty());
    QVERIFY(segundo.desbloquear(QStringLiteral("Aurora2026!")));
    QVERIFY(segundo.tokenDe(kPortalA) == kTokenA.toUtf8());
    QCOMPARE(segundo.filas(kPortalA, QStringLiteral("mascotas")).front()
                 .value(QStringLiteral("nombre")).toString(),
             QStringLiteral("LunaSecreta"));
}

QTEST_GUILESS_MAIN(MovilServiciosTest)

#include "tst_movil_servicios.moc"
