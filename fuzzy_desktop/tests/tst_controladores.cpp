#include "agenda/AgendaController.h"
#include "ajustes/AjustesController.h"
#include "facturacion/FacturacionController.h"
#include "fuzzy/core/AuthService.h"
#include "fuzzy/core/MigrationRunner.h"
#include "fuzzy/core/PortalBaseStore.h"
#include "fuzzy/core/PortalProvisioner.h"
#include "fuzzy/core/PortalTokenService.h"
#include "fuzzy/core/SqliteExecutor.h"
#include "fuzzy/core/Uuid.h"
#include "inventario/InventarioController.h"
#include "pacientes/PacientesController.h"
#include "propietarios/PropietariosController.h"
#include "session/SessionController.h"
#include "vinculos/VinculosController.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>
#include <vector>

namespace {

qint64 insertar(fuzzy::core::IQueryExecutor& db, const QString& sql,
                const std::vector<QVariant>& params = {})
{
    const auto resultado = db.execute(sql, params);
    if (!resultado.ok) {
        qFatal("SQL falló: %s", qPrintable(sql));
    }
    return resultado.last_insert_id.toLongLong();
}

QString crearCliente(fuzzy::core::IQueryExecutor& db, const QString& nombre,
                     const QString& correo = {})
{
    const QString uuid = fuzzy::core::uuidNuevo();
    insertar(db,
             QStringLiteral("INSERT INTO clientes "
                            "(uuid, nombre_completo, telefono, correo, direccion, dni, "
                            " created_at, updated_at, sync_state) "
                            "VALUES (?, ?, '+57 300 000 0000', ?, 'Calle 1 #2-3', ?, "
                            "        strftime('%s','now') * 1000, "
                            "        strftime('%s','now') * 1000, 'pendiente')"),
             {QVariant(uuid), QVariant(nombre),
              correo.isEmpty() ? QVariant() : QVariant(correo),
              QVariant(fuzzy::core::uuidNuevo().left(10))});
    return uuid;
}

QString crearMascota(fuzzy::core::IQueryExecutor& db, const QString& clienteUuid,
                     const QString& nombre, const QString& especie = QStringLiteral("Perro"))
{
    const QString uuid = fuzzy::core::uuidNuevo();
    const auto cliente = db.query(
        QStringLiteral("SELECT id FROM clientes WHERE uuid = ?"), {QVariant(clienteUuid)});
    insertar(db,
             QStringLiteral("INSERT INTO mascotas "
                            "(uuid, cliente_id, nombre, especie, raza, sexo, peso_kg, "
                            " numero_chip, fecha_nacimiento, activo, castrado, created_at, "
                            " updated_at, sync_state) "
                            "VALUES (?, ?, ?, ?, 'Mestizo', 'Hembra', 8.5, ?, '2022-01-15', "
                            "        1, 0, strftime('%s','now') * 1000, "
                            "        strftime('%s','now') * 1000, 'pendiente')"),
             {QVariant(uuid), cliente.front().values.front(), QVariant(nombre),
              QVariant(especie), QVariant(fuzzy::core::uuidNuevo().left(10))});
    return uuid;
}

QString crearServicio(fuzzy::core::IQueryExecutor& db, const QString& nombre, double precio)
{
    const QString uuid = fuzzy::core::uuidNuevo();
    insertar(db,
             QStringLiteral("INSERT INTO servicios "
                            "(uuid, codigo, nombre, precio, activo, created_at, updated_at, "
                            " sync_state) "
                            "VALUES (?, ?, ?, ?, 1, strftime('%s','now') * 1000, "
                            "        strftime('%s','now') * 1000, 'pendiente')"),
             {QVariant(uuid), QVariant(fuzzy::core::uuidNuevo().left(12)), QVariant(nombre),
              QVariant(precio)});
    return uuid;
}

QString crearProducto(fuzzy::core::IQueryExecutor& db, const QString& nombre, int stock,
                      int minimo, double venta)
{
    const QString uuid = fuzzy::core::uuidNuevo();
    insertar(db,
             QStringLiteral("INSERT INTO productos_insumos "
                            "(uuid, nombre_comercial, categoria_id, codigo, unidad, "
                            " stock_actual, stock_minimo, precio_compra, precio_venta, "
                            " created_at, updated_at, sync_state) "
                            "VALUES (?, ?, 1, ?, 'unidades', ?, ?, ?, ?, "
                            "        strftime('%s','now') * 1000, "
                            "        strftime('%s','now') * 1000, 'pendiente')"),
             {QVariant(uuid), QVariant(nombre),
              QVariant(fuzzy::core::uuidNuevo().left(10)), QVariant::fromValue(stock),
              QVariant::fromValue(minimo), QVariant(venta * 0.6), QVariant(venta)});
    return uuid;
}

struct Base
{
    explicit Base(bool instalar = true)
    {
        static int conexiones = 0;
        if (!dir.isValid()) {
            qFatal("No se pudo crear el directorio temporal");
        }
        db = std::make_unique<fuzzy::core::SqliteExecutor>(
            QStringLiteral("controladores_%1").arg(++conexiones),
            dir.filePath(QStringLiteral("clinica.db")));

        const auto migraciones = fuzzy::core::MigrationRunner::clinic();
        if (!migraciones.valido() || !migraciones.aplicar(*db)) {
            qFatal("No se pudo aplicar el esquema de la clínica");
        }

        auth = std::make_unique<fuzzy::core::AuthService>(*db);
        if (!instalar) {
            return;
        }
        const auto alta = auth->bootstrap(QStringLiteral("Clínica de Pruebas"),
                                          QStringLiteral("Medellín"),
                                          QStringLiteral("Admin de Pruebas"),
                                          QStringLiteral("admin@pruebas.test"),
                                          QStringLiteral("Password123!"));
        if (!alta.ok) {
            qFatal("Bootstrap falló: %s", qPrintable(alta.error));
        }
        adminUuid = alta.usuario_uuid;
    }

