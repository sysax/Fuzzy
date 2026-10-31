import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialogo

    title: qsTr("Nueva mascota en el teléfono")
    modal: true
    width: parent ? parent.width - estilo.spacing * 2 : 360
    anchors.centerIn: parent
    padding: estilo.spacing

    contentItem: ColumnLayout {
        spacing: estilo.spacing / 2

        Label {
            Layout.fillWidth: true
            text: qsTr("Quedará guardada mientras emparejas el portal. La clínica la "
                       + "registrará cuando importe tus datos.")
            font.pixelSize: 13
            color: estilo.secondary
            wrapMode: Text.WordWrap
        }

        Field { id: nombre; Layout.fillWidth: true; etiqueta: qsTr("Nombre") }
        Field { id: especie; Layout.fillWidth: true; etiqueta: qsTr("Especie") }
        Field { id: raza; Layout.fillWidth: true; etiqueta: qsTr("Raza") }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Label {
                text: qsTr("Sexo")
                font.pixelSize: 13
                color: estilo.secondary
            }

            ComboBox {
                id: sexo
                Layout.fillWidth: true
                implicitHeight: estilo.touch
                model: [qsTr("Desconocido"), qsTr("Macho"), qsTr("Hembra")]
            }
        }

        Field {
            id: peso
            Layout.fillWidth: true
            etiqueta: qsTr("Peso en kg")
            texto: "0"
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
                text: qsTr("Guardar")
                enabled: nombre.texto.trim().length > 0 && especie.texto.trim().length > 0
                onClicked: {
                    var valor = parseFloat(peso.texto.replace(",", "."))
                    movil.registrarMascota(nombre.texto, especie.texto, raza.texto,
                                           sexo.currentText, isNaN(valor) ? 0 : valor)
                    dialogo.close()
                }
            }
        }
    }
}
