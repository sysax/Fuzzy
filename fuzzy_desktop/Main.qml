import QtQuick
import QtQuick.Controls.Basic

ApplicationWindow {
    id: root

    property string appVersion: ""

    width: 960
    height: 600
    minimumWidth: 720
    minimumHeight: 480
    visible: true
    title: qsTr("Fuzzy")

    Label {
        anchors.centerIn: parent
        text: qsTr("Fuzzy %1 — fase 0").arg(root.appVersion)
        font.pixelSize: 20
    }
}
