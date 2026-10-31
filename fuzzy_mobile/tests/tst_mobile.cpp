#include "app/MobileController.h"
#include "fuzzy/core/MigrationRunner.h"
#include "fuzzy/core/MovilStore.h"
#include "fuzzy/core/PortalTokenService.h"
#include "fuzzy/core/SqliteExecutor.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QVariant>
#include <QtTest>

using fuzzy::core::MigrationRunner;
using fuzzy::core::MovilStore;
using fuzzy::core::PortalConocido;
using fuzzy::core::PortalTokenService;
using fuzzy::core::SqliteExecutor;
using fuzzy::mobile::ConexionPortal;
using fuzzy::mobile::MobileController;

namespace {

const QString kClinicaA = QStringLiteral("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa1");
const QString kClinicaB = QStringLiteral("bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbb2");
const QString kPortalA = QStringLiteral("11111111-1111-4111-8111-111111111111");
const QString kPortalB = QStringLiteral("22222222-2222-4222-8222-222222222222");
const QString kTokenA = QStringLiteral("token-portal-a");
const QString kTokenB = QStringLiteral("token-portal-b");
const QString kUrlA = QStringLiteral("fake://clinica-a");
const QString kUrlB = QStringLiteral("fake://clinica-b");
constexpr qint64 kAhora = 1760000000000LL;

int contadorConexiones()
{
    static int siguiente = 0;
    return ++siguiente;
}

QString conexion(const QString& etiqueta)
{
    return QStringLiteral("fase8_movil_%1_%2").arg(etiqueta).arg(contadorConexiones());
}

QByteArray qrDe(const QString& portal_uuid, const QString& clinica_uuid,
                const QString& clinica_nombre, const QString& token,
                const QString& huella = {})
{
    QJsonObject objeto;
    objeto.insert(QStringLiteral("v"), 1);
    objeto.insert(QStringLiteral("url"),
                  portal_uuid == kPortalA ? kUrlA : kUrlB);
    objeto.insert(QStringLiteral("token"), token);
    objeto.insert(QStringLiteral("portal_uuid"), portal_uuid);
    objeto.insert(QStringLiteral("clinica_uuid"), clinica_uuid);
    objeto.insert(QStringLiteral("clinica_nombre"), clinica_nombre);
    objeto.insert(QStringLiteral("huella"),
                  huella.isEmpty() ? PortalTokenService::huella(token) : huella);
    return QJsonDocument(objeto).toJson(QJsonDocument::Compact);
}

QString directorioJson()
{
    const auto clinica = [](const QString& uuid, const QString& nombre, const QString& ciudad,
                            const QString& direccion, const QString& telefono,
                            const QString& inbox, const QString& token, int actualizada) {
        QJsonObject objeto;
        objeto.insert(QStringLiteral("uuid"), uuid);
        objeto.insert(QStringLiteral("nombre"), nombre);
        objeto.insert(QStringLiteral("ciudad"), ciudad);
        objeto.insert(QStringLiteral("direccion"), direccion);
        objeto.insert(QStringLiteral("telefono"), telefono);
        objeto.insert(QStringLiteral("inbox_url"), inbox);
        objeto.insert(QStringLiteral("inbox_token"), token);
        objeto.insert(QStringLiteral("activa"), true);
        objeto.insert(QStringLiteral("updated_at"), actualizada);
        return objeto;
    };

    QJsonArray clinicas;
    clinicas.append(clinica(kClinicaA, QStringLiteral("Veterinaria Norte"),
                            QStringLiteral("Bogotá"), QStringLiteral("Calle 1"),
                            QStringLiteral("300 111 1111"), QStringLiteral("fake://buzon-a"),
                            QStringLiteral("publico-a"), 10));
    clinicas.append(clinica(kClinicaB, QStringLiteral("Clínica Sur"),
                            QStringLiteral("Medellín"), QStringLiteral("Carrera 2"),
                            QStringLiteral("300 222 2222"), QStringLiteral("fake://buzon-b"),
                            QStringLiteral("publico-b"), 20));

    QJsonObject raiz;
    raiz.insert(QStringLiteral("clinicas"), clinicas);
    return QString::fromUtf8(QJsonDocument(raiz).toJson(QJsonDocument::Compact));
}

} // namespace