    QTemporaryDir dir;
    std::unique_ptr<fuzzy::core::SqliteExecutor> db;
    std::unique_ptr<fuzzy::core::AuthService> auth;
    QString adminUuid;
};

} // namespace

class ControladoresTest : public QObject
{
    Q_OBJECT

private slots:
    void bootstrapYLogin();
    void reglasDelEquipo();
    void restablecerPassword();
    void pacientesYHistoria();
    void propietarios();
    void inventario();
    void facturacionYCaja();
    void vinculosDelPortal();
    void agendaDeCitas();
    void ajustesDeClinica();
};

void ControladoresTest::bootstrapYLogin()
{
    Base base(false);

    // La base recién migrada no tiene instalación y el bootstrap es único.
    QVERIFY(!base.auth->hayInstalacion());

    const auto primera = base.auth->bootstrap(QStringLiteral("Clínica de Pruebas"),
                                              QStringLiteral("Medellín"),
                                              QStringLiteral("Admin de Pruebas"),
                                              QStringLiteral("admin@pruebas.test"),
                                              QStringLiteral("Password123!"));
    QVERIFY2(primera.ok, qPrintable(primera.error));
    base.adminUuid = primera.usuario_uuid;
    QVERIFY(base.auth->hayInstalacion());

    const auto segunda = base.auth->bootstrap(QStringLiteral("Otra"), QStringLiteral("Cali"),
                                              QStringLiteral("Otro Admin"),
                                              QStringLiteral("otro@pruebas.test"),
                                              QStringLiteral("Password123!"));
    QVERIFY(!segunda.ok);

    const auto clinica = base.auth->clinica();
    QCOMPARE(clinica.nombre, QStringLiteral("Clínica de Pruebas"));
    QCOMPARE(clinica.ciudad, QStringLiteral("Medellín"));

    const auto equipo = base.auth->equipo();
    QCOMPARE(equipo.size(), 1);
    QCOMPARE(equipo.first().rol, QStringLiteral("Admin"));
    QVERIFY(equipo.first().activo);

    QVERIFY(!base.auth->iniciarSesion(QStringLiteral("admin@pruebas.test"),
                                      QStringLiteral("incorrecta")).ok);
    const auto sesion = base.auth->iniciarSesion(QStringLiteral("admin@pruebas.test"),
                                                 QStringLiteral("Password123!"));
    QVERIFY2(sesion.ok, qPrintable(sesion.error));
    QCOMPARE(sesion.sesion.rol, fuzzy::core::Rol::Admin);
    QVERIFY(!sesion.debeCambiarPassword);
    QCOMPARE(sesion.usuario_uuid, base.adminUuid);

    // El controlador de sesión adapta el mismo resultado a QML.
    fuzzy::desktop::SessionController controlador(*base.auth);
    QVERIFY(controlador.hayInstalacion());
    // Antes de entrar no hay permisos: es el estado con el que arranca la app.
    QVERIFY(controlador.permisos().isEmpty());
    controlador.iniciarSesion(QStringLiteral("admin@pruebas.test"),
                              QStringLiteral("Password123!"));
    QVERIFY(controlador.autenticado());
    QVERIFY(!controlador.debeCambiarPassword());
    QCOMPARE(controlador.rol(), QStringLiteral("Admin"));
    QCOMPARE(controlador.usuarioUuid(), base.adminUuid);
    QVERIFY(controlador.puede(QStringLiteral("GestionarUsuarios")));
    QVERIFY(controlador.permisos().contains(QStringLiteral("VerPanel")));
    QVERIFY(controlador.permisos().contains(QStringLiteral("AprobarVinculo")));

    controlador.cerrarSesion();
    QVERIFY(!controlador.autenticado());
    QVERIFY(controlador.permisos().isEmpty());
}

