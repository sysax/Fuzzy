import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Historial clínico global con búsqueda por mascota, dueño, motivo o
// diagnóstico. Cada resultado abre la ficha del paciente.
Item {
    id: root

    signal abrirPaciente(string mascotaUuid)

    Rectangle {
        anchors.fill: parent
        color: estilo.background
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 16

        PageHeader {
            Layout.fillWidth: true
            titulo: qsTr("Historial clínico")
            subtitulo: qsTr("Consulta transversal de todas las mascotas de la clínica")

            SearchBox {
                Layout.preferredWidth: 380
                placeholder: qsTr("Buscar mascota, dueño o diagnóstico…")
                onEditado: pacientes.buscarHistorial(texto)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            KpiCard { titulo: qsTr("Consultas encontradas"); valor: pacientes.historialGlobal.length; subtexto: qsTr("máximo 60 resultados"); icono: "file-medical" }
            KpiCard { titulo: qsTr("Pacientes activos"); valor: panel.pacientesActivos; subtexto: qsTr("en la clínica"); icono: "people" }
        }

        SectionCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            titulo: qsTr("Resultados")
            subtitulo: qsTr("El historial nunca se elimina al archivar una mascota")

            EmptyState {
                anchors.fill: parent
                visible: pacientes.historialGlobal.length === 0
                icono: "file-medical"
                titulo: qsTr("Sin consultas")
                detalle: qsTr("Prueba con otro término de búsqueda.")
            }

            ListView {
                anchors.fill: parent
                visible: pacientes.historialGlobal.length > 0
                clip: true
                spacing: 8
                model: pacientes.historialGlobal

                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width
                    height: 74
                    radius: 12
                    color: estilo.background
                    border.width: 1
                    border.color: estilo.border

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 14

                        Rectangle {
                            width: 38
                            height: 38
                            radius: 19
                            color: estilo.surface
                            border.width: 1
                            border.color: estilo.border
                            AppIcon {
                                anchors.centerIn: parent
                                nombre: "file-medical"
                                color: estilo.textSecondary
                                tamano: 15
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    text: modelData.mascota
                                    font.pixelSize: 13
                                    font.bold: true
                                    color: estilo.ink
                                }
                                Label {
                                    text: "· " + modelData.propietario
                                    font.pixelSize: 11
                                    color: estilo.textSecondary
                                }
                                Item { Layout.fillWidth: true }
                                Label {
                                    text: modelData.fecha
                                    font.pixelSize: 10
                                    color: estilo.textMuted
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: modelData.motivo
                                font.pixelSize: 12
                                color: estilo.ink
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: modelData.diagnostico.length > 0 ? modelData.diagnostico : qsTr("Sin diagnóstico")
                                font.pixelSize: 10
                                color: estilo.textMuted
                                elide: Text.ElideRight
                            }
                        }

                        StatusChip { estado: modelData.estado }

                        SecondaryButton {
                            text: qsTr("Ver ficha")
                            onClicked: {
                                pacientes.seleccionar(modelData.mascotaUuid)
                                root.abrirPaciente(modelData.mascotaUuid)
                            }
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: pacientes.buscarHistorial("")
}
