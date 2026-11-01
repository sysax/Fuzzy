#include "fuzzy/core/Politicas.h"

#include <QtTest>
#include <vector>

class TstPoliticas : public QObject
{
    Q_OBJECT

private slots:
    void matrizCoincideConElSnapshot();
    void adminPuedeGestionarCitas();
    void recepcionNoPuedeRegistrarConsulta();
    void recepcionPuedeEmitirFactura();
    void rolDesconocidoNoPuedeHacerNada();
    void elContextoDeOtraClinicaNoSeAcepta();
    void estilistaPuedeRegistrarEstetica();
};

void TstPoliticas::matrizCoincideConElSnapshot()
{
    const fuzzy::core::Politicas politicas;

    struct Fila {
        fuzzy::core::Rol rol;
        std::vector<fuzzy::core::Permiso> permitidos;
    };

    const std::vector<Fila> matriz = {
        {fuzzy::core::Rol::Admin, {fuzzy::core::Permiso::VerPanel,           fuzzy::core::Permiso::GestionarCitas,
                                 fuzzy::core::Permiso::RegistrarConsulta, fuzzy::core::Permiso::RegistrarCliente,
                                 fuzzy::core::Permiso::EmitirFactura, fuzzy::core::Permiso::CerrarCaja,
                                 fuzzy::core::Permiso::GestionarInventario, fuzzy::core::Permiso::SuministrarMedicamento,
                                 fuzzy::core::Permiso::GestionarUsuarios, fuzzy::core::Permiso::ConfigurarClinica,
                                 fuzzy::core::Permiso::AprobarVinculo, fuzzy::core::Permiso::VerAuditoria,
                                 fuzzy::core::Permiso::RegistrarEstetica}},
        {fuzzy::core::Rol::Veterinario, {fuzzy::core::Permiso::VerPanel, fuzzy::core::Permiso::GestionarCitas,
                                       fuzzy::core::Permiso::RegistrarConsulta, fuzzy::core::Permiso::GestionarInventario,
                                       fuzzy::core::Permiso::SuministrarMedicamento}},
        {fuzzy::core::Rol::Recepcion, {fuzzy::core::Permiso::VerPanel, fuzzy::core::Permiso::GestionarCitas,
                                     fuzzy::core::Permiso::RegistrarCliente, fuzzy::core::Permiso::EmitirFactura,
                                     fuzzy::core::Permiso::CerrarCaja, fuzzy::core::Permiso::GestionarInventario,
                                     fuzzy::core::Permiso::AprobarVinculo}},
        {fuzzy::core::Rol::Estilista, {fuzzy::core::Permiso::VerPanel, fuzzy::core::Permiso::GestionarCitas,
                                     fuzzy::core::Permiso::GestionarInventario,
                                     fuzzy::core::Permiso::RegistrarEstetica}},
        {fuzzy::core::Rol::Cliente,  {}},
        {fuzzy::core::Rol::Desconocido, {}},
    };

    for (const Fila& fila : matriz) {
        const std::vector<fuzzy::core::Permiso> actuales = politicas.permisosDe(fila.rol);
        QCOMPARE(actuales.size(), fila.permitidos.size());
        for (std::size_t i = 0; i < fila.permitidos.size(); ++i) {
            QCOMPARE(int(actuales[static_cast<std::size_t>(i)]), int(fila.permitidos[static_cast<std::size_t>(i)]));
        }
    }
}

void TstPoliticas::adminPuedeGestionarCitas()
{
    const fuzzy::core::Politicas politicas;
    const fuzzy::core::Sesion sesion{ fuzzy::core::Rol::Admin, QStringLiteral("clinica-a"), QString() };
    const fuzzy::core::ContextoPermiso contexto{ QStringLiteral("citas"), fuzzy::core::Accion::Leer,
                                                QString(), QStringLiteral("clinica-a") };

    QVERIFY(politicas.exigir(sesion, fuzzy::core::Permiso::GestionarCitas, contexto));
}

void TstPoliticas::recepcionNoPuedeRegistrarConsulta()
{
    const fuzzy::core::Politicas politicas;
    const fuzzy::core::Sesion sesion{ fuzzy::core::Rol::Recepcion, QStringLiteral("clinica-a"), QString() };
    const fuzzy::core::ContextoPermiso contexto{ QStringLiteral("consultas_medicas"), fuzzy::core::Accion::Crear,
                                                QString(), QStringLiteral("clinica-a") };

    QVERIFY(!politicas.exigir(sesion, fuzzy::core::Permiso::RegistrarConsulta, contexto));
}

void TstPoliticas::recepcionPuedeEmitirFactura()
{
    const fuzzy::core::Politicas politicas;
    const fuzzy::core::Sesion sesion{ fuzzy::core::Rol::Recepcion, QStringLiteral("clinica-a"), QString() };
    const fuzzy::core::ContextoPermiso contexto{ QStringLiteral("facturas"), fuzzy::core::Accion::Crear,
                                                QString(), QStringLiteral("clinica-a") };

    QVERIFY(politicas.exigir(sesion, fuzzy::core::Permiso::EmitirFactura, contexto));
}

void TstPoliticas::rolDesconocidoNoPuedeHacerNada()
{
    const fuzzy::core::Politicas politicas;
    const fuzzy::core::Sesion sesion{ fuzzy::core::Rol::Desconocido, QStringLiteral("clinica-a"), QString() };
    const fuzzy::core::ContextoPermiso contexto{ QStringLiteral("citas"), fuzzy::core::Accion::Leer,
                                                QString(), QStringLiteral("clinica-a") };

    QVERIFY(!politicas.exigir(sesion, fuzzy::core::Permiso::VerPanel, contexto));
}

void TstPoliticas::elContextoDeOtraClinicaNoSeAcepta()
{
    const fuzzy::core::Politicas politicas;
    const fuzzy::core::Sesion sesion{ fuzzy::core::Rol::Admin, QStringLiteral("clinica-a"), QString() };
    const fuzzy::core::ContextoPermiso contexto{ QStringLiteral("citas"), fuzzy::core::Accion::Leer,
                                                QString(), QStringLiteral("clinica-b") };

    QVERIFY(!politicas.exigir(sesion, fuzzy::core::Permiso::GestionarCitas, contexto));
}

void TstPoliticas::estilistaPuedeRegistrarEstetica()
{
    const fuzzy::core::Politicas politicas;
    const fuzzy::core::Sesion sesion{ fuzzy::core::Rol::Estilista, QStringLiteral("clinica-a"), QString() };
    const fuzzy::core::ContextoPermiso contexto{ QStringLiteral("estetica_servicios"),
                                                fuzzy::core::Accion::Crear,
                                                QString(), QStringLiteral("clinica-a") };

    QVERIFY(politicas.exigir(sesion, fuzzy::core::Permiso::RegistrarEstetica, contexto));
    QVERIFY(!politicas.exigir(sesion, fuzzy::core::Permiso::RegistrarConsulta, contexto));
}

QTEST_MAIN(TstPoliticas)

#include "tst_politicas.moc"