void ControladoresTest::reglasDelEquipo()
{
    Base base;

    const auto vet = base.auth->crearUsuario(base.adminUuid, QStringLiteral("Dra. Vera"),
                                             QStringLiteral("vera@pruebas.test"),
                                             QStringLiteral("Veterinario"));
    QVERIFY2(vet.ok, qPrintable(vet.error));
    QCOMPARE(vet.passwordTemporal.size(), 12);

    // El correo es único sin distinguir mayúsculas.
    QVERIFY(!base.auth->crearUsuario(base.adminUuid, QStringLiteral("Duplicada"),
                                     QStringLiteral("VERA@pruebas.test"),
                                     QStringLiteral("Recepcion")).ok);
    // Un rol inexistente o de cliente no se acepta.
    QVERIFY(!base.auth->crearUsuario(base.adminUuid, QStringLiteral("Rara"),
                                     QStringLiteral("rara@pruebas.test"),
                                     QStringLiteral("Invitado")).ok);
    // Solo un Admin activo puede crear personal.
    QVERIFY(!base.auth->crearUsuario(vet.uuid, QStringLiteral("Otra"),
                                     QStringLiteral("otra@pruebas.test"),
                                     QStringLiteral("Recepcion")).ok);

    // La contraseña temporal obliga al cambio y caduca a las 72 h.
    const auto temporal = base.auth->iniciarSesion(QStringLiteral("vera@pruebas.test"),
                                                   vet.passwordTemporal);
    QVERIFY2(temporal.ok, qPrintable(temporal.error));
    QVERIFY(temporal.debeCambiarPassword);
    QVERIFY(temporal.passwordExpiraAt > 0);

    QVERIFY(!base.auth->cambiarPassword(vet.uuid, QStringLiteral("incorrecta"),
                                        QStringLiteral("NuevaClave123")).ok);
    QVERIFY(!base.auth->cambiarPassword(vet.uuid, vet.passwordTemporal,
                                        QStringLiteral("corta")).ok);
    QVERIFY(base.auth->cambiarPassword(vet.uuid, vet.passwordTemporal,
                                       QStringLiteral("NuevaClave123")).ok);

    const auto renovada = base.auth->iniciarSesion(QStringLiteral("vera@pruebas.test"),
                                                   QStringLiteral("NuevaClave123"));
    QVERIFY(renovada.ok);
    QVERIFY(!renovada.debeCambiarPassword);

    // El último Admin no puede degradarse ni desactivarse a sí mismo.
    QVERIFY(!base.auth->actualizarUsuario(base.adminUuid, base.adminUuid,
                                          QStringLiteral("Admin de Pruebas"),
                                          QStringLiteral("admin@pruebas.test"),
                                          QStringLiteral("Veterinario"), true).ok);
    QVERIFY(!base.auth->actualizarUsuario(base.adminUuid, base.adminUuid,
                                          QStringLiteral("Admin de Pruebas"),
                                          QStringLiteral("admin@pruebas.test"),
                                          QStringLiteral("Admin"), false).ok);

    // Con un segundo Admin, el primero sí puede cambiar de rol.
    const auto admin2 = base.auth->crearUsuario(base.adminUuid, QStringLiteral("Admin Dos"),
                                                QStringLiteral("admin2@pruebas.test"),
                                                QStringLiteral("Admin"));
    QVERIFY2(admin2.ok, qPrintable(admin2.error));
    QVERIFY(base.auth->actualizarUsuario(admin2.uuid, base.adminUuid,
                                         QStringLiteral("Admin de Pruebas"),
                                         QStringLiteral("admin@pruebas.test"),
                                         QStringLiteral("Recepcion"), true).ok);

    const auto equipo = base.auth->equipo();
    QCOMPARE(equipo.size(), 3);
    bool hayRecepcion = false;
    bool hayDosAdmins = false;
    for (const auto& usuario : equipo) {
        if (usuario.uuid == base.adminUuid)
            hayRecepcion = usuario.rol == QStringLiteral("Recepcion");
        if (usuario.rol == QStringLiteral("Admin") && usuario.activo)
            hayDosAdmins = true;
    }
    QVERIFY(hayRecepcion);
    QVERIFY(hayDosAdmins);
}

