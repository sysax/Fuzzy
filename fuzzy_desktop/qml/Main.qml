import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Ventana raíz. Decide entre el acceso (bootstrap, login o cambio de la
// contraseña temporal) y el shell autenticado.
ApplicationWindow {
    id: root

    property string appVersion: ""
    property string modoAcceso: "login"

    width: 1440
    height: 900
    minimumWidth: 1100
    minimumHeight: 720
    visible: true
    title: qsTr("FuzzyCloud")
    color: estilo.background

    function recargarTodo() {
        panel.cargar()
        agenda.cargar()
        pacientes.cargar()
        pacientes.buscarHistorial("")
        propietarios.cargar()
        inventario.cargar()
        facturacion.cargar()
        facturacion.usuarioUuid = sesion.usuarioUuid
        ajustes.cargar()
        vinculos.cargar()
    }

    function modoActual() {
        if (sesion.debeCambiarPassword)
            return "cambio"
        return root.modoAcceso
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: sesion.autenticado && !sesion.debeCambiarPassword ? 1 : 0

        AccessView {
            id: acceso
            modo: root.modoActual()
            onCambiarModo: function (nuevo) { root.modoAcceso = nuevo }
        }

        ShellView {
            id: shell
        }
    }

    Connections {
        target: sesion

        function onSesionCambiada() {
            if (sesion.autenticado) {
                root.modoAcceso = "login"
                root.recargarTodo()
                shell.seleccion = 0
            } else {
                root.modoAcceso = sesion.hayInstalacion ? "login" : "bootstrap"
            }
        }

        function onInstalacionCambiada() {
            if (!sesion.autenticado && !sesion.hayInstalacion)
                root.modoAcceso = "bootstrap"
        }
    }

    Component.onCompleted: {
        root.modoAcceso = sesion.hayInstalacion ? "login" : "bootstrap"
        if (sesion.autenticado)
            root.recargarTodo()
    }
}
