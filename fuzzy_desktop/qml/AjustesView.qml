import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Ajustes: datos de la clínica y gestión del equipo con las reglas de auth
// (último Admin, correo único, temporal de 72 h).
Item {
    id: root
    objectName: "ajustesView"

    property string tab: "clinica"

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
            titulo: qsTr("Ajustes")
            subtitulo: qsTr("Configuración de la sede y del personal")

            StatusChip { estado: sesion.rol }

            PrimaryButton {
                text: qsTr("Invitar miembro")
                visible: root.tab === "equipo"
                onClicked: {
                    ajustes.limpiarMensajes()
                    dialogoUsuario.open()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Repeater {
                model: [
                    { clave: "clinica", texto: qsTr("Clínica") },
                    { clave: "equipo", texto: qsTr("Equipo") }
                ]

                delegate: Rectangle {
                    required property var modelData
                    width: 130
                    height: 38
                    radius: estilo.radiusPill
                    color: root.tab === modelData.clave ? estilo.ink : estilo.surface
                    border.width: 1
                    border.color: root.tab === modelData.clave ? estilo.ink : estilo.border

                    Label {
                        anchors.centerIn: parent
                        text: modelData.texto
                        font.pixelSize: 12
                        color: root.tab === modelData.clave ? "#FFFFFF" : estilo.textSecondary
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.tab = modelData.clave
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: avisoError.implicitHeight + 20
            radius: 10
            color: estilo.dangerBg
            border.width: 1
            border.color: "#F7C9CB"
            visible: ajustes.error.length > 0

            Label {
                id: avisoError
                anchors.fill: parent
                anchors.margins: 10
                text: ajustes.error
                color: estilo.danger
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: avisoOk.implicitHeight + 20
            radius: 10
            color: estilo.successBg
            border.width: 1
            border.color: "#CDEBD7"
            visible: ajustes.mensaje.length > 0

            Label {
                id: avisoOk
                anchors.fill: parent
                anchors.margins: 10
                text: ajustes.mensaje
                color: estilo.success
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }

        // ── Clínica ───────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14
            visible: root.tab === "clinica"

            SectionCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 520
                titulo: qsTr("Datos de la clínica")
                subtitulo: qsTr("Estos datos identifican la sede en el directorio comercial")

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    LabeledField { id: clinicaNombre; Layout.fillWidth: true; label: qsTr("Nombre"); text: ajustes.clinica.nombre || "" }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        LabeledField { id: clinicaCiudad; Layout.fillWidth: true; label: qsTr("Ciudad"); text: ajustes.clinica.ciudad || "" }
                        LabeledField { id: clinicaTelefono; Layout.fillWidth: true; label: qsTr("Teléfono"); text: ajustes.clinica.telefono || "" }
                    }
                    LabeledField { id: clinicaDireccion; Layout.fillWidth: true; label: qsTr("Dirección"); text: ajustes.clinica.direccion || "" }

                    Item { Layout.fillHeight: true }

                    PrimaryButton {
                        Layout.fillWidth: true
                        text: qsTr("Guardar cambios")
                        onClicked: ajustes.guardarClinica(clinicaNombre.text, clinicaCiudad.text,
                                                          clinicaDireccion.text, clinicaTelefono.text)
                    }
                }
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 380
                titulo: qsTr("Reglas de la instalación")
                subtitulo: qsTr("Fijas por diseño; no se editan desde la interfaz")

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    Repeater {
                        model: [
                            { icono: "shield-check", titulo: qsTr("Un Admin mínimo"), detalle: qsTr("El último administrador activo no se puede degradar ni desactivar.") },
                            { icono: "hourglass-split", titulo: qsTr("Temporal de 72 horas"), detalle: qsTr("Una clave temporal vence a las 72 h y obliga a cambiarla al entrar.") },
                            { icono: "people", titulo: qsTr("Correo único por clínica"), detalle: qsTr("El mismo correo puede existir en otra sede; nunca dos veces aquí.") },
                            { icono: "box-seam", titulo: qsTr("Datos que no salen"), detalle: qsTr("Las imágenes clínicas viven en el disco del nodo; solo viajan metadatos.") }
                        ]

                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 12

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
                                Label { text: modelData.titulo; font.pixelSize: 12; font.bold: true; color: estilo.ink }
                                Label { Layout.fillWidth: true; text: modelData.detalle; font.pixelSize: 10; color: estilo.textMuted; wrapMode: Text.WordWrap }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }

        // ── Equipo ────────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12
            visible: root.tab === "equipo"

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 58
                radius: 12
                color: estilo.infoBg
                border.width: 1
                border.color: "#C7D8FB"
                visible: ajustes.passwordTemporal.length > 0

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10
                    AppIcon { nombre: "shield-check"; color: estilo.info; tamano: 16 }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Contraseña temporal: %1 · válida 72 horas").arg(ajustes.passwordTemporal)
                        font.pixelSize: 12
                        font.family: "monospace"
                        color: estilo.info
                    }
                    SecondaryButton { text: qsTr("Ocultar"); onClicked: ajustes.limpiarPasswordTemporal() }
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
                        Layout.preferredHeight: 38
                        color: estilo.background

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18
                            anchors.rightMargin: 18
                            spacing: 12
                            Label { Layout.fillWidth: true; text: qsTr("PERSONA"); font.pixelSize: 10; font.letterSpacing: 1; color: estilo.textMuted }
                            Label { Layout.preferredWidth: 150; text: qsTr("ROL"); font.pixelSize: 10; font.letterSpacing: 1; color: estilo.textMuted }
                            Label { Layout.preferredWidth: 120; text: qsTr("ÚLTIMO ACCESO"); font.pixelSize: 10; font.letterSpacing: 1; color: estilo.textMuted }
                            Label { Layout.preferredWidth: 90; text: qsTr("ESTADO"); font.pixelSize: 10; font.letterSpacing: 1; color: estilo.textMuted }
                            Label { Layout.preferredWidth: 150; text: ""; font.pixelSize: 10 }
                        }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: ajustes.equipo

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
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Rectangle {
                                        width: 34
                                        height: 34
                                        radius: 17
                                        color: estilo.chip
                                        Label {
                                            anchors.centerIn: parent
                                            text: modelData.nombre.charAt(0).toUpperCase()
                                            font.pixelSize: 11
                                            font.bold: true
                                            color: estilo.textSecondary
                                        }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0
                                        Label { text: modelData.nombre; font.pixelSize: 12; font.bold: true; color: estilo.ink; elide: Text.ElideRight; Layout.fillWidth: true }
                                        Label { text: modelData.correo; font.pixelSize: 10; color: estilo.textMuted; elide: Text.ElideRight; Layout.fillWidth: true }
                                    }
                                }

                                Label { Layout.preferredWidth: 150; text: modelData.rol; font.pixelSize: 11; color: estilo.textSecondary }
                                Label { Layout.preferredWidth: 120; text: modelData.ultimoAcceso.length > 0 ? modelData.ultimoAcceso : "—"; font.pixelSize: 10; color: estilo.textMuted }
                                StatusChip { Layout.preferredWidth: 90; estado: modelData.activo ? qsTr("Activo") : qsTr("Inactivo") }

                                RowLayout {
                                    Layout.preferredWidth: 150
                                    spacing: 6
                                    SecondaryButton {
                                        text: qsTr("Clave")
                                        onClicked: ajustes.restablecerPassword(modelData.uuid)
                                    }
                                    SecondaryButton {
                                        text: qsTr("Editar")
                                        onClicked: {
                                            ajustes.limpiarMensajes()
                                            editarUsuarioUuid = modelData.uuid
                                            usuarioNombre.text = modelData.nombre
                                            usuarioCorreo.text = modelData.correo
                                            usuarioRol.text = modelData.rol
                                            usuarioActivo.checked = modelData.activo
                                            dialogoEditarUsuario.open()
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                anchors.bottom: parent.bottom
                                width: parent.width
                                height: 1
                                color: estilo.border
                            }
                        }
                    }
                }
            }
        }
    }

    property string editarUsuarioUuid: ""

    AppDialog {
        id: dialogoUsuario
        titulo: qsTr("Invitar miembro")
        descripcion: qsTr("Se emite una contraseña temporal de 12 caracteres, válida 72 horas.")

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12
            LabeledField { id: usuarioNombre; Layout.fillWidth: true; label: qsTr("Nombre completo"); placeholder: qsTr("Camila Ríos") }
            LabeledField { id: usuarioCorreo; Layout.fillWidth: true; label: qsTr("Correo"); placeholder: qsTr("camila@clinica.co") }
            ComboBox {
                id: usuarioRol
                Layout.fillWidth: true
                model: ajustes.roles
                textRole: "nombre"
            }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoUsuario.close() },
            PrimaryButton {
                text: qsTr("Invitar")
                onClicked: {
                    ajustes.crearUsuario(usuarioNombre.text, usuarioCorreo.text,
                                         usuarioRol.currentIndex >= 0 ? ajustes.roles[usuarioRol.currentIndex].nombre : "")
                    if (ajustes.error.length === 0)
                        dialogoUsuario.close()
                }
            }
        ]
    }

    AppDialog {
        id: dialogoEditarUsuario
        titulo: qsTr("Editar miembro")
        descripcion: qsTr("Los cambios de rol respetan la regla del último Admin.")

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12
            LabeledField { id: editarNombre; Layout.fillWidth: true; label: qsTr("Nombre completo") }
            LabeledField { id: editarCorreo; Layout.fillWidth: true; label: qsTr("Correo") }
            ComboBox { id: editarRol; Layout.fillWidth: true; model: ajustes.roles; textRole: "nombre" }
            CheckBox { id: editarActivo; text: qsTr("Cuenta activa") }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoEditarUsuario.close() },
            PrimaryButton {
                text: qsTr("Guardar")
                onClicked: {
                    ajustes.actualizarUsuario(root.editarUsuarioUuid, editarNombre.text,
                                              editarCorreo.text,
                                              editarRol.currentIndex >= 0 ? ajustes.roles[editarRol.currentIndex].nombre : "",
                                              editarActivo.checked)
                    if (ajustes.error.length === 0)
                        dialogoEditarUsuario.close()
                }
            }
        ]
    }

    Component.onCompleted: ajustes.cargar()
}