void ControladoresTest::restablecerPassword()
{
    Base base;

    const auto vet = base.auth->crearUsuario(base.adminUuid, QStringLiteral("Dr. Reset"),
                                             QStringLiteral("reset@pruebas.test"),
                                             QStringLiteral("Veterinario"));
    QVERIFY(vet.ok);
    QVERIFY(base.auth->cambiarPassword(vet.uuid, vet.passwordTemporal,
                                       QStringLiteral("ClavePropia123")).ok);

    const auto reinicio = base.auth->restablecerPassword(base.adminUuid, vet.uuid);
    QVERIFY(reinicio.ok);
    QCOMPARE(reinicio.passwordTemporal.size(), 12);
    QVERIFY(!base.auth->iniciarSesion(QStringLiteral("reset@pruebas.test"),
                                      QStringLiteral("ClavePropia123")).ok);
    const auto temporal = base.auth->iniciarSesion(QStringLiteral("reset@pruebas.test"),
                                                    reinicio.passwordTemporal);
    QVERIFY(temporal.ok);
    QVERIFY(temporal.debeCambiarPassword);

    // Un rol no Admin no puede restablecer claves ajenas.
    QVERIFY(!base.auth->restablecerPassword(vet.uuid, base.adminUuid).ok);
}

void ControladoresTest::pacientesYHistoria()
{
    Base base;
    const QString cliente = crearCliente(*base.db, QStringLiteral("Camila Restrepo"),
                                         QStringLiteral("camila@pruebas.test"));
    const QString luna = crearMascota(*base.db, cliente, QStringLiteral("Luna"));
    crearMascota(*base.db, cliente, QStringLiteral("Milo"), QStringLiteral("Gato"));

    fuzzy::desktop::PacientesController pacientes(*base.db);
    QCOMPARE(pacientes.total(), 2);
    QCOMPARE(pacientes.pacientes().size(), 2);
    QVERIFY(pacientes.especies().size() >= 2);

    pacientes.buscar(QStringLiteral("Luna"));
    QCOMPARE(pacientes.total(), 1);
    QCOMPARE(pacientes.pacientes().first().toMap().value(QStringLiteral("nombre")).toString(),
             QStringLiteral("Luna"));

    pacientes.buscar(QStringLiteral("no-existe"));
    QCOMPARE(pacientes.total(), 0);

    pacientes.buscar(QString());
    pacientes.filtrarEspecie(QStringLiteral("Gato"));
    QCOMPARE(pacientes.total(), 1);
    pacientes.filtrarEspecie(QString());

    pacientes.seleccionar(luna);
    QCOMPARE(pacientes.ficha().value(QStringLiteral("uuid")).toString(), luna);
    QCOMPARE(pacientes.ficha().value(QStringLiteral("propietario")).toString(),
             QStringLiteral("Camila Restrepo"));

    pacientes.registrarConsulta(luna, QStringLiteral("Control anual"),
                                QStringLiteral("Sin novedades"),
                                QStringLiteral("Examen normal"),
                                QStringLiteral("Paciente sana"),
                                QStringLiteral("Vacunación al día"), false);
    QCOMPARE(pacientes.historial().size(), 1);
    QCOMPARE(pacientes.historial().first().toMap().value(QStringLiteral("motivo")).toString(),
             QStringLiteral("Control anual"));

    pacientes.registrarSignos(luna, 24.5, 38.4, 84, 22, QStringLiteral("Basal"));
    QCOMPARE(pacientes.signos().size(), 1);

    pacientes.registrarVacuna(luna, QStringLiteral("Polivalente"),
                              QStringLiteral("2026-03-01"),
                              QStringLiteral("2027-03-01"), QStringLiteral("L-1"),
                              QStringLiteral("Sin reacción"));
    QCOMPARE(pacientes.vacunas().size(), 1);

    pacientes.buscarHistorial(QStringLiteral("Control"));
    QCOMPARE(pacientes.historialGlobal().size(), 1);

    // Alta de una mascota nueva desde el listado.
    pacientes.registrarMascota(cliente, QStringLiteral("Nala"), QStringLiteral("Gato"),
                               QStringLiteral("Siamés"), QStringLiteral("Hembra"), 3.6, true,
                               QStringLiteral("2023-02-14"), QStringLiteral("CHIP-99"),
                               QString(), QString());
    QCOMPARE(pacientes.total(), 3);
    QVERIFY(pacientes.error().isEmpty());
}

