import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Estado vacío con icono y texto, para listas sin resultados.
Item {
    id: root

    property string icono: "circle"
    property string titulo: qsTr("Sin datos")
    property string detalle: ""

    implicitHeight: 180

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(implicitWidth, Math.max(0, root.width - 40))
        spacing: 8

        AppIcon {
            Layout.alignment: Qt.AlignHCenter
            nombre: root.icono
            color: estilo.textMuted
            tamano: 30
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: root.titulo
            font.pixelSize: 15
            font.bold: true
            color: estilo.textSecondary
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: root.detalle
            font.pixelSize: 12
            color: estilo.textMuted
            wrapMode: Text.WordWrap
            visible: text.length > 0
        }
    }
}
