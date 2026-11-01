#pragma once

#include "fuzzy/core/IQueryExecutor.h"
#include "fuzzy/core/Permisos.h"
#include "fuzzy/core/ServicioClinico.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace fuzzy::desktop {

// Puente entre QML y ServicioClinico. No contiene reglas: traduce listas y
// formularios, y deja que el servicio aplique RBAC y transacciones.
class ClinicoController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString rol READ rol NOTIFY sesionCambiada)
    Q_PROPERTY(QVariantList agendaDelDia READ agendaDelDia NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList consultasAbiertas READ consultasAbiertas NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList laboratorio READ laboratorio NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList hospitalizaciones READ hospitalizaciones NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList cirugias READ cirugias NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList estetica READ estetica NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList cargos READ cargos NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList examenes READ examenes NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList servicios READ servicios NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList productos READ productos NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList mascotas READ mascotas NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList clientes READ clientes NOTIFY datosCambiados)
    Q_PROPERTY(QVariantMap resumen READ resumen NOTIFY datosCambiados)
    Q_PROPERTY(QString error READ error NOTIFY errorCambiado)
    Q_PROPERTY(QString mensaje READ mensaje NOTIFY mensajeCambiado)

public:
    explicit ClinicoController(fuzzy::core::IQueryExecutor& db, QObject* parent = nullptr);

    QString rol() const { return rol_; }
    QVariantList agendaDelDia() const { return agendaDelDia_; }
    QVariantList consultasAbiertas() const { return consultasAbiertas_; }
    QVariantList laboratorio() const { return laboratorio_; }
    QVariantList hospitalizaciones() const { return hospitalizaciones_; }
    QVariantList cirugias() const { return cirugias_; }
    QVariantList estetica() const { return estetica_; }
    QVariantList cargos() const { return cargos_; }
    QVariantList examenes() const { return examenes_; }
    QVariantList servicios() const { return servicios_; }
    QVariantList productos() const { return productos_; }
    QVariantList mascotas() const { return mascotas_; }
    QVariantList clientes() const { return clientes_; }
    QVariantMap resumen() const { return resumen_; }
    QString error() const { return error_; }
    QString mensaje() const { return mensaje_; }

    Q_INVOKABLE void setSesion(const QString& rol, const QString& clinicaUuid,
                               const QString& usuarioUuid);

    Q_INVOKABLE void cargar();
    Q_INVOKABLE void limpiarMensajes();

    // Recepción / triaje.
    Q_INVOKABLE void recibir(const QString& citaUuid);
    Q_INVOKABLE void registrarTriaje(const QString& mascotaUuid, const QString& consultaUuid,
                                     double peso, double temperatura, int cardiaca,
                                     int respiratoria, const QString& notas);

    // Consulta, recetas y cargos.
    Q_INVOKABLE void abrirConsulta(const QString& mascotaUuid, const QString& citaUuid,
                                   const QString& motivo, bool emergencia);
    Q_INVOKABLE void cerrarConsulta(const QString& consultaUuid, const QString& diagnostico,
                                    const QString& tratamiento, const QString& examen);
    Q_INVOKABLE void emitirReceta(const QString& consultaUuid, const QString& indicaciones,
                                  const QVariantList& lineas, bool descontarStock);
    Q_INVOKABLE void agregarCargo(const QString& origenTipo, const QString& origenUuid,
                                  const QString& concepto, double monto);

    // Catálogos.
    Q_INVOKABLE void guardarServicio(const QString& servicioUuid, const QString& codigo,
                                     const QString& nombre, double precio, bool activo);
    Q_INVOKABLE void guardarExamen(const QString& examenUuid, const QString& codigo,
                                   const QString& nombre, bool activo);

    // Laboratorio.
    Q_INVOKABLE void solicitarLaboratorio(const QString& mascotaUuid,
                                          const QString& consultaUuid,
                                          const QString& examenUuid,
                                          const QString& prioridad, const QString& notas);
    Q_INVOKABLE void iniciarLaboratorio(const QString& solicitudUuid);
    Q_INVOKABLE void resultadoLaboratorio(const QString& solicitudUuid,
                                          const QString& resultado);
    Q_INVOKABLE void entregarLaboratorio(const QString& solicitudUuid);

    // Hospitalización y cirugía.
    Q_INVOKABLE void ingresarHospitalizacion(const QString& mascotaUuid,
                                             const QString& consultaUuid,
                                             const QString& motivo,
                                             const QString& diagnostico,
                                             const QString& area);
    Q_INVOKABLE void registrarEvolucion(const QString& hospitalizacionUuid,
                                        const QString& nota, double peso,
                                        double temperatura, int cardiaca);
    Q_INVOKABLE void darAlta(const QString& hospitalizacionUuid, const QString& estado);
    Q_INVOKABLE void programarCirugia(const QString& mascotaUuid, const QString& consultaUuid,
                                      const QString& hospitalizacionUuid,
                                      const QString& procedimiento, const QString& fecha,
                                      const QString& anestesia);
    Q_INVOKABLE void iniciarCirugia(const QString& cirugiaUuid);
    Q_INVOKABLE void cerrarCirugia(const QString& cirugiaUuid,
                                   const QString& diagnosticoPost, const QString& notas);

    // Estética.
    Q_INVOKABLE void registrarEstetica(const QString& mascotaUuid, const QString& citaUuid,
                                       const QString& servicioUuid, const QString& tipo,
                                       double precio, const QString& observaciones,
                                       bool cobrar);

    // Caja e inventario.
    Q_INVOKABLE void facturarCargos(const QString& clienteUuid,
                                    const QVariantList& cargoUuids,
                                    const QString& metodoPago, double descuento,
                                    double impuesto);
    Q_INVOKABLE void cerrarCaja(double ingresosReales, const QString& observaciones);
    Q_INVOKABLE void ajustarStock(const QString& productoUuid, int delta,
                                  const QString& motivo);

signals:
    void sesionCambiada();
    void datosCambiados();
    void errorCambiado();
    void mensajeCambiado();

private:
    void recargar();
    void aplicar(const fuzzy::core::ResultadoClinico& resultado, const QString& exito);
    void fallo(const QString& texto);
    fuzzy::core::Sesion sesionActual() const;

    fuzzy::core::IQueryExecutor& db_;
    fuzzy::core::ServicioClinico servicio_;
    fuzzy::core::Sesion sesion_;
    QString rol_;
    QVariantList agendaDelDia_;
    QVariantList consultasAbiertas_;
    QVariantList laboratorio_;
    QVariantList hospitalizaciones_;
    QVariantList cirugias_;
    QVariantList estetica_;
    QVariantList cargos_;
    QVariantList examenes_;
    QVariantList servicios_;
    QVariantList productos_;
    QVariantList mascotas_;
    QVariantList clientes_;
    QVariantMap resumen_;
    QString error_;
    QString mensaje_;
};

} // namespace fuzzy::desktop
