import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Acceso del escritorio con tres estados: bootstrap de la instalación, inicio
// de sesión y cambio obligatorio de la contraseña temporal (D8).
Item {
    id: root
    objectName: "accessView"

    property string modo: "login"
    property string errorLocal: ""
    readonly property string errorTexto: root.errorLocal.length > 0 ? root.errorLocal : sesion.error
    readonly property string mensajeTexto: sesion.mensaje

    signal cambiarModo(string modo)

    Rectangle {
        anchors.fill: parent
        color: estilo.background
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ── Marca ─────────────────────────────────────────────
        Rectangle {
            Layout.preferredWidth: Math.max(420, root.width * 0.42)
            Layout.fillHeight: true
            color: estilo.ink
            clip: true

            Rectangle {
                width: 520
                height: 520
                radius: 260
                x: -160
                y: -180
                color: "#1D1D1B"
            }

            Rectangle {
                width: 380
                height: 380
                radius: 190
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: -120
                anchors.bottomMargin: -140
                color: "#1A1A18"
            }

            Rectangle {
                width: 2
                height: parent.height * 0.52
                anchors.right: parent.right
                anchors.rightMargin: 64
                anchors.verticalCenter: parent.verticalCenter
                color: "#2C2C29"
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 56
                spacing: 16

                RowLayout {
                    spacing: 12

                    Rectangle {
                        width: 46
                        height: 46
                        radius: 14
                        color: "#FFFFFF"

                        Label {
                            anchors.centerIn: parent
                            text: "FC"
                            color: estilo.ink
                            font.pixelSize: 16
                            font.bold: true
                        }
                    }

                    ColumnLayout {
                        spacing: 0

                        Label {
                            text: "FuzzyCloud"
                            color: "#FFFFFF"
                            font.pixelSize: 22
                            font.bold: true
                        }

                        Label {
                            text: qsTr("Gestión veterinaria")
                            color: estilo.textMuted
                            font.pixelSize: 12
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Label {
                    text: qsTr("SISTEMA VETERINARIO")
                    color: "#8C8C88"
                    font.pixelSize: 11
                    font.letterSpacing: 3
                }

                Label {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 420
                    text: qsTr("Recepción, agenda, pacientes, inventario y facturación en un solo lugar.")
                    color: "#F0F0EE"
                    font.pixelSize: 30
                    font.bold: true
                    wrapMode: Text.WordWrap
                    lineHeight: 0.95
                }

                Label {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 400
                    text: qsTr("El nodo de la clínica trabaja sin conexión. El portal del dueño se sincroniza cuando hay red.")
                    color: "#A8A8A4"
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                }

                ColumnLayout {
                    Layout.topMargin: 20
                    spacing: 12

                    Repeater {
                        model: [
                            qsTr("Historial clínico siempre disponible en el equipo"),
                            qsTr("Agenda por sede, sin mezclar clínicas"),
                            qsTr("Cada portal aislado con su propio token")
                        ]

                        delegate: RowLayout {
                            required property string modelData
                            spacing: 10

                            AppIcon {
                                nombre: "check-circle-white"
                                color: "#FFFFFF"
                                tamano: 16
                            }

                            Label {
                                text: modelData
                                color: "#C9C9C5"
                                font.pixelSize: 13
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Label {
                    text: qsTr("FuzzyCloud %1").arg(appVersion)
                    color: "#6F6F6B"
                    font.pixelSize: 11
                }
            }
        }

        // ── Formulario ────────────────────────────────────────
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Flickable {
                id: desplazable
                anchors.fill: parent
                contentWidth: width
                contentHeight: Math.max(height, formulario.implicitHeight + 96)
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                ColumnLayout {
                    id: formulario
                    width: Math.min(420, desplazable.width - 96)
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 14

                        Label {
                            text: root.modo === "bootstrap" ? qsTr("Alta de la clínica")
                                  : root.modo === "cambio" ? qsTr("Cambia tu contraseña")
                                  : qsTr("Inicia sesión")
                            font.pixelSize: 30
                            font.bold: true
                            color: estilo.ink
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.modo === "bootstrap"
                                  ? qsTr("Esta instalación aún no tiene clínica. Crea la primera cuenta de administración.")
                                  : root.modo === "cambio"
                                  ? qsTr("La contraseña temporal vence a las 72 horas. Define una nueva para continuar.")
                                  : qsTr("Usa el correo y la contraseña que te entregó el administrador de tu clínica.")
                            font.pixelSize: 13
                            color: estilo.textSecondary
                            wrapMode: Text.WordWrap
                        }

                        // Error / mensaje
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: mensaje.implicitHeight + 20
                            radius: 10
                            color: estilo.dangerBg
                            border.width: 1
                            border.color: "#F7C9CB"
                            visible: root.errorTexto.length > 0

                            Label {
                                id: mensaje
                                anchors.fill: parent
                                anchors.margins: 10
                                text: root.errorTexto
                                color: estilo.danger
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: aviso.implicitHeight + 20
                            radius: 10
                            color: estilo.successBg
                            border.width: 1
                            border.color: "#CDEBD7"
                            visible: root.mensajeTexto.length > 0

                            Label {
                                id: aviso
                                anchors.fill: parent
                                anchors.margins: 10
                                text: root.mensajeTexto
                                color: estilo.success
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                        }

                        // ── Bootstrap ─────────────────────────
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            visible: root.modo === "bootstrap"

                            LabeledField { id: clinicaNombre; label: qsTr("Nombre de la clínica"); placeholder: qsTr("Clínica Veterinaria Norte") }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12
                                LabeledField { id: clinicaCiudad; Layout.fillWidth: true; label: qsTr("Ciudad"); placeholder: qsTr("Bogotá") }
                                LabeledField { id: adminNombre; Layout.fillWidth: true; label: qsTr("Administrador"); placeholder: qsTr("Nombre completo") }
                            }
                            LabeledField { id: adminCorreo; Layout.fillWidth: true; label: qsTr("Correo"); placeholder: qsTr("admin@clinica.co") }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12
                                LabeledField { id: adminPassword; Layout.fillWidth: true; label: qsTr("Contraseña"); password: true; placeholder: qsTr("Mínimo 8 caracteres") }
                                LabeledField { id: adminPassword2; Layout.fillWidth: true; label: qsTr("Repetir contraseña"); password: true; placeholder: qsTr("Repite la contraseña") }
                            }
                        }

                        // ── Login ─────────────────────────────
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            visible: root.modo === "login"

                            LabeledField {
                                id: correo
                                Layout.fillWidth: true
                                label: qsTr("Correo")
                                placeholder: qsTr("tu@clinica.co")
                                onAccepted: entrar.clicked()
                            }

                            LabeledField {
                                id: password
                                Layout.fillWidth: true
                                label: qsTr("Contraseña")
                                password: true
                                placeholder: qsTr("Tu contraseña")
                                onAccepted: entrar.clicked()
                            }
                        }

                        // ── Cambio obligatorio ────────────────
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            visible: root.modo === "cambio"

                            LabeledField { id: actual; Layout.fillWidth: true; label: qsTr("Contraseña temporal"); password: true; placeholder: qsTr("La que te entregaron") }
                            LabeledField { id: nueva; Layout.fillWidth: true; label: qsTr("Nueva contraseña"); password: true; placeholder: qsTr("Mínimo 8 caracteres") }
                            LabeledField { id: nueva2; Layout.fillWidth: true; label: qsTr("Repetir nueva contraseña"); password: true; placeholder: qsTr("Repite la contraseña") }
                        }

                        PrimaryButton {
                            id: entrar
                            Layout.fillWidth: true
                            Layout.topMargin: 6
                            text: root.modo === "bootstrap" ? qsTr("Crear clínica y entrar")
                                  : root.modo === "cambio" ? qsTr("Guardar contraseña")
                                  : qsTr("Entrar")
                            onClicked: {
                                sesion.limpiarMensajes()
                                root.errorLocal = ""
                                if (root.modo === "bootstrap") {
                                    if (adminPassword.text !== adminPassword2.text) {
                                        root.errorLocal = qsTr("Las contraseñas no coinciden.")
                                        return
                                    }
                                    sesion.bootstrap(clinicaNombre.text, clinicaCiudad.text,
                                                     adminNombre.text, adminCorreo.text,
                                                     adminPassword.text)
                                } else if (root.modo === "cambio") {
                                    if (nueva.text !== nueva2.text) {
                                        root.errorLocal = qsTr("Las contraseñas no coinciden.")
                                        return
                                    }
                                    sesion.cambiarPassword(actual.text, nueva.text)
                                } else {
                                    sesion.iniciarSesion(correo.text, password.text)
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.topMargin: 4
                            visible: root.modo === "login"
                            text: qsTr("¿Primera vez en este equipo? La clínica se crea una sola vez.")
                            color: estilo.textMuted
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                        }

                        SecondaryButton {
                            Layout.fillWidth: true
                            visible: root.modo === "login" && !sesion.hayInstalacion
                            text: qsTr("Crear la clínica en este equipo")
                            onClicked: root.cambiarModo("bootstrap")
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.topMargin: 2
                            visible: root.modo === "bootstrap"
                            text: qsTr("El alta solo se ofrece mientras no exista una clínica en este equipo.")
                            color: estilo.textMuted
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                        }

                        SecondaryButton {
                            Layout.fillWidth: true
                            visible: root.modo === "bootstrap"
                            text: qsTr("Volver al acceso")
                            onClicked: {
                                sesion.limpiarMensajes()
                                root.cambiarModo("login")
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.topMargin: 2
                            visible: root.modo === "cambio"
                            text: qsTr("Al guardar podrás entrar con la nueva contraseña.")
                            color: estilo.textMuted
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
    }
}
