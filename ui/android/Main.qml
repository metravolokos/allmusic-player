import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Fusion
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 400
    height: 820
    visible: true
    title: "AllMusic Player · Cloud Mail"

    property int selectedCloudIndex: -1
    property string selectedCloudLabel: ""
    property string selectedFavoriteId: ""
    property bool showFavorites: false
    property bool showSyntheticTracks: false

    function formatTime(milliseconds) {
        const seconds = Math.max(0, Math.floor(milliseconds / 1000))
        return Math.floor(seconds / 60) + ":" + ("0" + (seconds % 60)).slice(-2)
    }

    function clearCloudSelection() {
        selectedCloudIndex = -1
        selectedCloudLabel = ""
        selectedFavoriteId = ""
    }

    function selectCloudEntry(sourceIndex, label, isDirectory) {
        if (isDirectory) {
            clearCloudSelection()
            media3.openCloudDirectory(sourceIndex)
        } else {
            selectedCloudIndex = sourceIndex
            selectedCloudLabel = label
            selectedFavoriteId = ""
        }
    }

    function clearSelectionIfHidden() {
        if (showFavorites) {
            if (selectedFavoriteId.length > 0
                    && !media3.favoritesModel.contains(selectedFavoriteId))
                clearCloudSelection()
            return
        }
        if (selectedCloudIndex >= 0
                && !media3.cloudEntriesModel.containsSourceIndex(selectedCloudIndex))
            clearCloudSelection()
    }

    ColumnLayout {
        id: topPanel
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: "AllMusic Player"
                font.pixelSize: 20
                font.bold: true
            }
            ComboBox {
                id: themeSelector
                Layout.preferredWidth: 142
                model: ["Системная", "Светлая", "Тёмная"]
                currentIndex: appearance.themeMode === "light" ? 1
                    : appearance.themeMode === "dark" ? 2 : 0
                Accessible.name: "Цветовая тема"
                onActivated: appearance.setThemeMode(["system", "light", "dark"][index])
            }
        }

        GroupBox {
            id: connectForm
            Layout.fillWidth: true
            visible: !media3.cloudConnected
            title: "Подключение Cloud Mail"
            ColumnLayout {
                anchors.fill: parent
                Label {
                    Layout.fillWidth: true
                    text: "Используйте пароль внешнего приложения Mail. Данные не сохраняются; пароль очищается после отправки."
                    wrapMode: Text.WordWrap
                    opacity: 0.78
                }
                TextField {
                    id: emailInput
                    Layout.fillWidth: true
                    implicitHeight: 48
                    placeholderText: "Адрес почты Mail"
                    maximumLength: 320
                    inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                    enabled: !media3.cloudBusy
                    Accessible.name: "Адрес почты Mail"
                }
                TextField {
                    id: passwordInput
                    Layout.fillWidth: true
                    implicitHeight: 48
                    placeholderText: "Пароль внешнего приложения"
                    maximumLength: 256
                    echoMode: TextInput.Password
                    passwordMaskDelay: 0
                    inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                    enabled: !media3.cloudBusy
                    Accessible.name: "Пароль внешнего приложения Mail"
                }
                Button {
                    Layout.fillWidth: true
                    implicitHeight: 48
                    text: media3.cloudBusy ? "Отменить подключение" : "Подключиться"
                    enabled: media3.cloudBusy || (emailInput.text.length > 0
                             && passwordInput.text.length > 0)
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
                Label {
                    Layout.fillWidth: true
                    visible: !media3.cloudConnected && media3.cloudStatus.length > 0
                    text: media3.cloudStatus
                    wrapMode: Text.WordWrap
                    Accessible.role: Accessible.AlertMessage
                }
            }
        }

        RowLayout {
            id: connectedBar
            Layout.fillWidth: true
            visible: media3.cloudConnected
            Label {
                Layout.fillWidth: true
                text: media3.cloudStatus
                elide: Text.ElideRight
            }
            Button {
                implicitHeight: 46
                text: "Отключиться"
                enabled: media3.cloudConnected || media3.cloudBusy
                onClicked: { media3.disconnectCloud(); root.clearCloudSelection() }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: media3.cloudConnected
            Button {
                implicitHeight: 46
                text: "↑ Назад"
                enabled: media3.canGoToCloudParent() && !media3.cloudBusy
                onClicked: { media3.goToCloudParent(); root.clearCloudSelection() }
            }
            Label {
                Layout.fillWidth: true
                text: media3.cloudDirectoryLabel
                elide: Text.ElideMiddle
                Accessible.name: "Текущая папка"
            }
        }

        RowLayout {
            id: searchPanel
            Layout.fillWidth: true
            visible: media3.cloudConnected
            TextField {
                id: cloudSearchInput
                Layout.fillWidth: true
                implicitHeight: 46
                placeholderText: "Поиск по названию"
                enabled: !media3.cloudBusy && !root.showFavorites
                inputMethodHints: Qt.ImhNoPredictiveText
                onTextChanged: {
                    if (media3.cloudEntriesModel.searchText !== text)
                        media3.cloudEntriesModel.searchText = text
                }
                onAccepted: focus = false
                Accessible.name: "Поиск по названию файла"
            }
            Button {
                implicitHeight: 46
                text: root.showFavorites ? "Все композиции" : "Избранное"
                onClicked: {
                    root.showFavorites = !root.showFavorites
                    root.clearCloudSelection()
                }
            }
        }

        RowLayout {
            id: sortAndCountPanel
            Layout.fillWidth: true
            visible: media3.cloudConnected
            Label {
                Layout.fillWidth: true
                text: root.showFavorites
                      ? "Избранное: " + media3.favoritesModel.count
                      : "Показано " + media3.cloudEntriesModel.resultCount
                        + " из " + media3.cloudEntriesModel.totalCount
            }
            ComboBox {
                id: sortSelector
                Layout.preferredWidth: 158
                enabled: !media3.cloudBusy && !root.showFavorites
                model: ["А → Я", "Я → А", "Исходный порядок"]
                currentIndex: media3.cloudEntriesModel.sortMode
                Accessible.name: "Сортировка списка"
                onActivated: media3.cloudEntriesModel.sortMode = index
            }
        }

        ListView {
            id: cloudList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 100
            clip: true
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            visible: media3.cloudConnected
            model: root.showFavorites ? media3.favoritesModel : media3.cloudEntriesModel
            Accessible.name: root.showFavorites ? "Избранные композиции" : "Папки и композиции"

            delegate: RowLayout {
                required property int sourceIndex
                required property string label
                required property bool isDirectory
                required property bool isAudio
                required property bool isFavorite
                required property bool isAvailable
                required property bool isMissing
                required property string stableId
                width: cloudList.width
                spacing: 4

                Button {
                    Layout.fillWidth: true
                    implicitHeight: 54
                    highlighted: root.showFavorites
                                 ? root.selectedFavoriteId === stableId
                                 : root.selectedCloudIndex === sourceIndex
                    text: (isDirectory ? "▸  " : "")
                          + (root.showFavorites && isMissing ? label + " · недоступен"
                             : root.showFavorites && !isAvailable
                               ? label + " · доступность не проверена" : label)
                    onClicked: {
                        if (root.showFavorites) {
                            root.selectedFavoriteId = stableId
                            root.selectedCloudIndex = -1
                            root.selectedCloudLabel = label
                        } else {
                            root.selectCloudEntry(sourceIndex, label, isDirectory)
                        }
                    }
                    Accessible.name: text
                }
                ToolButton {
                    implicitWidth: 52
                    implicitHeight: 52
                    visible: isAudio
                    enabled: root.showFavorites || (media3.cloudConnected && !media3.cloudBusy)
                    text: isFavorite ? "★" : "☆"
                    Accessible.name: isFavorite ? "Убрать из избранного" : "Добавить в избранное"
                    onClicked: {
                        if (root.showFavorites) {
                            if (media3.removeFavorite(stableId)
                                    && root.selectedFavoriteId === stableId)
                                root.clearCloudSelection()
                        } else {
                            media3.toggleFavorite(stableId, label)
                        }
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: media3.cloudConnected && !root.showFavorites
                     && media3.cloudEntriesModel.totalCount > 0
                     && media3.cloudEntriesModel.resultCount === 0
            text: "Совпадений нет"
            horizontalAlignment: Text.AlignHCenter
            opacity: 0.75
        }

        Label {
            Layout.fillWidth: true
            visible: root.showFavorites && media3.favoritesModel.count === 0
            text: "В избранном пока нет композиций."
            horizontalAlignment: Text.AlignHCenter
            opacity: 0.75
        }

        GroupBox {
            id: playerPanel
            Layout.fillWidth: true
            title: "Плеер"

            ColumnLayout {
                anchors.fill: parent
                spacing: 4
                Label {
                    Layout.fillWidth: true
                    text: (root.selectedCloudLabel.length > 0
                          ? "Выбрано: " + root.selectedCloudLabel + " · " : "")
                          + media3.activeTrackName + " · " + media3.stateText
                    wrapMode: Text.WordWrap
                    Accessible.name: "Текущая композиция и состояние"
                }
                Button {
                    Layout.fillWidth: true
                    implicitHeight: 48
                    text: root.showFavorites ? "Воспроизвести выбранное избранное"
                          : "Воспроизвести выбранную композицию"
                    enabled: root.showFavorites
                             ? root.selectedFavoriteId.length > 0
                               && media3.canPlayFavorite(root.selectedFavoriteId)
                             : root.selectedCloudIndex >= 0
                               && media3.canPlayCloud(root.selectedCloudIndex)
                    onClicked: {
                        if (root.showFavorites)
                            media3.playFavorite(root.selectedFavoriteId)
                        else
                            media3.playCloud(root.selectedCloudIndex)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        Layout.fillWidth: true
                        implicitHeight: 48
                        text: "Play / Resume"
                        enabled: media3.playEnabled
                        onClicked: media3.play()
                    }
                    Button {
                        Layout.fillWidth: true
                        implicitHeight: 48
                        text: "Пауза"
                        enabled: media3.pauseEnabled
                        onClicked: media3.pause()
                    }
                    Button {
                        Layout.fillWidth: true
                        implicitHeight: 48
                        text: "Stop"
                        enabled: media3.stopEnabled
                        onClicked: media3.stop()
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: root.formatTime(media3.positionMs) }
                    Slider {
                        id: seekBar
                        Layout.fillWidth: true
                        from: 0
                        to: Math.max(1, media3.durationMs)
                        value: media3.positionMs
                        enabled: media3.seekEnabled && media3.durationMs > 0
                        Accessible.name: "Позиция воспроизведения"
                        onMoved: media3.seekTo(Math.round(value))
                    }
                    Label { text: root.formatTime(media3.durationMs) }
                }
                Label {
                    Layout.fillWidth: true
                    text: media3.errorText.length > 0
                          ? "Ошибка: " + media3.errorText : media3.outputStatusText
                    color: media3.errorText.length > 0 ? "#b3261e" : palette.windowText
                    wrapMode: Text.WordWrap
                    Accessible.role: Accessible.AlertMessage
                }
                Button {
                    Layout.fillWidth: true
                    text: root.showSyntheticTracks
                          ? "Скрыть локальные тестовые треки" : "Локальные тестовые треки"
                    onClicked: root.showSyntheticTracks = !root.showSyntheticTracks
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: root.showSyntheticTracks
                    Repeater {
                        model: media3.trackNames
                        delegate: Button {
                            required property int index
                            required property string modelData
                            Layout.fillWidth: true
                            implicitHeight: 48
                            text: modelData
                            highlighted: media3.trackIndex === index
                            onClicked: media3.selectTrack(index)
                        }
                    }
                }
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
