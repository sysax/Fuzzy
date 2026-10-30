import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Vínculos con el portal del dueño: alta, QR de emparejamiento y revocación.
Item {
    id: root

    Rectangle {
        anchors.fill: parent
        color: estilo.background
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 14

        PageHeader {
            Layout.fillWidth: true
            titulo: qsTr("Vínculos con el portal")
            subtitulo: qsTr("Cada portal es una base aparte con su propio token")

            PrimaryButton {
                text: qsTr("Aprobar vínculo")
                enabled: vinculos.sinCuenta.length > 0
                onClicked: {
                    vinculos.limpiarMensajes()
                    dialogoAprobar.open()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            KpiCard { titulo: qsTr("Fichas con vínculo"); valor: vinculos.vinculos.length - vinculos.sinCuenta.length; subtexto: qsTr("de %1 propietarios").arg(vinculos.vinculos.length); icono: "shield-check" }
            KpiCard { titulo: qsTr("Pendientes de alta"); valor: vinculos.sinCuenta.length; subtexto: qsTr("sin cuenta de portal"); icono: "person-plus" }
            KpiCard { titulo: qsTr("Portales emparejados"); valor: vinculos.vinculos.filter(function (v) { return v.emparejado }).length; subtexto: qsTr("primera visita confirmada"); icono: "check-circle" }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14

            SectionCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 680
                titulo: qsTr("Directorio de vínculos")
                subtitulo: qsTr("El token real nunca se guarda en la base de la clínica")

                EmptyState {
                    anchors.fill: parent
                    visible: vinculos.vinculos.length === 0
                    icono: "shield-check"
                    titulo: qsTr("Sin propietarios")
                }

                ListView {
                    anchors.fill: parent
                    visible: vinculos.vinculos.length > 0
                    clip: true
                    spacing: 4
                    model: vinculos.vinculos

                    delegate: Rectangle {
                        required property var modelData
                        width: ListView.view.width
                        height: 58
                        radius: 10
                        color: estilo.background

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 10

                            Rectangle {
                                width: 32
                                height: 32
                                radius: 16
                                color: estilo.chip
                                Label {
                                    anchors.centerIn: parent
                                    text: modelData.cliente.charAt(0).toUpperCase()
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: estilo.textSecondary
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Label {
                                    text: modelData.cliente
                                    font.pixelSize: 12
                                    font.bold: true
                                    color: estilo.ink
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: modelData.correo.length > 0 ? modelData.correo : qsTr("Sin cuenta de portal")
                                    font.pixelSize: 10
                                    color: estilo.textMuted
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            StatusChip { estado: modelData.portalUuid.length > 0 ? (modelData.estadoPortal === "activo" ? qsTr("Activo") : qsTr("Revocado")) : qsTr("Sin portal") }

                            Label {
                                Layout.preferredWidth: 185
                                Layout.minimumWidth: 185
                                text: modelData.emparejado ? qsTr("Emparejado %1").arg(modelData.pareado) : qsTr("Sin emparejar")
                                font.pixelSize: 10
                                color: estilo.textMuted
                                elide: Text.ElideRight
                            }

                            SecondaryButton {
                                text: qsTr("Revocar")
                                visible: modelData.portalUuid.length > 0 && modelData.estadoPortal === "activo"
                                onClicked: vinculos.revocar(modelData.portalUuid)
                            }
                            SecondaryButton {
                                text: qsTr("Eliminar")
                                visible: modelData.portalUuid.length > 0
                                onClicked: vinculos.eliminar(modelData.portalUuid)
                            }
                        }
                    }
                }
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 330
                titulo: vinculos.emitido.portalUuid ? qsTr("Vínculo emitido") : qsTr("Emparejamiento")
                subtitulo: vinculos.emitido.portalUuid ? qsTr("Entrega el QR al dueño") : qsTr("El dueño escanea el QR desde la app móvil")

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    visible: !!vinculos.emitido.portalUuid

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 150
                        radius: 14
                        color: estilo.ink

                        Grid {
                            anchors.centerIn: parent
                            columns: 7
                            spacing: 4

                            Repeater {
                                model: 49
                                delegate: Rectangle {
                                    required property int index
                                    width: 12
                                    height: 12
                                    radius: 2
                                    color: (index * 7 + Math.floor(index / 3)) % 3 === 0 ? "#FFFFFF"
                                         : (index % 5 === 0 ? "#8C8C88" : "#2C2C29")
                                }
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Huella %1").arg(vinculos.emitido.huella || "")
                        font.pixelSize: 11
                        color: estilo.textMuted
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 42
                        radius: 10
                        color: estilo.background
                        border.width: 1
                        border.color: estilo.border
                        Label {
                            anchors.centerIn: parent
                            text: vinculos.emitido.token || ""
                            font.pixelSize: 10
                            font.family: "monospace"
                            color: estilo.ink
                            elide: Text.ElideMiddle
                            width: parent.width - 20
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("La base del portal ya está creada. Revocar el token no afecta a los demás dueños.")
                        font.pixelSize: 11
                        color: estilo.textSecondary
                        wrapMode: Text.WordWrap
                    }

                    SecondaryButton {
                        Layout.fillWidth: true
                        text: qsTr("Ocultar")
                        onClicked: vinculos.limpiarEmitido()
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    visible: !vinculos.emitido.portalUuid

                    EmptyState {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        icono: "shield-check"
                        titulo: qsTr("Sin emisión reciente")
                        detalle: qsTr("Aprueba un vínculo para generar el QR y el token de emparejamiento.")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: qsTr("Con token"); font.pixelSize: 11; color: estilo.textMuted }
                        Label { text: vinculos.vinculos.filter(function (v) { return v.conToken }).length; font.pixelSize: 11; color: estilo.ink; font.bold: true }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: qsTr("Sin cuenta"); font.pixelSize: 11; color: estilo.textMuted }
                        Label { text: vinculos.sinCuenta.length; font.pixelSize: 11; color: estilo.ink; font.bold: true }
                    }
                }
            }
        }
    }

    AppDialog {
        id: dialogoAprobar
        titulo: qsTr("Aprobar vínculo con el portal")
        descripcion: qsTr("La cuenta del dueño se crea en la clínica; el portal recibe solo sus datos.")

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12

            ComboBox {
                id: comboCliente
                Layout.fillWidth: true
                model: vinculos.sinCuenta
                textRole: "nombre"
                displayText: currentIndex >= 0 ? currentText : qsTr("Elige el propietario")
                onActivated: {
                    if (vinculos.sinCuenta[currentIndex].correo.length > 0)
                        aprobarCorreo.text = vinculos.sinCuenta[currentIndex].correo
                }
            }

            LabeledField {
                id: aprobarCorreo
                Layout.fillWidth: true
                label: qsTr("Correo del dueño")
                placeholder: qsTr("dueno@correo.co")
            }

            LabeledField {
                id: aprobarPassword
                Layout.fillWidth: true
                label: qsTr("Contraseña inicial")
                password: true
                placeholder: qsTr("Mínimo 8 caracteres")
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("El token del portal se muestra una sola vez. El dueño lo recibe en el QR.")
                font.pixelSize: 11
                color: estilo.textMuted
                wrapMode: Text.WordWrap
            }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoAprobar.close() },
            PrimaryButton {
                text: qsTr("Aprobar y emitir")
                onClicked: {
                    vinculos.aprobar(comboCliente.currentIndex >= 0 ? vinculos.sinCuenta[comboCliente.currentIndex].uuid : "",
                                     aprobarCorreo.text, aprobarPassword.text)
                    if (vinculos.error.length === 0) {
                        aprobarPassword.text = ""
                        dialogoAprobar.close()
                    }
                }
            }
        ]
    }
}
