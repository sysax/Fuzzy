import QtQuick
import QtQuick.Controls

// Botón primario del sistema de diseño (pill, relleno `ink`).
Button {
    id: root

    property color fillColor: estilo.ink
    property color textColor: "#FFFFFF"

    implicitHeight: 42

    background: Rectangle {
        radius: estilo.radiusPill
        color: !root.enabled ? estilo.textMuted
             : root.hovered ? Qt.darker(root.fillColor, 1.2) : root.fillColor
    }

    contentItem: Text {
        text: root.text
        color: root.textColor
        font.pixelSize: 14
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
