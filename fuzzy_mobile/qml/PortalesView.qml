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

                    PrimaryButton {
                        visible: StackView.view && StackView.view.depth > 1
                        text: qsTr("Volver")
                        secundario: true
                        implicitWidth: 88
                        onClicked: StackView.view.pop()
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Mis portales")
                        font.pixelSize: 22
                        font.weight: Font.Bold
                        color: estilo.ink
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Un portal por clínica. Cada uno se empareja con su propio "
                              + "código y se descifra con la clave de este teléfono.")
                    font.pixelSize: 13
                    color: estilo.secondary
                    wrapMode: Text.WordWrap
                }

                EmptyState {
                    Layout.fillWidth: true
                    visible: movil.portales.length === 0
                    titulo: qsTr("Sin portales")
                    detalle: qsTr("Pide en recepción el código QR de tu portal y pégalo aquí.")
                }

                Repeater {
                    model: movil.portales

                    Card {
                        Layout.fillWidth: true

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: estilo.spacing / 2

                            RowLayout {
                                Layout.fillWidth: true

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.clinica
                                        font.pixelSize: 16
                                        font.weight: Font.DemiBold
                                        color: estilo.ink
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.estado === "activo"
                                              ? qsTr("Última sincronización: %1")
                                                    .arg(modelData.ultima_sync || qsTr("nunca"))
                                              : modelData.estado === "revocado"
                                                ? qsTr("Acceso revocado: pide re-emparejar")
                                                : qsTr("Portal inaccesible")
                                        font.pixelSize: 12
                                        color: modelData.estado === "activo"
                                               ? estilo.secondary : estilo.warning
                                        wrapMode: Text.WordWrap
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: modelData.pendientes > 0
                                        text: qsTr("%1 cambio(s) por enviar")
                                            .arg(modelData.pendientes)
                                        font.pixelSize: 12
                                        color: estilo.accent
                                    }
                                }

                                Rectangle {
                                    visible: modelData.activo
                                    implicitWidth: 62
                                    implicitHeight: 26
                                    radius: 13
                                    color: estilo.accentSoft

                                    Label {
                                        anchors.centerIn: parent
                                        text: qsTr("Activo")
                                        font.pixelSize: 11
                                        color: estilo.accent
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: estilo.spacing / 2

                                PrimaryButton {
                                    Layout.fillWidth: true
                                    text: qsTr("Consultar")
                                    secundario: true
                                    enabled: !modelData.activo
                                    onClicked: movil.seleccionarPortal(modelData.uuid)
                                }

                                PrimaryButton {
                                    Layout.fillWidth: true
                                    text: qsTr("Olvidar")
                                    peligro: true
                                    onClicked: {
                                        portalAOlvidar = modelData.uuid
                                        confirmarOlvido.open()
                                    }
                                }
                            }
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Emparejar un portal")
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        color: estilo.ink
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Pega el contenido del QR que muestra la clínica.")
                        font.pixelSize: 13
                        color: estilo.secondary
                        wrapMode: Text.WordWrap
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 96

                        TextArea {
                            id: qr
                            placeholderText: qsTr("{\"v\":1,\"url\":…}")
                            wrapMode: TextArea.Wrap
                            font.pixelSize: 12
                        }
                    }

                    PrimaryButton {
                        Layout.fillWidth: true
                        text: qsTr("Leer código")
                        enabled: qr.text.trim().length > 0
                        onClicked: {
                            if (movil.leerQr(qr.text))
                                qr.text = ""
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        visible: movil.huellaPendiente.length > 0
                        implicitHeight: confirmacionHuella.implicitHeight + estilo.spacing * 1.5
                        radius: 12
                        color: estilo.accentSoft

                        ColumnLayout {
                            id: confirmacionHuella
                            anchors.fill: parent
                            anchors.margins: estilo.spacing * 0.75
                            spacing: 6

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Confirma con el operador que la huella es %1")
                                    .arg(movil.huellaPendiente)
                                font.pixelSize: 15
                                font.weight: Font.Bold
                                color: estilo.accent
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                Layout.fillWidth: true
                                text: movil.clinicaPendiente
                                font.pixelSize: 13
                                color: estilo.secondary
                            }

                            RowLayout {
                                Layout.fillWidth: true

                                PrimaryButton {
                                    Layout.fillWidth: true
                                    text: qsTr("No coincide")
                                    secundario: true
                                    onClicked: movil.cancelarEmparejamiento()
                                }

                                PrimaryButton {
                                    Layout.fillWidth: true
                                    text: qsTr("Coincide")
                                    onClicked: movil.confirmarEmparejamiento()
                                }
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }
    }

    property string portalAOlvidar: ""

    Dialog {
        id: confirmarOlvido

        title: qsTr("Olvidar portal")
        modal: true
        anchors.centerIn: parent
        width: parent ? parent.width - estilo.spacing * 2 : 340
        padding: estilo.spacing

        contentItem: ColumnLayout {
            spacing: estilo.spacing

            Label {
                Layout.fillWidth: true
                text: qsTr("Se borrará la caché de esa clínica en este teléfono. "
                           + "Podrás volver a emparejar cuando quieras.")
                wrapMode: Text.WordWrap
                color: estilo.secondary
            }

            RowLayout {
                Layout.fillWidth: true

                PrimaryButton {
                    Layout.fillWidth: true
                    text: qsTr("Cancelar")
                    secundario: true
                    onClicked: confirmarOlvido.close()
                }

                PrimaryButton {
                    Layout.fillWidth: true
                    text: qsTr("Olvidar")
                    peligro: true
                    onClicked: {
                        movil.olvidarPortal(pagina.portalAOlvidar)
                        confirmarOlvido.close()
                    }
                }
            }
        }
    }
}
