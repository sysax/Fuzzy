import QtQuick
import QtQuick.Controls

// Botón secundario: contorno neutro sobre superficie.
Button {
    id: root

    implicitHeight: 42

    background: Rectangle {
        radius: estilo.radiusPill
        color: root.hovered ? estilo.chip : estilo.surface
        border.width: 1
        border.color: estilo.border
    }

    contentItem: Text {
        text: root.text
        color: root.enabled ? estilo.ink : estilo.textMuted
        font.pixelSize: 14
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
