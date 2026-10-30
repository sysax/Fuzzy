import QtQuick

// Icono monocromo (bootstrap-icons) servido desde qrc:/icons.
Item {
    id: root

    property string nombre: ""
    property color color: estilo.ink
    property int tamano: 20

    readonly property bool blanco: color.r === 1 && color.g === 1 && color.b === 1
                                   && !root.nombre.endsWith("-white")

    implicitWidth: tamano
    implicitHeight: tamano

    Image {
        anchors.fill: parent
        source: root.nombre.length > 0
                ? "qrc:/icons/" + root.nombre + (root.blanco ? "-white" : "") + ".svg"
                : ""
        sourceSize.width: Math.round(root.tamano * 2)
        sourceSize.height: Math.round(root.tamano * 2)
        fillMode: Image.PreserveAspectFit
        smooth: true
    }
}