void ControladoresTest::propietarios()
{
    Base base;
    const QString cliente = crearCliente(*base.db, QStringLiteral("Andrés Salazar"),
                                         QStringLiteral("andres@pruebas.test"));
    crearMascota(*base.db, cliente, QStringLiteral("Rocky"));

    fuzzy::desktop::PropietariosController propietarios(*base.db);
    QCOMPARE(propietarios.total(), 1);
    QCOMPARE(propietarios.conMascotas(), 1);

    propietarios.buscar(QStringLiteral("Andrés"));
    QCOMPARE(propietarios.total(), 1);
    propietarios.buscar(QStringLiteral("zzz"));
    QCOMPARE(propietarios.total(), 0);
    propietarios.buscar(QString());

    propietarios.seleccionar(cliente);
    QCOMPARE(propietarios.seleccionado().value(QStringLiteral("uuid")).toString(), cliente);

    propietarios.actualizar(cliente, QStringLiteral("Andrés Salazar Ríos"),
                            QStringLiteral("+57 301 111 2222"),
                            QStringLiteral("andres@pruebas.test"),
                            QStringLiteral("Calle 2 #3-4"), QStringLiteral("99887766"), true);
    QCOMPARE(propietarios.seleccionado().value(QStringLiteral("nombre")).toString(),
             QStringLiteral("Andrés Salazar Ríos"));
    QVERIFY(propietarios.error().isEmpty());

    const QString nuevo = [&] {
        propietarios.crear(QStringLiteral("Mariana Torres"),
                           QStringLiteral("+57 304 555 6677"),
                           QStringLiteral("mariana@pruebas.test"),
                           QStringLiteral("Cra 1 #2-3"), QStringLiteral("11223344"));
        for (const auto& item : propietarios.propietarios()) {
            const auto mapa = item.toMap();
            if (mapa.value(QStringLiteral("nombre")).toString() == QStringLiteral("Mariana Torres"))
                return mapa.value(QStringLiteral("uuid")).toString();
        }
        return QString();
    }();
    QVERIFY(!nuevo.isEmpty());
    QCOMPARE(propietarios.total(), 2);

    propietarios.seleccionar(nuevo);
    QCOMPARE(propietarios.mascotas().size(), 0);
    QVERIFY(propietarios.error().isEmpty());
}

