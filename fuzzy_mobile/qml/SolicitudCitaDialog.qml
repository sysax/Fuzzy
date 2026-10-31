import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialogo

    title: qsTr("Pedir una cita")
    modal: true
    width: parent ? parent.width - estilo.spacing * 2 : 360
    anchors.centerIn: parent
    padding: estilo.spacing

    contentItem: ColumnLayout {
        spacing: estilo.spacing / 2

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Label {
                text: qsTr("Mascota")
                font.pixelSize: 13
                color: estilo.secondary
            }

            ComboBox {
                id: mascota
                Layout.fillWidth: true
                implicitHeight: estilo.touch
                model: movil.mascotas
                textRole: "nombre"
            }
        }

        Field {
            id: tipo
            Layout.fillWidth: true
            etiqueta: qsTr("Tipo de servicio")
            marcador: qsTr("Consulta, vacunación, baño…")
        }

        Field {
            id: fecha
            Layout.fillWidth: true
            etiqueta: qsTr("Fecha preferida")
            marcador: qsTr("2026-11-02")
        }

        Field {
            id: hora
            Layout.fillWidth: true
            etiqueta: qsTr("Hora preferida")
            marcador: qsTr("10:00")
        }

        Field {
            id: motivo
            Layout.fillWidth: true
            etiqueta: qsTr("Motivo")
            marcador: qsTr("Qué necesita tu mascota")
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: estilo.spacing / 2

            PrimaryButton {
                Layout.fillWidth: true
                text: qsTr("Cancelar")
                secundario: true
                onClicked: dialogo.close()
            }

            PrimaryButton {
                Layout.fillWidth: true
                text: qsTr("Enviar")
                enabled: tipo.texto.trim().length > 0 && fecha.texto.length === 10
                         && hora.texto.length >= 4 && mascota.currentIndex >= 0
                onClicked: {
                    var elegida = movil.mascotas[mascota.currentIndex]
                    movil.solicitarCita(elegida.uuid, tipo.texto, fecha.texto,
                                        hora.texto, motivo.texto)
                    dialogo.close()
                }
            }
        }
    }
}
