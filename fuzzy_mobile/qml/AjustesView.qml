import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: pagina

    background: Rectangle { color: estilo.background }

    ScrollView {
        anchors.fill: parent
        contentWidth: contenido.width
        clip: true

        ColumnLayout {
            id: contenido
            width: pagina.width
            spacing: estilo.spacing

            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: estilo.spacing
                spacing: estilo.spacing

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Ajustes")
                    font.pixelSize: 22
                    font.weight: Font.Bold
                    color: estilo.ink
                }

                Card {
                    Layout.fillWidth: true

                    RowLayout {
                        Layout.fillWidth: true

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Modo avión")
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                                color: estilo.ink
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Trabaja solo con lo descargado; la cola "
                                           + "se envía al desactivarlo.")
                                font.pixelSize: 12
                                color: estilo.secondary
                                wrapMode: Text.WordWrap
                            }
                        }

                        Switch {
                            checked: movil.modoAvion
                            onToggled: movil.setModoAvion(checked)
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Sincronización")
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        color: estilo.ink
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Estado: %1").arg(movil.estadoSync)
                              + " · " + qsTr("%1 en cola").arg(movil.pendientes)
                        font.pixelSize: 13
                        color: estilo.secondary
                    }

                    PrimaryButton {
                        Layout.fillWidth: true
                        text: qsTr("Sincronizar ahora")
                        enabled: movil.hayPortales && !movil.modoAvion
                        onClicked: movil.sincronizar()
                    }
                }

                Card {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Seguridad")
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        color: estilo.ink
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("La caché está cifrada y caduca a los 30 días. "
                                  + "Al cerrar sesión se borra de este teléfono.")
                        font.pixelSize: 12
                        color: estilo.secondary
                        wrapMode: Text.WordWrap
                    }

                    PrimaryButton {
                        Layout.fillWidth: true
                        text: qsTr("Bloquear ahora")
                        secundario: true
                        onClicked: movil.bloquear()
                    }

                    PrimaryButton {
                        Layout.fillWidth: true
                        text: qsTr("Borrar caché descargada")
                        secundario: true
                        onClicked: confirmarCache.open()
                    }

                    PrimaryButton {
                        Layout.fillWidth: true
                        text: qsTr("Cerrar sesión y borrar todo")
                        peligro: true
                        onClicked: confirmarSalida.open()
                    }
                }

                Card {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Acerca de")
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        color: estilo.ink
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("FuzzyCloud %1 · un portal por clínica. "
                                  + "El token de tu portal nunca viaja en claro.")
                            .arg(versionApp)
                        font.pixelSize: 12
                        color: estilo.secondary
                        wrapMode: Text.WordWrap
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }
    }

    Dialog {
        id: confirmarCache

        title: qsTr("Borrar caché")
        modal: true
        anchors.centerIn: parent
        width: parent ? parent.width - estilo.spacing * 2 : 340
        padding: estilo.spacing

        contentItem: ColumnLayout {
            spacing: estilo.spacing

            Label {
                Layout.fillWidth: true
                text: qsTr("Se borrará lo descargado, pero seguirás emparejado. "
                           + "Vuelve a sincronizar para recuperarlo.")
                wrapMode: Text.WordWrap
                color: estilo.secondary
            }

            RowLayout {
                Layout.fillWidth: true

                PrimaryButton {
                    Layout.fillWidth: true
                    text: qsTr("Cancelar")
                    secundario: true
                    onClicked: confirmarCache.close()
                }

                PrimaryButton {
                    Layout.fillWidth: true
                    text: qsTr("Borrar")
                    peligro: true
                    onClicked: {
                        movil.limpiarCache()
                        confirmarCache.close()
                    }
                }
            }
        }
    }

    Dialog {
        id: confirmarSalida

        title: qsTr("Cerrar sesión")
        modal: true
        anchors.centerIn: parent
        width: parent ? parent.width - estilo.spacing * 2 : 340
        padding: estilo.spacing

        contentItem: ColumnLayout {
            spacing: estilo.spacing

            Label {
                Layout.fillWidth: true
                text: qsTr("Se borrarán los portales, la caché y la clave de este "
                           + "teléfono. Necesitarás volver a emparejar.")
                wrapMode: Text.WordWrap
                color: estilo.danger
            }

            RowLayout {
                Layout.fillWidth: true

                PrimaryButton {
                    Layout.fillWidth: true
                    text: qsTr("Cancelar")
                    secundario: true
                    onClicked: confirmarSalida.close()
                }

                PrimaryButton {
                    Layout.fillWidth: true
                    text: qsTr("Cerrar sesión")
                    peligro: true
                    onClicked: {
                        movil.cerrarSesion()
                        confirmarSalida.close()
                    }
                }
            }
        }
    }
}
