import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Tarjeta de indicador.
Rectangle {
    id: card

    property string titulo: ""
    property string valor: ""
    property string subtexto: ""
    property string icono: ""
    property color acento: estilo.ink

    Layout.fillWidth: true
    implicitHeight: 104
    radius: estilo.radiusCard
    color: estilo.surface
    border.color: estilo.border

    AppIcon {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 16
        anchors.rightMargin: 16
        nombre: card.icono
        color: estilo.textMuted
        tamano: 20
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 2

        Label {
            text: card.titulo
            color: estilo.textSecondary
            font.pixelSize: 12
        }

        Label {
            Layout.fillWidth: true
            Layout.rightMargin: 28
            text: card.valor
            color: card.acento
            font.pixelSize: 26
            font.bold: true
            elide: Text.ElideRight
        }

        Label {
            text: card.subtexto
            color: estilo.textMuted
            font.pixelSize: 11
            visible: text.length > 0
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        Item { Layout.fillHeight: true }
    }
}
