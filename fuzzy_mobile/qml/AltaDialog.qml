import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialogo

    property string clinica: ""
    property string clinicaNombre: ""

    title: qsTr("Solicitud a %1").arg(clinicaNombre)
    modal: true
    width: parent ? parent.width - estilo.spacing * 2 : 360
    anchors.centerIn: parent
    padding: estilo.spacing

    contentItem: ColumnLayout {
        spacing: estilo.spacing / 2

        Label {
            Layout.fillWidth: true
            text: qsTr("La clínica recibirá estos datos para contactarte. "
                       + "No incluyas información clínica sensible.")
            font.pixelSize: 13
            color: estilo.secondary
            wrapMode: Text.WordWrap
        }

        Field { id: nombre; Layout.fillWidth: true; etiqueta: qsTr("Tu nombre") }
        Field { id: correo; Layout.fillWidth: true; etiqueta: qsTr("Correo") }
        Field { id: telefono; Layout.fillWidth: true; etiqueta: qsTr("Teléfono") }
        Field { id: mascota; Layout.fillWidth: true; etiqueta: qsTr("Mascota") }
        Field { id: motivo; Layout.fillWidth: true; etiqueta: qsTr("Motivo") }
        Field {
            id: franja
            Layout.fillWidth: true
            etiqueta: qsTr("Franja preferida")
            marcador: qsTr("Mañana, tarde o una hora")
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
                enabled: nombre.texto.trim().length >= 3
                         && telefono.texto.trim().length >= 7
                onClicked: {
                    movil.enviarAlta(dialogo.clinica, nombre.texto, correo.texto,
                                     telefono.texto, mascota.texto, motivo.texto,
                                     franja.texto)
                    dialogo.close()
                }
            }
        }
    }
}
