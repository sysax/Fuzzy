import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Pantalla de bloqueo: la primera vez crea la clave que cifra la caché; luego
// solo desbloquea. La clave no se guarda: se deriva con PBKDF2 en el core.
Page {
    id: pagina

    background: Rectangle { color: estilo.background }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: estilo.spacing * 1.5
        spacing: estilo.spacing

        Item { Layout.fillHeight: true }

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 72
            Layout.preferredHeight: 72
            radius: 24
            color: estilo.accentSoft

            Label {
                anchors.centerIn: parent
                text: "FC"
                font.pixelSize: 26
                font.weight: Font.Bold
                color: estilo.accent
            }
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: movil.configurada ? qsTr("Desbloquea tu FuzzyCloud")
                                    : qsTr("Protege tus datos")
            font.pixelSize: 24
            font.weight: Font.Bold
            color: estilo.ink
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: movil.configurada
                  ? qsTr("La caché de tus mascotas está cifrada en este teléfono.")
                  : qsTr("Elige una clave. Sin ella no se puede leer la caché ni el "
                         + "token de tus portales.")
            font.pixelSize: 14
            color: estilo.secondary
            wrapMode: Text.WordWrap
        }

        Field {
            id: clave
            Layout.fillWidth: true
            Layout.topMargin: estilo.spacing
            etiqueta: qsTr("Clave")
            marcador: qsTr("Mínimo 8 caracteres")
            eco: TextInput.Password
        }

        Field {
            id: confirmacion
            Layout.fillWidth: true
            visible: !movil.configurada
            etiqueta: qsTr("Repite la clave")
            marcador: qsTr("La misma clave")
            eco: TextInput.Password
        }

        PrimaryButton {
            Layout.fillWidth: true
            text: movil.configurada ? qsTr("Desbloquear") : qsTr("Crear clave")
            enabled: clave.texto.length >= 8
                     && (movil.configurada || clave.texto === confirmacion.texto)
            onClicked: {
                if (movil.configurada)
                    movil.desbloquear(clave.texto)
                else
                    movil.configurarClave(clave.texto)
            }
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("La clave se queda en tu cabeza: si la pierdes, "
                       + "tendrás que volver a emparejar tus portales.")
            font.pixelSize: 12
            color: estilo.muted
            wrapMode: Text.WordWrap
        }

        Item { Layout.fillHeight: true }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("FuzzyCloud %1").arg(versionApp)
            font.pixelSize: 11
            color: estilo.muted
        }
    }
}
