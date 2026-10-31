import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Contenedor con barra inferior. El contenido son páginas QML que solo
// enlazan propiedades del controlador y emiten intenciones.
Page {
    id: shell

    property int seccion: 0
    property var paginas: [
        { titulo: qsTr("Inicio"), archivo: "InicioView.qml" },
        { titulo: qsTr("Citas"), archivo: "CitasView.qml" },
        { titulo: qsTr("Historial"), archivo: "HistorialView.qml" },
        { titulo: qsTr("Portales"), archivo: "PortalesView.qml" },
        { titulo: qsTr("Ajustes"), archivo: "AjustesView.qml" }
    ]

    background: Rectangle { color: estilo.background }

    function ir(indice) {
        if (indice === shell.seccion && pila.depth === 1)
            return
        shell.seccion = indice
        while (pila.depth > 1)
            pila.pop()
        pila.replace(null, shell.paginas[indice].archivo)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Cabecera: clínica activa y estado de sincronización.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 68
            color: estilo.surface
            border.width: 0

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: estilo.spacing
                anchors.rightMargin: estilo.spacing
                spacing: estilo.spacing / 2

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: movil.hayPortales ? movil.nombrePortal
                                                : qsTr("Sin clínica emparejada")
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        color: estilo.ink
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: movil.modoAvion
                              ? qsTr("Modo avión")
                              : movil.sinRed
                                ? qsTr("Sin conexión · %1 en cola").arg(movil.pendientes)
                                : movil.pendientes > 0
                                  ? qsTr("%1 cambio(s) por enviar").arg(movil.pendientes)
                                  : qsTr("Al día")
                        font.pixelSize: 12
                        color: movil.sinRed || movil.modoAvion ? estilo.warning
                                                               : estilo.secondary
                    }
                }

                PrimaryButton {
                    text: qsTr("Sincronizar")
                    secundario: true
                    implicitWidth: 112
                    enabled: movil.hayPortales && !movil.modoAvion
                    onClicked: movil.sincronizar()
                }
            }
        }

        StackView {
            id: pila
            Layout.fillWidth: true
            Layout.fillHeight: true
            initialItem: "InicioView.qml"

            pushEnter: Transition { NumberAnimation { property: "x"; from: 40; to: 0; duration: 180 } }
            pushExit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 180 } }
            popEnter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180 } }
            popExit: Transition { NumberAnimation { property: "x"; from: 0; to: 40; duration: 180 } }
        }

        // Barra inferior.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 64
            color: estilo.surface
            border.color: estilo.border
            border.width: 1

            RowLayout {
                anchors.fill: parent
                spacing: 0

                Repeater {
                    model: shell.paginas.length

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 2

                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 4
                                radius: 2
                                color: shell.seccion === index ? estilo.accent : "transparent"
                            }

                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                text: shell.paginas[index].titulo
                                font.pixelSize: 11
                                font.weight: shell.seccion === index ? Font.DemiBold
                                                                     : Font.Normal
                                color: shell.seccion === index ? estilo.accent
                                                               : estilo.secondary
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: shell.ir(index)
                        }
                    }
                }
            }
        }
    }
}
