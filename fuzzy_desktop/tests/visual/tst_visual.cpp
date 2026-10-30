#include "agenda/AgendaController.h"
#include "ajustes/AjustesController.h"
#include "common/Consulta.h"
#include "facturacion/FacturacionController.h"
#include "fuzzy/core/AuthService.h"
#include "fuzzy/core/MigrationRunner.h"
#include "fuzzy/core/PasswordHasher.h"
#include "fuzzy/core/PortalBaseStore.h"
#include "fuzzy/core/PortalProvisioner.h"
#include "fuzzy/core/PortalTokenService.h"
#include "fuzzy/core/SqliteExecutor.h"
#include "fuzzy/core/Uuid.h"
#include "inventario/InventarioController.h"
#include "pacientes/PacientesController.h"
#include "panel/PanelController.h"
#include "propietarios/PropietariosController.h"
#include "session/SessionController.h"
#include "style/Style.h"
#include "vinculos/VinculosController.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

#include <functional>
#include <memory>
#include <vector>

namespace {

constexpr int kWidth = 1440;
constexpr int kHeight = 900;
constexpr double kMeanThreshold = 2.5;
constexpr double kBlockThreshold = 24.0;

// Fecha fija de las capturas. `FUZZY_HOY` hace que todos los controladores
// calculen la agenda, la caja y el panel contra este día.
const QDate kHoy(2026, 3, 9);

QString u(const QString& semilla)
{
    return fuzzy::core::uuidDeterminista(semilla);
}

qint64 msEn(const QDate& fecha, int hora, int minuto)
{
    return QDateTime(fecha, QTime(hora, minuto)).toMSecsSinceEpoch();
}

QImage normalizada(const QImage& source)
{
    return source.convertToFormat(QImage::Format_Grayscale8)
        .scaled(192, 120, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

double diferenciaMedia(const QImage& a, const QImage& b)
{
    double total = 0.0;
    for (int y = 0; y < a.height(); ++y) {
        const uchar* pa = a.constScanLine(y);
        const uchar* pb = b.constScanLine(y);
        for (int x = 0; x < a.width(); ++x)
            total += qAbs(int(pa[x]) - int(pb[x]));
    }
    return total / (a.width() * a.height());
}

double diferenciaMaximaPorBloque(const QImage& a, const QImage& b)
{
    double maximo = 0.0;
    constexpr int bloque = 8;
    for (int by = 0; by < a.height(); by += bloque) {
        for (int bx = 0; bx < a.width(); bx += bloque) {
            double suma = 0.0;
            int cuenta = 0;
            const int finY = qMin(by + bloque, a.height());
            const int finX = qMin(bx + bloque, a.width());
            for (int y = by; y < finY; ++y) {
                for (int x = bx; x < finX; ++x) {
                    suma += qAbs(int(a.pixel(x, y) & 0xff) - int(b.pixel(x, y) & 0xff));
                    ++cuenta;
                }
            }
            maximo = qMax(maximo, suma / cuenta);
        }
    }
    return maximo;
}

QString directorioArtefactos()
{
    const QString dir = qEnvironmentVariable(
        "FUZZY_VISUAL_ARTIFACT_DIR",
        QDir::tempPath() + QStringLiteral("/fuzzy-desktop-visual"));
    QDir().mkpath(dir);
    return dir;
}

qint64 insertar(fuzzy::core::IQueryExecutor& db, const QString& sql,
                const std::vector<QVariant>& params = {})
{
    const auto resultado = db.execute(sql, params);
    if (!resultado.ok) {
        qFatal("SQL falló: %s", qPrintable(sql));
    }
    return resultado.last_insert_id.toLongLong();
}

void ejecutar(fuzzy::core::IQueryExecutor& db, const QString& sql,
              const std::vector<QVariant>& params = {})
{
    if (!db.execute(sql, params).ok) {
        qFatal("SQL falló: %s", qPrintable(sql));
    }
}

qint64 idDe(fuzzy::core::IQueryExecutor& db, const QString& tabla, const QString& uuid)
{
    const auto filas = db.query(
        QStringLiteral("SELECT id FROM %1 WHERE uuid = ?").arg(tabla),
        {QVariant(uuid)});
    return filas.empty() ? 0 : filas.front().values.front().toLongLong();
}

struct DatosDemo
{
    QString adminUuid;
    QString passwordTemporalVet;
    QString clienteCamila;
    QString mascotaLuna;
};

// Crea una instalación realista usando los mismos servicios que la app. No hay
// mapas falsos: las capturas ejercitan los controladores contra SQLite.
DatosDemo sembrarDemo(fuzzy::core::AuthService& auth, fuzzy::core::IQueryExecutor& db)
{
    DatosDemo demo;
    const auto alta = auth.bootstrap(QStringLiteral("Clínica Veterinaria Aurora"),
                                     QStringLiteral("Medellín"),
                                     QStringLiteral("Dra. Elena Ruiz"),
                                     QStringLiteral("elena@aurora.test"),
                                     QStringLiteral("Aurora2026!"));
    if (!alta.ok) {
        qFatal("No se pudo crear la instalación de prueba: %s",
               qPrintable(alta.error));
    }
    demo.adminUuid = alta.usuario_uuid;

    // El alta solo pide nombre y ciudad; el resto de la ficha se completa
    // luego desde Ajustes.
    ejecutar(db,
             QStringLiteral("UPDATE clinicas SET telefono = ?, direccion = ?"),
             {QVariant(QStringLiteral("+57 604 555 0123")),
              QVariant(QStringLiteral("Cra 43A #1-50, El Poblado"))});

    const auto marco = auth.crearUsuario(demo.adminUuid, QStringLiteral("Dr. Marco Soler"),
                                         QStringLiteral("marco@aurora.test"),
                                         QStringLiteral("Veterinario"));
    if (!marco.ok) {
        qFatal("No se pudo crear el veterinario: %s", qPrintable(marco.error));
    }
    demo.passwordTemporalVet = marco.passwordTemporal;

    auth.crearUsuario(demo.adminUuid, QStringLiteral("Lucía Prado"),
                      QStringLiteral("lucia@aurora.test"), QStringLiteral("Recepcion"));
    auth.crearUsuario(demo.adminUuid, QStringLiteral("Nadia Ferrer"),
                      QStringLiteral("nadia@aurora.test"), QStringLiteral("Estilista"));

    // ── Propietarios ─────────────────────────────────────────
    struct Cliente
    {
        const char* nombre;
        const char* telefono;
        const char* correo;
        const char* direccion;
        const char* dni;
    };
    const Cliente clientes[] = {
        {"Camila Restrepo", "+57 300 123 4567", "camila.restrepo@correo.test",
         "Cra 43 #12-20, Medellín", "1020304050"},
        {"Andrés Salazar", "+57 301 555 8899", "andres.salazar@correo.test",
         "Cl 10 #35-11, Envigado", "1020304051"},
        {"Valentina Gómez", "+57 315 222 3344", "valentina.gomez@correo.test",
         "Cra 70 #44-18, Medellín", "1020304052"},
        {"Julián Ospina", "+57 310 888 1122", "julian.ospina@correo.test",
         "Cl 30 #65-40, Itagüí", "1020304053"},
        {"Mariana Torres", "+57 304 777 5566", "mariana.torres@correo.test",
         "Cra 48 #20-30, Medellín", "1020304054"},
        {"Esteban Ríos", "+57 312 444 9900", "esteban.rios@correo.test",
         "Cl 50 #40-12, Bello", "1020304055"},
    };

    for (int i = 0; i < 6; ++i) {
        insertar(db,
                 QStringLiteral("INSERT INTO clientes "
                                "(uuid, nombre_completo, telefono, correo, direccion, dni, "
                                " created_at, updated_at, sync_state) "
                                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 'pendiente')"),
                 {QVariant(u(QStringLiteral("cliente-%1").arg(i))),
                  QVariant(QString::fromUtf8(clientes[i].nombre)),
                  QVariant(QString::fromUtf8(clientes[i].telefono)),
                  QVariant(QString::fromUtf8(clientes[i].correo)),
                  QVariant(QString::fromUtf8(clientes[i].direccion)),
                  QVariant(QString::fromUtf8(clientes[i].dni)),
                  QVariant::fromValue(msEn(kHoy.addDays(-180), 9, 0)),
                  QVariant::fromValue(msEn(kHoy.addDays(-180), 9, 0))});
    }

    struct Mascota
    {
        const char* nombre;
        int cliente;
        const char* especie;
        const char* raza;
        const char* sexo;
        double peso;
        int castrado;
        const char* nacimiento;
        const char* chip;
        const char* alergias;
        const char* atencion;
    };
    const Mascota mascotas[] = {
        {"Luna", 0, "Perro", "Golden Retriever", "Hembra", 24.5, 1,
         "2021-04-12", "CHIP-0001", "Sensibilidad a la penicilina",
         "Cepillado diario"},
        {"Milo", 0, "Gato", "Mestizo", "Macho", 4.2, 1, "2022-09-03", "CHIP-0002",
         "", ""},
        {"Rocky", 1, "Perro", "Bulldog Francés", "Macho", 12.8, 0, "2020-01-22",
         "CHIP-0003", "Dermatitis atópica", "Evitar el calor"},
        {"Nala", 1, "Gato", "Siamés", "Hembra", 3.6, 1, "2023-02-14", "CHIP-0004",
         "", ""},
        {"Kira", 2, "Perro", "Border Collie", "Hembra", 18.3, 1, "2019-06-30",
         "CHIP-0005", "", "Alta energía"},
        {"Simba", 3, "Gato", "Persa", "Macho", 5.1, 1, "2018-11-08", "CHIP-0006",
         "Lagrimeo crónico", ""},
        {"Toby", 4, "Perro", "Beagle", "Macho", 11.0, 0, "2021-08-19", "CHIP-0007",
         "", ""},
        {"Coco", 4, "Conejo", "Enano", "Hembra", 1.6, 0, "2023-12-01", "CHIP-0008",
         "", ""},
        {"Aurora", 5, "Gato", "Mestizo", "Hembra", 3.9, 1, "2022-05-05", "CHIP-0009",
         "", ""},
    };

    for (int i = 0; i < 9; ++i) {
        insertar(db,
                 QStringLiteral("INSERT INTO mascotas "
                                "(uuid, cliente_id, nombre, especie, raza, sexo, peso_kg, "
                                " numero_chip, fecha_nacimiento, activo, castrado, alergias, "
                                " atencion_especial, created_at, updated_at, sync_state) "
                                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 1, ?, ?, ?, ?, ?, "
                                "        'pendiente')"),
                 {QVariant(u(QStringLiteral("mascota-%1").arg(i))),
                  QVariant::fromValue(idDe(db, QStringLiteral("clientes"),
                                           u(QStringLiteral("cliente-%1")
                                                 .arg(mascotas[i].cliente)))),
                  QVariant(QString::fromUtf8(mascotas[i].nombre)),
                  QVariant(QString::fromUtf8(mascotas[i].especie)),
                  QVariant(QString::fromUtf8(mascotas[i].raza)),
                  QVariant(QString::fromUtf8(mascotas[i].sexo)),
                  QVariant(mascotas[i].peso),
                  QVariant(QString::fromUtf8(mascotas[i].chip)),
                  QVariant(QString::fromUtf8(mascotas[i].nacimiento)),
                  QVariant::fromValue(mascotas[i].castrado),
                  QVariant(QString::fromUtf8(mascotas[i].alergias)),
                  QVariant(QString::fromUtf8(mascotas[i].atencion)),
                  QVariant::fromValue(msEn(kHoy.addDays(-120), 10, 0)),
                  QVariant::fromValue(msEn(kHoy.addDays(-120), 10, 0))});
    }

    demo.clienteCamila = u(QStringLiteral("cliente-0"));
    demo.mascotaLuna = u(QStringLiteral("mascota-0"));

    const qint64 idLuna = idDe(db, QStringLiteral("mascotas"), demo.mascotaLuna);
    const qint64 idRocky = idDe(db, QStringLiteral("mascotas"),
                                u(QStringLiteral("mascota-2")));

    // ── Historia clínica de Luna ─────────────────────────────
    const qint64 consulta1 = insertar(
        db,
        QStringLiteral("INSERT INTO consultas_medicas "
                       "(uuid, mascota_id, fecha_consulta, motivo_visita, es_emergencia, "
                       " diagnostico, tratamiento_indicado, anamnesis, examen_fisico, "
                       " estado, created_at, updated_at, sync_state) "
                       "VALUES (?, ?, ?, ?, 0, ?, ?, ?, ?, 'Cerrada', ?, ?, 'pendiente')"),
        {QVariant(u(QStringLiteral("consulta-1"))), QVariant::fromValue(idLuna),
         QVariant::fromValue(msEn(kHoy.addDays(-10), 16, 20)),
         QVariant(QStringLiteral("Control anual y vacunación")),
         QVariant(QStringLiteral("Paciente sana. Se refuerza calendario vacunal.")),
         QVariant(QStringLiteral("Vacuna polivalente y desparasitación interna.")),
         QVariant(QStringLiteral("Sin signos de dolor. Apetito y actividad normales.")),
         QVariant(QStringLiteral("Peso 24.5 kg. Mucosas rosadas. Auscultación limpia.")),
         QVariant::fromValue(msEn(kHoy.addDays(-10), 16, 25)),
         QVariant::fromValue(msEn(kHoy.addDays(-10), 16, 25))});

    insertar(db,
             QStringLiteral("INSERT INTO consultas_medicas "
                            "(uuid, mascota_id, fecha_consulta, motivo_visita, es_emergencia, "
                            " diagnostico, tratamiento_indicado, anamnesis, examen_fisico, "
                            " estado, created_at, updated_at, sync_state) "
                            "VALUES (?, ?, ?, ?, 0, ?, ?, ?, ?, 'Cerrada', ?, ?, 'pendiente')"),
             {QVariant(u(QStringLiteral("consulta-2"))), QVariant::fromValue(idLuna),
              QVariant::fromValue(msEn(kHoy.addDays(-45), 11, 40)),
              QVariant(QStringLiteral("Cojera intermitente")),
              QVariant(QStringLiteral("Displasia leve de cadera.")),
              QVariant(QStringLiteral("Condroprotector y control de peso.")),
              QVariant(QStringLiteral("Cojera al levantarse, mejora con el movimiento.")),
              QVariant(QStringLiteral("Dolor a la extensión de cadera derecha.")),
              QVariant::fromValue(msEn(kHoy.addDays(-45), 11, 55)),
              QVariant::fromValue(msEn(kHoy.addDays(-45), 11, 55))});

    insertar(db,
             QStringLiteral("INSERT INTO consultas_medicas "
                            "(uuid, mascota_id, fecha_consulta, motivo_visita, es_emergencia, "
                            " diagnostico, tratamiento_indicado, estado, created_at, "
                            " updated_at, sync_state) "
                            "VALUES (?, ?, ?, ?, 1, ?, ?, 'Cerrada', ?, ?, 'pendiente')"),
             {QVariant(u(QStringLiteral("consulta-3"))), QVariant::fromValue(idLuna),
              QVariant::fromValue(msEn(kHoy.addDays(-90), 20, 10)),
              QVariant(QStringLiteral("Ingestión de cuerpo extraño")),
              QVariant(QStringLiteral("Gastritis aguda. Sin obstrucción.")),
              QVariant(QStringLiteral("Ayuno, antiemético y control a las 24 h.")),
              QVariant::fromValue(msEn(kHoy.addDays(-90), 20, 20)),
              QVariant::fromValue(msEn(kHoy.addDays(-90), 20, 20))});

    // Las consultas quedan firmadas por el veterinario del equipo.
    ejecutar(db,
             QStringLiteral("UPDATE consultas_medicas SET veterinario_id = ?"),
             {QVariant::fromValue(
                 idDe(db, QStringLiteral("usuarios"), marco.uuid))});

    insertar(db,
             QStringLiteral("INSERT INTO signos_vitales "
                            "(uuid, mascota_id, consulta_id, fecha, peso_kg, temperatura_c, "
                            " frecuencia_cardiaca, frecuencia_respiratoria, created_at, "
                            " updated_at, sync_state) "
                            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'pendiente')"),
             {QVariant(u(QStringLiteral("signo-1"))), QVariant::fromValue(idLuna),
              QVariant::fromValue(consulta1),
              QVariant::fromValue(msEn(kHoy.addDays(-10), 16, 22)), QVariant(24.5),
              QVariant(38.4), QVariant::fromValue(84), QVariant::fromValue(22),
              QVariant::fromValue(msEn(kHoy.addDays(-10), 16, 22)),
              QVariant::fromValue(msEn(kHoy.addDays(-10), 16, 22))});

    insertar(db,
             QStringLiteral("INSERT INTO signos_vitales "
                            "(uuid, mascota_id, consulta_id, fecha, peso_kg, temperatura_c, "
                            " frecuencia_cardiaca, frecuencia_respiratoria, created_at, "
                            " updated_at, sync_state) "
                            "VALUES (?, ?, NULL, ?, ?, ?, ?, ?, ?, ?, 'pendiente')"),
             {QVariant(u(QStringLiteral("signo-2"))), QVariant::fromValue(idLuna),
              QVariant::fromValue(msEn(kHoy.addDays(-45), 11, 42)), QVariant(25.2),
              QVariant(38.7), QVariant::fromValue(90), QVariant::fromValue(24),
              QVariant::fromValue(msEn(kHoy.addDays(-45), 11, 42)),
              QVariant::fromValue(msEn(kHoy.addDays(-45), 11, 42))});

    insertar(db,
             QStringLiteral("INSERT INTO vacunas "
                            "(uuid, mascota_id, nombre_vacuna, fecha_aplicacion, proxima_dosis, "
                            " lote, notas, created_at, updated_at, sync_state) "
                            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 'pendiente')"),
             {QVariant(u(QStringLiteral("vacuna-1"))), QVariant::fromValue(idLuna),
              QVariant(QStringLiteral("Polivalente canina")),
              QVariant(kHoy.addDays(-10).toString(QStringLiteral("yyyy-MM-dd"))),
              QVariant(kHoy.addDays(355).toString(QStringLiteral("yyyy-MM-dd"))),
              QVariant(QStringLiteral("L-2026-041")),
              QVariant(QStringLiteral("Sin reacción adversa.")),
              QVariant::fromValue(msEn(kHoy.addDays(-10), 16, 24)),
              QVariant::fromValue(msEn(kHoy.addDays(-10), 16, 24))});

    insertar(db,
             QStringLiteral("INSERT INTO vacunas "
                            "(uuid, mascota_id, nombre_vacuna, fecha_aplicacion, proxima_dosis, "
                            " lote, notas, created_at, updated_at, sync_state) "
                            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 'pendiente')"),
             {QVariant(u(QStringLiteral("vacuna-2"))), QVariant::fromValue(idLuna),
              QVariant(QStringLiteral("Antirrábica")),
              QVariant(kHoy.addDays(-10).toString(QStringLiteral("yyyy-MM-dd"))),
              QVariant(kHoy.addDays(355).toString(QStringLiteral("yyyy-MM-dd"))),
              QVariant(QStringLiteral("R-2026-017")), QVariant(QString()),
              QVariant::fromValue(msEn(kHoy.addDays(-10), 16, 26)),
              QVariant::fromValue(msEn(kHoy.addDays(-10), 16, 26))});

    // ── Inventario ───────────────────────────────────────────
    struct Producto
    {
        const char* nombre;
        const char* principio;
        int categoria;
        const char* codigo;
        int stock;
        int minimo;
        double compra;
        double venta;
        const char* caducidad;
        const char* proveedor;
    };
    const Producto productos[] = {
        {"Amoxicilina 250 mg", "Amoxicilina trihidrato", 1, "MED-001", 4, 10, 12000,
         22000, "2027-05-30", "Distribuciones VetAndes"},
        {"Meloxicam 2 mg/ml", "Meloxicam", 1, "MED-002", 18, 8, 28000, 46000,
         "2027-01-15", "Distribuciones VetAndes"},
        {"Vacuna antirrábica", "Virus inactivado", 2, "VAC-001", 6, 12, 18000, 32000,
         "2026-09-30", "Biológicos del Sur"},
        {"Vacuna quíntuple", "Polivalente canina", 2, "VAC-002", 25, 10, 24000, 41000,
         "2026-11-20", "Biológicos del Sur"},
        {"Suero fisiológico 500 ml", "Cloruro de sodio 0.9%", 3, "INS-001", 40, 15,
         4500, 9000, "2028-02-28", "Insumos Médicos Antioquia"},
        {"Alimento renal gato 2 kg", "Dieta renal felina", 4, "ALI-001", 3, 6, 62000,
         98000, "2026-04-05", "Nutrición Animal"},
        {"Shampoo medicado", "Clorhexidina 2%", 5, "OTR-001", 12, 4, 21000, 36000,
         "2027-07-01", "PetCare Colombia"},
        {"Gasas estériles 10x10", "Algodón estéril", 3, "INS-002", 80, 20, 800, 1800,
         "2029-01-01", "Insumos Médicos Antioquia"},
    };

    for (int i = 0; i < 8; ++i) {
        insertar(db,
                 QStringLiteral("INSERT INTO productos_insumos "
                                "(uuid, nombre_comercial, principio_activo, categoria_id, "
                                " codigo, unidad, stock_actual, stock_minimo, precio_compra, "
                                " precio_venta, fecha_caducidad, proveedor_nombre, "
                                " created_at, updated_at, sync_state) "
                                "VALUES (?, ?, ?, ?, ?, 'unidades', ?, ?, ?, ?, ?, ?, ?, ?, "
                                "        'pendiente')"),
                 {QVariant(u(QStringLiteral("producto-%1").arg(i))),
                  QVariant(QString::fromUtf8(productos[i].nombre)),
                  QVariant(QString::fromUtf8(productos[i].principio)),
                  QVariant::fromValue(productos[i].categoria),
                  QVariant(QString::fromUtf8(productos[i].codigo)),
                  QVariant::fromValue(productos[i].stock),
                  QVariant::fromValue(productos[i].minimo),
                  QVariant(productos[i].compra), QVariant(productos[i].venta),
                  QVariant(QString::fromUtf8(productos[i].caducidad)),
                  QVariant(QString::fromUtf8(productos[i].proveedor)),
                  QVariant::fromValue(msEn(kHoy.addDays(-60), 8, 0)),
                  QVariant::fromValue(msEn(kHoy.addDays(-60), 8, 0))});
    }

    // ── Servicios ────────────────────────────────────────────
    struct Servicio
    {
        const char* codigo;
        const char* nombre;
        double precio;
    };
    const Servicio servicios[] = {
        {"CON-001", "Consulta general", 85000},
        {"VAC-001", "Vacunación", 45000},
        {"EST-001", "Baño y corte", 60000},
        {"LAB-001", "Hemograma completo", 120000},
        {"CIR-001", "Esterilización", 450000},
    };
    for (int i = 0; i < 5; ++i) {
        insertar(db,
                 QStringLiteral("INSERT INTO servicios "
                                "(uuid, codigo, nombre, precio, activo, created_at, "
                                " updated_at, sync_state) "
                                "VALUES (?, ?, ?, ?, 1, ?, ?, 'pendiente')"),
                 {QVariant(u(QStringLiteral("servicio-%1").arg(i))),
                  QVariant(QString::fromUtf8(servicios[i].codigo)),
                  QVariant(QString::fromUtf8(servicios[i].nombre)),
                  QVariant(servicios[i].precio),
                  QVariant::fromValue(msEn(kHoy.addDays(-200), 9, 0)),
                  QVariant::fromValue(msEn(kHoy.addDays(-200), 9, 0))});
    }

    // ── Citas de la semana ───────────────────────────────────
    struct Cita
    {
        int mascota;
        int tipo;
        int estado;
        int diaOffset;
        int hora;
        int minuto;
        const char* notas;
    };
    // kHoy es lunes; la vista semanal abarca de lunes a domingo.
    const Cita citas[] = {
        {0, 1, 6, 0, 8, 30, "Control postoperatorio"},
        {2, 1, 5, 0, 9, 15, "Revisión dermatológica"},
        {4, 2, 1, 0, 10, 0, "Baño medicado"},
        {5, 1, 4, 0, 11, 30, "Vacunación anual"},
        {6, 1, 5, 0, 12, 0, "Cojera leve"},
        {1, 1, 1, 0, 15, 0, "Primera visita"},
        {3, 1, 1, 1, 9, 0, "Control de peso"},
        {7, 1, 1, 1, 11, 0, "Revisión dental"},
        {0, 2, 1, 2, 10, 30, "Baño y corte"},
        {8, 1, 1, 2, 14, 0, "Vacunación"},
        {2, 1, 1, 3, 9, 30, "Seguimiento dermatológico"},
        {5, 1, 1, 3, 16, 0, "Control geriátrico"},
        {4, 3, 1, 4, 10, 0, "Consulta a domicilio"},
        {1, 1, 1, 4, 12, 30, "Esterilización"},
        {6, 2, 1, 5, 9, 0, "Baño de mantenimiento"},
        {3, 1, 1, 5, 11, 30, "Vacunación"},
    };

    for (int i = 0; i < 16; ++i) {
        const QDate fecha = kHoy.addDays(citas[i].diaOffset);
        insertar(db,
                 QStringLiteral("INSERT INTO citas "
                                "(uuid, mascota_id, tipo_servicio_id, estado_cita_id, "
                                " fecha_hora_programada, prioridad, es_emergencia, origen, "
                                " notas_cliente, fecha_creacion, fecha_actualizacion, "
                                " created_at, updated_at, sync_state) "
                                "VALUES (?, ?, ?, ?, ?, 'Normal', 0, 'mostrador', ?, ?, ?, ?, ?, "
                                "        'pendiente')"),
                 {QVariant(u(QStringLiteral("cita-%1").arg(i))),
                  QVariant::fromValue(idDe(db, QStringLiteral("mascotas"),
                                           u(QStringLiteral("mascota-%1")
                                                 .arg(citas[i].mascota)))),
                  QVariant::fromValue(citas[i].tipo),
                  QVariant::fromValue(citas[i].estado),
                  QVariant::fromValue(msEn(fecha, citas[i].hora, citas[i].minuto)),
                  QVariant(QString::fromUtf8(citas[i].notas)),
                  QVariant::fromValue(msEn(kHoy.addDays(-3), 9, 0)),
                  QVariant::fromValue(msEn(kHoy.addDays(-3), 9, 0)),
                  QVariant::fromValue(msEn(kHoy.addDays(-3), 9, 0)),
                  QVariant::fromValue(msEn(kHoy.addDays(-3), 9, 0))});
    }

    // Rocky es el paciente de la consulta de hoy: mantiene la sala de espera.
    Q_UNUSED(idRocky);

    // ── Facturación ──────────────────────────────────────────
    struct Factura
    {
        int cliente;
        const char* numero;
        int estado; // 0 pagada hoy, 1 emitida hoy, 2 pagada anterior, 3 pendiente
        double subtotal;
        double total;
        const char* metodo;
        const char* detalle;
    };
    const Factura facturas[] = {
        {0, "FV-2026-0041", 0, 85000, 85000, "Efectivo", "Consulta general"},
        {1, "FV-2026-0042", 0, 45000, 45000, "Tarjeta", "Vacunación"},
        {2, "FV-2026-0043", 1, 120000, 120000, "Transferencia", "Hemograma completo"},
        {4, "FV-2026-0040", 2, 60000, 60000, "Efectivo", "Baño y corte"},
        {3, "FV-2026-0044", 3, 450000, 450000, "Credito", "Esterilización"},
    };

    for (int i = 0; i < 5; ++i) {
        const Factura& f = facturas[i];
        const bool hoy = f.estado != 2;
        const qint64 emision = msEn(hoy ? kHoy : kHoy.addDays(-2), 10 + i % 4, 15);
        const QString estado = f.estado == 1 ? QStringLiteral("Emitida")
                               : f.estado == 3 ? QStringLiteral("Pendiente")
                                               : QStringLiteral("Pagada");
        const qint64 facturaId = insertar(
            db,
            QStringLiteral("INSERT INTO facturas "
                           "(uuid, cliente_id, numero_factura, fecha_emision, subtotal, "
                           " impuesto, descuento, monto_total, metodo_pago, estado, "
                           " created_at, updated_at, sync_state) "
                           "VALUES (?, ?, ?, ?, ?, 0, 0, ?, ?, ?, ?, ?, 'pendiente')"),
            {QVariant(u(QStringLiteral("factura-%1").arg(i))),
             QVariant::fromValue(idDe(db, QStringLiteral("clientes"),
                                      u(QStringLiteral("cliente-%1").arg(f.cliente)))),
             QVariant(QString::fromUtf8(f.numero)),
             QVariant::fromValue(emision), QVariant(f.subtotal), QVariant(f.total),
             QVariant(QString::fromUtf8(f.metodo)), QVariant(estado),
             QVariant::fromValue(emision), QVariant::fromValue(emision)});

        insertar(db,
                 QStringLiteral("INSERT INTO detalles_factura "
                                "(uuid, factura_id, codigo, nombre, tipo, cantidad, "
                                " precio_unitario, created_at, updated_at, sync_state) "
                                "VALUES (?, ?, ?, ?, 'Servicio', 1, ?, ?, ?, 'pendiente')"),
                 {QVariant(u(QStringLiteral("detalle-factura-%1").arg(i))),
                  QVariant::fromValue(facturaId),
                  QVariant(QStringLiteral("SRV-001")),
                  QVariant(QString::fromUtf8(f.detalle)),
                  QVariant(f.total), QVariant::fromValue(emision),
                  QVariant::fromValue(emision)});
    }

    // ── Vínculos con el portal ───────────────────────────────
    const qint64 clienteCamilaId = idDe(db, QStringLiteral("clientes"), demo.clienteCamila);
    const qint64 cuentaId = insertar(
        db,
        QStringLiteral("INSERT INTO cuentas "
                       "(uuid, cliente_id, correo, password_hash, estado, activo, "
                       " debe_cambiar_password, verificado_at, created_at, updated_at, "
                       " sync_state) "
                       "VALUES (?, ?, ?, ?, 'Activa', 1, 0, ?, ?, ?, 'pendiente')"),
        {QVariant(u(QStringLiteral("cuenta-0"))), QVariant::fromValue(clienteCamilaId),
         QVariant(QStringLiteral("camila.restrepo@correo.test")),
         QVariant(fuzzy::core::PasswordHasher::generar(QStringLiteral("Portal2026!"))),
         QVariant::fromValue(msEn(kHoy.addDays(-30), 12, 0)),
         QVariant::fromValue(msEn(kHoy.addDays(-30), 12, 0)),
         QVariant::fromValue(msEn(kHoy.addDays(-30), 12, 0))});

    insertar(db,
             QStringLiteral("INSERT INTO portales "
                            "(uuid, cuenta_id, cliente_id, url, estado, pareado_at, "
                            " ultima_sync_at, created_at, updated_at) "
                            "VALUES (?, ?, ?, ?, 'activo', ?, ?, ?, ?)"),
             {QVariant(u(QStringLiteral("portal-0"))), QVariant::fromValue(cuentaId),
              QVariant::fromValue(clienteCamilaId),
              QVariant(QStringLiteral("https://aurora.fuzzy.app/p/portal-0")),
              QVariant::fromValue(msEn(kHoy.addDays(-20), 18, 40)),
              QVariant::fromValue(msEn(kHoy.addDays(-1), 21, 10)),
              QVariant::fromValue(msEn(kHoy.addDays(-30), 12, 0)),
              QVariant::fromValue(msEn(kHoy.addDays(-1), 21, 10))});

    return demo;
}

enum class Escenario
{
    Bootstrap,
    Login,
    CambioPassword,
    Demo,
};

struct DatosDemo;

class Montaje
{
public:
    explicit Montaje(Escenario escenario)
        : escenario_(escenario),
          conexion_(QStringLiteral("visual_%1").arg(++sConexiones))
    {
        QDir().mkpath(dir_.path());
        db_ = std::make_unique<fuzzy::core::SqliteExecutor>(
            conexion_, dir_.filePath(QStringLiteral("clinica.db")));

        const auto migraciones = fuzzy::core::MigrationRunner::clinic();
        if (!migraciones.valido() || !migraciones.aplicar(*db_)) {
            qFatal("No se pudo aplicar el esquema de la clínica");
        }

        auth_ = std::make_unique<fuzzy::core::AuthService>(*db_);
        if (escenario != Escenario::Bootstrap) {
            demo_ = sembrarDemo(*auth_, *db_);
        }

        const QString portales = dir_.filePath(QStringLiteral("portales"));
        QDir().mkpath(portales);
        bases_ = std::make_unique<fuzzy::core::LocalPortalBaseStore>(portales);
        tokens_ = std::make_unique<fuzzy::core::PortalTokenService>(*db_, bases_.get());
        provisioner_ = std::make_unique<fuzzy::core::PortalProvisioner>(
            *db_, *tokens_, *bases_);

        estilo_ = std::make_unique<fuzzy::desktop::Style>();

        sesion_ = std::make_unique<fuzzy::desktop::SessionController>(*auth_);
        panel_ = std::make_unique<fuzzy::desktop::PanelController>(*db_);
        agenda_ = std::make_unique<fuzzy::desktop::AgendaController>(*db_);
        pacientes_ = std::make_unique<fuzzy::desktop::PacientesController>(*db_);
        propietarios_ = std::make_unique<fuzzy::desktop::PropietariosController>(*db_);
        inventario_ = std::make_unique<fuzzy::desktop::InventarioController>(*db_);
        facturacion_ = std::make_unique<fuzzy::desktop::FacturacionController>(*db_);
        ajustes_ = std::make_unique<fuzzy::desktop::AjustesController>(*auth_);
        vinculos_ = std::make_unique<fuzzy::desktop::VinculosController>(
            *db_, *provisioner_, *bases_);

        if (escenario == Escenario::Demo) {
            sesion_->iniciarSesion(QStringLiteral("elena@aurora.test"),
                                   QStringLiteral("Aurora2026!"));
            facturacion_->setUsuarioUuid(sesion_->usuarioUuid());
            ajustes_->setAdminUuid(sesion_->usuarioUuid());
        } else if (escenario == Escenario::CambioPassword) {
            sesion_->iniciarSesion(QStringLiteral("marco@aurora.test"),
                                   demo_.passwordTemporalVet);
            facturacion_->setUsuarioUuid(sesion_->usuarioUuid());
            ajustes_->setAdminUuid(sesion_->usuarioUuid());
        }

        engine_ = std::make_unique<QQmlApplicationEngine>();
        QQmlContext* contexto = engine_->rootContext();
        contexto->setContextProperty(QStringLiteral("estilo"), estilo_.get());
        contexto->setContextProperty(QStringLiteral("sesion"), sesion_.get());
        contexto->setContextProperty(QStringLiteral("panel"), panel_.get());
        contexto->setContextProperty(QStringLiteral("agenda"), agenda_.get());
        contexto->setContextProperty(QStringLiteral("pacientes"), pacientes_.get());
        contexto->setContextProperty(QStringLiteral("propietarios"), propietarios_.get());
        contexto->setContextProperty(QStringLiteral("inventario"), inventario_.get());
        contexto->setContextProperty(QStringLiteral("facturacion"), facturacion_.get());
        contexto->setContextProperty(QStringLiteral("ajustes"), ajustes_.get());
        contexto->setContextProperty(QStringLiteral("vinculos"), vinculos_.get());

        engine_->setInitialProperties(
            {{QStringLiteral("appVersion"), QStringLiteral("0.1.0")}});
        engine_->loadFromModule(QStringLiteral("Fuzzy"), QStringLiteral("Main"));

        if (engine_->rootObjects().isEmpty()) {
            qFatal("No se pudo cargar Main.qml");
        }
        ventana_ = qobject_cast<QQuickWindow*>(engine_->rootObjects().constFirst());
        if (!ventana_) {
            qFatal("Main.qml no creó una ventana");
        }
        ventana_->show();
        QTest::qWait(900);
    }

    QQuickWindow* ventana() const { return ventana_; }
    const DatosDemo& demo() const { return demo_; }
    fuzzy::desktop::SessionController* sesion() const { return sesion_.get(); }
    fuzzy::desktop::AgendaController* agenda() const { return agenda_.get(); }
    fuzzy::desktop::PacientesController* pacientes() const { return pacientes_.get(); }
    fuzzy::desktop::FacturacionController* facturacion() const
    {
        return facturacion_.get();
    }

private:
    static int sConexiones;

    Escenario escenario_;
    QString conexion_;
    QTemporaryDir dir_;
    std::unique_ptr<fuzzy::core::SqliteExecutor> db_;
    std::unique_ptr<fuzzy::core::AuthService> auth_;
    std::unique_ptr<fuzzy::core::LocalPortalBaseStore> bases_;
    std::unique_ptr<fuzzy::core::PortalTokenService> tokens_;
    std::unique_ptr<fuzzy::core::PortalProvisioner> provisioner_;
    std::unique_ptr<fuzzy::desktop::Style> estilo_;
    std::unique_ptr<fuzzy::desktop::SessionController> sesion_;
    std::unique_ptr<fuzzy::desktop::PanelController> panel_;
    std::unique_ptr<fuzzy::desktop::AgendaController> agenda_;
    std::unique_ptr<fuzzy::desktop::PacientesController> pacientes_;
    std::unique_ptr<fuzzy::desktop::PropietariosController> propietarios_;
    std::unique_ptr<fuzzy::desktop::InventarioController> inventario_;
    std::unique_ptr<fuzzy::desktop::FacturacionController> facturacion_;
    std::unique_ptr<fuzzy::desktop::AjustesController> ajustes_;
    std::unique_ptr<fuzzy::desktop::VinculosController> vinculos_;
    std::unique_ptr<QQmlApplicationEngine> engine_;
    QQuickWindow* ventana_ = nullptr;
    DatosDemo demo_;
};

int Montaje::sConexiones = 0;

} // namespace

class VisualTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void accesoLogin();
    void accesoBootstrap();
    void accesoCambioPassword();
    void modulos();

private:
    bool comparar(const QImage& image, const QString& nombre);
    void esperar(int ms = 450) { QTest::qWait(ms); }

    QString m_baselineDir;
    bool m_actualizar = false;
};

void VisualTest::initTestCase()
{
    const int idFuente = QFontDatabase::addApplicationFont(
        QStringLiteral(":/fonts/DejaVuSans.ttf"));
    QVERIFY2(idFuente != -1, "No se pudo cargar la fuente de la interfaz");
    const QStringList familias = QFontDatabase::applicationFontFamilies(idFuente);
    QVERIFY2(!familias.isEmpty(), "La fuente de la interfaz no expone familias");
    QGuiApplication::setFont(QFont(familias.constFirst()));

    m_baselineDir = QStringLiteral(FUZZY_VISUAL_BASELINE_DIR);
    m_actualizar = qEnvironmentVariableIsSet("FUZZY_UPDATE_BASELINES");
    QVERIFY2(QDir(m_baselineDir).exists(), qPrintable(m_baselineDir));
}

bool VisualTest::comparar(const QImage& image, const QString& nombre)
{
    if (image.isNull()) {
        qCritical("%s: captura vacía", qPrintable(nombre));
        return false;
    }

    const QString rutaArtefacto = QDir(directorioArtefactos())
                                      .filePath(nombre + QStringLiteral(".png"));
    if (!image.save(rutaArtefacto)) {
        qCritical("%s: no se pudo guardar %s", qPrintable(nombre),
                  qPrintable(rutaArtefacto));
        return false;
    }

    const QString rutaBase = QDir(m_baselineDir)
                                 .filePath(nombre + QStringLiteral(".png"));
    if (m_actualizar) {
        if (!image.save(rutaBase)) {
            qCritical("%s: no se pudo guardar %s", qPrintable(nombre),
                      qPrintable(rutaBase));
            return false;
        }
        qInfo("Baseline actualizada: %s", qPrintable(rutaBase));
        return true;
    }

    QImage base(rutaBase);
    if (base.isNull()) {
        qCritical("Falta la baseline %s. Ejecuta con FUZZY_UPDATE_BASELINES=1.",
                  qPrintable(rutaBase));
        return false;
    }

    if (base.size() != image.size()) {
        qCritical("%s: tamaño %dx%d, baseline %dx%d", qPrintable(nombre),
                  image.width(), image.height(), base.width(), base.height());
        return false;
    }

    const QImage a = normalizada(base);
    const QImage b = normalizada(image);
    const double media = diferenciaMedia(a, b);
    const double bloque = diferenciaMaximaPorBloque(a, b);
    qInfo("%s: diferencia media %.2f, máximo por bloque %.2f (%s)",
          qPrintable(nombre), media, bloque, qPrintable(rutaArtefacto));

    if (media > kMeanThreshold || bloque > kBlockThreshold) {
        qCritical("%s: regresión visual (media %.2f > %.2f o bloque %.2f > %.2f). "
                  "Captura en %s",
                  qPrintable(nombre), media, kMeanThreshold, bloque, kBlockThreshold,
                  qPrintable(rutaArtefacto));
        return false;
    }
    return true;
}

void VisualTest::accesoLogin()
{
    Montaje montaje(Escenario::Login);
    QVERIFY(comparar(montaje.ventana()->grabWindow(), QStringLiteral("access-login")));
}

void VisualTest::accesoBootstrap()
{
    Montaje montaje(Escenario::Bootstrap);
    QVERIFY(comparar(montaje.ventana()->grabWindow(),
                     QStringLiteral("access-bootstrap")));
}

void VisualTest::accesoCambioPassword()
{
    Montaje montaje(Escenario::CambioPassword);
    QVERIFY(comparar(montaje.ventana()->grabWindow(),
                     QStringLiteral("access-change")));
}

void VisualTest::modulos()
{
    Montaje montaje(Escenario::Demo);
    QQuickWindow* ventana = montaje.ventana();

    auto* shell = ventana->findChild<QQuickItem*>(QStringLiteral("shellView"));
    QVERIFY(shell);

    // Panel del día.
    QVERIFY(comparar(ventana->grabWindow(), QStringLiteral("panel")));

    // Agenda semanal y diaria.
    shell->setProperty("seleccion", 1);
    esperar();
    QVERIFY(comparar(ventana->grabWindow(), QStringLiteral("agenda-semana")));

    montaje.agenda()->cambiarModo(QStringLiteral("dia"));
    esperar();
    QVERIFY(comparar(ventana->grabWindow(), QStringLiteral("agenda-dia")));
    montaje.agenda()->cambiarModo(QStringLiteral("semana"));

    // Pacientes y ficha con historia clínica.
    shell->setProperty("seleccion", 2);
    esperar();
    QVERIFY(comparar(ventana->grabWindow(), QStringLiteral("pacientes")));

    montaje.pacientes()->seleccionar(montaje.demo().mascotaLuna);
    esperar(600);
    QVERIFY(comparar(ventana->grabWindow(), QStringLiteral("ficha-paciente")));
    montaje.pacientes()->limpiarSeleccion();

    // Historial clínico global.
    shell->setProperty("seleccion", 3);
    esperar();
    QVERIFY(comparar(ventana->grabWindow(), QStringLiteral("historial")));

    // Propietarios.
    shell->setProperty("seleccion", 4);
    esperar();
    QVERIFY(comparar(ventana->grabWindow(), QStringLiteral("propietarios")));

    // Inventario.
    shell->setProperty("seleccion", 5);
    esperar();
    QVERIFY(comparar(ventana->grabWindow(), QStringLiteral("inventario")));

    // Facturación.
    shell->setProperty("seleccion", 6);
    esperar();
    QVERIFY(comparar(ventana->grabWindow(), QStringLiteral("facturacion")));

    // Vínculos con el portal.
    shell->setProperty("seleccion", 7);
    esperar();
    QVERIFY(comparar(ventana->grabWindow(), QStringLiteral("vinculos")));

    // Ajustes: clínica y equipo.
    shell->setProperty("seleccion", 8);
    esperar();
    auto* ajustes = ventana->findChild<QQuickItem*>(QStringLiteral("ajustesView"));
    QVERIFY(ajustes);
    QVERIFY(comparar(ventana->grabWindow(), QStringLiteral("ajustes-clinica")));

    ajustes->setProperty("tab", QStringLiteral("equipo"));
    esperar();
    QVERIFY(comparar(ventana->grabWindow(), QStringLiteral("ajustes-equipo")));
}

int main(int argc, char** argv)
{
    // Las capturas no deben depender del día en que se ejecutan.
    if (!qEnvironmentVariableIsSet("FUZZY_HOY")) {
        qputenv("FUZZY_HOY", "2026-03-09");
    }

    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("FuzzyCloud"));
    app.setApplicationName(QStringLiteral("FuzzyCloudTests"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    VisualTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_visual.moc"
