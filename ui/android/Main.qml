import QtQuick
import QtQuick.Window

Window {
    width: 400
    height: 720
    visible: true
    title: "AllMusic Player — тест 002-A"
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
                text: "Проверка локального звука"
                color: "#17212b"
                font.pixelSize: 25
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: "Короткий синтетический сигнал 440 Гц. Интернет и доступ к файлам не используются."
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
                text: audioTest.outputStatusText
                color: "#526171"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WrapAnywhere
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
