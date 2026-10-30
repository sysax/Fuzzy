import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Shell post-login: sidebar + topbar. El contenido es un StackLayout con una
// página por módulo; cada página consulta su controlador al cargarse.
Item {
    id: root
    objectName: "shellView"

    property int seleccion: 0
    property string seccionForzada: ""

    readonly property var navegacion: [
        { nombre: qsTr("Panel"), icono: "grid-1x2", permiso: "VerPanel" },
        { nombre: qsTr("Agenda"), icono: "calendar3", permiso: "GestionarCitas" },
        { nombre: qsTr("Pacientes"), icono: "people", permiso: "RegistrarConsulta" },
        { nombre: qsTr("Historial clínico"), icono: "file-medical", permiso: "RegistrarConsulta" },
        { nombre: qsTr("Propietarios"), icono: "person-vcard", permiso: "RegistrarCliente" },
        { nombre: qsTr("Inventario"), icono: "box-seam", permiso: "GestionarInventario" },
        { nombre: qsTr("Facturación"), icono: "receipt", permiso: "EmitirFactura" },
        { nombre: qsTr("Vínculos"), icono: "shield-check", permiso: "AprobarVinculo" },
        { nombre: qsTr("Ajustes"), icono: "gear", permiso: "GestionarUsuarios" }
    ]

    function irA(indice) {
        if (indice >= 0 && indice < navegacion.length)
            root.seleccion = indice
    }

    Rectangle {
        anchors.fill: parent
        color: estilo.background
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ── Sidebar ───────────────────────────────────────────
        Rectangle {
            Layout.preferredWidth: 232
            Layout.fillHeight: true
            color: estilo.ink

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: 22
                anchors.bottomMargin: 18
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 6

                RowLayout {
                    Layout.leftMargin: 6
                    Layout.bottomMargin: 20
                    spacing: 10

                    Rectangle {
                        width: 34
                        height: 34
                        radius: 10
                        color: "#FFFFFF"

                        Label {
                            anchors.centerIn: parent
                            text: "FC"
                            color: estilo.ink
                            font.pixelSize: 12
                            font.bold: true
                        }
                    }

                    ColumnLayout {
                        spacing: 0

                        Label {
                            text: "FuzzyCloud"
                            color: "#FFFFFF"
                            font.pixelSize: 16
                            font.bold: true
                        }

                        Label {
                            text: sesion.nombreClinica.length > 0 ? sesion.nombreClinica
                                                                  : qsTr("Clínica")
                            color: "#8C8C88"
                            font.pixelSize: 10
                            elide: Text.ElideRight
                            Layout.maximumWidth: 140
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                Repeater {
                    model: root.navegacion

                    delegate: Rectangle {
                        required property int index
                        required property var modelData

                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        radius: 10
                        color: root.seleccion === index ? "#FFFFFF" : "transparent"
                        opacity: sesion.puede(modelData.permiso) ? 1 : 0.35

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 10

                            AppIcon {
                                nombre: modelData.icono + (root.seleccion === index ? "" : "-white")
                                color: root.seleccion === index ? estilo.ink : "#FFFFFF"
                                tamano: 16
                            }

                            Label {
                                Layout.fillWidth: true
                                text: modelData.nombre
                                color: root.seleccion === index ? estilo.ink : "#D2D2CE"
                                font.pixelSize: 13
                                font.bold: root.seleccion === index
                                elide: Text.ElideRight
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            enabled: sesion.puede(modelData.permiso)
                            onClicked: root.irA(index)
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#2C2C29"
                }

                RowLayout {
                    Layout.topMargin: 12
                    Layout.leftMargin: 4
                    spacing: 10

                    Rectangle {
                        width: 32
                        height: 32
                        radius: 16
                        color: "#2C2C29"

                        Label {
                            anchors.centerIn: parent
                            text: sesion.nombre.length > 0 ? sesion.nombre.charAt(0).toUpperCase() : "?"
                            color: "#FFFFFF"
                            font.bold: true
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        Label {
                            Layout.fillWidth: true
                            text: sesion.nombre
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }

                        Label {
                            text: sesion.rol
                            color: "#8C8C88"
                            font.pixelSize: 10
                        }
                    }

                    Rectangle {
                        width: 28
                        height: 28
                        radius: 8
                        color: "transparent"
                        AppIcon {
                            anchors.centerIn: parent
                            nombre: "box-arrow-right"
                            color: "#8C8C88"
                            tamano: 15
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: sesion.cerrarSesion()
                        }
                    }
                }
            }
        }

        // ── Contenido ─────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                color: estilo.surface
                border.color: estilo.border

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 28
                    anchors.rightMargin: 28
                    spacing: 16

                    ColumnLayout {
                        spacing: 0

                        Label {
                            text: root.navegacion[root.seleccion].nombre
                            font.pixelSize: 14
                            font.bold: true
                            color: estilo.ink
                        }

                        Label {
                            text: qsTr("Nodo local · %1").arg(Qt.application.name)
                            font.pixelSize: 10
                            color: estilo.textMuted
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        implicitWidth: 190
                        implicitHeight: 36
                        radius: estilo.radiusPill
                        color: estilo.background
                        border.width: 1
                        border.color: estilo.border

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 8

                            AppIcon { nombre: "shield-check"; color: estilo.textMuted; tamano: 14 }
                            Label {
                                Layout.fillWidth: true
                                text: sesion.rol
                                color: estilo.textSecondary
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                        }
                    }

                    Rectangle {
                        implicitWidth: 36
                        implicitHeight: 36
                        radius: 18
                        color: estilo.surface
                        border.width: 1
                        border.color: estilo.border

                        AppIcon {
                            anchors.centerIn: parent
                            nombre: "bell"
                            color: estilo.textSecondary
                            tamano: 16
                        }
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.seleccion

                PanelView { onIrAgenda: root.irA(1) }
                AgendaView {}
                PacientesView {}
                HistorialView { onAbrirPaciente: root.irA(2) }
                PropietariosView { onIrAPacientes: root.irA(2) }
                InventarioView {}
                FacturacionView {}
                VinculosView {}
                AjustesView { tab: root.seccionForzada === "equipo" ? "equipo" : "clinica" }
            }
        }
    }
}
