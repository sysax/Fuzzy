import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Ficha de una mascota con su historia visible. La historia sale del portal
// activo y se filtra por `movil.mascotaActiva`; al cerrar se limpia el filtro.
Dialog {
    id: dialogo

    modal: true
    width: parent ? parent.width - estilo.spacing * 2 : 380
    height: parent ? parent.height - estilo.spacing * 4 : 620
    anchors.centerIn: parent
    padding: estilo.spacing

    onClosed: movil.cerrarMascota()

    function mascotaActual() {
        for (var i = 0; i < movil.mascotas.length; ++i) {
            if (movil.mascotas[i].uuid === movil.mascotaActiva)
                return movil.mascotas[i]
        }
        return null
    }

    contentItem: ColumnLayout {
        spacing: estilo.spacing

        RowLayout {
            Layout.fillWidth: true
            spacing: estilo.spacing

            Rectangle {
                Layout.preferredWidth: 64
                Layout.preferredHeight: 64
                radius: 20
                color: estilo.accentSoft
                clip: true

                Image {
                    id: retrato
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop
                    source: {
                        var m = dialogo.mascotaActual()
                        return m && m.foto !== undefined ? m.foto : ""
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: retrato.source.toString().length === 0
                    text: "🐾"
                    font.pixelSize: 28
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: {
                        var m = dialogo.mascotaActual()
                        return m ? m.nombre : qsTr("Mascota")
                    }
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    color: estilo.ink
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: {
                        var m = dialogo.mascotaActual()
                        return m ? (m.especie + (m.raza ? " · " + m.raza : "")) : ""
                    }
                    font.pixelSize: 13
                    color: estilo.secondary
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: {
                        var m = dialogo.mascotaActual()
                        return m ? m.clinica : ""
                    }
                    font.pixelSize: 12
                    color: estilo.accent
                    elide: Text.ElideRight
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: {
                var m = dialogo.mascotaActual()
                if (!m)
                    return ""
                var partes = []
                if (m.sexo && m.sexo !== "Desconocido")
                    partes.push(m.sexo)
                if (m.peso_kg)
                    partes.push(Number(m.peso_kg).toFixed(1) + " kg")
                if (m.numero_chip)
                    partes.push(qsTr("Chip %1").arg(m.numero_chip))
                if (m.alergias)
                    partes.push(qsTr("Alergias: %1").arg(m.alergias))
                return partes.join(" · ")
            }
            font.pixelSize: 13
            color: estilo.secondary
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Historia visible")
            font.pixelSize: 16
            font.weight: Font.DemiBold
            color: estilo.ink
        }

        EmptyState {
            Layout.fillWidth: true
            visible: movil.historial.length === 0
            titulo: qsTr("Sin registros todavía")
            detalle: qsTr("Cuando la clínica cierre una visita, emita una receta o "
                          + "registre una vacuna, aparecerá aquí.")
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: dialogo.width - estilo.spacing * 2
                spacing: estilo.spacing / 2

                Repeater {
                    model: movil.historial

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 72
                        radius: 12
                        color: estilo.background

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 2

                            RowLayout {
                                Layout.fillWidth: true

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.tipo + " · " + modelData.fecha
                                    font.pixelSize: 12
                                    color: estilo.accent
                                }

                                Label {
                                    text: modelData.mascota
                                    font.pixelSize: 11
                                    color: estilo.muted
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: modelData.titulo
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                color: estilo.ink
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: text.length > 0
                                text: modelData.detalle
                                font.pixelSize: 12
                                color: estilo.secondary
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }

        PrimaryButton {
            Layout.fillWidth: true
            text: qsTr("Cerrar")
            secundario: true
            onClicked: dialogo.close()
        }
    }
}
