import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Agenda semanal y diaria con filtros, alta y cambio de estado de la cita.
Item {
    id: root

    property var citaAbierta: null

    function citasDe(fecha) {
        return agenda.citas.filter(function (c) { return c.fecha === fecha })
    }

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
            titulo: qsTr("Agenda")
            subtitulo: agenda.etiquetaPeriodo

            SecondaryButton {
                text: qsTr("Hoy")
                onClicked: agenda.irHoy()
            }
            Rectangle {
                width: 132
                height: 42
                radius: estilo.radiusPill
                color: estilo.surface
                border.width: 1
                border.color: estilo.border

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 3
                    spacing: 2

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: estilo.radiusPill
                        color: agenda.modo === "dia" ? estilo.ink : "transparent"
                        Label {
                            anchors.centerIn: parent
                            text: qsTr("Día")
                            font.pixelSize: 12
                            color: agenda.modo === "dia" ? "#FFFFFF" : estilo.textSecondary
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: agenda.cambiarModo("dia")
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: estilo.radiusPill
                        color: agenda.modo === "semana" ? estilo.ink : "transparent"
                        Label {
                            anchors.centerIn: parent
                            text: qsTr("Semana")
                            font.pixelSize: 12
                            color: agenda.modo === "semana" ? "#FFFFFF" : estilo.textSecondary
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: agenda.cambiarModo("semana")
                        }
                    }
                }
            }

            PrimaryButton {
                text: qsTr("Agendar cita")
                onClicked: {
                    agenda.limpiarMensajes()
                    dialogoAgendar.open()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: aviso.implicitHeight + 22
            radius: 10
            color: estilo.dangerBg
            border.width: 1
            border.color: "#F7C9CB"
            visible: agenda.error.length > 0

            Label {
                id: aviso
                anchors.fill: parent
                anchors.margins: 11
                text: agenda.error
                color: estilo.danger
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: avisoOk.implicitHeight + 22
            radius: 10
            color: estilo.successBg
            border.width: 1
            border.color: "#CDEBD7"
            visible: agenda.mensaje.length > 0

            Label {
                id: avisoOk
                anchors.fill: parent
                anchors.margins: 11
                text: agenda.mensaje
                color: estilo.success
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }

        // ── Semana ────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10
            visible: agenda.modo === "semana"

            Repeater {
                model: agenda.dias

                delegate: ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 6

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 52
                        radius: 12
                        color: modelData.hoy ? estilo.ink : estilo.surface
                        border.width: modelData.hoy ? 0 : 1
                        border.color: estilo.border

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 0
                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                text: modelData.nombre
                                font.pixelSize: 10
                                font.capitalization: Font.Capitalize
                                color: modelData.hoy ? "#B9B9B5" : estilo.textMuted
                            }
                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                text: modelData.numero
                                font.pixelSize: 18
                                font.bold: true
                                color: modelData.hoy ? "#FFFFFF" : estilo.ink
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: estilo.radiusCard
                        color: estilo.surface
                        border.width: 1
                        border.color: estilo.border
                        clip: true

                        ListView {
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 6
                            clip: true
                            model: root.citasDe(modelData.fecha)

                            delegate: Rectangle {
                                required property var modelData
                                width: ListView.view.width
                                height: 68
                                radius: 10
                                color: modelData.estado === "En curso" ? estilo.ink
                                     : modelData.emergencia ? estilo.dangerBg : estilo.background
                                border.width: 1
                                border.color: modelData.estado === "En curso" ? estilo.ink : estilo.border

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 1

                                    Label {
                                        text: modelData.hora
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: modelData.estado === "En curso" ? "#FFFFFF" : estilo.textSecondary
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.mascota
                                        font.pixelSize: 11
                                        font.bold: true
                                        color: modelData.estado === "En curso" ? "#FFFFFF" : estilo.ink
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.tipo
                                        font.pixelSize: 9
                                        color: modelData.estado === "En curso" ? "#B9B9B5" : estilo.textMuted
                                        elide: Text.ElideRight
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        root.citaAbierta = modelData
                                        dialogoCita.open()
                                    }
                                }
                            }
                        }

                        EmptyState {
                            anchors.fill: parent
                            visible: root.citasDe(modelData.fecha).length === 0
                            icono: "calendar3"
                            titulo: qsTr("Sin citas")
                        }
                    }
                }
            }
        }

        // ── Día ───────────────────────────────────────────────
        SectionCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: agenda.modo === "dia"
            titulo: qsTr("Citas del día")
            subtitulo: agenda.etiquetaPeriodo

            EmptyState {
                anchors.fill: parent
                visible: agenda.citas.length === 0
                icono: "calendar3"
                titulo: qsTr("Sin citas")
                detalle: qsTr("Agenda una cita para este día.")
            }

            ListView {
                anchors.fill: parent
                visible: agenda.citas.length > 0
                clip: true
                spacing: 4
                model: agenda.citas

                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width
                    height: 64
                    radius: 12
                    color: estilo.background
                    border.width: 1
                    border.color: estilo.border

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 14

                        Label {
                            Layout.preferredWidth: 52
                            text: modelData.hora
                            font.pixelSize: 14
                            font.bold: true
                            color: estilo.ink
                        }

                        Rectangle {
                            Layout.preferredWidth: 3
                            Layout.preferredHeight: 36
                            radius: 2
                            color: modelData.emergencia ? estilo.danger
                                 : modelData.estado === "En curso" ? estilo.ink : estilo.border
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Label {
                                text: modelData.mascota
                                font.pixelSize: 13
                                font.bold: true
                                color: estilo.ink
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                text: modelData.propietario + " · " + modelData.tipo
                                font.pixelSize: 11
                                color: estilo.textSecondary
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        Label {
                            text: modelData.empleado.length > 0 ? modelData.empleado : qsTr("Sin asignar")
                            font.pixelSize: 11
                            color: estilo.textMuted
                        }

                        StatusChip { estado: modelData.estado }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.citaAbierta = modelData
                            dialogoCita.open()
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            SecondaryButton {
                text: qsTr("← Anterior")
                onClicked: agenda.anterior()
            }
            SecondaryButton {
                text: qsTr("Siguiente →")
                onClicked: agenda.siguiente()
            }
            Item { Layout.fillWidth: true }
            Label {
                text: qsTr("%1 citas en el periodo").arg(agenda.citas.length)
                font.pixelSize: 12
                color: estilo.textMuted
            }
        }
    }

    // ── Alta de cita ──────────────────────────────────────────
    AppDialog {
        id: dialogoAgendar
        titulo: qsTr("Agendar nueva cita")
        descripcion: qsTr("La mascota debe estar registrada. Se evita duplicar citas en la misma franja.")

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 12

            ComboBox {
                id: comboMascota
                Layout.fillWidth: true
                Layout.columnSpan: 2
                model: agenda.mascotas
                textRole: "nombre"
                displayText: currentIndex >= 0 ? currentText + " · " + agenda.mascotas[currentIndex].propietario
                                               : qsTr("Elige la mascota")
            }

            ComboBox {
                id: comboTipo
                Layout.fillWidth: true
                model: agenda.tipos
                textRole: "nombre"
            }

            LabeledField {
                id: campoFecha
                Layout.fillWidth: true
                label: qsTr("Fecha")
                text: agenda.fechaActual
                placeholder: "yyyy-MM-dd"
            }

            LabeledField {
                id: campoHora
                Layout.fillWidth: true
                label: qsTr("Hora")
                text: "09:00"
                placeholder: "HH:mm"
            }

            LabeledField {
                id: campoNotas
                Layout.fillWidth: true
                label: qsTr("Notas")
                placeholder: qsTr("Motivo o indicaciones")
            }

            CheckBox {
                id: checkEmergencia
                Layout.columnSpan: 2
                text: qsTr("Marcar como emergencia")
            }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoAgendar.close() },
            PrimaryButton {
                text: qsTr("Agendar")
                onClicked: {
                    agenda.agendar(comboMascota.currentIndex >= 0 ? agenda.mascotas[comboMascota.currentIndex].uuid : "",
                                   comboTipo.currentIndex >= 0 ? agenda.tipos[comboTipo.currentIndex].uuid : "",
                                   campoFecha.text, campoHora.text, campoNotas.text,
                                   checkEmergencia.checked)
                    if (agenda.error.length === 0)
                        dialogoAgendar.close()
                }
            }
        ]
    }

    // ── Detalle de cita ───────────────────────────────────────
    AppDialog {
        id: dialogoCita
        titulo: root.citaAbierta ? root.citaAbierta.mascota : ""
        descripcion: root.citaAbierta ? root.citaAbierta.fechaHora + " · " + root.citaAbierta.propietario : ""

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Tipo"); Layout.fillWidth: true; color: estilo.textMuted; font.pixelSize: 12 }
                Label { text: root.citaAbierta ? root.citaAbierta.tipo : ""; color: estilo.ink; font.pixelSize: 12; font.bold: true }
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Estado"); Layout.fillWidth: true; color: estilo.textMuted; font.pixelSize: 12 }
                StatusChip { estado: root.citaAbierta ? root.citaAbierta.estado : "" }
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Profesional"); Layout.fillWidth: true; color: estilo.textMuted; font.pixelSize: 12 }
                Label { text: root.citaAbierta && root.citaAbierta.empleado.length > 0 ? root.citaAbierta.empleado : qsTr("Sin asignar"); color: estilo.ink; font.pixelSize: 12 }
            }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: estilo.border }
            Label {
                Layout.fillWidth: true
                text: qsTr("Mover la cita")
                font.pixelSize: 11
                color: estilo.textMuted
            }
            RowLayout {
                Layout.fillWidth: true
                LabeledField { id: moverFecha; Layout.fillWidth: true; label: qsTr("Nueva fecha"); text: root.citaAbierta ? root.citaAbierta.fecha : "" }
                LabeledField { id: moverHora; Layout.fillWidth: true; label: qsTr("Nueva hora"); text: root.citaAbierta ? root.citaAbierta.hora : "" }
                SecondaryButton {
                    text: qsTr("Mover")
                    onClicked: {
                        if (root.citaAbierta)
                            agenda.mover(root.citaAbierta.uuid, moverFecha.text, moverHora.text)
                        if (agenda.error.length === 0)
                            dialogoCita.close()
                    }
                }
            }
        }

        acciones: [
            SecondaryButton {
                text: qsTr("Cancelar cita")
                onClicked: {
                    if (root.citaAbierta)
                        agenda.cancelar(root.citaAbierta.uuid)
                    dialogoCita.close()
                }
            },
            SecondaryButton {
                text: qsTr("En espera")
                onClicked: {
                    if (root.citaAbierta && root.citaAbierta.estadoUuid)
                        agenda.cambiarEstado(root.citaAbierta.uuid, root.citaAbierta.estadoUuid)
                    dialogoCita.close()
                }
            },
            PrimaryButton {
                text: qsTr("Cerrar")
                onClicked: dialogoCita.close()
            }
        ]
    }
}
