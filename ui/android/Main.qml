import QtQuick
import QtQuick.Window

Window {
    width: 400
    height: 720
    visible: true
    title: "AllMusic Player — FFmpeg spike 002-D"
    color: "#f5f6f8"

    Flickable {
        id: content
        anchors.fill: parent
        contentWidth: width
        contentHeight: Math.max(contentColumn.implicitHeight + 40, height)
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: contentColumn
            x: (content.width - width) / 2
            y: Math.max(20, (content.height - implicitHeight) / 2)
            width: Math.max(1, content.width - 40)
            spacing: 16

            Text {
                width: parent.width
                text: "Android Qt FFmpeg spike"
                color: "#17212b"
                font.pixelSize: 25
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: "Локальные синтетические WAV, MP3 и FLAC. Интернет и доступ к пользовательским файлам не используются."
                color: "#526171"
                font.pixelSize: 16
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: "Состояние: " + audioTest.stateText
                color: "#17212b"
                font.pixelSize: 17
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: "Источник: " + audioTest.mediaStatusText
                color: "#526171"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: "Аудиодвижок: " + audioTest.backendStatusText
                color: audioTest.backendStatusText.indexOf("подтверждён") >= 0 ? "#176b38" : "#8a5a00"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: audioTest.outputStatusText
                color: "#526171"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WrapAnywhere
            }

            Text {
                width: parent.width
                text: "Выбранный формат: " + audioTest.trackText
                color: "#17212b"
                font.pixelSize: 15
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: audioTest.playerAudioStatusText
                color: "#526171"
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: audioTest.pcmStatusText
                color: audioTest.pcmStatusText.indexOf("ненулевых 0") >= 0 ? "#8a5a00" : "#526171"
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: audioTest.syntheticOutputStatusText
                color: "#526171"
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: "Источник: " + audioTest.sourceDeviceText
                color: "#526171"
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Row {
                width: parent.width
                spacing: 8

                Repeater {
                    model: ["WAV 002-A", "MP3 QBuffer", "FLAC QBuffer"]
                    delegate: Rectangle {
                        required property string modelData
                        required property int index
                        property bool selected: audioTest.selectedTrackIndex === index
                        width: (parent.width - 16) / 3
                        height: 48
                        radius: 10
                        color: selected ? "#1769aa" : "#dce5ed"
                        border.width: selected ? 2 : 0
                        border.color: "#0e4f83"
                        Text {
                            anchors.centerIn: parent
                            width: parent.width - 8
                            text: modelData
                            color: selected ? "white" : "#17212b"
                            font.pixelSize: 12
                            font.bold: selected
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: audioTest.selectTrack(index)
                        }
                    }
                }
            }

            Text {
                width: parent.width
                text: audioTest.duration > 0
                    ? Math.floor(audioTest.position / 60000) + ":"
                        + ("0" + Math.floor((audioTest.position / 1000) % 60)).slice(-2)
                        + " / "
                        + Math.floor(audioTest.duration / 60000) + ":"
                        + ("0" + Math.floor((audioTest.duration / 1000) % 60)).slice(-2)
                    : "Позиция / длительность: ожидание источника"
                color: "#526171"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
            }

            Rectangle {
                id: seekBar
                width: parent.width
                height: 30
                radius: 8
                color: "#dce5ed"
                property real progress: audioTest.duration > 0
                    ? Math.max(0, Math.min(1, audioTest.position / audioTest.duration)) : 0

                Rectangle {
                    width: parent.width * seekBar.progress
                    height: parent.height
                    radius: parent.radius
                    color: "#4c8fbe"
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: audioTest.seekEnabled
                    function moveTo(x) {
                        audioTest.seekTo(Math.round(audioTest.duration
                            * Math.max(0, Math.min(1, x / width))))
                    }
                    onPressed: moveTo(mouse.x)
                    onPositionChanged: if (pressed) moveTo(mouse.x)
                }
            }

            Rectangle {
                width: parent.width
                height: 54
                radius: 12
                color: audioTest.syntheticOutputTestEnabled ? "#596f7e" : "#aab6c2"

                Text {
                    anchors.centerIn: parent
                    text: "Короткий тест аудиовыхода (QAudioSink)"
                    color: "white"
                    font.pixelSize: 15
                    font.bold: true
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: audioTest.syntheticOutputTestEnabled
                    onClicked: audioTest.startSyntheticOutputTest()
                }
            }

            Rectangle {
                width: parent.width
                height: 54
                radius: 12
                color: audioTest.playEnabled ? "#1769aa" : "#aab6c2"

                Text {
                    anchors.centerIn: parent
                    text: "Воспроизвести / продолжить"
                    color: "white"
                    font.pixelSize: 16
                    font.bold: true
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: audioTest.playEnabled
                    onClicked: audioTest.play()
                }
            }

            Rectangle {
                width: parent.width
                height: 54
                radius: 12
                color: audioTest.pauseEnabled ? "#1769aa" : "#aab6c2"

                Text {
                    anchors.centerIn: parent
                    text: "Пауза"
                    color: "white"
                    font.pixelSize: 16
                    font.bold: true
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: audioTest.pauseEnabled
                    onClicked: audioTest.pause()
                }
            }

            Rectangle {
                width: parent.width
                height: 54
                radius: 12
                color: audioTest.stopEnabled ? "#a43b3b" : "#aab6c2"

                Text {
                    anchors.centerIn: parent
                    text: "Стоп"
                    color: "white"
                    font.pixelSize: 16
                    font.bold: true
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: audioTest.stopEnabled
                    onClicked: audioTest.stop()
                }
            }

            Rectangle {
                width: parent.width
                height: 54
                radius: 12
                color: audioTest.restartEnabled ? "#1769aa" : "#aab6c2"

                Text {
                    anchors.centerIn: parent
                    text: "С начала / повторить"
                    color: "white"
                    font.pixelSize: 16
                    font.bold: true
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: audioTest.restartEnabled
                    onClicked: audioTest.restart()
                }
            }

            Text {
                width: parent.width
                visible: audioTest.errorText.length > 0
                text: "Ошибка: " + audioTest.errorText
                color: "#a32020"
                font.pixelSize: 14
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
