import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Ventana raíz del teléfono. Decide entre el bloqueo (crear clave o
// desbloquear) y el contenido ya descifrado.
ApplicationWindow {
    id: raiz

    property string appVersion: ""

    width: 420
    height: 840
    minimumWidth: 360
    minimumHeight: 640
    visible: true
    title: qsTr("FuzzyCloud")
    color: estilo.background

    StackLayout {
        anchors.fill: parent
        currentIndex: movil.desbloqueada ? 1 : 0

        BloqueoView {}

        ShellView {}
    }

    // Avisos del controlador. No son lógica: solo muestran el mensaje ya
    // calculado por el core (validación, cola, revocación).
    Rectangle {
        id: aviso

        readonly property bool hayTexto: movil.error.length > 0 || movil.mensaje.length > 0

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: estilo.spacing
        height: visible ? textoAviso.implicitHeight + estilo.spacing * 1.5 : 0
        visible: hayTexto
        radius: 12
        color: movil.error.length > 0 ? estilo.dangerSoft : estilo.successSoft
        border.width: 1
        border.color: movil.error.length > 0 ? estilo.danger : estilo.success
        z: 10

        Label {
            id: textoAviso
            anchors.fill: parent
            anchors.margins: estilo.spacing * 0.75
            text: movil.error.length > 0 ? movil.error : movil.mensaje
            color: movil.error.length > 0 ? estilo.danger : estilo.success
            font.pixelSize: 13
            wrapMode: Text.WordWrap
            verticalAlignment: Text.AlignVCenter
        }

        MouseArea {
            anchors.fill: parent
            onClicked: movil.limpiarMensajes()
        }
    }
}
