import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Directorio de propietarios con sus mascotas asociadas.
Item {
    id: root

    property bool enDetalle: (propietarios.seleccionado.uuid || "").length > 0

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
            titulo: qsTr("Propietarios")
            subtitulo: qsTr("%1 fichas · %2 con mascotas").arg(propietarios.total).arg(propietarios.conMascotas)

            SecondaryButton {
                text: qsTr("Exportar")
                onClicked: propietarios.buscar(propietarios.filtro)
            }
            PrimaryButton {
                text: qsTr("Nuevo propietario")
                onClicked: {
                    propietarios.limpiarMensajes()
                    dialogoNuevo.open()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            SearchBox {
                Layout.preferredWidth: 340
                placeholder: qsTr("Buscar por nombre, teléfono, correo o DNI…")
                onEditado: propietarios.buscar(texto)
            }

            Item { Layout.fillWidth: true }

            Label {
                text: qsTr("%1 resultados").arg(propietarios.propietarios.length)
                font.pixelSize: 12
                color: estilo.textMuted
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14

            SectionCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 600
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
                            Label { Layout.fillWidth: true; text: qsTr("PROPIETARIO"); font.pixelSize: 10; font.letterSpacing: 1; color: estilo.textMuted }
                            Label { Layout.preferredWidth: 120; text: qsTr("TELÉFONO"); font.pixelSize: 10; font.letterSpacing: 1; color: estilo.textMuted }
                            Label { Layout.preferredWidth: 80; text: qsTr("MASCOTAS"); font.pixelSize: 10; font.letterSpacing: 1; color: estilo.textMuted }
                        }
                    }

                    EmptyState {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: propietarios.propietarios.length === 0
                        icono: "person-vcard"
                        titulo: qsTr("Sin propietarios")
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: propietarios.propietarios.length > 0
                        clip: true
                        model: propietarios.propietarios

                        delegate: Item {
                            required property var modelData
                            width: ListView.view.width
                            height: 56

                            Rectangle {
                                anchors.fill: parent
                                color: propietarios.seleccionado.uuid === modelData.uuid ? estilo.background : "transparent"
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 18
                                anchors.rightMargin: 18
                                spacing: 12

                                Rectangle {
                                    width: 32
                                    height: 32
                                    radius: 16
                                    color: estilo.chip
                                    Label {
                                        anchors.centerIn: parent
                                        text: modelData.nombre.charAt(0).toUpperCase()
                                        font.bold: true
                                        font.pixelSize: 11
                                        color: estilo.textSecondary
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0
                                    Label {
                                        text: modelData.nombre
                                        font.pixelSize: 12
                                        font.bold: true
                                        color: estilo.ink
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Label {
                                        text: modelData.mascotasTexto.length > 0 ? modelData.mascotasTexto : qsTr("Sin mascotas")
                                        font.pixelSize: 10
                                        color: estilo.textMuted
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }

                                Label {
                                    Layout.preferredWidth: 120
                                    text: modelData.telefono.length > 0 ? modelData.telefono : "—"
                                    font.pixelSize: 11
                                    color: estilo.textSecondary
                                }

                                Label {
                                    Layout.preferredWidth: 80
                                    text: modelData.mascotas
                                    font.pixelSize: 11
                                    color: estilo.ink
                                    font.bold: true
                                }
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
                                onClicked: propietarios.seleccionar(modelData.uuid)
                            }
                        }
                    }
                }
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 360
                titulo: root.enDetalle ? (propietarios.seleccionado.nombre || qsTr("Propietario"))
                                       : qsTr("Ficha del propietario")
                subtitulo: root.enDetalle ? qsTr("Alta %1").arg(propietarios.seleccionado.alta || "")
                                          : qsTr("Selecciona una ficha para ver el detalle")

                EmptyState {
                    anchors.fill: parent
                    visible: !root.enDetalle
                    icono: "person-vcard"
                    titulo: qsTr("Ninguna ficha seleccionada")
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12
                    visible: root.enDetalle

                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: qsTr("Teléfono"); font.pixelSize: 12; color: estilo.textMuted }
                        Label { text: propietarios.seleccionado.telefono || "—"; font.pixelSize: 12; color: estilo.ink }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: qsTr("Correo"); font.pixelSize: 12; color: estilo.textMuted }
                        Label { text: propietarios.seleccionado.correo || "—"; font.pixelSize: 12; color: estilo.ink }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: qsTr("Dirección"); font.pixelSize: 12; color: estilo.textMuted }
                        Label { Layout.fillWidth: true; horizontalAlignment: Text.AlignRight; text: propietarios.seleccionado.direccion || "—"; font.pixelSize: 12; color: estilo.ink; elide: Text.ElideRight }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: qsTr("DNI"); font.pixelSize: 12; color: estilo.textMuted }
                        Label { text: propietarios.seleccionado.dni || "—"; font.pixelSize: 12; color: estilo.ink }
                    }

                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: estilo.border }

                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: qsTr("Mascotas"); font.pixelSize: 12; font.bold: true; color: estilo.ink }
                        StatusChip { estado: propietarios.seleccionado.activo ? qsTr("Activo") : qsTr("Inactivo") }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 6
                        model: propietarios.mascotas

                        delegate: Rectangle {
                            required property var modelData
                            width: ListView.view.width
                            height: 48
                            radius: 10
                            color: estilo.background

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 10
                                Rectangle {
                                    width: 28
                                    height: 28
                                    radius: 14
                                    color: estilo.chip
                                    Label {
                                        anchors.centerIn: parent
                                        text: modelData.nombre.charAt(0).toUpperCase()
                                        font.pixelSize: 10
                                        font.bold: true
                                        color: estilo.textSecondary
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0
                                    Label { text: modelData.nombre; font.pixelSize: 12; font.bold: true; color: estilo.ink }
                                    Label { text: modelData.especie + (modelData.raza.length > 0 ? " · " + modelData.raza : ""); font.pixelSize: 10; color: estilo.textMuted }
                                }
                                StatusChip { estado: modelData.activo ? qsTr("Activo") : qsTr("Archivado") }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    pacientes.seleccionar(modelData.uuid)
                                    root.irAPacientes()
                                }
                            }
                        }
                    }

                    SecondaryButton {
                        Layout.fillWidth: true
                        text: qsTr("Editar propietario")
                        onClicked: {
                            propietarios.limpiarMensajes()
                            dialogoEditar.open()
                        }
                    }
                }
            }
        }
    }

    // Acceso al shell para saltar a la ficha del paciente.
    signal irAPacientes()

    AppDialog {
        id: dialogoNuevo
        titulo: qsTr("Nuevo propietario")
        descripcion: qsTr("El correo se usa para el portal del dueño; no es obligatorio ahora.")

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 12

            LabeledField { id: nuevoNombre; Layout.fillWidth: true; Layout.columnSpan: 2; label: qsTr("Nombre completo"); placeholder: qsTr("María Gómez") }
            LabeledField { id: nuevoTelefono; Layout.fillWidth: true; label: qsTr("Teléfono"); placeholder: qsTr("+57 300 000 0000") }
            LabeledField { id: nuevoCorreo; Layout.fillWidth: true; label: qsTr("Correo"); placeholder: qsTr("maria@correo.co") }
            LabeledField { id: nuevoDireccion; Layout.fillWidth: true; Layout.columnSpan: 2; label: qsTr("Dirección"); placeholder: qsTr("Calle 00 #00-00") }
            LabeledField { id: nuevoDni; Layout.fillWidth: true; Layout.columnSpan: 2; label: qsTr("DNI"); placeholder: qsTr("Opcional") }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoNuevo.close() },
            PrimaryButton {
                text: qsTr("Registrar")
                onClicked: {
                    propietarios.crear(nuevoNombre.text, nuevoTelefono.text, nuevoCorreo.text,
                                       nuevoDireccion.text, nuevoDni.text)
                    if (propietarios.error.length === 0)
                        dialogoNuevo.close()
                }
            }
        ]
    }

    AppDialog {
        id: dialogoEditar
        titulo: qsTr("Editar propietario")
        descripcion: propietarios.seleccionado.nombre || ""

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 12

            LabeledField { id: editarNombre; Layout.fillWidth: true; Layout.columnSpan: 2; label: qsTr("Nombre completo"); text: propietarios.seleccionado.nombre || "" }
            LabeledField { id: editarTelefono; Layout.fillWidth: true; label: qsTr("Teléfono"); text: propietarios.seleccionado.telefono || "" }
            LabeledField { id: editarCorreo; Layout.fillWidth: true; label: qsTr("Correo"); text: propietarios.seleccionado.correo || "" }
            LabeledField { id: editarDireccion; Layout.fillWidth: true; Layout.columnSpan: 2; label: qsTr("Dirección"); text: propietarios.seleccionado.direccion || "" }
            LabeledField { id: editarDni; Layout.fillWidth: true; Layout.columnSpan: 2; label: qsTr("DNI"); text: propietarios.seleccionado.dni || "" }
            CheckBox { id: editarActivo; text: qsTr("Activo"); checked: propietarios.seleccionado.activo === true }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoEditar.close() },
            PrimaryButton {
                text: qsTr("Guardar")
                onClicked: {
                    propietarios.actualizar(propietarios.seleccionado.uuid, editarNombre.text,
                                            editarTelefono.text, editarCorreo.text,
                                            editarDireccion.text, editarDni.text,
                                            editarActivo.checked)
                    if (propietarios.error.length === 0)
                        dialogoEditar.close()
                }
            }
        ]
    }
}