// Pruebas de la capa de presentación del teléfono. Usan bases SQLite reales y
// portales falsos servidos por el mismo contrato `IQueryExecutor`; no hay
// dobles de la lógica de negocio.
class MobileTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void flujoOfflineASyncConAltaYEmparejamiento();
    void dosPortalesAisladosYSelector();
    void huellaIncorrectaNoEmpareja();
    void revocacionNoTumbaElOtroPortal();
    void cerrarSesionBorraTodo();
    void mascotaAjenaNoSePuedeAgendar();

private:
    void crearPortal(const QString& ruta, const QString& clinica, const QString& cuenta,
                     const QString& perfil, const QString& mascota,
                     const QString& nombreMascota);
    void crearBuzon(const QString& ruta);
    MobileController* control();
    ConexionPortal abrirPortal(const PortalConocido& portal, const QString& token);

    std::unique_ptr<QTemporaryDir> dir_;
    std::unique_ptr<SqliteExecutor> localDb_;
    QString rutaLocal_;
    QString rutaA_;
    QString rutaB_;
    QString rutaBuzon_;
    std::unique_ptr<MovilStore> store_;
    std::unique_ptr<MobileController> control_;
    QString saludA_ = QStringLiteral("ok");
    QString saludB_ = QStringLiteral("ok");
};

void MobileTest::crearPortal(const QString& ruta, const QString& clinica,
                             const QString& cuenta, const QString& perfil,
                             const QString& mascota, const QString& nombreMascota)
{
    SqliteExecutor db(conexion(QStringLiteral("portal")), ruta);
    QVERIFY(MigrationRunner::portal().aplicar(db));
    db.execute(
        QStringLiteral("INSERT INTO portal_meta "
                       "(id, esquema_version, cuenta_uuid, clinica_uuid, generado_at, "
                       "clinica_nombre, proyeccion_version, proyectado_at) "
                       "VALUES (1, 2, ?, ?, ?, 'Clínica', 1, ?)"),
        {QVariant(cuenta), QVariant(clinica), QVariant::fromValue(kAhora),
         QVariant::fromValue(kAhora)});
    db.execute(
        QStringLiteral("INSERT INTO perfil "
                       "(uuid, nombre_completo, correo, telefono, activo, created_at, "
                       "updated_at) VALUES (?, 'Ana Pérez', 'ana@example.com', "
                       "'3001234567', 1, ?, ?)"),
        {QVariant(perfil), QVariant::fromValue(kAhora), QVariant::fromValue(kAhora)});
    db.execute(
        QStringLiteral("INSERT INTO mascotas "
                       "(uuid, nombre, especie, raza, sexo, peso_kg, activo, castrado, "
                       "created_at, updated_at) VALUES (?, ?, 'Perro', 'Mestiza', "
                       "'Hembra', 8.0, 1, 0, ?, ?)"),
        {QVariant(mascota), QVariant(nombreMascota), QVariant::fromValue(kAhora),
         QVariant::fromValue(kAhora)});
    db.execute(
        QStringLiteral("INSERT INTO citas "
                       "(uuid, mascota_uuid, fecha_hora, tipo_servicio, estado, origen, "
                       "created_at, updated_at) VALUES (?, ?, ?, 'Consulta', 'Agendada', "
                       "'app', ?, ?)"),
        {QVariant(QStringLiteral("cita-") + clinica.left(4)), QVariant(mascota),
         QVariant::fromValue(kAhora + 86400000), QVariant::fromValue(kAhora),
         QVariant::fromValue(kAhora)});
}

