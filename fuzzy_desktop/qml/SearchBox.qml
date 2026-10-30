import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Buscador con icono, integrado en el sistema de diseño.
Rectangle {
    id: root

    property alias text: campo.text
    property string placeholder: qsTr("Buscar…")

    signal editado(string texto)

    implicitHeight: 42
    implicitWidth: 280
    radius: 10
    color: estilo.surface
    border.width: 1
    border.color: campo.activeFocus ? estilo.ink : estilo.border

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 10
        spacing: 8

        AppIcon {
            nombre: "search"
            color: estilo.textMuted
            tamano: 15
        }

        TextField {
            id: campo
            Layout.fillWidth: true
            placeholderText: root.placeholder
            placeholderTextColor: estilo.textMuted
            color: estilo.ink
            background: null
            selectByMouse: true
            onTextChanged: root.editado(text)
        }
    }
}