void ControladoresTest::inventario()
{
    Base base;
    fuzzy::desktop::InventarioController inventario(*base.db);
    QCOMPARE(inventario.categorias().size(), 5);
    QCOMPARE(inventario.total(), 0);

    auto categoriaUuid = [&](const QString& nombre) {
        for (const auto& item : inventario.categorias()) {
            const auto mapa = item.toMap();
            if (mapa.value(QStringLiteral("nombre")).toString() == nombre)
                return mapa.value(QStringLiteral("uuid")).toString();
        }
        return QString();
    }(QStringLiteral("Medicamentos"));
    QVERIFY(!categoriaUuid.isEmpty());

    inventario.crearProducto(QStringLiteral("Amoxicilina 250 mg"),
                             QStringLiteral("Amoxicilina"), categoriaUuid,
                             QStringLiteral("MED-900"), QStringLiteral("unidades"), 3, 10,
                             12000.0, 22000.0, QStringLiteral("2027-01-01"),
                             QStringLiteral("Proveedor"));
    QCOMPARE(inventario.total(), 1);
    QCOMPARE(inventario.criticos(), 1);
    QVERIFY(inventario.error().isEmpty());

    const QString producto = inventario.productos().first().toMap()
                                 .value(QStringLiteral("uuid")).toString();
    inventario.ajustarStock(producto, 12, QStringLiteral("compra"));
    QCOMPARE(inventario.seleccionado().value(QStringLiteral("stock")).toInt(), 15);
    QCOMPARE(inventario.criticos(), 0);

    inventario.ajustarStock(producto, -20, QStringLiteral("ajuste"));
    QVERIFY(!inventario.error().isEmpty());
    inventario.limpiarMensajes();

    inventario.marcarPedido(producto, true);
    QCOMPARE(inventario.seleccionado().value(QStringLiteral("pedido")).toBool(), true);
    inventario.marcarPedido(producto, false);
    QCOMPARE(inventario.seleccionado().value(QStringLiteral("pedido")).toBool(), false);

    inventario.cambiarVista(QStringLiteral("critico"));
    QCOMPARE(inventario.total(), 0);
    inventario.cambiarVista(QStringLiteral("todos"));
    QCOMPARE(inventario.total(), 1);
    QVERIFY(inventario.valorInventario() > 0.0);

    inventario.buscar(QStringLiteral("Amoxi"));
    QCOMPARE(inventario.total(), 1);
}

void ControladoresTest::facturacionYCaja()
{
    Base base;
    const QString cliente = crearCliente(*base.db, QStringLiteral("Camila Restrepo"));
    const QString servicio = crearServicio(*base.db, QStringLiteral("Consulta general"),
                                           85000.0);
    const QString producto = crearProducto(*base.db, QStringLiteral("Shampoo"), 10, 2,
                                           36000.0);

    fuzzy::desktop::FacturacionController facturacion(*base.db);
    facturacion.setUsuarioUuid(base.adminUuid);
    QCOMPARE(facturacion.total(), 0);
    QCOMPARE(facturacion.servicios().size(), 1);

    facturacion.iniciarBorrador(cliente);
    facturacion.agregarServicio(servicio, 2);
    QCOMPARE(facturacion.lineas().size(), 1);
    QCOMPARE(facturacion.subtotalBorrador(), 170000.0);

    facturacion.agregarProducto(producto, 1);
    QCOMPARE(facturacion.lineas().size(), 2);
    QCOMPARE(facturacion.totalBorrador(), 206000.0);

    facturacion.emitir(QStringLiteral("Efectivo"), 6000.0, 0.0);
    QVERIFY2(facturacion.error().isEmpty(), qPrintable(facturacion.error()));
    QCOMPARE(facturacion.total(), 1);
    QCOMPARE(facturacion.facturas().first().toMap()
                 .value(QStringLiteral("estado")).toString(),
             QStringLiteral("Pagada"));
    QCOMPARE(facturacion.totalFacturado(), 200000.0);
    QCOMPARE(facturacion.lineas().size(), 0);

    const auto stock = base.db->query(
        QStringLiteral("SELECT stock_actual FROM productos_insumos WHERE uuid = ?"),
        {QVariant(producto)});
    QCOMPARE(stock.front().values.front().toInt(), 9);

    const QString factura = facturacion.facturas().first().toMap()
                                .value(QStringLiteral("uuid")).toString();
    QVERIFY(!factura.isEmpty());

    // Una factura pagada no se puede anular.
    facturacion.anular(factura);
    QVERIFY(!facturacion.error().isEmpty());
    facturacion.limpiarMensajes();

    // Caja del día: el pago quedó registrado.
    facturacion.recargarCaja();
    QCOMPARE(facturacion.resumenCaja().value(QStringLiteral("cantidad")).toInt(), 1);
    QCOMPARE(facturacion.resumenCaja().value(QStringLiteral("total")).toDouble(),
             200000.0);

    facturacion.cerrarCaja(200000.0, QStringLiteral("Sin novedades"));
    QVERIFY2(facturacion.error().isEmpty(), qPrintable(facturacion.error()));
    const auto cierres = base.db->query(QStringLiteral("SELECT count(*) FROM cierres_caja"));
    QCOMPARE(cierres.front().values.front().toInt(), 1);
}

