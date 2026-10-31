import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: pagina

    background: Rectangle { color: estilo.background }

    ScrollView {
        anchors.fill: parent
        contentWidth: contenido.width
        clip: true

        ColumnLayout {
            id: contenido
            width: pagina.width
            spacing: estilo.spacing

            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: estilo.spacing
                spacing: estilo.spacing

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Mis citas")
                        font.pixelSize: 22
                        font.weight: Font.Bold
                        color: estilo.ink
                    }

                    PrimaryButton {
                        text: qsTr("Pedir cita")
                        implicitWidth: 112
                        enabled: movil.mascotas.length > 0
                        onClicked: dialogoCita.open()
                    }
                }

                EmptyState {
                    Layout.fillWidth: true
                    visible: movil.citas.length === 0 && movil.solicitudes.length === 0
                    titulo: qsTr("Sin citas")
                    detalle: movil.hayPortales
                            ? qsTr("Pide una cita y la clínica la confirmará.")
                            : qsTr("Empareja el portal de tu clínica para ver y pedir citas.")
                }

                Repeater {
                    model: movil.citas

                    Card {
                        Layout.fillWidth: true

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: estilo.spacing

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.fecha + " · " + modelData.hora
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                    color: estilo.ink
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.tipo_servicio + " · " + modelData.mascota
                                    font.pixelSize: 13
                                    color: estilo.secondary
                                    elide: Text.ElideRight
                                }
                            }

                            Rectangle {
                                implicitWidth: 92
                                implicitHeight: 28
                                radius: 14
                                color: modelData.estado === "Cancelada"
                                       ? estilo.dangerSoft
                                       : modelData.estado === "Completada"
                                         ? estilo.successSoft : estilo.accentSoft

                                Label {
                                    anchors.centerIn: parent
                                    text: modelData.estado
                                    font.pixelSize: 11
                                    color: modelData.estado === "Cancelada"
                                           ? estilo.danger
                                           : modelData.estado === "Completada"
                                             ? estilo.success : estilo.accent
                                }
                            }
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.topMargin: estilo.spacing / 2
                    visible: movil.solicitudes.length > 0
                    text: qsTr("Solicitudes enviadas")
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    color: estilo.ink
                }

                Repeater {
                    model: movil.solicitudes

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 76
                        radius: 12
                        color: estilo.surface
                        border.width: 1
                        border.color: estilo.border

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 2

                            Label {
                                Layout.fillWidth: true
                                text: modelData.fecha + " · " + modelData.hora
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                color: estilo.ink
                            }

                            Label {
                                Layout.fillWidth: true
                                text: modelData.tipo_servicio + " · " + modelData.mascota
                                font.pixelSize: 12
                                color: estilo.secondary
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: modelData.estado
                                      + (modelData.respuesta
                                         ? " · " + modelData.respuesta : "")
                                font.pixelSize: 12
                                color: modelData.estado === "Rechazada"
                                       ? estilo.danger : estilo.accent
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }
    }

    SolicitudCitaDialog {
        id: dialogoCita
    }
}
