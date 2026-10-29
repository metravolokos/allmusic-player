import QtQuick
import QtQuick.Window

Window {
    width: 720
    height: 360
    minimumWidth: 400
    minimumHeight: 240
    visible: true
    title: "AllMusic Player"
    color: "#f5f5f5"

    Text {
        anchors.centerIn: parent
        width: parent.width - 48
        text: "AllMusic Player — проект запущен"
        color: "#202020"
        font.pixelSize: 24
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }
}