void ControladoresTest::vinculosDelPortal()
{
    Base base;
    const QString cliente = crearCliente(*base.db, QStringLiteral("Camila Restrepo"),
                                         QStringLiteral("camila@pruebas.test"));

    const QString portales = base.dir.filePath(QStringLiteral("portales"));
    QDir().mkpath(portales);
    fuzzy::core::LocalPortalBaseStore bases(portales);
    fuzzy::core::PortalTokenService tokens(*base.db, &bases);
    fuzzy::core::PortalProvisioner provisioner(*base.db, tokens, bases);

    fuzzy::desktop::VinculosController vinculos(*base.db, provisioner, bases);
    QCOMPARE(vinculos.vinculos().size(), 1);
    QCOMPARE(vinculos.sinCuenta().size(), 1);

    vinculos.aprobar(cliente, QStringLiteral("camila@pruebas.test"),
                     QStringLiteral("Portal2026!"));
    QVERIFY2(vinculos.error().isEmpty(), qPrintable(vinculos.error()));
    QCOMPARE(vinculos.vinculos().size(), 1);
    QCOMPARE(vinculos.sinCuenta().size(), 0);

    const auto vinculo = vinculos.vinculos().first().toMap();
    const QString portal = vinculo.value(QStringLiteral("portalUuid")).toString();
    QVERIFY(!portal.isEmpty());
    QCOMPARE(vinculo.value(QStringLiteral("estadoCuenta")).toString(),
             QStringLiteral("Activa"));
    QVERIFY(vinculo.value(QStringLiteral("conToken")).toBool());

    vinculos.revocar(portal);
    QVERIFY(vinculos.error().isEmpty());
    QCOMPARE(vinculos.vinculos().first().toMap()
                 .value(QStringLiteral("estadoPortal")).toString(),
             QStringLiteral("revocado"));

    vinculos.eliminar(portal);
    QVERIFY(vinculos.error().isEmpty());
    QVERIFY(vinculos.vinculos().first().toMap()
                .value(QStringLiteral("portalUuid")).toString().isEmpty());

    // Una contraseña corta no crea la cuenta de portal.
    vinculos.aprobar(cliente, QStringLiteral("otra@pruebas.test"), QStringLiteral("corta"));
    QVERIFY(!vinculos.error().isEmpty());
}

