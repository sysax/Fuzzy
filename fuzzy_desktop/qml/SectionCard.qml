import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Superficie blanca con borde y, opcionalmente, un encabezado.
Rectangle {
    id: root

    property string titulo: ""
    property string subtitulo: ""
    property int margen: 18

    default property alias datos: contenido.data

    radius: estilo.radiusCard
    color: estilo.surface
    border.width: 1
    border.color: estilo.border

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.margen
        spacing: 12

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            visible: root.titulo.length > 0

            Label {
                text: root.titulo
                font.pixelSize: 16
                font.bold: true
                color: estilo.ink
            }

            Label {
                text: root.subtitulo
                font.pixelSize: 12
                color: estilo.textSecondary
                visible: text.length > 0
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }

        Item {
            id: contenido
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
