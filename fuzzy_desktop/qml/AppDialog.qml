import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Diálogo modal del sistema de diseño. El contenido se declara como hijo; el
// pie se compone con los botones que se pasen en `acciones`.
Dialog {
    id: root

    property string titulo: ""
    property string descripcion: ""
    property alias acciones: pie.data
    property alias contenido: cuerpo.data

    default property alias datos: cuerpo.data

    modal: true
    anchors.centerIn: parent
    width: Math.min(560, parent ? parent.width - 80 : 560)
    padding: 24
    closePolicy: Popup.CloseOnEscape
    standardButtons: Dialog.NoButton

    background: Rectangle {
        radius: estilo.radiusCard
        color: estilo.surface
        border.width: 1
        border.color: estilo.border
    }

    header: ColumnLayout {
        spacing: 4

        Label {
            text: root.titulo
            font.pixelSize: 20
            font.bold: true
            color: estilo.ink
        }

        Label {
            text: root.descripcion
            font.pixelSize: 12
            color: estilo.textSecondary
            visible: text.length > 0
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    contentItem: ColumnLayout {
        id: cuerpo
        spacing: 12
    }

    footer: RowLayout {
        spacing: 8
        Item { Layout.fillWidth: true }
        RowLayout {
            id: pie
            spacing: 8
        }
    }
}
