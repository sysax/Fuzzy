import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Tarjeta base del teléfono: una sola sombra suave y bordes redondeados.
Rectangle {
    id: card

    default property alias contenido: cuerpo.data

    color: estilo.surface
    radius: estilo.radius
    border.color: estilo.border
    border.width: 1

    implicitHeight: cuerpo.implicitHeight + estilo.spacing * 2

    ColumnLayout {
        id: cuerpo
        anchors.fill: parent
        anchors.margins: estilo.spacing
        spacing: estilo.spacing / 2
    }
}
