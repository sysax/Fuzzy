import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Encabezado de página con título, subtítulo y acciones alineadas a la derecha.
Item {
    id: root

    property string titulo: ""
    property string subtitulo: ""
    default property alias acciones: acciones.data

    implicitHeight: 58

    ColumnLayout {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        Label {
            text: root.titulo
            font.pixelSize: 24
            font.bold: true
            color: estilo.ink
        }

        Label {
            text: root.subtitulo
            font.pixelSize: 13
            color: estilo.textSecondary
            visible: text.length > 0
        }
    }

    RowLayout {
        id: acciones
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
    }
}
