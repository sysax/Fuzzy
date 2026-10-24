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
    title: qsTr("FuzzyCloud")

    Label {
        anchors.centerIn: parent
        text: qsTr("FuzzyCloud %1 — fase 0").arg(root.appVersion)
        font.pixelSize: 20
    }
}
