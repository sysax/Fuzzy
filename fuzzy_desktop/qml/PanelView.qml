import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Panel del día: KPIs, agenda, sala de espera, semana y alertas de inventario.
Item {
    id: root

    signal irAgenda()

    Rectangle {
        anchors.fill: parent
        color: estilo.background
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 18

        PageHeader {
            Layout.fillWidth: true
            titulo: qsTr("Panel del día")
            subtitulo: panel.fechaHoy

            SecondaryButton {
                text: qsTr("Ver agenda")
                onClicked: root.irAgenda()
            }
            PrimaryButton {
                text: qsTr("Nueva cita")
                onClicked: root.irAgenda()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            KpiCard { titulo: qsTr("Citas de hoy"); valor: panel.citasHoy; subtexto: qsTr("todas las sedes del nodo"); icono: "calendar3" }
            KpiCard { titulo: qsTr("En espera"); valor: panel.enEspera; subtexto: qsTr("recepción"); icono: "hourglass-split" }
            KpiCard { titulo: qsTr("En curso"); valor: panel.enCurso; subtexto: qsTr("consulta activa"); icono: "file-medical" }
            KpiCard { titulo: qsTr("Ingresos de hoy"); valor: panel.ingresosHoyTexto; subtexto: qsTr("facturas pagadas"); icono: "cash-coin" }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 18

            SectionCard {
                Layout.preferredWidth: 640
                Layout.fillHeight: true
                titulo: qsTr("Agenda de hoy")
                subtitulo: qsTr("%1 citas programadas").arg(panel.agendaHoy.length)

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    EmptyState {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: panel.agendaHoy.length === 0
                        icono: "calendar3"
                        titulo: qsTr("Sin citas para hoy")
                        detalle: qsTr("Crea la primera desde la agenda.")
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: panel.agendaHoy.length > 0
                        clip: true
                        spacing: 2
                        model: panel.agendaHoy

                        delegate: Item {
                            required property var modelData
                            width: ListView.view.width
                            height: 58

                            RowLayout {
                                anchors.fill: parent
                                spacing: 12

                                Label {
                                    Layout.preferredWidth: 52
                                    text: modelData.hora
                                    color: estilo.ink
                                    font.pixelSize: 13
                                    font.bold: true
                                }

                                Rectangle {
                                    Layout.preferredWidth: 3
                                    Layout.preferredHeight: 34
                                    radius: 2
                                    color: modelData.emergencia ? estilo.danger
                                         : modelData.estado === "En curso" ? estilo.ink
                                         : estilo.border
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1

                                    Label {
                                        text: modelData.mascota + " · " + modelData.tipo
                                        color: estilo.ink
                                        font.pixelSize: 13
                                        font.bold: true
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    Label {
                                        text: modelData.propietario
                                        color: estilo.textSecondary
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }

                                StatusChip { estado: modelData.estado }
                            }

                            Rectangle {
                                anchors.bottom: parent.bottom
                                width: parent.width
                                height: 1
                                color: estilo.border
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 18

                SectionCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 214
                    titulo: qsTr("Sala de espera")

                    EmptyState {
                        anchors.fill: parent
                        visible: panel.salaEspera.length === 0
                        icono: "hourglass-split"
                        titulo: qsTr("Nadie esperando")
                    }

                    ListView {
                        anchors.fill: parent
                        visible: panel.salaEspera.length > 0
                        clip: true
                        model: panel.salaEspera

                        delegate: RowLayout {
                            required property var modelData
                            width: ListView.view.width
                            height: 40
                            spacing: 8

                            Rectangle {
                                width: 28
                                height: 28
                                radius: 14
                                color: estilo.chip
                                Label {
                                    anchors.centerIn: parent
                                    text: modelData.mascota.charAt(0).toUpperCase()
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: estilo.textSecondary
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Label {
                                    text: modelData.mascota
                                    font.pixelSize: 12
                                    font.bold: true
                                    color: estilo.ink
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: modelData.hora + " · " + modelData.estado
                                    font.pixelSize: 10
                                    color: estilo.textMuted
                                }
                            }

                            StatusChip { estado: modelData.estado }
                        }
                    }
                }

                SectionCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    titulo: qsTr("Citas de la semana")

                    RowLayout {
                        anchors.fill: parent
                        spacing: 8

                        Repeater {
                            model: panel.graficoSemana

                            delegate: ColumnLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 4

                                Item {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true

                                    Rectangle {
                                        anchors.bottom: parent.bottom
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        width: Math.min(parent.width * 0.52, 30)
                                        height: Math.max(4, parent.height * modelData.porcentaje / 100)
                                        radius: 5
                                        color: modelData.total > 0 ? estilo.ink : estilo.chip
                                    }
                                }

                                Label {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: modelData.dia
                                    font.pixelSize: 10
                                    color: estilo.textSecondary
                                    font.capitalization: Font.Capitalize
                                }

                                Label {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: modelData.total
                                    font.pixelSize: 10
                                    font.bold: true
                                    color: estilo.ink
                                }
                            }
                        }
                    }
                }

                SectionCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 188
                    titulo: qsTr("Alertas de inventario")

                    EmptyState {
                        anchors.fill: parent
                        visible: panel.alertasInventario.length === 0
                        icono: "check-circle"
                        titulo: qsTr("Stock saludable")
                    }

                    ListView {
                        anchors.fill: parent
                        visible: panel.alertasInventario.length > 0
                        clip: true
                        model: panel.alertasInventario

                        delegate: RowLayout {
                            required property var modelData
                            width: ListView.view.width
                            height: 34
                            spacing: 8

                            AppIcon { nombre: "exclamation-triangle"; color: estilo.warning; tamano: 14 }
                            Label {
                                Layout.fillWidth: true
                                text: modelData.nombre
                                font.pixelSize: 12
                                color: estilo.ink
                                elide: Text.ElideRight
                            }
                            Label {
                                text: modelData.stock + " / " + modelData.minimo
                                font.pixelSize: 11
                                color: estilo.warning
                                font.bold: true
                            }
                        }
                    }
                }
            }
        }
    }
}
