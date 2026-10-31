import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: campo

    property alias etiqueta: titulo.text
    property alias texto: entrada.text
    property alias marcador: entrada.placeholderText
    property alias eco: entrada.echoMode
    property alias entrada: entrada

    spacing: 6

    Label {
        id: titulo
        font.pixelSize: 13
        color: estilo.secondary
    }

    TextField {
        id: entrada
        Layout.fillWidth: true
        implicitHeight: estilo.touch
        font.pixelSize: 15
        leftPadding: 14
        rightPadding: 14
        background: Rectangle {
            radius: 12
            color: estilo.background
            border.width: 1
            border.color: entrada.activeFocus ? estilo.accent : estilo.border
        }
    }
}
