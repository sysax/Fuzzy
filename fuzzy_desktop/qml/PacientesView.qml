import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Base de pacientes: búsqueda, filtro por especie, paginación y acceso a la
// ficha. La ficha se muestra en la misma página para no perder el contexto.
Item {
    id: root

    readonly property bool enFicha: (pacientes.ficha.uuid || "").length > 0

    Rectangle {
        anchors.fill: parent
        color: estilo.background
    }

    // ── Listado ───────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 14
        visible: !root.enFicha

        PageHeader {
            Layout.fillWidth: true
            titulo: qsTr("Pacientes")
            subtitulo: qsTr("%1 pacientes registrados · %2 activos").arg(pacientes.total).arg(panel.pacientesActivos)

            PrimaryButton {
                text: qsTr("Nuevo paciente")
                onClicked: {
                    pacientes.limpiarMensajes()
                    dialogoPaciente.open()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            SearchBox {
                Layout.preferredWidth: 340
                placeholder: qsTr("Buscar paciente, dueño o chip…")
                onEditado: pacientes.buscar(texto)
            }

            ComboBox {
                id: comboEspecie
                Layout.preferredWidth: 215
                model: [{ "especie": "" }].concat(pacientes.especies)
                textRole: "especie"
                displayText: currentIndex <= 0 ? qsTr("Todas las especies") : currentText
                onActivated: pacientes.filtrarEspecie(currentIndex <= 0 ? "" : pacientes.especies[currentIndex - 1].especie)
            }

            Item { Layout.fillWidth: true }

            Label {
                text: qsTr("Página %1 de %2").arg(pacientes.pagina + 1).arg(pacientes.totalPaginas)
                color: estilo.textSecondary
                font.pixelSize: 12
            }

            SecondaryButton {
                text: qsTr("←")
                enabled: pacientes.pagina > 0
                onClicked: pacientes.irPagina(pacientes.pagina - 1)
            }
            SecondaryButton {
                text: qsTr("→")
                enabled: pacientes.pagina + 1 < pacientes.totalPaginas
                onClicked: pacientes.irPagina(pacientes.pagina + 1)
            }
        }

        SectionCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            margen: 0

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    color: estilo.background

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 18
                        anchors.rightMargin: 18
                        spacing: 12

                        Label { Layout.preferredWidth: 220; text: qsTr("PACIENTE"); font.pixelSize: 10; font.letterSpacing: 1; color: estilo.textMuted }
                        Label { Layout.preferredWidth: 130; text: qsTr("ESPECIE"); font.pixelSize: 10; font.letterSpacing: 1; color: estilo.textMuted }
                        Label { Layout.fillWidth: true; text: qsTr("PROPIETARIO"); font.pixelSize: 10; font.letterSpacing: 1; color: estilo.textMuted }
                        Label { Layout.preferredWidth: 100; text: qsTr("PESO"); font.pixelSize: 10; font.letterSpacing: 1; color: estilo.textMuted }
                        Label { Layout.preferredWidth: 90; text: qsTr("ESTADO"); font.pixelSize: 10; font.letterSpacing: 1; color: estilo.textMuted }
                    }
                }

                EmptyState {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: pacientes.pacientes.length === 0
                    icono: "people"
                    titulo: qsTr("Sin pacientes")
                    detalle: qsTr("Ajusta la búsqueda o registra una mascota.")
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: pacientes.pacientes.length > 0
                    clip: true
                    model: pacientes.pacientes

                    delegate: Item {
                        required property var modelData
                        width: ListView.view.width
                        height: 58

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18
                            anchors.rightMargin: 18
                            spacing: 12

                            RowLayout {
                                Layout.preferredWidth: 220
                                spacing: 10

                                Rectangle {
                                    width: 34
                                    height: 34
                                    radius: 17
                                    color: estilo.chip
                                    Label {
                                        anchors.centerIn: parent
                                        text: modelData.nombre.charAt(0).toUpperCase()
                                        font.bold: true
                                        color: estilo.textSecondary
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0
                                    Label {
                                        text: modelData.nombre
                                        font.pixelSize: 13
                                        font.bold: true
                                        color: estilo.ink
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Label {
                                        text: modelData.raza.length > 0 ? modelData.raza : modelData.sexo
                                        font.pixelSize: 10
                                        color: estilo.textMuted
                                    }
                                }
                            }

                            Label { Layout.preferredWidth: 130; text: modelData.especie; font.pixelSize: 12; color: estilo.textSecondary }
                            Label {
                                Layout.fillWidth: true
                                text: modelData.propietario
                                font.pixelSize: 12
                                color: estilo.ink
                                elide: Text.ElideRight
                            }
                            Label { Layout.preferredWidth: 100; text: modelData.peso.toFixed(1) + " kg"; font.pixelSize: 12; color: estilo.textSecondary }
                            StatusChip { Layout.preferredWidth: 90; estado: modelData.activo ? qsTr("Activo") : qsTr("Archivado") }
                        }

                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: estilo.border
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: pacientes.seleccionar(modelData.uuid)
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: aviso.implicitHeight + 20
            radius: 10
            color: estilo.dangerBg
            border.width: 1
            border.color: "#F7C9CB"
            visible: pacientes.error.length > 0

            Label {
                id: aviso
                anchors.fill: parent
                anchors.margins: 10
                text: pacientes.error
                color: estilo.danger
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }
    }

    // ── Ficha ─────────────────────────────────────────────────
    PacienteFichaView {
        anchors.fill: parent
        visible: root.enFicha
        onVolver: pacientes.limpiarSeleccion()
    }

    // ── Alta de paciente ──────────────────────────────────────
    AppDialog {
        id: dialogoPaciente
        titulo: qsTr("Registrar paciente")
        descripcion: qsTr("El propietario debe existir en el directorio.")

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 12

            ComboBox {
                id: comboPropietario
                Layout.fillWidth: true
                Layout.columnSpan: 2
                model: propietarios.propietarios
                textRole: "nombre"
                displayText: currentIndex >= 0 ? currentText : qsTr("Elige el propietario")
            }

            LabeledField { id: pacienteNombre; Layout.fillWidth: true; label: qsTr("Nombre"); placeholder: qsTr("Luna") }
            LabeledField { id: pacienteEspecie; Layout.fillWidth: true; label: qsTr("Especie"); placeholder: qsTr("Perro") }
            LabeledField { id: pacienteRaza; Layout.fillWidth: true; label: qsTr("Raza"); placeholder: qsTr("Mestizo") }
            LabeledField { id: pacientePeso; Layout.fillWidth: true; label: qsTr("Peso (kg)"); placeholder: "0.0"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            LabeledField { id: pacienteNacimiento; Layout.fillWidth: true; label: qsTr("Nacimiento"); placeholder: "yyyy-MM-dd" }
            LabeledField { id: pacienteChip; Layout.fillWidth: true; label: qsTr("Número de chip"); placeholder: qsTr("Opcional") }
            LabeledField { id: pacienteAlergias; Layout.fillWidth: true; Layout.columnSpan: 2; label: qsTr("Alergias"); placeholder: qsTr("Ninguna conocida") }
            CheckBox { id: pacienteCastrado; text: qsTr("Castrado"); Layout.columnSpan: 2 }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoPaciente.close() },
            PrimaryButton {
                text: qsTr("Registrar")
                onClicked: {
                    pacientes.registrarMascota(
                        comboPropietario.currentIndex >= 0 ? propietarios.propietarios[comboPropietario.currentIndex].uuid : "",
                        pacienteNombre.text, pacienteEspecie.text, pacienteRaza.text,
                        "Desconocido", Number(pacientePeso.text), pacienteCastrado.checked,
                        pacienteNacimiento.text, pacienteChip.text, pacienteAlergias.text, "")
                    if (pacientes.error.length === 0)
                        dialogoPaciente.close()
                }
            }
        ]
    }
}
