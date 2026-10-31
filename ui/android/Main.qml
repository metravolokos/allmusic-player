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
    property string selectedCloudLabel: ""
    property bool showSyntheticTracks: false

    function formatTime(milliseconds) {
        var seconds = Math.max(0, Math.floor(milliseconds / 1000))
        return Math.floor(seconds / 60) + ":" + ("0" + (seconds % 60)).slice(-2)
    }

    function clearCloudSelection() {
        selectedCloudIndex = -1
        selectedCloudLabel = ""
    }

    function selectCloudEntry(sourceIndex, label, isDirectory) {
        if (isDirectory) {
            clearCloudSelection()
            media3.openCloudDirectory(sourceIndex)
        } else {
            selectedCloudIndex = sourceIndex
            selectedCloudLabel = label
        }
    }

    function clearSelectionIfHidden() {
        if (selectedCloudIndex >= 0
                && !media3.cloudEntriesModel.containsSourceIndex(selectedCloudIndex))
            clearCloudSelection()
    }

    // Only this ListView scrolls; search and playback controls stay in place.
    Column {
        id: topPanel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: implicitHeight
        spacing: 6
        z: 2

        Text {
            x: 12
            width: parent.width - 24
            height: 26
            text: "Cloud Mail · MP3 / FLAC"
            color: "#17212b"
            font.pixelSize: 20
            font.bold: true
            verticalAlignment: Text.AlignVCenter
        }

        Column {
            id: connectForm
            x: 12
            width: parent.width - 24
            spacing: 6
            visible: !media3.cloudConnected

            Rectangle {
                width: parent.width
                height: 42
                radius: 8
                color: "white"
                border.color: "#d5dce3"
                Text {
                    anchors.fill: parent
                    anchors.margins: 10
                    verticalAlignment: Text.AlignVCenter
                    text: "Адрес почты Cloud Mail"
                    color: "#7f8e9b"
                    font.pixelSize: 14
                    visible: emailInput.text.length === 0
                }
                TextInput {
                    id: emailInput
                    anchors.fill: parent
                    anchors.margins: 10
                    verticalAlignment: TextInput.AlignVCenter
                    font.pixelSize: 14
                    color: "#17212b"
                    inputMethodHints: Qt.ImhEmailCharactersOnly
                    enabled: !media3.cloudBusy
                }
            }

            Rectangle {
                width: parent.width
                height: 42
                radius: 8
                color: "white"
                border.color: "#d5dce3"
                Text {
                    anchors.fill: parent
                    anchors.margins: 10
                    verticalAlignment: Text.AlignVCenter
                    text: "Пароль внешнего приложения"
                    color: "#7f8e9b"
                    font.pixelSize: 14
                    visible: passwordInput.text.length === 0
                }
                TextInput {
                    id: passwordInput
                    anchors.fill: parent
                    anchors.margins: 10
                    verticalAlignment: TextInput.AlignVCenter
                    font.pixelSize: 14
                    color: "#17212b"
                    echoMode: TextInput.Password
                    inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                    enabled: !media3.cloudBusy
                }
            }

            Rectangle {
                width: parent.width
                height: 40
                radius: 8
                color: media3.cloudBusy ? "#a43b3b" : "#1769aa"
                Text {
                    anchors.centerIn: parent
                    text: media3.cloudBusy ? "Отменить подключение" : "Подключиться"
                    color: "white"
                    font.pixelSize: 14
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (media3.cloudBusy) {
                            media3.disconnectCloud()
                            root.clearCloudSelection()
                        } else {
                            media3.connectCloud(emailInput.text, passwordInput.text)
                            passwordInput.clear()
                            root.clearCloudSelection()
                        }
                    }
                }
            }
        }

        Row {
            id: connectedBar
            x: 12
            width: parent.width - 24
            height: 38
            spacing: 8
            visible: media3.cloudConnected

            Text {
                width: parent.width - 94
                height: parent.height
                text: media3.cloudStatus
                color: "#526171"
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            Rectangle {
                width: 86
                height: parent.height
                radius: 8
                color: "#a43b3b"
                Text { anchors.centerIn: parent; text: "Отключиться"; color: "white"; font.pixelSize: 11 }
                MouseArea {
                    anchors.fill: parent
                    enabled: media3.cloudConnected || media3.cloudBusy
                    onClicked: {
                        media3.disconnectCloud()
                        root.clearCloudSelection()
                    }
                }
            }
        }

        Text {
            x: 12
            width: parent.width - 24
            visible: !media3.cloudConnected && media3.cloudStatus.length > 0
            text: media3.cloudStatus
            color: "#526171"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        Row {
            id: directoryBar
            x: 12
            width: parent.width - 24
            height: 34
            spacing: 8
            visible: media3.cloudConnected

            Rectangle {
                width: 76
                height: parent.height
                radius: 8
                color: media3.canGoToCloudParent() ? "#dceaf5" : "#e5e9ed"
                Text { anchors.centerIn: parent; text: "↑ Назад"; color: "#17212b"; font.pixelSize: 12 }
                MouseArea {
                    anchors.fill: parent
                    enabled: media3.canGoToCloudParent()
                    onClicked: {
                        media3.goToCloudParent()
                        root.clearCloudSelection()
                    }
                }
            }
            Text {
                width: parent.width - 84
                height: parent.height
                text: media3.cloudDirectoryLabel
                color: "#17212b"
                font.pixelSize: 13
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideMiddle
            }
        }

        Row {
            id: searchPanel
            x: 12
            width: parent.width - 24
            height: 42
            spacing: 8

            Rectangle {
                width: parent.width - 80
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
                    font.pixelSize: 13
                    visible: cloudSearchInput.text.length === 0
                }
                TextInput {
                    id: cloudSearchInput
                    anchors.fill: parent
                    anchors.margins: 10
                    verticalAlignment: TextInput.AlignVCenter
                    font.pixelSize: 13
                    color: "#17212b"
                    inputMethodHints: Qt.ImhNoPredictiveText
                    enabled: media3.cloudConnected && !media3.cloudBusy
                    onTextChanged: {
                        if (media3.cloudEntriesModel.searchText !== text)
                            media3.cloudEntriesModel.searchText = text
                    }
                    onAccepted: focus = false
                }
            }

            Rectangle {
                width: 72
                height: parent.height
                radius: 8
                color: cloudSearchInput.text.length > 0 ? "#dceaf5" : "#e5e9ed"
                Text { anchors.centerIn: parent; text: "Очистить"; color: "#17212b"; font.pixelSize: 11 }
                MouseArea {
                    anchors.fill: parent
                    enabled: cloudSearchInput.text.length > 0 && !media3.cloudBusy
                    onClicked: cloudSearchInput.clear()
                }
            }
        }

        Row {
            id: sortAndCountPanel
            x: 12
            width: parent.width - 24
            height: 34
            spacing: 8

            Text {
                width: parent.width - 146
                height: parent.height
                text: "Показано " + media3.cloudEntriesModel.resultCount
                      + " из " + media3.cloudEntriesModel.totalCount
                color: "#526171"
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
            }
            Rectangle {
                width: 138
                height: parent.height
                radius: 8
                color: media3.cloudConnected ? "#dceaf5" : "#e5e9ed"
                Text {
                    anchors.centerIn: parent
                    text: media3.cloudEntriesModel.sortMode === 0 ? "Сортировка: А → Я"
                          : media3.cloudEntriesModel.sortMode === 1 ? "Сортировка: Я → А"
                          : "Сортировка: исходная"
                    color: "#17212b"
                    font.pixelSize: 11
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: media3.cloudConnected && !media3.cloudBusy
                    onClicked: media3.cloudEntriesModel.sortMode = (media3.cloudEntriesModel.sortMode + 1) % 3
                }
            }
        }

        Text {
            x: 12
            width: parent.width - 24
            visible: media3.cloudConnected && media3.cloudEntriesModel.totalCount > 0
                     && media3.cloudEntriesModel.resultCount === 0
            text: "Совпадений нет"
            color: "#526171"
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            x: 12
            width: parent.width - 24
            visible: media3.cloudConnected && !media3.cloudBusy
                     && media3.cloudEntriesModel.totalCount === 0
            text: "В этой папке нет поддерживаемых композиций."
            color: "#526171"
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }

    ListView {
        id: cloudList
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: topPanel.bottom
        anchors.bottom: playerPanel.top
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 4
        anchors.bottomMargin: 4
        clip: true
        spacing: 6
        boundsBehavior: Flickable.StopAtBounds
        model: media3.cloudEntriesModel

        delegate: Rectangle {
            required property int sourceIndex
            required property string label
            required property bool isDirectory
            width: cloudList.width
            height: 50
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
                onClicked: root.selectCloudEntry(sourceIndex, label, isDirectory)
            }
        }
    }

    Rectangle {
        id: playerPanel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: playerControls.implicitHeight + 16
        color: "white"
        border.color: "#d5dce3"
        z: 3

        Column {
            id: playerControls
            anchors.fill: parent
            anchors.margins: 8
            spacing: 5

            Text {
                width: parent.width
                height: 18
                text: root.selectedCloudIndex >= 0
                      ? "Выбрано: " + root.selectedCloudLabel
                      : "Активный трек: " + media3.activeTrackName + " · " + media3.stateText
                color: "#17212b"
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideMiddle
            }

            Rectangle {
                id: playSelectedButton
                width: parent.width
                height: 38
                radius: 8
                color: (root.selectedCloudIndex >= 0
                        && media3.canPlayCloud(root.selectedCloudIndex)) ? "#1769aa" : "#aab6c2"
                Text {
                    anchors.centerIn: parent
                    text: "Воспроизвести выбранный MP3 / FLAC"
                    color: "white"
                    font.pixelSize: 13
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: root.selectedCloudIndex >= 0
                             && media3.canPlayCloud(root.selectedCloudIndex)
                    onClicked: media3.playCloud(root.selectedCloudIndex)
                }
            }

            Row {
                width: parent.width
                height: 34
                spacing: 8
                Rectangle {
                    width: (parent.width - parent.spacing * 2) / 3
                    height: parent.height
                    radius: 8
                    color: media3.playEnabled ? "#1769aa" : "#aab6c2"
                    Text { anchors.centerIn: parent; text: "Play / Resume"; color: "white"; font.pixelSize: 11 }
                    MouseArea { anchors.fill: parent; enabled: media3.playEnabled; onClicked: media3.play() }
                }
                Rectangle {
                    width: (parent.width - parent.spacing * 2) / 3
                    height: parent.height
                    radius: 8
                    color: media3.pauseEnabled ? "#1769aa" : "#aab6c2"
                    Text { anchors.centerIn: parent; text: "Пауза"; color: "white"; font.pixelSize: 11 }
                    MouseArea { anchors.fill: parent; enabled: media3.pauseEnabled; onClicked: media3.pause() }
                }
                Rectangle {
                    width: (parent.width - parent.spacing * 2) / 3
                    height: parent.height
                    radius: 8
                    color: media3.stopEnabled ? "#a43b3b" : "#aab6c2"
                    Text { anchors.centerIn: parent; text: "Stop"; color: "white"; font.pixelSize: 11 }
                    MouseArea { anchors.fill: parent; enabled: media3.stopEnabled; onClicked: media3.stop() }
                }
            }

            Row {
                width: parent.width
                height: 14
                Text { id: elapsed; text: formatTime(media3.positionMs); color: "#526171"; font.pixelSize: 11 }
                Item { width: parent.width - elapsed.width - remaining.width; height: 1 }
                Text { id: remaining; text: formatTime(media3.durationMs); color: "#526171"; font.pixelSize: 11 }
            }

            Rectangle {
                id: seekBar
                width: parent.width
                height: 14
                radius: 7
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
                height: 14
                text: media3.outputStatusText
                color: "#526171"
                font.pixelSize: 10
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                visible: media3.errorText.length > 0
                text: "Ошибка: " + media3.errorText
                color: "#a32020"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            Rectangle {
                width: parent.width
                height: 24
                radius: 6
                color: "#eef1f4"
                Text {
                    anchors.centerIn: parent
                    text: root.showSyntheticTracks
                          ? "Скрыть локальные тестовые треки ▲"
                          : "Локальные тестовые треки ▼"
                    color: "#526171"
                    font.pixelSize: 10
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.showSyntheticTracks = !root.showSyntheticTracks
                }
            }

            Column {
                width: parent.width
                spacing: 4
                visible: root.showSyntheticTracks
                Repeater {
                    model: media3.trackNames
                    delegate: Rectangle {
                        required property int index
                        required property string modelData
                        width: playerControls.width
                        height: 32
                        radius: 6
                        color: media3.trackIndex === index ? "#dceaf5" : "#f5f6f8"
                        border.color: "#d5dce3"
                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            text: modelData
                            color: "#17212b"
                            font.pixelSize: 11
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideMiddle
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: media3.selectTrack(index)
                        }
                    }
                }
            }

            Text {
                width: parent.width
                height: 12
                text: buildLabel
                color: "#7f8e9b"
                font.pixelSize: 9
                elide: Text.ElideMiddle
            }
        }
    }

    Connections {
        target: media3
        function onCloudSessionEnded() {
            emailInput.clear()
            passwordInput.clear()
            root.clearCloudSelection()
        }
        function onCloudEntriesReset() { root.clearCloudSelection() }
    }

    Connections {
        target: media3.cloudEntriesModel
        function onSearchTextChanged() { root.clearSelectionIfHidden() }
        function onSortModeChanged() { root.clearSelectionIfHidden() }
    }
}
