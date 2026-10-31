import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Inicio: datos del dueño, mascotas del portal activo y el registro local que
// todavía no existe en la clínica.
Page {
    id: inicio

    background: Rectangle { color: estilo.background }

    ScrollView {
        anchors.fill: parent
        contentWidth: disponible.width
        clip: true

        ColumnLayout {
            id: disponible
            width: inicio.width
            spacing: estilo.spacing

            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: estilo.spacing
                spacing: estilo.spacing

                Card {
                    Layout.fillWidth: true

                    RowLayout {
                        Layout.fillWidth: true

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Label {
                                Layout.fillWidth: true
                                text: movil.perfil.length > 0
                                      ? movil.perfil[0].nombre_completo
                                      : qsTr("Tus datos")
                                font.pixelSize: 18
                                font.weight: Font.DemiBold
                                color: estilo.ink
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: movil.perfil.length > 0
                                      ? (movil.perfil[0].telefono || "")
                                        + (movil.perfil[0].correo
                                           ? " · " + movil.perfil[0].correo : "")
                                      : qsTr("Regístralos ahora; se importan al emparejar.")
                                font.pixelSize: 13
                                color: estilo.secondary
                                elide: Text.ElideRight
                            }
                        }

                        PrimaryButton {
                            text: movil.perfil.length > 0 ? qsTr("Editar") : qsTr("Completar")
                            secundario: true
                            implicitWidth: 96
                            onClicked: dialogoPerfil.open()
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: estilo.spacing / 2

                    PrimaryButton {
                        Layout.fillWidth: true
                        text: qsTr("Primera visita")
                        secundario: true
                        onClicked: StackView.view.push("DirectorioView.qml")
                    }

                    PrimaryButton {
                        Layout.fillWidth: true
                        text: qsTr("Emparejar portal")
                        onClicked: StackView.view.push("PortalesView.qml")
                    }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.topMargin: estilo.spacing / 2
                    text: qsTr("Mis mascotas")
                    font.pixelSize: 18
                    font.weight: Font.Bold
                    color: estilo.ink
                }

                EmptyState {
                    Layout.fillWidth: true
                    Layout.topMargin: estilo.spacing
                    visible: movil.mascotas.length === 0
                    titulo: qsTr("Aún no hay mascotas")
                    detalle: qsTr("Registra a tu mascota en el teléfono o empareja el portal "
                                  + "de tu clínica para ver su historia.")
                }

                Repeater {
                    model: movil.mascotas

                    Card {
                        Layout.fillWidth: true

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: estilo.spacing

                            Rectangle {
                                Layout.preferredWidth: 56
                                Layout.preferredHeight: 56
                                radius: 18
                                color: estilo.accentSoft
                                clip: true

                                Image {
                                    anchors.fill: parent
                                    visible: modelData.foto !== undefined
                                    source: modelData.foto !== undefined ? modelData.foto : ""
                                    fillMode: Image.PreserveAspectCrop
                                }

                                Label {
                                    anchors.centerIn: parent
                                    visible: modelData.foto === undefined
                                    text: "🐾"
                                    font.pixelSize: 24
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.nombre
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                    color: estilo.ink
                                    elide: Text.ElideRight
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.especie + (modelData.raza
                                                               ? " · " + modelData.raza : "")
                                    font.pixelSize: 13
                                    color: estilo.secondary
                                    elide: Text.ElideRight
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.clinica
                                    font.pixelSize: 12
                                    color: modelData.portal_estado === "activo"
                                           ? estilo.accent : estilo.warning
                                    elide: Text.ElideRight
                                }
                            }

                            PrimaryButton {
                                text: qsTr("Ver")
                                secundario: true
                                implicitWidth: 72
                                onClicked: {
                                    movil.abrirMascota(modelData.portal_uuid, modelData.uuid)
                                    dialogoMascota.open()
                                }
                            }
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.topMargin: estilo.spacing / 2
                    visible: movil.registroPendiente.length > 0
                    text: qsTr("Pendiente de registrar en la clínica")
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    color: estilo.warning
                }

                Repeater {
                    model: movil.registroPendiente

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 56
                        radius: 12
                        color: estilo.warningSoft
                        border.width: 1
                        border.color: estilo.warning

                        Label {
                            anchors.fill: parent
                            anchors.margins: 12
                            text: modelData.tipo === "perfil"
                                  ? qsTr("Tus datos: %1").arg(modelData.nombre_completo || "")
                                  : qsTr("Mascota: %1 · %2")
                                        .arg(modelData.nombre || "",
                                             modelData.especie || "")
                            font.pixelSize: 13
                            color: estilo.warning
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                PrimaryButton {
                    Layout.fillWidth: true
                    Layout.topMargin: estilo.spacing / 2
                    visible: !movil.hayPortales
                    text: qsTr("Registrar mi mascota aquí")
                    secundario: true
                    onClicked: dialogoNuevaMascota.open()
                }

                Item { Layout.fillHeight: true }
            }
        }
    }

    RegistroPerfilDialog {
        id: dialogoPerfil
    }

    RegistroMascotaDialog {
        id: dialogoNuevaMascota
    }

    MascotaFichaDialog {
        id: dialogoMascota
    }
}
