import QtQuick
import QtQuick.Window

Window {
    id: root
    width: 400
    height: 820
    visible: true
    title: "AllMusic Player — Cloud Mail Media3"
    color: "#f5f6f8"
    property int selectedCloudIndex: -1

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
            x: 20
            y: 20
            width: Math.max(1, content.width - 40)
            spacing: 12

            Text {
                width: parent.width
                text: "Cloud Mail · MP3 / FLAC"
                color: "#17212b"
                font.pixelSize: 24
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: "Потоковое воспроизведение через Media3. Пароль используется только в памяти приложения."
                color: "#526171"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Rectangle {
                width: parent.width
                height: 48
                radius: 8
                color: "white"
                border.color: "#d5dce3"
                // QtQuick.TextInput does not have placeholderText (unlike Controls.TextField).
                // Draw a non-interactive hint behind the plain TextInput so the
                // Android APK does not require the Qt Quick Controls module.
                Text {
                    anchors.fill: parent
                    anchors.margins: 12
                    verticalAlignment: Text.AlignVCenter
                    text: "Адрес почты Cloud Mail"
                    color: "#7f8e9b"
                    font.pixelSize: 15
                    visible: emailInput.text.length === 0
                }
                TextInput {
                    id: emailInput
                    anchors.fill: parent
                    anchors.margins: 12
                    verticalAlignment: TextInput.AlignVCenter
                    font.pixelSize: 15
                    color: "#17212b"
                    inputMethodHints: Qt.ImhEmailCharactersOnly
                    enabled: !media3.cloudConnected && !media3.cloudBusy
                }
            }

            Rectangle {
                width: parent.width
                height: 48
                radius: 8
                color: "white"
                border.color: "#d5dce3"
                Text {
                    anchors.fill: parent
                    anchors.margins: 12
                    verticalAlignment: Text.AlignVCenter
                    text: "Пароль внешнего приложения"
                    color: "#7f8e9b"
                    font.pixelSize: 15
                    visible: passwordInput.text.length === 0
                }
                TextInput {
                    id: passwordInput
                    anchors.fill: parent
                    anchors.margins: 12
                    verticalAlignment: TextInput.AlignVCenter
                    font.pixelSize: 15
                    color: "#17212b"
                    echoMode: TextInput.Password
                    inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                    enabled: !media3.cloudConnected && !media3.cloudBusy
                }
            }

            Row {
                width: parent.width
                spacing: 8
                Rectangle {
                    width: (parent.width - parent.spacing) / 2
                    height: 48
                    radius: 9
                    color: (!media3.cloudConnected && !media3.cloudBusy) ? "#1769aa" : "#aab6c2"
                    Text { anchors.centerIn: parent; text: "Подключить"; color: "white"; font.pixelSize: 15 }
                    MouseArea {
                        anchors.fill: parent
                        enabled: !media3.cloudConnected && !media3.cloudBusy
                        onClicked: {
                            media3.connectCloud(emailInput.text, passwordInput.text)
                            passwordInput.clear()
                            root.selectedCloudIndex = -1
                        }
                    }
                }
                Rectangle {
                    width: (parent.width - parent.spacing) / 2
                    height: 48
                    radius: 9
                    color: media3.cloudConnected || media3.cloudBusy ? "#a43b3b" : "#aab6c2"
                    Text { anchors.centerIn: parent; text: "Отключиться"; color: "white"; font.pixelSize: 15 }
                    MouseArea {
                        anchors.fill: parent
                        enabled: media3.cloudConnected || media3.cloudBusy
                        onClicked: {
                            media3.disconnectCloud()
                            root.selectedCloudIndex = -1
                        }
                    }
                }
            }

            Text {
                width: parent.width
                text: media3.cloudStatus
                color: "#526171"
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }

            Row {
                width: parent.width
                visible: media3.cloudConnected
                spacing: 10
                Rectangle {
                    width: 92
                    height: 40
                    radius: 8
                    color: media3.canGoToCloudParent() ? "#dceaf5" : "#e5e9ed"
                    Text { anchors.centerIn: parent; text: "↑ Назад"; color: "#17212b"; font.pixelSize: 14 }
                    MouseArea { anchors.fill: parent; enabled: media3.canGoToCloudParent(); onClicked: media3.goToCloudParent() }
                }
                Text {
                    width: parent.width - 102
                    height: 40
                    text: media3.cloudDirectoryLabel
                    color: "#17212b"
                    font.pixelSize: 14
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideMiddle
                }
            }

            Repeater {
                model: media3.cloudEntries
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    width: contentColumn.width
                    height: 54
                    radius: 8
                    color: root.selectedCloudIndex === index ? "#dceaf5" : "white"
                    border.width: root.selectedCloudIndex === index ? 2 : 1
                    border.color: root.selectedCloudIndex === index ? "#1769aa" : "#d5dce3"

                    Text {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label
                        color: "#17212b"
                        font.pixelSize: 14
                        elide: Text.ElideMiddle
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: media3.cloudConnected && !media3.cloudBusy
                        onClicked: {
                            if (modelData.isDirectory) {
                                media3.openCloudDirectory(index)
                                root.selectedCloudIndex = -1
                            } else {
                                root.selectedCloudIndex = index
                            }
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 48
                radius: 9
                color: (root.selectedCloudIndex >= 0 && media3.canPlayCloud(root.selectedCloudIndex))
                       ? "#1769aa" : "#aab6c2"
                Text { anchors.centerIn: parent; text: "Воспроизвести выбранный MP3 / FLAC"; color: "white"; font.pixelSize: 14 }
                MouseArea {
                    anchors.fill: parent
                    enabled: root.selectedCloudIndex >= 0 && media3.canPlayCloud(root.selectedCloudIndex)
                    onClicked: media3.playCloud(root.selectedCloudIndex)
                }
            }

            Text {
                width: parent.width
                text: "Активный трек: " + media3.activeTrackName + "\nСостояние: " + media3.stateText
                color: "#17212b"
                font.pixelSize: 15
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: media3.outputStatusText
                color: "#526171"
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Row {
                width: parent.width
                spacing: 8
                Rectangle {
                    width: (parent.width - parent.spacing * 2) / 3; height: 46; radius: 8
                    color: media3.playEnabled ? "#1769aa" : "#aab6c2"
                    Text { anchors.centerIn: parent; text: "Play / Resume"; color: "white"; font.pixelSize: 12 }
                    MouseArea { anchors.fill: parent; enabled: media3.playEnabled; onClicked: media3.play() }
                }
                Rectangle {
                    width: (parent.width - parent.spacing * 2) / 3; height: 46; radius: 8
                    color: media3.pauseEnabled ? "#1769aa" : "#aab6c2"
                    Text { anchors.centerIn: parent; text: "Пауза"; color: "white"; font.pixelSize: 12 }
                    MouseArea { anchors.fill: parent; enabled: media3.pauseEnabled; onClicked: media3.pause() }
                }
                Rectangle {
                    width: (parent.width - parent.spacing * 2) / 3; height: 46; radius: 8
                    color: media3.stopEnabled ? "#a43b3b" : "#aab6c2"
                    Text { anchors.centerIn: parent; text: "Stop"; color: "white"; font.pixelSize: 12 }
                    MouseArea { anchors.fill: parent; enabled: media3.stopEnabled; onClicked: media3.stop() }
                }
            }

            Row {
                width: parent.width
                Text { id: elapsed; text: formatTime(media3.positionMs); color: "#526171"; font.pixelSize: 13 }
                Item { width: parent.width - elapsed.width - remaining.width; height: 1 }
                Text { id: remaining; text: formatTime(media3.durationMs); color: "#526171"; font.pixelSize: 13 }
            }

            Rectangle {
                id: seekBar
                width: parent.width
                height: 22
                radius: 11
                color: "#d5dce3"
                property real fraction: media3.durationMs > 0
                    ? Math.max(0, Math.min(1, media3.positionMs / media3.durationMs)) : 0
                Rectangle { width: seekBar.width * seekBar.fraction; height: parent.height; radius: parent.radius; color: media3.seekEnabled ? "#4e91c3" : "#aab6c2" }
                MouseArea {
                    anchors.fill: parent
                    enabled: media3.seekEnabled
                    onClicked: function(mouse) { media3.seekTo(Math.round(mouse.x / width * media3.durationMs)) }
                }
            }

            Text {
                width: parent.width
                visible: media3.errorText.length > 0
                text: "Ошибка: " + media3.errorText
                color: "#a32020"
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                text: "Локальные MP3 / FLAC / WAV для регрессии"
                color: "#526171"
                font.pixelSize: 14
                font.bold: true
            }

            Repeater {
                model: media3.trackNames
                delegate: Rectangle {
                    required property int index
                    required property string modelData
                    width: contentColumn.width
                    height: 44
                    radius: 8
                    color: media3.trackIndex === index ? "#dceaf5" : "white"
                    border.color: "#d5dce3"
                    Text { anchors.centerIn: parent; text: modelData; color: "#17212b"; font.pixelSize: 13 }
                    MouseArea { anchors.fill: parent; onClicked: media3.selectTrack(index) }
                }
            }

            Text {
                width: parent.width
                text: "При сворачивании приложения воспроизведение останавливается. Фоновая служба не включена."
                color: "#526171"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            Text {
                width: parent.width
                text: buildLabel
                color: "#526171"
                font.pixelSize: 12
                wrapMode: Text.WrapAnywhere
            }
        }
    }

    Connections {
        target: media3
        function onCloudSessionEnded() {
            emailInput.clear()
            passwordInput.clear()
            root.selectedCloudIndex = -1
        }
    }

}
