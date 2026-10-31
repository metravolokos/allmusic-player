import QtQuick
import QtQuick.Window

Window {
    width: 400
    height: 760
    visible: true
    title: "AllMusic Player — Media3 spike"
    color: "#f5f6f8"

    function formatTime(milliseconds) {
        var seconds = Math.max(0, Math.floor(milliseconds / 1000))
        return Math.floor(seconds / 60) + ":" + ("0" + (seconds % 60)).slice(-2)
    }

    Flickable {
        id: content
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.implicitHeight + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: contentColumn
            x: (content.width - width) / 2
            y: 20
            width: Math.max(1, content.width - 40)
            spacing: 13

            Text {
                width: parent.width
                text: "Локальная проверка Media3"
                color: "#17212b"
                font.pixelSize: 25
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: "MP3 и FLAC · системные декодеры Android · AudioTrack"
                color: "#526171"
                font.pixelSize: 15
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: "Активный трек: " + media3.activeTrackName
                color: "#17212b"
                font.pixelSize: 16
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Repeater {
                model: media3.trackNames

                delegate: Rectangle {
                    required property int index
                    required property string modelData
                    width: contentColumn.width
                    height: 48
                    radius: 10
                    color: media3.trackIndex === index ? "#dceaf5" : "#ffffff"
                    border.width: media3.trackIndex === index ? 2 : 1
                    border.color: media3.trackIndex === index ? "#1769aa" : "#d5dce3"

                    Text {
                        anchors.fill: parent
                        anchors.margins: 8
                        text: modelData
                        color: "#17212b"
                        font.pixelSize: 14
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: media3.selectTrack(index)
                    }
                }
            }

            Text {
                width: parent.width
                text: "Состояние: " + media3.stateText
                color: "#17212b"
                font.pixelSize: 16
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: media3.outputStatusText
                color: "#526171"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Row {
                width: parent.width
                spacing: 8

                Rectangle {
                    width: (parent.width - parent.spacing * 2) / 3
                    height: 52
                    radius: 10
                    color: media3.playEnabled ? "#1769aa" : "#aab6c2"
                    Text {
                        anchors.centerIn: parent
                        text: "Play / Resume"
                        color: "white"
                        font.pixelSize: 13
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: media3.playEnabled
                        onClicked: media3.play()
                    }
                }

                Rectangle {
                    width: (parent.width - parent.spacing * 2) / 3
                    height: 52
                    radius: 10
                    color: media3.pauseEnabled ? "#1769aa" : "#aab6c2"
                    Text {
                        anchors.centerIn: parent
                        text: "Пауза"
                        color: "white"
                        font.pixelSize: 14
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: media3.pauseEnabled
                        onClicked: media3.pause()
                    }
                }

                Rectangle {
                    width: (parent.width - parent.spacing * 2) / 3
                    height: 52
                    radius: 10
                    color: media3.stopEnabled ? "#a43b3b" : "#aab6c2"
                    Text {
                        anchors.centerIn: parent
                        text: "Stop"
                        color: "white"
                        font.pixelSize: 14
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: media3.stopEnabled
                        onClicked: media3.stop()
                    }
                }
            }

            Row {
                id: timeLabels
                width: parent.width
                Text {
                    id: elapsed
                    text: formatTime(media3.positionMs)
                    color: "#526171"
                    font.pixelSize: 13
                }
                Item {
                    width: timeLabels.width - elapsed.width - remaining.width
                    height: 1
                }
                Text {
                    id: remaining
                    text: formatTime(media3.durationMs)
                    color: "#526171"
                    font.pixelSize: 13
                }
            }

            Rectangle {
                id: seekBar
                width: parent.width
                height: 24
                radius: 12
                color: "#d5dce3"
                property real fraction: media3.durationMs > 0
                    ? Math.max(0, Math.min(1, media3.positionMs / media3.durationMs)) : 0

                Rectangle {
                    width: seekBar.width * seekBar.fraction
                    height: parent.height
                    radius: parent.radius
                    color: media3.seekEnabled ? "#4e91c3" : "#aab6c2"
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: media3.seekEnabled
                    onClicked: function(mouse) {
                        media3.seekTo(Math.round(mouse.x / width * media3.durationMs))
                    }
                }
            }

            Text {
                width: parent.width
                visible: media3.errorText.length > 0
                text: "Ошибка: " + media3.errorText
                color: "#a32020"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: "При сворачивании приложения воспроизведение останавливается. Фоновая служба в spike не включена."
                color: "#526171"
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: buildLabel
                color: "#526171"
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WrapAnywhere
            }
        }
    }
}
