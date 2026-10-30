#pragma once

#include "fuzzy/core/IQueryExecutor.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace fuzzy::desktop {

class PropietariosController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList propietarios READ propietarios NOTIFY datosCambiados)
    Q_PROPERTY(QString filtro READ filtro NOTIFY datosCambiados)
    Q_PROPERTY(int total READ total NOTIFY datosCambiados)
    Q_PROPERTY(int conMascotas READ conMascotas NOTIFY datosCambiados)
    Q_PROPERTY(QVariantMap seleccionado READ seleccionado NOTIFY seleccionCambiada)
    Q_PROPERTY(QVariantList mascotas READ mascotas NOTIFY seleccionCambiada)
    Q_PROPERTY(QString error READ error NOTIFY errorCambiado)
    Q_PROPERTY(QString mensaje READ mensaje NOTIFY mensajeCambiado)

public:
    explicit PropietariosController(fuzzy::core::IQueryExecutor& db, QObject* parent = nullptr);

    QVariantList propietarios() const { return propietarios_; }
    QString filtro() const { return filtro_; }
    int total() const { return total_; }
    int conMascotas() const { return conMascotas_; }
    QVariantMap seleccionado() const { return seleccionado_; }
    QVariantList mascotas() const { return mascotas_; }
    QString error() const { return error_; }
    QString mensaje() const { return mensaje_; }

    Q_INVOKABLE void cargar();
    Q_INVOKABLE void buscar(const QString& texto);
    Q_INVOKABLE void seleccionar(const QString& clienteUuid);
    Q_INVOKABLE void limpiarSeleccion();
    Q_INVOKABLE void limpiarMensajes();
    Q_INVOKABLE void crear(const QString& nombre, const QString& telefono,
                           const QString& correo, const QString& direccion,
                           const QString& dni);
    Q_INVOKABLE void actualizar(const QString& clienteUuid, const QString& nombre,
                                const QString& telefono, const QString& correo,
                                const QString& direccion, const QString& dni, bool activo);

signals:
    void datosCambiados();
    void seleccionCambiada();
    void errorCambiado();
    void mensajeCambiado();

private:
    void recargar();
    void fallo(const QString& texto);
    void exito(const QString& texto);

    fuzzy::core::IQueryExecutor& db_;
    QVariantList propietarios_;
    QString filtro_;
    int total_ = 0;
    int conMascotas_ = 0;
    QVariantMap seleccionado_;
    QVariantList mascotas_;
    QString error_;
    QString mensaje_;
};

} // namespace fuzzy::desktop
