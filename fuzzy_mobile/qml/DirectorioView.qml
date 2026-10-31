import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: pagina

    background: Rectangle { color: estilo.background }

    property string clinicaElegida: ""
    property string clinicaNombre: ""

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

                    PrimaryButton {
                        visible: StackView.view && StackView.view.depth > 1
                        text: qsTr("Volver")
                        secundario: true
                        implicitWidth: 88
                        onClicked: StackView.view.pop()
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Primera visita")
                        font.pixelSize: 22
                        font.weight: Font.Bold
                        color: estilo.ink
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Elige la clínica y deja tus datos. La clínica te "
                              + "contactará; todavía no necesitas portal.")
                    font.pixelSize: 13
                    color: estilo.secondary
                    wrapMode: Text.WordWrap
                }

                Field {
                    Layout.fillWidth: true
                    etiqueta: qsTr("Buscar clínica")
                    marcador: qsTr("Nombre, ciudad o dirección")
                    onTextoChanged: movil.buscarDirectorio(texto)
                }

                EmptyState {
                    Layout.fillWidth: true
                    visible: movil.directorio.length === 0
                    titulo: qsTr("No hay clínicas")
                    detalle: qsTr("Actualiza el directorio o cambia la búsqueda.")
                }

                Repeater {
                    model: movil.directorio

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 88
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
                                text: modelData.nombre
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                                color: estilo.ink
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: (modelData.ciudad ? modelData.ciudad + " · " : "")
                                      + (modelData.direccion || "")
                                font.pixelSize: 12
                                color: estilo.secondary
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: modelData.telefono
                                font.pixelSize: 12
                                color: estilo.accent
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                pagina.clinicaElegida = modelData.uuid
                                pagina.clinicaNombre = modelData.nombre
                                dialogoAlta.open()
                            }
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.topMargin: estilo.spacing / 2
                    visible: movil.altas.length > 0
                    text: qsTr("Solicitudes enviadas")
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    color: estilo.ink
                }

                Repeater {
                    model: movil.altas

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 68
                        radius: 12
                        color: estilo.surface
                        border.width: 1
                        border.color: modelData.estado === "enviada"
                                             ? estilo.border : estilo.warning

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 2

                            Label {
                                Layout.fillWidth: true
                                text: modelData.clinica + " · " + modelData.fecha
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                color: estilo.ink
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: modelData.estado === "enviada"
                                      ? qsTr("Enviada. La clínica te contactará.")
                                      : modelData.error && modelData.error.length > 0
                                        ? modelData.error
                                        : qsTr("Pendiente de enviar")
                                font.pixelSize: 12
                                color: modelData.estado === "enviada"
                                       ? estilo.success : estilo.warning
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                PrimaryButton {
                    Layout.fillWidth: true
                    visible: movil.altas.length > 0
                    text: qsTr("Reintentar solicitudes pendientes")
                    secundario: true
                    onClicked: movil.reintentarAltas()
                }

                Item { Layout.fillHeight: true }
            }
        }
    }

    AltaDialog {
        id: dialogoAlta
        clinica: pagina.clinicaElegida
        clinicaNombre: pagina.clinicaNombre
    }
}
