import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: vacio

    property alias titulo: encabezado.text
    property alias detalle: descripcion.text

    spacing: 8

    Label {
        id: encabezado
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: 18
        font.weight: Font.DemiBold
        color: estilo.ink
        wrapMode: Text.WordWrap
    }

    Label {
        id: descripcion
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: 14
        color: estilo.secondary
        wrapMode: Text.WordWrap
    }
}
