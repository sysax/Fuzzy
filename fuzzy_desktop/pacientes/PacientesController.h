#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace fuzzy::desktop {

// Listado, ficha e historial clínico. Lee del SQLite del nodo; no conoce la
// red ni el portal. Los filtros son server-side (SQL) para que la paginación
// siga siendo correcta con muchos registros.
class PacientesController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList pacientes READ pacientes NOTIFY datosCambiados)
    Q_PROPERTY(QVariantList especies READ especies NOTIFY datosCambiados)
    Q_PROPERTY(QString filtro READ filtro NOTIFY datosCambiados)
    Q_PROPERTY(QString especieFiltro READ especieFiltro NOTIFY datosCambiados)
    Q_PROPERTY(int total READ total NOTIFY datosCambiados)
    Q_PROPERTY(int pagina READ pagina NOTIFY datosCambiados)
    Q_PROPERTY(int porPagina READ porPagina CONSTANT)
    Q_PROPERTY(int totalPaginas READ totalPaginas NOTIFY datosCambiados)
    Q_PROPERTY(QVariantMap ficha READ ficha NOTIFY fichaCambiada)
    Q_PROPERTY(QVariantList historial READ historial NOTIFY fichaCambiada)
    Q_PROPERTY(QVariantList vacunas READ vacunas NOTIFY fichaCambiada)
    Q_PROPERTY(QVariantList signos READ signos NOTIFY fichaCambiada)
    Q_PROPERTY(QVariantList galeria READ galeria NOTIFY fichaCambiada)
    Q_PROPERTY(QVariantList historialGlobal READ historialGlobal NOTIFY historialCambiado)
    Q_PROPERTY(QString error READ error NOTIFY errorCambiado)
    Q_PROPERTY(QString mensaje READ mensaje NOTIFY mensajeCambiado)

public:
    explicit PacientesController(fuzzy::core::IQueryExecutor& db, QObject* parent = nullptr);

    QVariantList pacientes() const { return pacientes_; }
    QVariantList especies() const { return especies_; }
    QString filtro() const { return filtro_; }
    QString especieFiltro() const { return especieFiltro_; }
    int total() const { return total_; }
    int pagina() const { return pagina_; }
    int porPagina() const { return porPagina_; }
    int totalPaginas() const;
    QVariantMap ficha() const { return ficha_; }
    QVariantList historial() const { return historial_; }
    QVariantList vacunas() const { return vacunas_; }
    QVariantList signos() const { return signos_; }
    QVariantList galeria() const { return galeria_; }
    QVariantList historialGlobal() const { return historialGlobal_; }
    QString error() const { return error_; }
    QString mensaje() const { return mensaje_; }

    Q_INVOKABLE void cargar();
    Q_INVOKABLE void buscar(const QString& texto);
    Q_INVOKABLE void filtrarEspecie(const QString& especie);
    Q_INVOKABLE void irPagina(int pagina);
    Q_INVOKABLE void seleccionar(const QString& mascotaUuid);
    Q_INVOKABLE void limpiarSeleccion();
    Q_INVOKABLE void buscarHistorial(const QString& texto);
    Q_INVOKABLE void limpiarMensajes();

    Q_INVOKABLE void registrarMascota(const QString& clienteUuid, const QString& nombre,
                                      const QString& especie, const QString& raza,
                                      const QString& sexo, double peso, bool castrado,
                                      const QString& nacimiento, const QString& chip,
                                      const QString& alergias,
                                      const QString& atencionEspecial);
    Q_INVOKABLE void actualizarMascota(const QString& mascotaUuid, const QString& nombre,
                                       const QString& especie, const QString& raza,
                                       const QString& sexo, double peso, bool castrado,
                                       const QString& nacimiento, const QString& chip,
                                       const QString& alergias,
                                       const QString& atencionEspecial,
                                       bool activo);
    Q_INVOKABLE void archivarMascota(const QString& mascotaUuid);
    Q_INVOKABLE void registrarConsulta(const QString& mascotaUuid,
                                       const QString& motivo, const QString& anamnesis,
                                       const QString& examen, const QString& diagnostico,
                                       const QString& tratamiento, bool emergencia);
    Q_INVOKABLE void registrarSignos(const QString& mascotaUuid, double peso,
                                     double temperatura, int cardiaca, int respiratoria,
                                     const QString& notas);
    Q_INVOKABLE void registrarVacuna(const QString& mascotaUuid, const QString& nombre,
                                     const QString& aplicacion, const QString& proxima,
                                     const QString& lote, const QString& notas);

signals:
    void datosCambiados();
    void fichaCambiada();
    void historialCambiado();
    void errorCambiado();
    void mensajeCambiado();

private:
    void recargar();
    void fallo(const QString& texto);
    void exito(const QString& texto);
    qint64 idDe(const QString& tabla, const QString& uuid) const;

    fuzzy::core::IQueryExecutor& db_;
    QVariantList pacientes_;
    QVariantList especies_;
    QString filtro_;
    QString especieFiltro_;
    int total_ = 0;
    int pagina_ = 0;
    int porPagina_ = 10;
    QVariantMap ficha_;
    QVariantList historial_;
    QVariantList vacunas_;
    QVariantList signos_;
    QVariantList galeria_;
    QVariantList historialGlobal_;
    QString error_;
    QString mensaje_;
};

} // namespace fuzzy::desktop
