import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Campo de texto con micro-etiqueta en mayúsculas.
ColumnLayout {
    id: root

    property alias label: etiqueta.text
    property alias text: campo.text
    property alias placeholder: campo.placeholderText
    property alias echoMode: campo.echoMode
    property alias maximumLength: campo.maximumLength
    property alias inputMethodHints: campo.inputMethodHints
    property bool password: false

    signal accepted()

    spacing: 6

    Label {
        id: etiqueta
        font.pixelSize: 10
        font.letterSpacing: 1
        font.capitalization: Font.AllUppercase
        color: estilo.textMuted
    }

    TextField {
        id: campo
        Layout.fillWidth: true
        implicitHeight: 42
        color: estilo.ink
        placeholderTextColor: estilo.textMuted
        echoMode: root.password ? TextInput.Password : TextInput.Normal
        selectByMouse: true
        onAccepted: root.accepted()

        background: Rectangle {
            radius: 10
            color: estilo.surface
            border.width: 1
            border.color: campo.activeFocus ? estilo.ink : estilo.border
        }
    }
}
