import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: boton

    property bool secundario: false
    property bool peligro: false

    implicitHeight: estilo.touch
    implicitWidth: Math.max(120, contenidoTexto.implicitWidth + estilo.spacing * 3)
    font.pixelSize: 15
    font.weight: Font.DemiBold

    background: Rectangle {
        radius: estilo.radius
        color: !boton.enabled
               ? estilo.border
               : boton.peligro
                 ? estilo.dangerSoft
                 : boton.secundario
                   ? estilo.surface
                   : estilo.accent
        border.width: boton.secundario || boton.peligro ? 1 : 0
        border.color: boton.peligro ? estilo.danger : estilo.border
    }

    contentItem: Text {
        id: contenidoTexto
        text: boton.text
        font: boton.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: !boton.enabled
               ? estilo.muted
               : boton.peligro
                 ? estilo.danger
                 : boton.secundario
                   ? estilo.ink
                   : "white"
    }
}
