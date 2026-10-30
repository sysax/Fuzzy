import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Perfil del paciente: datos, alerta médica, historial, signos, vacunas y
// galería. Todo se lee de la base del nodo.
Item {
    id: root

    signal volver()

    Rectangle {
        anchors.fill: parent
        color: estilo.background
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Rectangle {
                width: 38
                height: 38
                radius: 19
                color: estilo.surface
                border.width: 1
                border.color: estilo.border

                AppIcon {
                    anchors.centerIn: parent
                    nombre: "chevron-left"
                    color: estilo.ink
                    tamano: 16
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.volver()
                }
            }

            ColumnLayout {
                spacing: 2
                RowLayout {
                    spacing: 8
                    Label {
                        text: pacientes.ficha.nombre || ""
                        font.pixelSize: 24
                        font.bold: true
                        color: estilo.ink
                    }
                    StatusChip { estado: pacientes.ficha.activo ? qsTr("Activo") : qsTr("Archivado") }
                    StatusChip { estado: pacientes.ficha.especie || "" }
                }
                Label {
                    text: (pacientes.ficha.raza || qsTr("Sin raza")) + " · "
                          + (pacientes.ficha.sexo || "") + " · "
                          + qsTr("alta %1").arg(pacientes.ficha.alta || "")
                    font.pixelSize: 12
                    color: estilo.textSecondary
                }
            }

            Item { Layout.fillWidth: true }

            SecondaryButton { text: qsTr("Signos"); onClicked: { pacientes.limpiarMensajes(); dialogoSignos.open() } }
            SecondaryButton { text: qsTr("Vacuna"); onClicked: { pacientes.limpiarMensajes(); dialogoVacuna.open() } }
            SecondaryButton { text: qsTr("Editar"); onClicked: { pacientes.limpiarMensajes(); dialogoEditar.open() } }
            PrimaryButton { text: qsTr("Nueva consulta"); onClicked: { pacientes.limpiarMensajes(); dialogoConsulta.open() } }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: alerta.implicitHeight + 20
            radius: 10
            color: estilo.warningBg
            border.width: 1
            border.color: "#F3DFA8"
            visible: (pacientes.ficha.alergias || "").length > 0 || (pacientes.ficha.atencionEspecial || "").length > 0

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                AppIcon { nombre: "exclamation-triangle"; color: estilo.warning; tamano: 15 }
                Label {
                    id: alerta
                    Layout.fillWidth: true
                    text: {
                        var partes = []
                        if ((pacientes.ficha.alergias || "").length > 0)
                            partes.push(qsTr("Alergias: %1").arg(pacientes.ficha.alergias))
                        if ((pacientes.ficha.atencionEspecial || "").length > 0)
                            partes.push(qsTr("Atención especial: %1").arg(pacientes.ficha.atencionEspecial))
                        return partes.join(" · ")
                    }
                    color: estilo.warning
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Repeater {
                model: [
                    { titulo: qsTr("Propietario"), valor: pacientes.ficha.propietario || "", icono: "person-vcard" },
                    { titulo: qsTr("Teléfono"), valor: pacientes.ficha.telefono || "—", icono: "people" },
                    { titulo: qsTr("Peso actual"), valor: (pacientes.ficha.peso || 0).toFixed(1) + " kg", icono: "capsule" },
                    { titulo: qsTr("Microchip"), valor: pacientes.ficha.chip || "—", icono: "shield-check" }
                ]

                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 74
                    radius: 14
                    color: estilo.surface
                    border.width: 1
                    border.color: estilo.border

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10

                        Rectangle {
                            width: 34
                            height: 34
                            radius: 10
                            color: estilo.background
                            AppIcon {
                                anchors.centerIn: parent
                                nombre: modelData.icono
                                color: estilo.textSecondary
                                tamano: 15
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Label { text: modelData.titulo; font.pixelSize: 10; color: estilo.textMuted; font.capitalization: Font.AllUppercase }
                            Label {
                                Layout.fillWidth: true
                                text: modelData.valor
                                font.pixelSize: 13
                                font.bold: true
                                color: estilo.ink
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14

            SectionCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 620
                titulo: qsTr("Historial clínico")
                subtitulo: qsTr("%1 consultas registradas").arg(pacientes.historial.length)

                EmptyState {
                    anchors.fill: parent
                    visible: pacientes.historial.length === 0
                    icono: "file-medical"
                    titulo: qsTr("Sin consultas")
                    detalle: qsTr("La primera consulta aparecerá aquí.")
                }

                ListView {
                    anchors.fill: parent
                    visible: pacientes.historial.length > 0
                    clip: true
                    spacing: 8
                    model: pacientes.historial

                    delegate: Rectangle {
                        required property var modelData
                        width: ListView.view.width
                        height: 92
                        radius: 12
                        color: estilo.background
                        border.width: 1
                        border.color: estilo.border

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 3

                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: modelData.fecha; font.pixelSize: 10; color: estilo.textMuted }
                                Item { Layout.fillWidth: true }
                                StatusChip { estado: modelData.estado }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: modelData.motivo
                                font.pixelSize: 13
                                font.bold: true
                                color: estilo.ink
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: modelData.diagnostico.length > 0 ? modelData.diagnostico : qsTr("Sin diagnóstico registrado")
                                font.pixelSize: 11
                                color: estilo.textSecondary
                                elide: Text.ElideRight
                            }
                            Label {
                                text: modelData.veterinario
                                font.pixelSize: 10
                                color: estilo.textMuted
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 380
                Layout.minimumWidth: 340
                Layout.maximumWidth: 520
                spacing: 14

                SectionCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 196
                    titulo: qsTr("Signos vitales")
                    subtitulo: qsTr("Últimos registros")

                    EmptyState {
                        anchors.fill: parent
                        visible: pacientes.signos.length === 0
                        icono: "capsule"
                        titulo: qsTr("Sin signos")
                    }

                    ListView {
                        anchors.fill: parent
                        visible: pacientes.signos.length > 0
                        clip: true
                        model: pacientes.signos

                        delegate: RowLayout {
                            required property var modelData
                            width: ListView.view.width
                            height: 32
                            spacing: 8

                            Label {
                                Layout.preferredWidth: 62
                                text: Qt.formatDateTime(new Date(modelData.fechaMs), "dd/MM")
                                font.pixelSize: 10
                                color: estilo.textMuted
                            }
                            Label { Layout.fillWidth: true; text: modelData.peso.toFixed(1) + " kg"; font.pixelSize: 11; color: estilo.ink }
                            Label { text: modelData.temperatura.toFixed(1) + " °C"; font.pixelSize: 11; color: estilo.textSecondary }
                            Label { text: modelData.cardiaca > 0 ? modelData.cardiaca + " lpm" : "—"; font.pixelSize: 11; color: estilo.textSecondary }
                        }
                    }
                }

                SectionCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    titulo: qsTr("Vacunas")
                    subtitulo: qsTr("%1 aplicadas").arg(pacientes.vacunas.length)

                    EmptyState {
                        anchors.fill: parent
                        visible: pacientes.vacunas.length === 0
                        icono: "shield-check"
                        titulo: qsTr("Sin vacunas")
                    }

                    ListView {
                        anchors.fill: parent
                        visible: pacientes.vacunas.length > 0
                        clip: true
                        spacing: 2
                        model: pacientes.vacunas

                        delegate: RowLayout {
                            required property var modelData
                            width: ListView.view.width
                            height: 34
                            spacing: 8

                            AppIcon { nombre: "check-circle"; color: estilo.success; tamano: 14 }
                            Label {
                                Layout.fillWidth: true
                                text: modelData.nombre
                                font.pixelSize: 11
                                color: estilo.ink
                                elide: Text.ElideRight
                            }
                            Label {
                                text: modelData.aplicacion + (modelData.proxima.length > 0 ? " → " + modelData.proxima : "")
                                font.pixelSize: 10
                                color: estilo.textMuted
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Diálogos ──────────────────────────────────────────────
    AppDialog {
        id: dialogoConsulta
        titulo: qsTr("Nueva consulta")
        descripcion: pacientes.ficha.nombre || ""

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 12

            LabeledField { id: consultaMotivo; Layout.fillWidth: true; Layout.columnSpan: 2; label: qsTr("Motivo"); placeholder: qsTr("Control anual") }
            LabeledField { id: consultaAnamnesis; Layout.fillWidth: true; label: qsTr("Anamnesis"); placeholder: qsTr("Relato del dueño") }
            LabeledField { id: consultaExamen; Layout.fillWidth: true; label: qsTr("Examen físico"); placeholder: qsTr("Hallazgos") }
            LabeledField { id: consultaDiagnostico; Layout.fillWidth: true; label: qsTr("Diagnóstico"); placeholder: qsTr("Impresión clínica") }
            LabeledField { id: consultaTratamiento; Layout.fillWidth: true; label: qsTr("Tratamiento"); placeholder: qsTr("Indicaciones") }
            CheckBox { id: consultaEmergencia; Layout.columnSpan: 2; text: qsTr("Emergencia") }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoConsulta.close() },
            PrimaryButton {
                text: qsTr("Guardar")
                onClicked: {
                    pacientes.registrarConsulta(pacientes.ficha.uuid, consultaMotivo.text,
                                                consultaAnamnesis.text, consultaExamen.text,
                                                consultaDiagnostico.text, consultaTratamiento.text,
                                                consultaEmergencia.checked)
                    if (pacientes.error.length === 0)
                        dialogoConsulta.close()
                }
            }
        ]
    }

    AppDialog {
        id: dialogoSignos
        titulo: qsTr("Registrar signos vitales")
        descripcion: pacientes.ficha.nombre || ""

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 12

            LabeledField { id: signoPeso; Layout.fillWidth: true; label: qsTr("Peso (kg)"); placeholder: "0.0"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            LabeledField { id: signoTemperatura; Layout.fillWidth: true; label: qsTr("Temperatura (°C)"); placeholder: "38.5"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            LabeledField { id: signoCardiaca; Layout.fillWidth: true; label: qsTr("Frecuencia cardiaca"); placeholder: "lpm"; inputMethodHints: Qt.ImhDigitsOnly }
            LabeledField { id: signoRespiratoria; Layout.fillWidth: true; label: qsTr("Frecuencia respiratoria"); placeholder: "rpm"; inputMethodHints: Qt.ImhDigitsOnly }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoSignos.close() },
            PrimaryButton {
                text: qsTr("Guardar")
                onClicked: {
                    pacientes.registrarSignos(pacientes.ficha.uuid, Number(signoPeso.text),
                                              Number(signoTemperatura.text),
                                              Number(signoCardiaca.text),
                                              Number(signoRespiratoria.text), "")
                    if (pacientes.error.length === 0)
                        dialogoSignos.close()
                }
            }
        ]
    }

    AppDialog {
        id: dialogoVacuna
        titulo: qsTr("Registrar vacuna")
        descripcion: pacientes.ficha.nombre || ""

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 12

            LabeledField { id: vacunaNombre; Layout.fillWidth: true; Layout.columnSpan: 2; label: qsTr("Vacuna"); placeholder: qsTr("Rabia") }
            LabeledField { id: vacunaAplicacion; Layout.fillWidth: true; label: qsTr("Aplicación"); text: Qt.formatDate(new Date(), "yyyy-MM-dd") }
            LabeledField { id: vacunaProxima; Layout.fillWidth: true; label: qsTr("Próxima dosis"); placeholder: "yyyy-MM-dd" }
            LabeledField { id: vacunaLote; Layout.fillWidth: true; Layout.columnSpan: 2; label: qsTr("Lote"); placeholder: qsTr("Opcional") }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoVacuna.close() },
            PrimaryButton {
                text: qsTr("Guardar")
                onClicked: {
                    pacientes.registrarVacuna(pacientes.ficha.uuid, vacunaNombre.text,
                                              vacunaAplicacion.text, vacunaProxima.text,
                                              vacunaLote.text, "")
                    if (pacientes.error.length === 0)
                        dialogoVacuna.close()
                }
            }
        ]
    }

    AppDialog {
        id: dialogoEditar
        titulo: qsTr("Editar ficha")
        descripcion: qsTr("Los cambios quedan en el historial de sincronización.")

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 12

            LabeledField { id: editarNombre; Layout.fillWidth: true; label: qsTr("Nombre"); text: pacientes.ficha.nombre || "" }
            LabeledField { id: editarEspecie; Layout.fillWidth: true; label: qsTr("Especie"); text: pacientes.ficha.especie || "" }
            LabeledField { id: editarRaza; Layout.fillWidth: true; label: qsTr("Raza"); text: pacientes.ficha.raza || "" }
            LabeledField { id: editarPeso; Layout.fillWidth: true; label: qsTr("Peso (kg)"); text: (pacientes.ficha.peso || 0).toFixed(1) }
            LabeledField { id: editarNacimiento; Layout.fillWidth: true; label: qsTr("Nacimiento"); text: pacientes.ficha.nacimiento || "" }
            LabeledField { id: editarChip; Layout.fillWidth: true; label: qsTr("Chip"); text: pacientes.ficha.chip || "" }
            LabeledField { id: editarAlergias; Layout.fillWidth: true; label: qsTr("Alergias"); text: pacientes.ficha.alergias || "" }
            LabeledField { id: editarAtencion; Layout.fillWidth: true; label: qsTr("Atención especial"); text: pacientes.ficha.atencionEspecial || "" }
            CheckBox { id: editarActivo; text: qsTr("Activo"); checked: pacientes.ficha.activo === true }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoEditar.close() },
            PrimaryButton {
                text: qsTr("Guardar")
                onClicked: {
                    pacientes.actualizarMascota(pacientes.ficha.uuid, editarNombre.text,
                                                editarEspecie.text, editarRaza.text,
                                                pacientes.ficha.sexo, Number(editarPeso.text),
                                                pacientes.ficha.castrado, editarNacimiento.text,
                                                editarChip.text, editarAlergias.text,
                                                editarAtencion.text, editarActivo.checked)
                    if (pacientes.error.length === 0)
                        dialogoEditar.close()
                }
            }
        ]
    }
}
