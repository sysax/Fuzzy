import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Cabecera de tabla reutilizable. Las vistas declaran sus columnas y anchos y
// esta fila se encarga del estilo; el cuerpo sigue siendo un ListView.
Rectangle {
    id: root

    property var columnas: []
    property var anchos: []

    implicitHeight: 42
    color: estilo.background

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        spacing: 12

        Repeater {
            model: root.columnas.length

            Label {
                required property int index

                Layout.preferredWidth: index < root.anchos.length ? root.anchos[index] : -1
                Layout.fillWidth: index >= root.anchos.length
                text: root.columnas[index]
                font.pixelSize: 10
                font.letterSpacing: 1
                color: estilo.textMuted
                elide: Text.ElideRight
            }
        }
    }
}
