import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialogo

    title: movil.perfil.length > 0 ? qsTr("Editar mis datos") : qsTr("Mis datos")
    modal: true
    width: parent ? parent.width - estilo.spacing * 2 : 360
    anchors.centerIn: parent
    padding: estilo.spacing

    onOpened: {
        if (movil.perfil.length > 0) {
            nombre.texto = movil.perfil[0].nombre_completo || ""
            correo.texto = movil.perfil[0].correo || ""
            telefono.texto = movil.perfil[0].telefono || ""
            direccion.texto = movil.perfil[0].direccion || ""
            dni.texto = movil.perfil[0].dni || ""
        }
    }

    contentItem: ColumnLayout {
        spacing: estilo.spacing / 2

        Field { id: nombre; Layout.fillWidth: true; etiqueta: qsTr("Nombre completo") }
        Field { id: correo; Layout.fillWidth: true; etiqueta: qsTr("Correo") }
        Field { id: telefono; Layout.fillWidth: true; etiqueta: qsTr("Teléfono") }
        Field { id: direccion; Layout.fillWidth: true; etiqueta: qsTr("Dirección") }
        Field { id: dni; Layout.fillWidth: true; etiqueta: qsTr("Documento") }

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
                enabled: nombre.texto.trim().length >= 3
                onClicked: {
                    if (movil.perfil.length > 0)
                        movil.actualizarPerfil(nombre.texto, correo.texto, telefono.texto,
                                               direccion.texto, dni.texto)
                    else
                        movil.registrarPerfil(nombre.texto, correo.texto, telefono.texto,
                                              direccion.texto, dni.texto)
                    dialogo.close()
                }
            }
        }
    }
}