void ControladoresTest::agendaDeCitas()
{
    Base base;
    const QString cliente = crearCliente(*base.db, QStringLiteral("Camila Restrepo"));
    const QString luna = crearMascota(*base.db, cliente, QStringLiteral("Luna"));

    fuzzy::desktop::AgendaController agenda(*base.db);
    QCOMPARE(agenda.modo(), QStringLiteral("semana"));
    QVERIFY(agenda.citas().isEmpty());
    QCOMPARE(agenda.tipos().size(), 3);
    QCOMPARE(agenda.estados().size(), 6);

    const QString tipo = agenda.tipos().first().toMap()
                             .value(QStringLiteral("uuid")).toString();
    const QString agendada = agenda.estados().first().toMap()
                                 .value(QStringLiteral("uuid")).toString();
    QVERIFY(!tipo.isEmpty());
    QVERIFY(!agendada.isEmpty());

    agenda.cambiarModo(QStringLiteral("dia"));
    QCOMPARE(agenda.modo(), QStringLiteral("dia"));

    const QString hoy = agenda.fechaActual();
    agenda.agendar(luna, tipo, hoy, QStringLiteral("15:45"),
                   QStringLiteral("Control"), false);
    QVERIFY2(agenda.error().isEmpty(), qPrintable(agenda.error()));
    QCOMPARE(agenda.citas().size(), 1);

    const auto cita = agenda.citas().first().toMap();
    const QString citaUuid = cita.value(QStringLiteral("uuid")).toString();
    QCOMPARE(cita.value(QStringLiteral("hora")).toString(), QStringLiteral("15:45"));
    QCOMPARE(cita.value(QStringLiteral("estado")).toString(), QStringLiteral("Agendada"));

    // No se solapan dos citas de la misma mascota.
    agenda.agendar(luna, tipo, hoy, QStringLiteral("15:50"),
                   QStringLiteral("Duplicada"), false);
    QVERIFY(!agenda.error().isEmpty());
    QCOMPARE(agenda.citas().size(), 1);
    agenda.limpiarMensajes();

    const QString completada = [&] {
        for (const auto& item : agenda.estados()) {
            const auto mapa = item.toMap();
            if (mapa.value(QStringLiteral("nombre")).toString() == QStringLiteral("Completada"))
                return mapa.value(QStringLiteral("uuid")).toString();
        }
        return QString();
    }();
    QVERIFY(!completada.isEmpty());

    agenda.cambiarEstado(citaUuid, completada);
    QVERIFY(agenda.error().isEmpty());
    QCOMPARE(agenda.citas().first().toMap()
                 .value(QStringLiteral("estado")).toString(),
             QStringLiteral("Completada"));

    agenda.mover(citaUuid, hoy, QStringLiteral("16:30"));
    QVERIFY(agenda.error().isEmpty());
    QCOMPARE(agenda.citas().first().toMap()
                 .value(QStringLiteral("hora")).toString(),
             QStringLiteral("16:30"));

    const QString fechaPrevia = agenda.fechaActual();
    agenda.siguiente();
    QVERIFY(agenda.fechaActual() != fechaPrevia);
    agenda.irHoy();
    QCOMPARE(agenda.fechaActual(), fechaPrevia);

    agenda.cancelar(citaUuid);
    QVERIFY(agenda.error().isEmpty());
    QCOMPARE(agenda.citas().first().toMap()
                 .value(QStringLiteral("estado")).toString(),
             QStringLiteral("Cancelada"));
    QVERIFY(!agenda.citas().first().toMap()
                 .value(QStringLiteral("uuid")).toString().isEmpty());
    Q_UNUSED(agendada);
}

void ControladoresTest::ajustesDeClinica()
{
    Base base;
    fuzzy::desktop::AjustesController ajustes(*base.auth);
    ajustes.setAdminUuid(base.adminUuid);
    ajustes.cargar();

    QCOMPARE(ajustes.clinica().value(QStringLiteral("nombre")).toString(),
             QStringLiteral("Clínica de Pruebas"));
    QCOMPARE(ajustes.equipo().size(), 1);
    QCOMPARE(ajustes.roles().size(), 4);

    ajustes.guardarClinica(QStringLiteral("Clínica Aurora"),
                           QStringLiteral("Envigado"),
                           QStringLiteral("Calle 10 #20-30"),
                           QStringLiteral("+57 604 444 5566"));
    QVERIFY2(ajustes.error().isEmpty(), qPrintable(ajustes.error()));
    QCOMPARE(ajustes.clinica().value(QStringLiteral("nombre")).toString(),
             QStringLiteral("Clínica Aurora"));
    QCOMPARE(ajustes.clinica().value(QStringLiteral("ciudad")).toString(),
             QStringLiteral("Envigado"));

    ajustes.crearUsuario(QStringLiteral("Dr. Marco"), QStringLiteral("marco@pruebas.test"),
                         QStringLiteral("Veterinario"));
    QVERIFY2(ajustes.error().isEmpty(), qPrintable(ajustes.error()));
    QCOMPARE(ajustes.equipo().size(), 2);
    QVERIFY(!ajustes.passwordTemporal().isEmpty());

    QString marcoUuid;
    for (const auto& item : ajustes.equipo()) {
        const auto mapa = item.toMap();
        if (mapa.value(QStringLiteral("correo")).toString()
            == QStringLiteral("marco@pruebas.test")) {
            marcoUuid = mapa.value(QStringLiteral("uuid")).toString();
        }
    }
    QVERIFY(!marcoUuid.isEmpty());

    ajustes.actualizarUsuario(marcoUuid, QStringLiteral("Dr. Marco Soler"),
                              QStringLiteral("marco@pruebas.test"),
                              QStringLiteral("Veterinario"), true);
    QVERIFY(ajustes.error().isEmpty());

    ajustes.restablecerPassword(marcoUuid);
    QVERIFY(ajustes.error().isEmpty());
    QCOMPARE(ajustes.passwordTemporal().size(), 12);
}

QTEST_GUILESS_MAIN(ControladoresTest)

#include "tst_controladores.moc"
