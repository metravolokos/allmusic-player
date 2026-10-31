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

    function selectCloudEntry(sourceIndex, isDirectory) {
        if (isDirectory) {
            media3.openCloudDirectory(sourceIndex)
            selectedCloudIndex = -1
        } else {
            selectedCloudIndex = sourceIndex
        }
    }

    function clearSelectionIfHidden() {
        if (selectedCloudIndex >= 0
                && !media3.cloudEntriesModel.containsSourceIndex(selectedCloudIndex))
            selectedCloudIndex = -1
    }

    ListView {
        id: page
        anchors.fill: parent
        clip: true
        spacing: 10
        boundsBehavior: Flickable.StopAtBounds
        model: media3.cloudEntriesModel

        header: Column {
            width: page.width
            spacing: 12

            Item { width: parent.width; height: 10 }

            Text {
                x: 20
                width: page.width - 40
                text: "Cloud Mail · MP3 / FLAC"
                color: "#17212b"
                font.pixelSize: 24
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                x: 20
                width: page.width - 40
                text: "Потоковое воспроизведение через Media3. Пароль используется только в памяти приложения."
                color: "#526171"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Rectangle {
                x: 20
                width: page.width - 40
                height: 48
                radius: 8
                color: "white"
                border.color: "#d5dce3"
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
                x: 20
                width: page.width - 40
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
                x: 20
                width: page.width - 40
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
                x: 20
                width: page.width - 40
                text: media3.cloudStatus
                color: "#526171"
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }

            Row {
                x: 20
                width: page.width - 40
                visible: media3.cloudConnected
                spacing: 10
                Rectangle {
                    width: 92
                    height: 40
                    radius: 8
                    color: media3.canGoToCloudParent() ? "#dceaf5" : "#e5e9ed"
                    Text { anchors.centerIn: parent; text: "↑ Назад"; color: "#17212b"; font.pixelSize: 14 }
                    MouseArea {
                        anchors.fill: parent
                        enabled: media3.canGoToCloudParent()
                        onClicked: {
                            media3.goToCloudParent()
                            root.selectedCloudIndex = -1
                        }
                    }
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

            Row {
                x: 20
                width: page.width - 40
                height: 46
                spacing: 8
                visible: media3.cloudConnected
                Rectangle {
                    width: parent.width - 82
                    height: parent.height
                    radius: 8
                    color: "white"
                    border.color: "#d5dce3"
                    Text {
                        anchors.fill: parent
                        anchors.margins: 10
                        verticalAlignment: Text.AlignVCenter
                        text: "Поиск по названию файла"
                        color: "#7f8e9b"
                        font.pixelSize: 14
                        visible: cloudSearchInput.text.length === 0
                    }
                    TextInput {
                        id: cloudSearchInput
                        anchors.fill: parent
                        anchors.margins: 10
                        verticalAlignment: TextInput.AlignVCenter
                        font.pixelSize: 14
                        color: "#17212b"
                        inputMethodHints: Qt.ImhNoPredictiveText
                        enabled: media3.cloudConnected && !media3.cloudBusy
                        onTextChanged: {
                            if (media3.cloudEntriesModel.searchText !== text)
                                media3.cloudEntriesModel.searchText = text
                        }
                        onActiveFocusChanged: {
                            if (activeFocus)
                                page.positionViewAtBeginning()
                        }
                        onAccepted: focus = false
                    }
                }
                Rectangle {
                    width: 74
                    height: parent.height
                    radius: 8
                    color: cloudSearchInput.text.length > 0 ? "#dceaf5" : "#e5e9ed"
                    Text { anchors.centerIn: parent; text: "Очистить"; color: "#17212b"; font.pixelSize: 12 }
                    MouseArea {
                        anchors.fill: parent
                        enabled: cloudSearchInput.text.length > 0 && !media3.cloudBusy
                        onClicked: {
                            cloudSearchInput.clear()
                            cloudSearchInput.forceActiveFocus()
                        }
                    }
                }
            }

            Row {
                x: 20
                width: page.width - 40
                spacing: 8
                visible: media3.cloudConnected
                Text {
                    width: parent.width - 154
                    height: 42
                    text: "Показано " + media3.cloudEntriesModel.resultCount
                          + " из " + media3.cloudEntriesModel.totalCount
                    color: "#526171"
                    font.pixelSize: 13
                    verticalAlignment: Text.AlignVCenter
                }
                Rectangle {
                    width: 146
                    height: 42
                    radius: 8
                    color: "#dceaf5"
                    Text {
                        anchors.centerIn: parent
                        text: media3.cloudEntriesModel.sortMode === 0 ? "Сортировка: А → Я"
                              : media3.cloudEntriesModel.sortMode === 1 ? "Сортировка: Я → А"
                              : "Сортировка: исходная"
                        color: "#17212b"
                        font.pixelSize: 12
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: !media3.cloudBusy
                        onClicked: media3.cloudEntriesModel.sortMode = (media3.cloudEntriesModel.sortMode + 1) % 3
                    }
                }
            }

            Text {
                x: 20
                width: page.width - 40
                visible: media3.cloudConnected && media3.cloudEntriesModel.totalCount > 0
                         && media3.cloudEntriesModel.resultCount === 0
                text: "Совпадений нет"
                color: "#526171"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                x: 20
                width: page.width - 40
                visible: media3.cloudConnected && !media3.cloudBusy
                         && media3.cloudEntriesModel.totalCount === 0
                text: "В этой папке нет поддерживаемых композиций."
                color: "#526171"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Item { width: parent.width; height: 2 }
        }

        delegate: Rectangle {
            required property int sourceIndex
            required property string label
            required property bool isDirectory
            width: Math.max(1, page.width - 40)
            x: 20
            height: 54
            radius: 8
            color: root.selectedCloudIndex === sourceIndex ? "#dceaf5" : "white"
            border.width: root.selectedCloudIndex === sourceIndex ? 2 : 1
            border.color: root.selectedCloudIndex === sourceIndex ? "#1769aa" : "#d5dce3"

            Text {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: label
                color: "#17212b"
                font.pixelSize: 14
                elide: Text.ElideMiddle
            }
            MouseArea {
                anchors.fill: parent
                enabled: media3.cloudConnected && !media3.cloudBusy
                onClicked: root.selectCloudEntry(sourceIndex, isDirectory)
            }
        }

        footer: Column {
            width: page.width
            spacing: 12

            Item { width: parent.width; height: 4 }

            Rectangle {
                x: 20
                width: page.width - 40
                height: 48
                radius: 9
                color: (root.selectedCloudIndex >= 0
                        && media3.canPlayCloud(root.selectedCloudIndex)) ? "#1769aa" : "#aab6c2"
                Text {
                    anchors.centerIn: parent
                    text: "Воспроизвести выбранный MP3 / FLAC"
                    color: "white"
                    font.pixelSize: 14
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: root.selectedCloudIndex >= 0
                             && media3.canPlayCloud(root.selectedCloudIndex)
                    onClicked: media3.playCloud(root.selectedCloudIndex)
                }
            }

            Text {
                x: 20
                width: page.width - 40
                text: "Активный трек: " + media3.activeTrackName + "\nСостояние: " + media3.stateText
                color: "#17212b"
                font.pixelSize: 15
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                x: 20
                width: page.width - 40
                text: media3.outputStatusText
                color: "#526171"
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Row {
                x: 20
                width: page.width - 40
                spacing: 8
                Rectangle {
                    width: (parent.width - parent.spacing * 2) / 3
                    height: 46
                    radius: 8
                    color: media3.playEnabled ? "#1769aa" : "#aab6c2"
                    Text { anchors.centerIn: parent; text: "Play / Resume"; color: "white"; font.pixelSize: 12 }
                    MouseArea { anchors.fill: parent; enabled: media3.playEnabled; onClicked: media3.play() }
                }
                Rectangle {
                    width: (parent.width - parent.spacing * 2) / 3
                    height: 46
                    radius: 8
                    color: media3.pauseEnabled ? "#1769aa" : "#aab6c2"
                    Text { anchors.centerIn: parent; text: "Пауза"; color: "white"; font.pixelSize: 12 }
                    MouseArea { anchors.fill: parent; enabled: media3.pauseEnabled; onClicked: media3.pause() }
                }
                Rectangle {
                    width: (parent.width - parent.spacing * 2) / 3
                    height: 46
                    radius: 8
                    color: media3.stopEnabled ? "#a43b3b" : "#aab6c2"
                    Text { anchors.centerIn: parent; text: "Stop"; color: "white"; font.pixelSize: 12 }
                    MouseArea { anchors.fill: parent; enabled: media3.stopEnabled; onClicked: media3.stop() }
                }
            }

            Row {
                x: 20
                width: page.width - 40
                Text { id: elapsed; text: formatTime(media3.positionMs); color: "#526171"; font.pixelSize: 13 }
                Item { width: parent.width - elapsed.width - remaining.width; height: 1 }
                Text { id: remaining; text: formatTime(media3.durationMs); color: "#526171"; font.pixelSize: 13 }
            }

            Rectangle {
                id: seekBar
                x: 20
                width: page.width - 40
                height: 22
                radius: 11
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
                x: 20
                width: page.width - 40
                visible: media3.errorText.length > 0
                text: "Ошибка: " + media3.errorText
                color: "#a32020"
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }

            Text {
                x: 20
                width: page.width - 40
                text: "Локальные MP3 / FLAC / WAV для регрессии"
                color: "#17212b"
                font.pixelSize: 14
                font.bold: true
            }

            Repeater {
                model: media3.trackNames
                delegate: Rectangle {
                    required property int index
                    required property string modelData
                    x: 20
                    width: page.width - 40
                    height: 44
                    radius: 8
                    color: media3.trackIndex === index ? "#dceaf5" : "white"
                    border.color: "#d5dce3"
                    Text { anchors.centerIn: parent; text: modelData; color: "#17212b"; font.pixelSize: 13 }
                    MouseArea { anchors.fill: parent; onClicked: media3.selectTrack(index) }
                }
            }

            Text {
                x: 20
                width: page.width - 40
                text: "При сворачивании приложения воспроизведение останавливается. Фоновая служба не включена."
                color: "#526171"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            Text {
                x: 20
                width: page.width - 40
                text: buildLabel
                color: "#526171"
                font.pixelSize: 12
                wrapMode: Text.WrapAnywhere
            }

            Item { width: parent.width; height: 10 }
        }
    }

    Connections {
        target: media3
        function onCloudSessionEnded() {
            emailInput.clear()
            passwordInput.clear()
            root.selectedCloudIndex = -1
        }
        function onCloudEntriesReset() {
            root.selectedCloudIndex = -1
        }
    }

    Connections {
        target: media3.cloudEntriesModel
        function onSearchTextChanged() { root.clearSelectionIfHidden() }
        function onSortModeChanged() { root.clearSelectionIfHidden() }
    }
}
