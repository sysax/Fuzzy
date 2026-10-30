import QtQuick
import QtQuick.Controls

// Etiqueta de estado. El estado activo va en pill oscuro y el resto en
// relleno/borde neutro, con acentos solo para alertas.
Rectangle {
    id: chip

    property string estado: ""

    readonly property bool positivo: estado === "Pagada" || estado === "Activa"
                                     || estado === "Completada" || estado === "Activo"
    readonly property bool alerta: estado === "En espera" || estado === "Pendiente"
                                   || estado === "Pendiente de cambio" || estado === "Borrador"
    readonly property bool solido: estado === "En curso" || estado === "Emergencia"

    implicitHeight: 24
    implicitWidth: etiqueta.implicitWidth + 20
    radius: estilo.radiusPill
    color: solido ? estilo.ink
         : positivo ? estilo.successBg
         : alerta ? estilo.warningBg
         : estado === "Cancelada" || estado === "Revocado" ? estilo.dangerBg
         : estilo.surface
    border.width: 1
    border.color: solido ? "transparent"
                : positivo ? "#CDEBD7"
                : alerta ? "#F3DFA8"
                : estado === "Cancelada" || estado === "Revocado" ? "#F7C9CB"
                : estilo.border

    Label {
        id: etiqueta
        anchors.centerIn: parent
        text: chip.estado
        color: chip.solido ? "#FFFFFF"
             : chip.positivo ? estilo.success
             : chip.alerta ? estilo.warning
             : chip.estado === "Cancelada" || chip.estado === "Revocado" ? estilo.danger
             : estilo.textSecondary
        font.pixelSize: 12
    }
}
