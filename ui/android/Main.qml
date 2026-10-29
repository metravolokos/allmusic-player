import QtQuick
import QtQuick.Window

Window {
    width: 400
    height: 720
    visible: true
    title: "AllMusic Player — тест 001-C"
    color: "#f5f6f8"

    Column {
        anchors.centerIn: parent
        width: Math.max(1, parent.width - 64)
        spacing: 20

        Text {
            width: parent.width
            text: "AllMusic Player — проект запущен"
            color: "#17212b"
            font.pixelSize: 24
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
        Text {
            width: parent.width
            text: buildLabel
            color: "#526171"
            font.pixelSize: 14
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WrapAnywhere
        }
    }
}
