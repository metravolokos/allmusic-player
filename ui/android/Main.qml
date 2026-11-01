import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Fusion
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 400
    height: 820
    visible: true
    title: "AllMusic Player"
    color: palette.window

    property string screen: "tracks"
    property string selectedArtist: ""
    property string selectedAlbumKey: ""
    property string selectedAlbumName: ""
    property bool albumFromArtist: false
    property int selectedFolderIndex: -1
    property string selectedFolderTrackId: ""

    function pageIndex() {
        switch (screen) {
        case "artists": return 1
        case "artistTracks": return 2
        case "artistAlbums": return 3
        case "albums": return 4
        case "albumTracks": return 5
        case "folders": return 6
        case "now": return 7
        case "settings": return 8
        default: return 0
        }
    }

    function setScreen(name) {
        screen = name
        if (name === "tracks" || name === "artists" || name === "albums") {
            library.filterArtist = ""
            library.filterAlbum = ""
            library.filterFolder = ""
            library.searchText = ""
            library.mode = name === "artists" ? 1 : name === "albums" ? 2 : 0
        }
        if (name === "artistAlbums") {
            library.mode = 2
            library.filterArtist = selectedArtist
            library.filterAlbum = ""
            library.filterFolder = ""
            library.searchText = ""
        }
        if (name === "artistTracks") {
            library.mode = 0
            library.filterArtist = selectedArtist
            library.filterAlbum = ""
            library.filterFolder = ""
            library.searchText = ""
        }
        if (name === "folders") {
            selectedFolderIndex = -1
            selectedFolderTrackId = ""
        }
    }

    function openArtist(name) {
        selectedArtist = name
        library.mode = 0
        library.filterArtist = name
        library.filterAlbum = ""
        library.filterFolder = ""
        library.searchText = ""
        screen = "artistTracks"
    }

    function openArtistAlbums() {
        root.setScreen("artistAlbums")
    }

    function openAlbum(key, name) {
        albumFromArtist = screen === "artistAlbums"
        selectedAlbumKey = key
        selectedAlbumName = name
        library.mode = 0
        library.filterArtist = ""
        library.filterAlbum = key
        library.filterFolder = ""
        library.searchText = ""
        screen = "albumTracks"
    }

    function playTrack(stableId, scope) {
        if (scope === "artist")
            queue.startArtist(selectedArtist, stableId)
        else if (scope === "album")
            queue.startAlbum(selectedAlbumKey, stableId)
        else
            queue.startAll(stableId)
    }

    function toggleQueuePlayback() {
        if (media3.stateText === "Пауза")
            media3.play()
        else if (queue.active && media3.pauseEnabled)
            media3.pause()
        else if (queue.active && media3.playEnabled)
            media3.play()
        else if (!queue.active)
            queue.play()
    }

    function queuePlaybackLabel() {
        if (media3.pauseEnabled)
            return "Пауза"
        return media3.stateText === "Пауза" ? "Продолжить" : "Воспроизвести"
    }

    function repeatLabel() {
        return queue.repeatMode === 1 ? "Повтор: всё" : queue.repeatMode === 2 ? "Повтор: трек" : "Повтор: выкл."
    }

    function formatTime(milliseconds) {
        const seconds = Math.max(0, Math.floor(milliseconds / 1000))
        return Math.floor(seconds / 60) + ":" + ("0" + (seconds % 60)).slice(-2)
    }

    Component.onCompleted: {
        if (typeof smokePage === "string" && smokePage.length > 0)
            root.setScreen(smokePage)
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            Label {
                Layout.fillWidth: true
                text: root.screen === "artistTracks" ? root.selectedArtist
                    : root.screen === "artistAlbums" ? "Альбомы · " + root.selectedArtist
                    : root.screen === "albumTracks" ? root.selectedAlbumName
                    : root.screen === "now" ? "Сейчас играет"
                    : root.screen === "settings" ? "Настройки"
                    : root.screen === "folders" ? "Папки"
                    : root.screen === "artists" ? "Исполнители"
                    : root.screen === "albums" ? "Альбомы" : "Композиции"
                font.bold: true
                font.pixelSize: 20
                elide: Text.ElideRight
            }
            Label {
                text: media3.cloudConnected ? "●" : "○"
                color: media3.cloudConnected ? "#2e9b55" : palette.mid
                Accessible.name: media3.cloudConnected ? "Cloud Mail подключён" : "Cloud Mail отключён"
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        StackLayout {
            id: pages
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.pageIndex()

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8
                    TextField {
                        Layout.fillWidth: true
                        implicitHeight: 48
                        placeholderText: "Поиск композиций"
                        placeholderTextColor: palette.placeholderText
                        Accessible.name: "Поиск композиций"
                        onTextChanged: library.searchText = text
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: library.trackCount + " композиций"
                            elide: Text.ElideRight
                        }
                        ComboBox {
                            Layout.preferredWidth: 152
                            model: ["Название ↑", "Название ↓", "Путь"]
                            currentIndex: library.sortMode
                            Accessible.name: "Сортировка композиций"
                            onActivated: library.sortMode = index
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: library.scanRunning
                        text: library.scanTotal > 0
                              ? "Индекс медиатеки: " + library.scanCompleted + " / " + library.scanTotal
                              : "Поиск папок и композиций в Cloud Mail…"
                        wrapMode: Text.WordWrap
                        color: palette.highlight
                    }
                    ListView {
                        id: trackList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 2
                        model: library.mode === 0 ? library : null
                        Accessible.name: "Все композиции"
                        delegate: ItemDelegate {
                            required property string stableId
                            required property string label
                            required property string artist
                            required property string album
                            required property bool isPlayable
                            width: trackList.width
                            implicitHeight: 58
                            enabled: isPlayable
                            text: label + (artist.length ? "\n" + artist : album.length ? "\n" + album : "")
                            onClicked: root.playTrack(stableId, "all")
                            Accessible.name: label + (artist.length ? ", " + artist : "")
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: library.trackCount === 0 && !library.scanRunning
                        text: media3.cloudConnected ? "Медиатека пока не проиндексирована." : "Подключите Cloud Mail в настройках."
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        opacity: 0.8
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    Label { Layout.fillWidth: true; text: "Исполнители из тегов композиций"; wrapMode: Text.WordWrap; opacity: 0.8 }
                    TextField {
                        Layout.fillWidth: true
                        implicitHeight: 48
                        placeholderText: "Поиск исполнителя"
                        placeholderTextColor: palette.placeholderText
                        onTextChanged: library.searchText = text
                        Accessible.name: "Поиск исполнителя"
                    }
                    ListView {
                        id: artistList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: library
                        Accessible.name: "Исполнители"
                        delegate: ItemDelegate {
                            required property string label
                            required property int trackCount
                            width: artistList.width
                            implicitHeight: 56
                            text: label + "\n" + trackCount + " композиций"
                            onClicked: root.openArtist(label)
                            Accessible.name: label + ", " + trackCount + " композиций"
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: artistList.count === 0
                        text: library.scanRunning ? "Исполнители появятся после чтения тегов." : "Исполнители не найдены."
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    RowLayout {
                        Layout.fillWidth: true
                        Button { text: "← Исполнители"; implicitHeight: 48; onClicked: root.setScreen("artists") }
                        Item { Layout.fillWidth: true }
                        Button { text: "Альбомы"; implicitHeight: 48; onClicked: root.openArtistAlbums() }
                        Button {
                            text: "Воспроизвести исполнителя"
                            implicitHeight: 48
                            enabled: library.firstPlayableStableId(1, root.selectedArtist).length > 0
                            onClicked: queue.startArtist(root.selectedArtist, library.firstPlayableStableId(1, root.selectedArtist))
                        }
                    }
                    TextField {
                        Layout.fillWidth: true
                        implicitHeight: 48
                        placeholderText: "Поиск композиции исполнителя"
                        placeholderTextColor: palette.placeholderText
                        onTextChanged: library.searchText = text
                        Accessible.name: "Поиск по композициям исполнителя"
                    }
                    ListView {
                        id: artistTrackList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: library
                        Accessible.name: "Композиции исполнителя"
                        delegate: ItemDelegate {
                            required property string stableId
                            required property string label
                            required property string album
                            required property bool isPlayable
                            width: artistTrackList.width
                            implicitHeight: 56
                            enabled: isPlayable
                            text: label + (album.length ? "\n" + album : "")
                            onClicked: root.playTrack(stableId, "artist")
                        }
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    RowLayout {
                        Layout.fillWidth: true
                        Button { text: "← Композиции"; implicitHeight: 48; onClicked: root.setScreen("artistTracks") }
                        Item { Layout.fillWidth: true }
                        Label { text: root.selectedArtist; elide: Text.ElideRight }
                    }
                    TextField {
                        Layout.fillWidth: true
                        implicitHeight: 48
                        placeholderText: "Поиск альбома исполнителя"
                        placeholderTextColor: palette.placeholderText
                        onTextChanged: library.searchText = text
                        Accessible.name: "Поиск альбома исполнителя"
                    }
                    ListView {
                        id: artistAlbumList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: library
                        Accessible.name: "Альбомы исполнителя"
                        delegate: ItemDelegate {
                            required property string label
                            required property string artist
                            required property string albumKey
                            required property int trackCount
                            width: artistAlbumList.width
                            implicitHeight: 60
                            text: label + "\n" + artist + " · " + trackCount + " тр."
                            onClicked: root.openAlbum(albumKey, label)
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: artistAlbumList.count === 0
                        text: "У исполнителя нет альбомов с тегами."
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    Label { Layout.fillWidth: true; text: "Альбомы сгруппированы по альбомному исполнителю"; wrapMode: Text.WordWrap; opacity: 0.8 }
                    TextField {
                        Layout.fillWidth: true
                        implicitHeight: 48
                        placeholderText: "Поиск альбома"
                        placeholderTextColor: palette.placeholderText
                        onTextChanged: library.searchText = text
                        Accessible.name: "Поиск альбома"
                    }
                    ListView {
                        id: albumList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: library
                        Accessible.name: "Альбомы"
                        delegate: ItemDelegate {
                            required property string label
                            required property string artist
                            required property string albumKey
                            required property int trackCount
                            width: albumList.width
                            implicitHeight: 60
                            text: label + (artist.length ? "\n" + artist : "") + " · " + trackCount + " тр."
                            onClicked: root.openAlbum(albumKey, label)
                        }
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    RowLayout {
                        Layout.fillWidth: true
                        Button { text: "← Альбомы"; implicitHeight: 48; onClicked: root.setScreen(root.albumFromArtist ? "artistAlbums" : "albums") }
                        Item { Layout.fillWidth: true }
                        Button {
                            text: "Воспроизвести альбом"
                            implicitHeight: 48
                            enabled: library.firstPlayableStableId(2, root.selectedAlbumKey).length > 0
                            onClicked: queue.startAlbum(root.selectedAlbumKey, library.firstPlayableStableId(2, root.selectedAlbumKey))
                        }
                    }
                    TextField {
                        Layout.fillWidth: true
                        implicitHeight: 48
                        placeholderText: "Поиск композиции в альбоме"
                        placeholderTextColor: palette.placeholderText
                        onTextChanged: library.searchText = text
                        Accessible.name: "Поиск по композициям альбома"
                    }
                    ListView {
                        id: albumTrackList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: library
                        Accessible.name: "Композиции альбома"
                        delegate: ItemDelegate {
                            required property string stableId
                            required property string label
                            required property int trackNumber
                            required property bool isPlayable
                            width: albumTrackList.width
                            implicitHeight: 54
                            enabled: isPlayable
                            text: (trackNumber > 0 ? trackNumber + ". " : "") + label
                            onClicked: root.playTrack(stableId, "album")
                        }
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    RowLayout {
                        Layout.fillWidth: true
                        Button {
                            text: "↑ Назад"
                            implicitHeight: 48
                            enabled: media3.canGoToCloudParent() && !media3.cloudBusy
                            onClicked: {
                                media3.goToCloudParent()
                                root.selectedFolderIndex = -1
                                root.selectedFolderTrackId = ""
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: media3.cloudDirectoryPath
                            elide: Text.ElideMiddle
                            Accessible.name: "Текущая папка"
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: !media3.cloudConnected
                        text: "Подключите Cloud Mail в настройках, чтобы просматривать папки."
                        wrapMode: Text.WordWrap
                    }
                    ListView {
                        id: folderList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 2
                        model: media3.cloudConnected ? mailWebDav.entries : []
                        Accessible.name: "Папки и композиции текущего каталога"
                        delegate: ItemDelegate {
                            required property int index
                            required property var modelData
                            width: folderList.width
                            implicitHeight: 56
                            text: (modelData.isDirectory ? "▸  " : "♫  ") + modelData.label
                            onClicked: {
                                if (modelData.isDirectory) {
                                    media3.openCloudDirectory(index)
                                    root.selectedFolderIndex = -1
                                    root.selectedFolderTrackId = ""
                                } else {
                                    root.selectedFolderIndex = index
                                    root.selectedFolderTrackId = modelData.stableId
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Button {
                            Layout.fillWidth: true
                            implicitHeight: 50
                            text: "Папка в очередь"
                            enabled: root.selectedFolderTrackId.length > 0 && mailWebDav.canPlayStableId(root.selectedFolderTrackId)
                            onClicked: queue.startFolder(media3.cloudDirectoryPath, root.selectedFolderTrackId)
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: media3.cloudStatus
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    Label { Layout.fillWidth: true; text: queue.currentTitle.length ? queue.currentTitle : "Ничего не играет"; font.pixelSize: 22; font.bold: true; wrapMode: Text.WordWrap }
                    Label { Layout.fillWidth: true; text: queue.currentArtist; opacity: 0.8; wrapMode: Text.WordWrap }
                    Label { Layout.fillWidth: true; text: queue.contextText; opacity: 0.7; elide: Text.ElideRight }
                    Item { Layout.fillHeight: true }
                    Slider {
                        id: seekSlider
                        Layout.fillWidth: true
                        from: 0
                        to: Math.max(1, media3.durationMs)
                        value: media3.positionMs
                        enabled: media3.seekEnabled
                        onMoved: media3.seekTo(Math.round(value))
                        Accessible.name: "Позиция воспроизведения"
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: root.formatTime(media3.positionMs) }
                        Label { text: root.formatTime(media3.durationMs) }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Button { Layout.fillWidth: true; implicitHeight: 52; text: "Предыдущая"; enabled: queue.canPrevious; onClicked: queue.previous() }
                        Button { Layout.fillWidth: true; implicitHeight: 52; text: root.queuePlaybackLabel(); enabled: queue.count > 0; onClicked: root.toggleQueuePlayback() }
                        Button { Layout.fillWidth: true; implicitHeight: 52; text: "Следующая"; enabled: queue.canNext; onClicked: queue.next() }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Button { Layout.fillWidth: true; implicitHeight: 48; text: root.repeatLabel(); enabled: queue.count > 0; onClicked: queue.repeatMode = (queue.repeatMode + 1) % 3 }
                        Button { Layout.fillWidth: true; implicitHeight: 48; text: queue.shuffleEnabled ? "Перемешано" : "Перемешать"; enabled: queue.count > 0; checkable: true; checked: queue.shuffleEnabled; onToggled: queue.shuffleEnabled = checked }
                        Button { Layout.fillWidth: true; implicitHeight: 48; text: "Стоп"; enabled: queue.active; onClicked: queue.stop() }
                    }
                    Label { Layout.fillWidth: true; text: queue.count > 0 ? media3.stateText : "Ничего не играет"; opacity: 0.8 }
                    Label { Layout.fillWidth: true; visible: queue.error.length > 0 || media3.errorText.length > 0; text: queue.error.length ? queue.error : media3.errorText; color: palette.text; wrapMode: Text.WordWrap; Accessible.role: Accessible.AlertMessage }
                    ListView {
                        id: queueList
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(240, contentHeight)
                        clip: true
                        model: queue
                        Accessible.name: "Очередь воспроизведения"
                        delegate: ItemDelegate {
                            required property string stableId
                            required property string label
                            required property string artist
                            required property bool isCurrent
                            width: queueList.width
                            implicitHeight: 52
                            highlighted: isCurrent
                            text: label + (artist.length ? "\n" + artist : "")
                            onClicked: queue.jumpTo(stableId)
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
            }

            Item {
                Flickable {
                    anchors.fill: parent
                    contentHeight: settingsColumn.implicitHeight + 28
                    clip: true
                    ColumnLayout {
                        id: settingsColumn
                        width: parent.width
                        anchors.top: parent.top
                        anchors.margins: 14
                        spacing: 12
                        Label { Layout.fillWidth: true; text: "Cloud Mail"; font.pixelSize: 20; font.bold: true }
                        Label {
                            Layout.fillWidth: true
                            text: "Используется пароль внешнего приложения. Реквизиты не сохраняются; пароль очищается сразу после отправки."
                            wrapMode: Text.WordWrap
                            opacity: 0.8
                        }
                        TextField {
                            id: emailInput
                            Layout.fillWidth: true
                            implicitHeight: 50
                            placeholderText: "Адрес почты Mail"
                            placeholderTextColor: palette.placeholderText
                            maximumLength: 320
                            inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                            enabled: !media3.cloudBusy && !media3.cloudConnected
                            Accessible.name: "Адрес почты Mail"
                        }
                        TextField {
                            id: passwordInput
                            Layout.fillWidth: true
                            implicitHeight: 50
                            placeholderText: "Пароль внешнего приложения"
                            placeholderTextColor: palette.placeholderText
                            maximumLength: 256
                            echoMode: TextInput.Password
                            passwordMaskDelay: 0
                            inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                            enabled: !media3.cloudBusy && !media3.cloudConnected
                            Accessible.name: "Пароль внешнего приложения Mail"
                        }
                        Button {
                            Layout.fillWidth: true
                            implicitHeight: 52
                            text: media3.cloudConnected ? "Отключиться" : media3.cloudBusy ? "Отменить подключение" : "Подключиться"
                            enabled: media3.cloudConnected || media3.cloudBusy || (emailInput.text.length > 0 && passwordInput.text.length > 0)
                            onClicked: {
                                if (media3.cloudConnected || media3.cloudBusy) {
                                    media3.disconnectCloud()
                                } else {
                                    media3.connectCloud(emailInput.text, passwordInput.text)
                                    passwordInput.clear()
                                }
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: media3.cloudStatus
                            wrapMode: Text.WordWrap
                            Accessible.role: Accessible.AlertMessage
                        }
                        Label { Layout.fillWidth: true; text: "Воспроизведение"; font.pixelSize: 20; font.bold: true; topPadding: 10 }
                        Label { Layout.fillWidth: true; text: "Вывод звука: " + media3.outputStatusText; wrapMode: Text.WordWrap }
                        Label { Layout.fillWidth: true; text: "Media3: " + media3.stateText; wrapMode: Text.WordWrap }
                        GroupBox {
                            Layout.fillWidth: true
                            title: "Диагностика Media3"
                            ColumnLayout {
                                anchors.fill: parent
                                Label { Layout.fillWidth: true; text: media3.activeTrackName; wrapMode: Text.WordWrap }
                                ComboBox {
                                    id: diagnosticTrackPicker
                                    Layout.fillWidth: true
                                    model: media3.trackNames
                                    currentIndex: media3.trackIndex >= 0 ? media3.trackIndex : 0
                                    Accessible.name: "Тестовая композиция Media3"
                                    onActivated: {
                                        queue.clear()
                                        media3.selectTrack(index)
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Button {
                                        Layout.fillWidth: true
                                        implicitHeight: 48
                                        text: media3.pauseEnabled ? "Пауза" : "Воспроизвести"
                                        enabled: media3.pauseEnabled || media3.playEnabled
                                        onClicked: media3.pauseEnabled ? media3.pause() : media3.play()
                                    }
                                    Button {
                                        Layout.fillWidth: true
                                        implicitHeight: 48
                                        text: "Стоп"
                                        enabled: media3.stopEnabled
                                        onClicked: media3.stop()
                                    }
                                }
                            }
                        }
                        Label { Layout.fillWidth: true; text: "Индекс: " + library.trackCount + " композиций"; wrapMode: Text.WordWrap }
                        Label {
                            Layout.fillWidth: true
                            visible: library.scanRunning
                            text: library.scanTotal > 0 ? "Читает теги: " + library.scanCompleted + " / " + library.scanTotal : "Сканирует вложенные папки…"
                            wrapMode: Text.WordWrap
                        }
                        Label { Layout.fillWidth: true; text: "Внешний вид"; font.pixelSize: 20; font.bold: true; topPadding: 10 }
                        ComboBox {
                            Layout.fillWidth: true
                            model: ["Системная", "Светлая", "Тёмная"]
                            currentIndex: appearance.themeMode === "light" ? 1 : appearance.themeMode === "dark" ? 2 : 0
                            Accessible.name: "Цветовая тема"
                            onActivated: appearance.setThemeMode(["system", "light", "dark"][index])
                        }
                        Label { Layout.fillWidth: true; text: "AllMusic Player · " + buildLabel; opacity: 0.65; wrapMode: Text.WordWrap }
                    }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: palette.midlight }

        RowLayout {
            id: miniPlayer
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Layout.topMargin: 4
            Layout.bottomMargin: 4
            spacing: 4
            Button { implicitWidth: 48; implicitHeight: 48; text: "‹"; enabled: queue.canPrevious; Accessible.name: "Предыдущая композиция"; onClicked: queue.previous() }
            Button { implicitWidth: 48; implicitHeight: 48; text: media3.pauseEnabled ? "Ⅱ" : "▶"; enabled: queue.count > 0; Accessible.name: "Воспроизведение и пауза"; onClicked: root.toggleQueuePlayback() }
            Button { implicitWidth: 48; implicitHeight: 48; text: "›"; enabled: queue.canNext; Accessible.name: "Следующая композиция"; onClicked: queue.next() }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Label { Layout.fillWidth: true; text: queue.currentTitle.length ? queue.currentTitle : "Ничего не играет"; elide: Text.ElideRight; font.bold: true }
                Label { Layout.fillWidth: true; text: queue.count > 0 ? queue.currentArtist : ""; elide: Text.ElideRight; font.pixelSize: 12; opacity: 0.75 }
            }
            ToolButton { implicitWidth: 48; implicitHeight: 48; text: "▤"; Accessible.name: "Открыть очередь"; onClicked: root.setScreen("now") }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: palette.midlight }

        RowLayout {
            id: navigation
            Layout.fillWidth: true
            Layout.leftMargin: 2
            Layout.rightMargin: 2
            spacing: 0
            Repeater {
                model: [
                    { label: "Треки", page: "tracks" },
                    { label: "Артисты", page: "artists" },
                    { label: "Альбомы", page: "albums" },
                    { label: "Папки", page: "folders" },
                    { label: "Ещё", page: "settings" }
                ]
                delegate: Button {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 54
                    text: modelData.label
                    flat: true
                    highlighted: root.screen === modelData.page
                    onClicked: root.setScreen(modelData.page)
                }
            }
        }
    }
}
