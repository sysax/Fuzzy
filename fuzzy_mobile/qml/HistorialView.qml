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

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Historia clínica")
                            font.pixelSize: 22
                            font.weight: Font.Bold
                            color: estilo.ink
                        }

                        Label {
                            Layout.fillWidth: true
                            text: movil.mascotaActiva.length > 0
                                  ? qsTr("Filtrada por la mascota seleccionada")
                                  : movil.nombrePortal
                            font.pixelSize: 12
                            color: estilo.secondary
                            elide: Text.ElideRight
                        }
                    }

                    PrimaryButton {
                        visible: movil.mascotaActiva.length > 0
                        text: qsTr("Ver todo")
                        secundario: true
                        implicitWidth: 96
                        onClicked: movil.cerrarMascota()
                    }
                }

                EmptyState {
                    Layout.fillWidth: true
                    visible: movil.historial.length === 0
                    titulo: qsTr("Nada que mostrar")
                    detalle: movil.hayPortales
                            ? qsTr("La clínica irá publicando visitas, vacunas y recetas.")
                            : qsTr("Empareja el portal de tu clínica para ver la historia.")
                }

                Repeater {
                    model: movil.historial

                    Card {
                        Layout.fillWidth: true

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.tipo.toUpperCase()
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                    color: estilo.accent
                                }

                                Label {
                                    text: modelData.fecha
                                    font.pixelSize: 11
                                    color: estilo.muted
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: modelData.titulo
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                                color: estilo.ink
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: text.length > 0
                                text: modelData.detalle
                                font.pixelSize: 13
                                color: estilo.secondary
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                Layout.fillWidth: true
                                text: modelData.mascota
                                font.pixelSize: 12
                                color: estilo.muted
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }
    }
}
