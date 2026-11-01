#pragma once

#include "fuzzy/core/IQueryExecutor.h"
#include "fuzzy/core/Permisos.h"
#include "fuzzy/core/Politicas.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace fuzzy::core {

struct ResultadoClinico
{
    bool ok = false;
    QString error;
    QString uuid;
};

// Una línea de receta. El precio se congela al emitir: cambiar el catálogo
// después no reescribe recetas ya dispensadas.
struct LineaReceta
{
    QString producto_uuid;
    QString medicamento;
    QString dosis;
    QString frecuencia;
    QString duracion;
    int cantidad = 1;
    double precio = 0.0;
};

// Flujo clínico de la fase 9. Toda escritura pasa por aquí para que el rol
// salga de la sesión y no del formulario. El controlador de escritorio solo
// traduce QML → estructuras; las reglas viven en este servicio.
class ServicioClinico
{
public:
    explicit ServicioClinico(IQueryExecutor& clinica, Politicas politicas = Politicas());

    // Recepción y triaje (Recepción/Admin).
    ResultadoClinico recibir(const Sesion& sesion, const QString& cita_uuid);
    ResultadoClinico registrarTriaje(const Sesion& sesion, const QString& mascota_uuid,
                                     const QString& consulta_uuid, double peso,
                                     double temperatura, int frecuencia_cardiaca,
                                     int frecuencia_respiratoria, const QString& notas);

    // Consulta, receta y cargos (Veterinario/Admin).
    ResultadoClinico abrirConsulta(const Sesion& sesion, const QString& mascota_uuid,
                                   const QString& cita_uuid, const QString& motivo,
                                   bool emergencia);
    ResultadoClinico cerrarConsulta(const Sesion& sesion, const QString& consulta_uuid,
                                    const QString& diagnostico, const QString& tratamiento,
                                    const QString& examen_fisico);
    ResultadoClinico emitirReceta(const Sesion& sesion, const QString& consulta_uuid,
                                  const QString& indicaciones,
                                  const QList<LineaReceta>& lineas, bool descontarStock);
    ResultadoClinico agregarCargo(const Sesion& sesion, const QString& origen_tipo,
                                  const QString& origen_uuid, const QString& concepto,
                                  double monto);

    // Catálogos clínicos (Admin).
    ResultadoClinico guardarServicio(const Sesion& sesion, const QString& servicio_uuid,
                                     const QString& codigo, const QString& nombre,
                                     double precio, bool activo);
    ResultadoClinico guardarTipoExamen(const Sesion& sesion, const QString& examen_uuid,
                                       const QString& codigo, const QString& nombre,
                                       bool activo);

    // Laboratorio.
    ResultadoClinico solicitarLaboratorio(const Sesion& sesion, const QString& mascota_uuid,
                                          const QString& consulta_uuid,
                                          const QString& tipo_examen_uuid,
                                          const QString& prioridad, const QString& notas);
    ResultadoClinico iniciarLaboratorio(const Sesion& sesion, const QString& solicitud_uuid);
    ResultadoClinico registrarResultadoLaboratorio(const Sesion& sesion,
                                                   const QString& solicitud_uuid,
                                                   const QString& resultado);
    ResultadoClinico entregarLaboratorio(const Sesion& sesion, const QString& solicitud_uuid);

    // Hospitalización y cirugía.
    ResultadoClinico ingresarHospitalizacion(const Sesion& sesion,
                                             const QString& mascota_uuid,
                                             const QString& consulta_uuid,
                                             const QString& motivo,
                                             const QString& diagnostico,
                                             const QString& area);
    ResultadoClinico registrarEvolucion(const Sesion& sesion,
                                        const QString& hospitalizacion_uuid,
                                        const QString& nota, double peso,
                                        double temperatura, int frecuencia_cardiaca);
    ResultadoClinico darAltaHospitalizacion(const Sesion& sesion,
                                            const QString& hospitalizacion_uuid,
                                            const QString& estado);
    ResultadoClinico programarCirugia(const Sesion& sesion, const QString& mascota_uuid,
                                      const QString& consulta_uuid,
                                      const QString& hospitalizacion_uuid,
                                      const QString& procedimiento,
                                      const QString& fecha_programada,
                                      const QString& anestesia);
    ResultadoClinico iniciarCirugia(const Sesion& sesion, const QString& cirugia_uuid);
    ResultadoClinico cerrarCirugia(const Sesion& sesion, const QString& cirugia_uuid,
                                   const QString& diagnostico_post, const QString& notas);

    // Estética.
    ResultadoClinico registrarEstetica(const Sesion& sesion, const QString& mascota_uuid,
                                       const QString& cita_uuid, const QString& servicio_uuid,
                                       const QString& tipo, double precio,
                                       const QString& observaciones, bool cobrar);

    // Caja e inventario.
    ResultadoClinico facturarCargos(const Sesion& sesion, const QString& cliente_uuid,
                                    const QVariantList& cargo_uuids,
                                    const QString& metodo_pago, double descuento,
                                    double impuesto);
    ResultadoClinico cerrarCaja(const Sesion& sesion, double ingresos_reales,
                                const QString& observaciones);
    ResultadoClinico ajustarStock(const Sesion& sesion, const QString& producto_uuid,
                                  int delta, const QString& motivo);

    static QVariantList filasDeCargos(IQueryExecutor& clinica, const QString& cliente_uuid);

private:
    ResultadoClinico denegado(const Sesion& sesion, Permiso permiso,
                              const QString& recurso) const;
    bool permitido(const Sesion& sesion, Permiso permiso, Accion accion,
                   const QString& recurso) const;
    qint64 idDe(const QString& tabla, const QString& uuid,
                const QString& extra = QString()) const;
    qint64 usuarioId(const Sesion& sesion) const;
    QString clinicaUuid() const;
    void auditar(const Sesion& sesion, const QString& accion, const QString& entidad,
                 const QString& entidad_uuid, const QString& detalle);
    bool crearCargo(const QString& origen_tipo, qint64 origen_id,
                    const QString& origen_uuid, const QString& concepto, double monto,
                    QString* error);

    IQueryExecutor& clinica_;
    Politicas politicas_;
};

} // namespace fuzzy::core