void MobileTest::crearBuzon(const QString& ruta)
{
    SqliteExecutor db(conexion(QStringLiteral("buzon")), ruta);
    db.execute(QStringLiteral(
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
}

ConexionPortal MobileTest::abrirPortal(const PortalConocido& portal, const QString& token)
{
    ConexionPortal resultado;
    if (portal.portal_uuid == kPortalA && token != kTokenA) {
        return resultado;
    }
    if (portal.portal_uuid == kPortalB && token != kTokenB) {
        return resultado;
    }
    const QString ruta = portal.portal_uuid == kPortalA ? rutaA_ : rutaB_;
    if (ruta.isEmpty()) {
        return resultado;
    }
    resultado.ejecutor = std::make_unique<SqliteExecutor>(
        conexion(QStringLiteral("abrir")), ruta);
    const QString uuid = portal.portal_uuid;
    resultado.sonda = [this, uuid] {
        return uuid == kPortalA ? saludA_ : saludB_;
    };
    return resultado;
}

MobileController* MobileTest::control()
{
    const QString ruta = dir_->filePath(QStringLiteral("local.db"));
    localDb_ = std::make_unique<SqliteExecutor>(conexion(QStringLiteral("local")), ruta);
    if (!MigrationRunner::mobile().aplicar(*localDb_)) {
        qFatal("No se pudo aplicar el esquema móvil");
    }
    store_ = std::make_unique<MovilStore>(*localDb_);
    store_->setIteraciones(1000);
    control_ = std::make_unique<MobileController>(
        *store_,
        [this](const PortalConocido& portal, const QString& token) {
            return abrirPortal(portal, token);
        },
        [this](const fuzzy::core::ClinicaPublica&) {
            return std::unique_ptr<fuzzy::core::IQueryExecutor>(
                std::make_unique<SqliteExecutor>(conexion(QStringLiteral("buzon")),
                                                 rutaBuzon_));
        });
    return control_.get();
}

void MobileTest::init()
{
    dir_ = std::make_unique<QTemporaryDir>();
    QVERIFY(dir_->isValid());
    rutaA_ = dir_->filePath(QStringLiteral("portal_a.db"));
    rutaB_ = dir_->filePath(QStringLiteral("portal_b.db"));
    rutaBuzon_ = dir_->filePath(QStringLiteral("buzon.db"));
    crearPortal(rutaA_, kClinicaA, QStringLiteral("cuenta-a"),
                QStringLiteral("perfil-a"), QStringLiteral("mascota-a"),
                QStringLiteral("Luna"));
    crearPortal(rutaB_, kClinicaB, QStringLiteral("cuenta-b"),
                QStringLiteral("perfil-b"), QStringLiteral("mascota-b"),
                QStringLiteral("Rocky"));
    crearBuzon(rutaBuzon_);
    saludA_ = QStringLiteral("ok");
    saludB_ = QStringLiteral("ok");
}

void MobileTest::cleanup()
{
    control_.reset();
    store_.reset();
    localDb_.reset();
    dir_.reset();
}

void MobileTest::flujoOfflineASyncConAltaYEmparejamiento()
{
    MobileController* movil = control();
    QVERIFY(!movil->configurada());
    movil->configurarClave(QStringLiteral("Aurora2026!"));
    QVERIFY(movil->configurada());
    QVERIFY(movil->desbloqueada());

    movil->cargarDirectorio(directorioJson());
    QCOMPARE(movil->directorio().size(), 2);
    movil->buscarDirectorio(QStringLiteral("medell"));
    QCOMPARE(movil->directorio().size(), 1);
    movil->buscarDirectorio({});

    movil->registrarPerfil(QStringLiteral("Ana Pérez"), QStringLiteral("ana@example.com"),
                           QStringLiteral("3001234567"), QStringLiteral("Calle 1"),
                           QStringLiteral("123"));
    movil->registrarMascota(QStringLiteral("Luna"), QStringLiteral("Perro"),
                            QStringLiteral("Mestiza"), QStringLiteral("Hembra"), 8.5);
    QCOMPARE(movil->registroPendiente().size(), 2);

    // Alta de primera visita: llega al buzón y reenviarla no la duplica.
    movil->enviarAlta(kClinicaA, QStringLiteral("Ana Pérez"),
                      QStringLiteral("ana@example.com"), QStringLiteral("3001234567"),
                      QStringLiteral("Luna"), QStringLiteral("Vacunación"),
                      QStringLiteral("mañana"));
    QCOMPARE(movil->altas().size(), 1);
    QCOMPARE(movil->altas().front().toMap().value(QStringLiteral("estado")).toString(),
             QStringLiteral("enviada"));
    movil->reintentarAltas();
    {
        SqliteExecutor buzon(conexion(QStringLiteral("verificar")), rutaBuzon_);
        QCOMPARE(buzon.query(QStringLiteral("SELECT count(*) FROM altas"))
                     .front().values.front().toInt(),
                 1);
    }

    // Emparejar: la huella se muestra antes de confirmar.
    QVERIFY(movil->leerQr(QString::fromUtf8(
        qrDe(kPortalA, kClinicaA, QStringLiteral("Veterinaria Norte"), kTokenA))));
    QCOMPARE(movil->huellaPendiente().size(), 6);
    QCOMPARE(movil->clinicaPendiente(), QStringLiteral("Veterinaria Norte"));
    movil->confirmarEmparejamiento();
    QCOMPARE(movil->portales().size(), 1);
    QCOMPARE(movil->mascotas().size(), 1);
    QCOMPARE(movil->mascotas().front().toMap().value(QStringLiteral("nombre")).toString(),
             QStringLiteral("Luna"));
    QCOMPARE(movil->citas().size(), 1);
    // El perfil local coincide por correo y la mascota por nombre: importados.
    QCOMPARE(movil->registroPendiente().size(), 0);

    // Modo avión: la cita queda en cola, no en el portal remoto.
    movil->setModoAvion(true);
    const QString mascota = movil->mascotas().front().toMap()
                                .value(QStringLiteral("uuid")).toString();
    movil->solicitarCita(mascota, QStringLiteral("Vacunación"),
                         QStringLiteral("2026-11-02"), QStringLiteral("10:00"),
                         QStringLiteral("Refuerzo"));
    QCOMPARE(movil->pendientes(), 1);
    QCOMPARE(movil->solicitudes().size(), 1);
    {
        SqliteExecutor portal(conexion(QStringLiteral("remoto")), rutaA_);
        QCOMPARE(portal.query(QStringLiteral("SELECT count(*) FROM solicitudes_cita"))
                     .front().values.front().toInt(),
                 0);
    }

    // Al volver la red, la cola sube una sola vez y la caché se libera.
    movil->setModoAvion(false);
    movil->sincronizar();
    QCOMPARE(movil->pendientes(), 0);
    {
        SqliteExecutor portal(conexion(QStringLiteral("remoto2")), rutaA_);
        QCOMPARE(portal.query(QStringLiteral("SELECT count(*) FROM solicitudes_cita"))
                     .front().values.front().toInt(),
                 1);
    }
}

void MobileTest::dosPortalesAisladosYSelector()
{
    MobileController* movil = control();
    movil->configurarClave(QStringLiteral("Aurora2026!"));
    movil->cargarDirectorio(directorioJson());

    QVERIFY(movil->leerQr(QString::fromUtf8(
        qrDe(kPortalA, kClinicaA, QStringLiteral("Norte"), kTokenA))));
    movil->confirmarEmparejamiento();
    QVERIFY(movil->leerQr(QString::fromUtf8(
        qrDe(kPortalB, kClinicaB, QStringLiteral("Sur"), kTokenB))));
    movil->confirmarEmparejamiento();

    QCOMPARE(movil->portales().size(), 2);
    QCOMPARE(movil->mascotas().size(), 2);
    const QString activo = movil->portalActivo();
    QCOMPARE(activo, kPortalB); // el último emparejado queda activo
    QCOMPARE(movil->perfil().front().toMap().value(QStringLiteral("uuid")).toString(),
             QStringLiteral("perfil-b"));

    movil->seleccionarPortal(kPortalA);
    QCOMPARE(movil->nombrePortal(), QStringLiteral("Norte"));
    QCOMPARE(movil->perfil().front().toMap().value(QStringLiteral("uuid")).toString(),
             QStringLiteral("perfil-a"));

    movil->olvidarPortal(kPortalA);
    QCOMPARE(movil->portales().size(), 1);
    QCOMPARE(movil->portalActivo(), kPortalB);
    QCOMPARE(movil->mascotas().size(), 1);
}

void MobileTest::huellaIncorrectaNoEmpareja()
{
    MobileController* movil = control();
    movil->configurarClave(QStringLiteral("Aurora2026!"));
    QVERIFY(!movil->leerQr(QString::fromUtf8(
        qrDe(kPortalA, kClinicaA, QStringLiteral("Norte"), kTokenA,
             QStringLiteral("000000")))));
    QVERIFY(movil->error().contains(QStringLiteral("huella")));
    QVERIFY(movil->huellaPendiente().isEmpty());
    QCOMPARE(movil->portales().size(), 0);
}

void MobileTest::revocacionNoTumbaElOtroPortal()
{
    MobileController* movil = control();
    movil->configurarClave(QStringLiteral("Aurora2026!"));
    QVERIFY(movil->leerQr(QString::fromUtf8(
        qrDe(kPortalA, kClinicaA, QStringLiteral("Norte"), kTokenA))));
    movil->confirmarEmparejamiento();
    QVERIFY(movil->leerQr(QString::fromUtf8(
        qrDe(kPortalB, kClinicaB, QStringLiteral("Sur"), kTokenB))));
    movil->confirmarEmparejamiento();

    saludA_ = QStringLiteral("sin_acceso");
    movil->sincronizar();

    const QVariantList portales = movil->portales();
    QCOMPARE(portales.size(), 2);
    for (const QVariant& portal : portales) {
        const QVariantMap mapa = portal.toMap();
        if (mapa.value(QStringLiteral("uuid")).toString() == kPortalA) {
            QCOMPARE(mapa.value(QStringLiteral("estado")).toString(),
                     QStringLiteral("revocado"));
        } else {
            QCOMPARE(mapa.value(QStringLiteral("estado")).toString(),
                     QStringLiteral("activo"));
        }
    }
    // El portal B sigue sirviendo datos. A conserva su caché (para poder
    // re-emparejar), pero marcada como revocada.
    movil->seleccionarPortal(kPortalB);
    const QVariantList mascotas = movil->mascotas();
    QCOMPARE(mascotas.size(), 2);
    bool rockyActivo = false;
    bool lunaRevocada = false;
    for (const QVariant& mascota : mascotas) {
        const QVariantMap mapa = mascota.toMap();
        const QString nombre = mapa.value(QStringLiteral("nombre")).toString();
        if (nombre == QStringLiteral("Rocky")) {
            rockyActivo = mapa.value(QStringLiteral("portal_uuid")).toString() == kPortalB
                && mapa.value(QStringLiteral("portal_estado")).toString()
                    == QStringLiteral("activo");
        }
        if (nombre == QStringLiteral("Luna")) {
            lunaRevocada = mapa.value(QStringLiteral("portal_estado")).toString()
                == QStringLiteral("revocado");
        }
    }
    QVERIFY(rockyActivo);
    QVERIFY(lunaRevocada);
}

void MobileTest::cerrarSesionBorraTodo()
{
    MobileController* movil = control();
    movil->configurarClave(QStringLiteral("Aurora2026!"));
    QVERIFY(movil->leerQr(QString::fromUtf8(
        qrDe(kPortalA, kClinicaA, QStringLiteral("Norte"), kTokenA))));
    movil->confirmarEmparejamiento();
    QCOMPARE(movil->portales().size(), 1);

    movil->cerrarSesion();
    QVERIFY(!movil->configurada());
    QVERIFY(!movil->desbloqueada());
    QCOMPARE(movil->portales().size(), 0);
    QCOMPARE(movil->mascotas().size(), 0);
}

void MobileTest::mascotaAjenaNoSePuedeAgendar()
{
    MobileController* movil = control();
    movil->configurarClave(QStringLiteral("Aurora2026!"));
    QVERIFY(movil->leerQr(QString::fromUtf8(
        qrDe(kPortalA, kClinicaA, QStringLiteral("Norte"), kTokenA))));
    movil->confirmarEmparejamiento();
    QVERIFY(movil->leerQr(QString::fromUtf8(
        qrDe(kPortalB, kClinicaB, QStringLiteral("Sur"), kTokenB))));
    movil->confirmarEmparejamiento();
    movil->seleccionarPortal(kPortalA);

    movil->solicitarCita(QStringLiteral("mascota-b"), QStringLiteral("Consulta"),
                         QStringLiteral("2026-11-02"), QStringLiteral("09:00"), {});
    QVERIFY(movil->error().contains(QStringLiteral("no pertenece")));
    QCOMPARE(movil->pendientes(), 0);
    QCOMPARE(movil->solicitudes().size(), 0);
}

QTEST_GUILESS_MAIN(MobileTest)

#include "tst_mobile.moc"
