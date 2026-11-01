import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Fusion
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: root.androidPreview ? 412 : 400
    height: root.androidPreview ? 915 : 820
    visible: true
    title: "AllMusic Player"
    color: root.canvasColor

    readonly property bool androidPreview: typeof smokePlatform === "string" && smokePlatform === "android"
    readonly property bool showcaseMode: typeof smokeTheme === "string" && smokeTheme.length > 0
    readonly property bool darkTheme: appearance.themeMode === "dark"
        || (appearance.themeMode === "system" && palette.window.hsvValue < 0.5)
    readonly property color canvasColor: darkTheme ? "#111815" : "#f3f5f2"
    readonly property color surfaceColor: darkTheme ? "#19231e" : "#ffffff"
    readonly property color raisedColor: darkTheme ? "#202c26" : "#eaf0eb"
    readonly property color textColor: darkTheme ? "#eef4ef" : "#202a25"
    readonly property color secondaryColor: darkTheme ? "#b0bdb4" : "#637068"
    readonly property color borderColor: darkTheme ? "#334239" : "#dce4dd"
    readonly property color accentColor: darkTheme ? "#82cba9" : "#397b60"
    readonly property color accentTextColor: darkTheme ? "#102019" : "#ffffff"
    readonly property color accentWash: darkTheme ? "#263c32" : "#e0eee5"

    palette.windowText: root.textColor
    palette.base: root.surfaceColor
    palette.alternateBase: root.raisedColor
    palette.text: root.textColor
    palette.button: root.surfaceColor
    palette.buttonText: root.textColor
    palette.highlight: root.accentColor
    palette.highlightedText: root.accentTextColor
    palette.placeholderText: root.secondaryColor

    property string screen: "tracks"
    property string selectedArtist: ""
    property string selectedAlbumKey: ""
    property string selectedAlbumName: ""
    property bool albumFromArtist: false
    property int selectedFolderIndex: -1
    property string selectedFolderTrackId: ""
    property var mediaController: typeof media3 !== "undefined" ? media3 : previewMedia3

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
        if (root.showcaseMode)
            return
        if (scope === "artist")
            queue.startArtist(selectedArtist, stableId)
        else if (scope === "album")
            queue.startAlbum(selectedAlbumKey, stableId)
        else
            queue.startAll(stableId)
    }

    function togglePlayback() {
        if (mediaController.stateText === "Пауза")
            mediaController.play()
        else if (queue.active && mediaController.pauseEnabled)
            mediaController.pause()
        else if (queue.active && mediaController.playEnabled)
            mediaController.play()
        else if (!root.showcaseMode && !queue.active)
            queue.play()
    }

    function pageTitle() {
        if (screen === "artistTracks") return selectedArtist
        if (screen === "artistAlbums") return "Альбомы исполнителя"
        if (screen === "albumTracks") return selectedAlbumName
        if (screen === "now") return "Сейчас играет"
        if (screen === "settings") return "Настройки"
        if (screen === "folders") return "Папки"
        if (screen === "artists") return "Исполнители"
        if (screen === "albums") return "Альбомы"
        return "Композиции"
    }

    function displayedTitle() {
        return root.showcaseMode ? "Тихие улицы" : queue.currentTitle
    }

    function displayedArtist() {
        return root.showcaseMode ? "Мира Северова" : queue.currentArtist
    }

    function displayedPosition() {
        return root.showcaseMode ? 84000 : mediaController.positionMs
    }

    function displayedDuration() {
        return root.showcaseMode ? 225000 : mediaController.durationMs
    }

    function formatTime(milliseconds) {
        const seconds = Math.max(0, Math.floor(milliseconds / 1000))
        return Math.floor(seconds / 60) + ":" + ("0" + (seconds % 60)).slice(-2)
    }

    function repeatLabel() {
        if (queue.repeatMode === 1) return "Повтор: все"
        if (queue.repeatMode === 2) return "Повтор: трек"
        return "Повтор: выкл."
    }

    function cloudConnected() {
        return mediaController.cloudConnected
    }

    function cloudStatus() {
        return root.androidPreview ? mailWebDav.status : mediaController.cloudStatus
    }

    function cloudDirectory() {
        return root.androidPreview ? mailWebDav.currentDirectory : mediaController.cloudDirectoryPath
    }

    function cloudCanGoUp() {
        return root.androidPreview ? mailWebDav.canGoToParent() && !mailWebDav.busy
                                  : mediaController.canGoToCloudParent() && !mediaController.cloudBusy
    }

    function cloudGoUp() {
        if (root.androidPreview)
            mailWebDav.goToParent()
        else
            mediaController.goToCloudParent()
        selectedFolderIndex = -1
        selectedFolderTrackId = ""
    }

    function cloudOpenDirectory(index) {
        if (root.androidPreview)
            mailWebDav.openDirectory(index)
        else
            mediaController.openCloudDirectory(index)
    }

    function isCloudBusy() {
        return root.androidPreview ? mailWebDav.busy : mediaController.cloudBusy
    }

    Component.onCompleted: {
        if (typeof smokePage === "string" && smokePage.length > 0)
            root.setScreen(smokePage)
    }

    QtObject {
        id: previewMedia3
        property bool cloudConnected: true
        property bool cloudBusy: false
        property string cloudStatus: "Подключено"
        property string cloudDirectoryPath: "/AllMusic/"
        property string stateText: "Воспроизведение"
        property string errorText: ""
        property string outputStatusText: "Готово"
        property string activeTrackName: "Тихие улицы"
        property var trackNames: []
        property int trackIndex: -1
        property int positionMs: 84000
        property int durationMs: 225000
        property bool pauseEnabled: true
        property bool playEnabled: false
        property bool stopEnabled: true
        property bool seekEnabled: true
        function canGoToCloudParent() { return false }
        function goToCloudParent() {}
        function openCloudDirectory(index) {}
        function play() {}
        function pause() {}
        function stop() {}
        function seekTo(position) {}
        function connectCloud(email, password) {}
        function disconnectCloud() {}
        function selectTrack(index) {}
    }

    ListModel {
        id: showcaseTracks
        ListElement { stableId: "preview-01"; label: "Первые огни"; artist: "Мира Северова"; album: "После дождя"; isPlayable: true; isCurrent: false; trackNumber: 1 }
        ListElement { stableId: "preview-02"; label: "Тихие улицы"; artist: "Мира Северова"; album: "После дождя"; isPlayable: true; isCurrent: true; trackNumber: 2 }
        ListElement { stableId: "preview-03"; label: "Сторона ветра"; artist: "Мира Северова"; album: "После дождя"; isPlayable: true; isCurrent: false; trackNumber: 3 }
        ListElement { stableId: "preview-04"; label: "Там, где свет остаётся дольше"; artist: "Никита Белый"; album: "Медленная вода"; isPlayable: true; isCurrent: false; trackNumber: 4 }
        ListElement { stableId: "preview-05"; label: "Полуночный поезд"; artist: "Никита Белый"; album: "Медленная вода"; isPlayable: true; isCurrent: false; trackNumber: 5 }
        ListElement { stableId: "preview-06"; label: "Июльский воздух"; artist: "Лада Волкова"; album: "Линии горизонта"; isPlayable: true; isCurrent: false; trackNumber: 6 }
        ListElement { stableId: "preview-07"; label: "Ниже облаков"; artist: "Лада Волкова"; album: "Линии горизонта"; isPlayable: true; isCurrent: false; trackNumber: 7 }
        ListElement { stableId: "preview-08"; label: "Снег на стекле"; artist: "Антон Лесной"; album: "Тихий город"; isPlayable: true; isCurrent: false; trackNumber: 8 }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        StackLayout {
            id: pages
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: Math.max(1, root.height - miniPlayer.Layout.preferredHeight
                - navigation.Layout.preferredHeight - miniPlayerSeparator.height)
            currentIndex: root.pageIndex()

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    anchors.topMargin: 18
                    anchors.bottomMargin: 10
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: "Композиции"
                            color: root.textColor
                            font.pixelSize: 25
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        ToolButton {
                            implicitWidth: 46
                            implicitHeight: 46
                            text: "⚙"
                            Accessible.name: "Настройки"
                            onClicked: root.setScreen("settings")
                            background: Rectangle {
                                radius: 14
                                color: parent.down ? root.raisedColor : "transparent"
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Rectangle { width: 8; height: 8; radius: 4; color: root.showcaseMode ? root.secondaryColor : root.cloudConnected() ? root.accentColor : root.secondaryColor }
                        Label {
                            Layout.fillWidth: true
                            text: root.showcaseMode ? "Предпросмотр интерфейса" : root.cloudConnected() ? "Cloud Mail подключён" : "Cloud Mail не подключён"
                            color: root.secondaryColor
                            font.pixelSize: 13
                        }
                        Label {
                            text: (root.showcaseMode ? showcaseTracks.count : library.trackCount) + " треков"
                            color: root.secondaryColor
                            font.pixelSize: 13
                        }
                    }

                    TextField {
                        id: trackSearch
                        Layout.fillWidth: true
                        implicitHeight: 48
                        leftPadding: 42
                        placeholderText: "Поиск по медиатеке"
                        placeholderTextColor: palette.placeholderText
                        color: root.textColor
                        Accessible.name: "Поиск по названию, исполнителю или альбому"
                        onTextChanged: if (!root.showcaseMode) library.searchText = text
                        background: Rectangle {
                            radius: 14
                            color: root.surfaceColor
                            border.color: trackSearch.activeFocus ? root.accentColor : root.borderColor
                            border.width: trackSearch.activeFocus ? 2 : 1
                        }
                        Label {
                            anchors.left: parent.left
                            anchors.leftMargin: 15
                            anchors.verticalCenter: parent.verticalCenter
                            text: "⌕"
                            color: root.secondaryColor
                            font.pixelSize: 24
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: library.scanRunning ? "Обновление медиатеки…" : "Недавно добавленные"
                            color: root.secondaryColor
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                        ComboBox {
                            id: trackSortBox
                            Layout.preferredWidth: 132
                            implicitHeight: 38
                            model: ["Название ↑", "Название ↓", "Путь"]
                            currentIndex: library.sortMode
                            Accessible.name: "Сортировка композиций"
                            onActivated: library.sortMode = index
                            contentItem: Label { leftPadding: 12; rightPadding: 26; text: trackSortBox.displayText; color: root.textColor; font: trackSortBox.font; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            indicator: Label { x: trackSortBox.width - width - 10; anchors.verticalCenter: parent.verticalCenter; text: "⌄"; color: root.secondaryColor; font.pixelSize: 15 }
                            background: Rectangle { radius: 10; color: root.surfaceColor; border.color: trackSortBox.activeFocus ? root.accentColor : root.borderColor; border.width: trackSortBox.activeFocus ? 2 : 1 }
                        }
                    }

                    ListView {
                        id: trackList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 3
                        model: root.showcaseMode ? showcaseTracks : library.mode === 0 ? library : null
                        Accessible.name: "Все композиции"
                        delegate: ItemDelegate {
                            id: trackRow
                            required property string stableId
                            required property string label
                            required property string artist
                            required property string album
                            required property bool isPlayable
                            property bool isCurrent: root.showcaseMode
                                ? stableId === "preview-02" : stableId === queue.currentTrackId
                            width: trackList.width
                            implicitHeight: label.length > 36 ? 66 : 60
                            enabled: isPlayable
                            Accessible.name: label + (artist.length ? ", " + artist : "")
                            highlighted: isCurrent || stableId === queue.currentTrackId
                            onClicked: root.playTrack(stableId, "all")
                            background: Rectangle {
                                radius: 12
                                color: trackRow.highlighted ? root.accentWash
                                    : trackRow.down ? root.raisedColor
                                    : trackRow.hovered ? root.raisedColor : "transparent"
                                border.color: trackRow.highlighted ? root.accentColor : "transparent"
                                border.width: trackRow.highlighted ? 1 : 0
                            }
                            contentItem: RowLayout {
                                spacing: 12
                                Rectangle {
                                    Layout.preferredWidth: 42
                                    Layout.preferredHeight: 42
                                    radius: 11
                                    color: root.raisedColor
                                    Accessible.ignored: true
                                    Label {
                                        anchors.centerIn: parent
                                        text: "♪"
                                        color: root.accentColor
                                        font.pixelSize: 21
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Label {
                                        Layout.fillWidth: true
                                        text: trackRow.label
                                        color: root.textColor
                                        font.pixelSize: 15
                                        font.weight: trackRow.highlighted ? Font.DemiBold : Font.Normal
                                        maximumLineCount: 2
                                        wrapMode: Text.Wrap
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        visible: trackRow.artist.length > 0 || trackRow.album.length > 0
                                        text: trackRow.artist.length > 0 ? trackRow.artist : trackRow.album
                                        color: root.secondaryColor
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                }
                                Label {
                                    visible: trackRow.isCurrent || trackRow.stableId === queue.currentTrackId
                                    text: "♫"
                                    color: root.accentColor
                                    font.pixelSize: 18
                                }
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: trackList.count === 0 && !library.scanRunning
                        text: root.cloudConnected() ? "Медиатека пока пуста." : "Подключите Cloud Mail в настройках."
                        color: root.secondaryColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: "Исполнители"; color: root.textColor; font.pixelSize: 25; font.weight: Font.DemiBold }
                        ToolButton { text: "⚙"; Accessible.name: "Настройки"; onClicked: root.setScreen("settings") }
                    }
                    TextField {
                        Layout.fillWidth: true
                        implicitHeight: 48
                        placeholderText: "Поиск исполнителя"
                        placeholderTextColor: palette.placeholderText
                        color: root.textColor
                        Accessible.name: "Поиск исполнителя"
                        onTextChanged: if (!root.showcaseMode) library.searchText = text
                        background: Rectangle { radius: 14; color: root.surfaceColor; border.color: parent.activeFocus ? root.accentColor : root.borderColor; border.width: 1 }
                    }
                    ListView {
                        id: artistList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 3
                        model: library
                        Accessible.name: "Исполнители"
                        delegate: ItemDelegate {
                            id: artistRow
                            required property string label
                            required property int trackCount
                            width: artistList.width
                            implicitHeight: 62
                            Accessible.name: label + ", " + trackCount + " композиций"
                            onClicked: root.openArtist(label)
                            background: Rectangle { radius: 12; color: artistRow.down ? root.raisedColor : artistRow.hovered ? root.raisedColor : "transparent" }
                            contentItem: RowLayout {
                                spacing: 12
                                Rectangle {
                                    Layout.preferredWidth: 42; Layout.preferredHeight: 42; radius: 21; color: root.accentWash
                                    Label { anchors.centerIn: parent; text: "•"; color: root.accentColor; font.pixelSize: 25 }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true; spacing: 2
                                    Label { Layout.fillWidth: true; text: artistRow.label; color: root.textColor; font.pixelSize: 15; elide: Text.ElideRight }
                                    Label { Layout.fillWidth: true; text: artistRow.trackCount + " композиций"; color: root.secondaryColor; font.pixelSize: 12 }
                                }
                                Label { text: "›"; color: root.secondaryColor; font.pixelSize: 22 }
                            }
                        }
                    }
                    Label { Layout.fillWidth: true; visible: artistList.count === 0; text: library.scanRunning ? "Исполнители появятся после чтения тегов." : "Исполнители не найдены."; color: root.secondaryColor; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        ToolButton { text: "‹"; Accessible.name: "Назад к исполнителям"; onClicked: root.setScreen("artists") }
                        Label { Layout.fillWidth: true; text: root.selectedArtist; color: root.textColor; font.pixelSize: 22; font.weight: Font.DemiBold; elide: Text.ElideRight }
                        ToolButton { text: "⚙"; Accessible.name: "Настройки"; onClicked: root.setScreen("settings") }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: "Композиции"; color: root.secondaryColor; font.pixelSize: 13 }
                        ToolButton {
                            text: "Альбомы"
                            Accessible.name: "Альбомы исполнителя"
                            onClicked: root.setScreen("artistAlbums")
                        }
                        ToolButton {
                            text: "▶"
                            Accessible.name: "Воспроизвести исполнителя"
                            enabled: library.firstPlayableStableId(1, root.selectedArtist).length > 0
                            onClicked: queue.startArtist(root.selectedArtist, library.firstPlayableStableId(1, root.selectedArtist))
                        }
                    }
                    TextField {
                        Layout.fillWidth: true
                        implicitHeight: 46
                        placeholderText: "Поиск в исполнителе"
                        placeholderTextColor: palette.placeholderText
                        color: root.textColor
                        Accessible.name: "Поиск композиции исполнителя"
                        onTextChanged: if (!root.showcaseMode) library.searchText = text
                        background: Rectangle { radius: 14; color: root.surfaceColor; border.color: parent.activeFocus ? root.accentColor : root.borderColor; border.width: 1 }
                    }
                    ListView {
                        id: artistTrackList
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 3
                        model: library
                        Accessible.name: "Композиции исполнителя"
                        delegate: ItemDelegate {
                            id: artistTrackRow
                            required property string stableId
                            required property string label
                            required property string album
                            required property bool isPlayable
                            width: artistTrackList.width; implicitHeight: 60; enabled: isPlayable
                            onClicked: root.playTrack(stableId, "artist")
                            background: Rectangle { radius: 12; color: artistTrackRow.highlighted ? root.accentWash : artistTrackRow.hovered ? root.raisedColor : "transparent" }
                            contentItem: RowLayout {
                                spacing: 12
                                Rectangle { Layout.preferredWidth: 40; Layout.preferredHeight: 40; radius: 10; color: root.raisedColor; Label { anchors.centerIn: parent; text: "♪"; color: root.accentColor } }
                                ColumnLayout { Layout.fillWidth: true; spacing: 2; Label { Layout.fillWidth: true; text: artistTrackRow.label; color: root.textColor; elide: Text.ElideRight } Label { Layout.fillWidth: true; visible: artistTrackRow.album.length > 0; text: artistTrackRow.album; color: root.secondaryColor; font.pixelSize: 12; elide: Text.ElideRight } }
                            }
                        }
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 20; spacing: 12
                    RowLayout { Layout.fillWidth: true; ToolButton { text: "‹"; Accessible.name: "Назад к композициям"; onClicked: root.setScreen("artistTracks") } Label { Layout.fillWidth: true; text: "Альбомы · " + root.selectedArtist; color: root.textColor; font.pixelSize: 22; font.weight: Font.DemiBold; elide: Text.ElideRight } }
                    TextField { Layout.fillWidth: true; implicitHeight: 46; placeholderText: "Поиск альбома"; placeholderTextColor: palette.placeholderText; color: root.textColor; onTextChanged: if (!root.showcaseMode) library.searchText = text; background: Rectangle { radius: 14; color: root.surfaceColor; border.color: parent.activeFocus ? root.accentColor : root.borderColor; border.width: 1 } }
                    ListView {
                        id: artistAlbumList; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 3; model: library; Accessible.name: "Альбомы исполнителя"
                        delegate: ItemDelegate {
                            id: artistAlbumRow
                            required property string label; required property string artist; required property string albumKey; required property int trackCount
                            width: artistAlbumList.width; implicitHeight: 64; onClicked: root.openAlbum(albumKey, label)
                            background: Rectangle { radius: 12; color: artistAlbumRow.hovered ? root.raisedColor : "transparent" }
                            contentItem: RowLayout { spacing: 12; Rectangle { Layout.preferredWidth: 44; Layout.preferredHeight: 44; radius: 11; color: root.accentWash; Label { anchors.centerIn: parent; text: "▣"; color: root.accentColor } } ColumnLayout { Layout.fillWidth: true; spacing: 2; Label { Layout.fillWidth: true; text: artistAlbumRow.label; color: root.textColor; elide: Text.ElideRight } Label { Layout.fillWidth: true; text: artistAlbumRow.artist + " · " + artistAlbumRow.trackCount + " тр."; color: root.secondaryColor; font.pixelSize: 12; elide: Text.ElideRight } } Label { text: "›"; color: root.secondaryColor; font.pixelSize: 22 } }
                        }
                    }
                    Label { Layout.fillWidth: true; visible: artistAlbumList.count === 0; text: "У исполнителя нет альбомов с тегами."; color: root.secondaryColor; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 20; spacing: 12
                    RowLayout { Layout.fillWidth: true; Label { Layout.fillWidth: true; text: "Альбомы"; color: root.textColor; font.pixelSize: 25; font.weight: Font.DemiBold } ToolButton { text: "⚙"; Accessible.name: "Настройки"; onClicked: root.setScreen("settings") } }
                    TextField { Layout.fillWidth: true; implicitHeight: 48; placeholderText: "Поиск альбома"; placeholderTextColor: palette.placeholderText; color: root.textColor; onTextChanged: if (!root.showcaseMode) library.searchText = text; background: Rectangle { radius: 14; color: root.surfaceColor; border.color: parent.activeFocus ? root.accentColor : root.borderColor; border.width: 1 } }
                    ListView {
                        id: albumList; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 3; model: library; Accessible.name: "Альбомы"
                        delegate: ItemDelegate {
                            id: albumRow
                            required property string label; required property string artist; required property string albumKey; required property int trackCount
                            width: albumList.width; implicitHeight: 64; onClicked: root.openAlbum(albumKey, label)
                            background: Rectangle { radius: 12; color: albumRow.hovered ? root.raisedColor : "transparent" }
                            contentItem: RowLayout { spacing: 12; Rectangle { Layout.preferredWidth: 44; Layout.preferredHeight: 44; radius: 11; color: root.accentWash; Label { anchors.centerIn: parent; text: "▣"; color: root.accentColor } } ColumnLayout { Layout.fillWidth: true; spacing: 2; Label { Layout.fillWidth: true; text: albumRow.label; color: root.textColor; elide: Text.ElideRight } Label { Layout.fillWidth: true; text: albumRow.artist + " · " + albumRow.trackCount + " тр."; color: root.secondaryColor; font.pixelSize: 12; elide: Text.ElideRight } } Label { text: "›"; color: root.secondaryColor; font.pixelSize: 22 } }
                        }
                    }
                    Label { Layout.fillWidth: true; visible: albumList.count === 0; text: "Альбомы появятся после чтения тегов."; color: root.secondaryColor; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 20; spacing: 12
                    RowLayout { Layout.fillWidth: true; ToolButton { text: "‹"; Accessible.name: "Назад к альбомам"; onClicked: root.setScreen(root.albumFromArtist ? "artistAlbums" : "albums") } Label { Layout.fillWidth: true; text: root.selectedAlbumName; color: root.textColor; font.pixelSize: 22; font.weight: Font.DemiBold; elide: Text.ElideRight } }
                    RowLayout { Layout.fillWidth: true; Label { Layout.fillWidth: true; text: "Композиции альбома"; color: root.secondaryColor; font.pixelSize: 13 } ToolButton { text: "▶  Воспроизвести"; Accessible.name: "Воспроизвести альбом"; enabled: library.firstPlayableStableId(2, root.selectedAlbumKey).length > 0; onClicked: queue.startAlbum(root.selectedAlbumKey, library.firstPlayableStableId(2, root.selectedAlbumKey)) } }
                    TextField { Layout.fillWidth: true; implicitHeight: 46; placeholderText: "Поиск в альбоме"; placeholderTextColor: palette.placeholderText; color: root.textColor; onTextChanged: if (!root.showcaseMode) library.searchText = text; background: Rectangle { radius: 14; color: root.surfaceColor; border.color: parent.activeFocus ? root.accentColor : root.borderColor; border.width: 1 } }
                    ListView {
                        id: albumTrackList; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 3; model: library; Accessible.name: "Композиции альбома"
                        delegate: ItemDelegate {
                            id: albumTrackRow
                            required property string stableId; required property string label; required property int trackNumber; required property bool isPlayable
                            width: albumTrackList.width; implicitHeight: 58; enabled: isPlayable; onClicked: root.playTrack(stableId, "album")
                            background: Rectangle { radius: 12; color: albumTrackRow.highlighted ? root.accentWash : albumTrackRow.hovered ? root.raisedColor : "transparent" }
                            contentItem: RowLayout { spacing: 12; Label { text: albumTrackRow.trackNumber > 0 ? ("0" + albumTrackRow.trackNumber).slice(-2) : "–"; color: root.secondaryColor; font.pixelSize: 12 } Label { Layout.fillWidth: true; text: albumTrackRow.label; color: root.textColor; elide: Text.ElideRight } Label { visible: albumTrackRow.highlighted; text: "♫"; color: root.accentColor } }
                        }
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 20; spacing: 12
                    RowLayout { Layout.fillWidth: true; Label { Layout.fillWidth: true; text: "Папки"; color: root.textColor; font.pixelSize: 25; font.weight: Font.DemiBold } ToolButton { text: "⚙"; Accessible.name: "Настройки"; onClicked: root.setScreen("settings") } }
                    Rectangle {
                        Layout.fillWidth: true; implicitHeight: 48; radius: 12; color: root.surfaceColor; border.color: root.borderColor
                        RowLayout { anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 6; ToolButton { text: "‹"; Accessible.name: "На уровень выше"; enabled: root.cloudCanGoUp(); onClicked: root.cloudGoUp() } Label { Layout.fillWidth: true; text: root.cloudDirectory(); color: root.textColor; elide: Text.ElideMiddle; Accessible.name: "Текущая папка" } }
                    }
                    Label { Layout.fillWidth: true; visible: !root.cloudConnected(); text: "Подключите Cloud Mail в настройках, чтобы просматривать папки."; color: root.secondaryColor; wrapMode: Text.WordWrap }
                    ListView {
                        id: folderList; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 3
                        model: root.cloudConnected() ? mailWebDav.entries : []
                        Accessible.name: "Папки и композиции текущего каталога"
                        delegate: ItemDelegate {
                            id: folderRow; required property int index; required property var modelData
                            width: folderList.width; implicitHeight: 58; highlighted: root.selectedFolderIndex === index
                            background: Rectangle { radius: 12; color: folderRow.highlighted ? root.accentWash : folderRow.hovered ? root.raisedColor : "transparent" }
                            contentItem: RowLayout { spacing: 12; Rectangle { Layout.preferredWidth: 38; Layout.preferredHeight: 38; radius: 10; color: root.raisedColor; Label { anchors.centerIn: parent; text: folderRow.modelData.isDirectory ? "▱" : "♪"; color: root.accentColor } } Label { Layout.fillWidth: true; text: folderRow.modelData.label; color: root.textColor; maximumLineCount: 2; wrapMode: Text.Wrap; elide: Text.ElideRight } Label { text: folderRow.modelData.isDirectory ? "›" : ""; color: root.secondaryColor; font.pixelSize: 22 } }
                            onClicked: {
                                if (modelData.isDirectory) {
                                    root.cloudOpenDirectory(index)
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
                        Label { Layout.fillWidth: true; text: root.cloudStatus(); color: root.secondaryColor; font.pixelSize: 12; wrapMode: Text.WordWrap; Accessible.role: Accessible.AlertMessage }
                        Button { text: "В очередь"; enabled: root.selectedFolderTrackId.length > 0 && mailWebDav.canPlayStableId(root.selectedFolderTrackId); onClicked: queue.startFolder(root.cloudDirectory(), root.selectedFolderTrackId) }
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 22; spacing: 12
                    RowLayout { Layout.fillWidth: true; ToolButton { text: "‹"; Accessible.name: "Назад в медиатеку"; onClicked: root.setScreen("tracks") } Label { Layout.fillWidth: true; text: "Сейчас играет"; color: root.textColor; font.pixelSize: 22; font.weight: Font.DemiBold } }
                    Item { Layout.fillHeight: true }
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: Math.min(root.width - 96, 188)
                        Layout.preferredHeight: Layout.preferredWidth
                        radius: 22; color: root.accentWash
                        border.color: root.borderColor; border.width: 1
                        Label { anchors.centerIn: parent; text: "♫"; color: root.accentColor; font.pixelSize: 66 }
                    }
                    Label { Layout.fillWidth: true; text: root.displayedTitle().length ? root.displayedTitle() : "Нет выбранной композиции"; color: root.textColor; font.pixelSize: 23; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; maximumLineCount: 2; wrapMode: Text.Wrap; elide: Text.ElideRight }
                    Label { Layout.fillWidth: true; visible: root.displayedArtist().length > 0; text: root.displayedArtist(); color: root.secondaryColor; font.pixelSize: 15; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }
                    Label { Layout.fillWidth: true; visible: queue.contextText.length > 0 && !root.showcaseMode; text: queue.contextText; color: root.secondaryColor; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }
                    Slider { id: seekSlider; Layout.fillWidth: true; from: 0; to: Math.max(1, root.displayedDuration()); value: root.displayedPosition(); enabled: root.showcaseMode || mediaController.seekEnabled; onMoved: if (!root.showcaseMode) mediaController.seekTo(Math.round(value)); Accessible.name: "Позиция воспроизведения" }
                    RowLayout { Layout.fillWidth: true; Label { Layout.fillWidth: true; text: root.formatTime(seekSlider.pressed ? seekSlider.value : root.displayedPosition()); color: root.secondaryColor; font.pixelSize: 12 } Label { text: root.formatTime(root.displayedDuration()); color: root.secondaryColor; font.pixelSize: 12 } }
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 14
                        ToolButton {
                            implicitWidth: 52; implicitHeight: 52
                            text: "|‹"; Accessible.name: "Предыдущая композиция"
                            enabled: root.showcaseMode || queue.canPrevious
                            onClicked: if (!root.showcaseMode) queue.previous()
                            contentItem: Label { text: parent.text; color: root.secondaryColor; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { radius: 26; color: parent.down ? root.raisedColor : "transparent" }
                        }
                        ToolButton {
                            implicitWidth: 66; implicitHeight: 66
                            text: mediaController.pauseEnabled ? "Ⅱ" : "▶"
                            Accessible.name: mediaController.pauseEnabled ? "Пауза" : "Воспроизвести"
                            enabled: root.showcaseMode || queue.count > 0
                            onClicked: root.togglePlayback()
                            contentItem: Label { text: parent.text; color: root.accentTextColor; font.pixelSize: 24; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { radius: 33; color: root.accentColor }
                        }
                        ToolButton {
                            implicitWidth: 52; implicitHeight: 52
                            text: "›|"; Accessible.name: "Следующая композиция"
                            enabled: root.showcaseMode || queue.canNext
                            onClicked: if (!root.showcaseMode) queue.next()
                            contentItem: Label { text: parent.text; color: root.secondaryColor; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { radius: 26; color: parent.down ? root.raisedColor : "transparent" }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        ToolButton {
                            Layout.fillWidth: true; implicitHeight: 42
                            text: queue.shuffleEnabled ? "⇄  Перемешано" : "⇄  Перемешать"
                            Accessible.name: "Перемешать очередь"
                            checkable: true; checked: queue.shuffleEnabled
                            enabled: root.showcaseMode || queue.count > 0
                            onToggled: if (!root.showcaseMode) queue.shuffleEnabled = checked
                            contentItem: Label { text: parent.text; color: parent.checked ? root.accentColor : root.textColor; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            background: Rectangle { radius: 12; color: parent.checked ? root.accentWash : root.surfaceColor; border.color: parent.checked ? root.accentColor : root.borderColor }
                        }
                        ToolButton {
                            Layout.fillWidth: true; implicitHeight: 42
                            text: root.repeatLabel(); Accessible.name: root.repeatLabel()
                            enabled: root.showcaseMode || queue.count > 0
                            onClicked: if (!root.showcaseMode) queue.repeatMode = (queue.repeatMode + 1) % 3
                            contentItem: Label { text: parent.text; color: root.textColor; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            background: Rectangle { radius: 12; color: root.surfaceColor; border.color: root.borderColor }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: queue.count > 0 || root.showcaseMode ? mediaController.stateText : "Очередь пуста"; color: root.secondaryColor; font.pixelSize: 12 }
                        ToolButton {
                            implicitHeight: 36; text: "Стоп"; enabled: root.showcaseMode || queue.active
                            onClicked: if (!root.showcaseMode) queue.stop()
                            contentItem: Label { text: parent.text; color: root.textColor; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { radius: 10; color: parent.down ? root.raisedColor : root.surfaceColor; border.color: root.borderColor }
                        }
                    }
                    Label { Layout.fillWidth: true; visible: queue.error.length > 0 || mediaController.errorText.length > 0; text: queue.error.length ? queue.error : mediaController.errorText; color: root.darkTheme ? "#ff9a91" : "#a32119"; wrapMode: Text.WordWrap; Accessible.role: Accessible.AlertMessage }
                    RowLayout { Layout.fillWidth: true; Label { Layout.fillWidth: true; text: "Очередь"; color: root.textColor; font.pixelSize: 16; font.weight: Font.DemiBold } Label { text: (root.showcaseMode ? showcaseTracks.count : queue.count) + " тр."; color: root.secondaryColor; font.pixelSize: 12 } }
                    ListView {
                        id: queueList; Layout.fillWidth: true; Layout.preferredHeight: Math.min(142, Math.max(0, contentHeight)); clip: true; spacing: 1
                        model: root.showcaseMode ? showcaseTracks : queue
                        Accessible.name: "Очередь воспроизведения"
                        delegate: ItemDelegate {
                            id: queueRow; required property string stableId; required property string label; required property string artist; required property bool isCurrent
                            width: queueList.width; implicitHeight: 44; highlighted: isCurrent || stableId === queue.currentTrackId
                            onClicked: if (!root.showcaseMode) queue.jumpTo(stableId)
                            background: Rectangle { radius: 9; color: queueRow.highlighted ? root.accentWash : queueRow.hovered ? root.raisedColor : "transparent" }
                            contentItem: RowLayout { Label { Layout.preferredWidth: 22; text: queueRow.highlighted ? "♫" : ""; color: root.accentColor } ColumnLayout { Layout.fillWidth: true; spacing: 0; Label { Layout.fillWidth: true; text: queueRow.label; color: root.textColor; font.pixelSize: 13; elide: Text.ElideRight } Label { Layout.fillWidth: true; visible: queueRow.artist.length > 0; text: queueRow.artist; color: root.secondaryColor; font.pixelSize: 11; elide: Text.ElideRight } } }
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
            }

            Item {
                Flickable {
                    anchors.fill: parent
                    contentHeight: settingsColumn.implicitHeight + 32
                    clip: true
                    ColumnLayout {
                        id: settingsColumn
                        width: parent.width
                        anchors.top: parent.top
                        anchors.margins: 20
                        spacing: 14
                        RowLayout { Layout.fillWidth: true; ToolButton { text: "‹"; Accessible.name: "Назад в медиатеку"; onClicked: root.setScreen("tracks") } Label { Layout.fillWidth: true; text: "Настройки"; color: root.textColor; font.pixelSize: 24; font.weight: Font.DemiBold } }
                        Rectangle {
                            Layout.fillWidth: true; implicitHeight: cloudSettings.implicitHeight + 28; radius: 18; color: root.surfaceColor; border.color: root.borderColor
                            ColumnLayout { id: cloudSettings; anchors.fill: parent; anchors.margins: 16; spacing: 10
                                Label { Layout.fillWidth: true; text: "Cloud Mail"; color: root.textColor; font.pixelSize: 17; font.weight: Font.DemiBold }
                                Label { Layout.fillWidth: true; text: "Подключите музыкальную папку, чтобы открыть медиатеку."; color: root.secondaryColor; font.pixelSize: 12; wrapMode: Text.WordWrap }
                                TextField { id: emailInput; Layout.fillWidth: true; implicitHeight: 46; placeholderText: "Адрес почты Mail"; placeholderTextColor: palette.placeholderText; color: root.textColor; maximumLength: 320; inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase; enabled: !root.isCloudBusy() && !root.cloudConnected(); Accessible.name: "Адрес почты Mail"; background: Rectangle { radius: 12; color: root.canvasColor; border.color: emailInput.activeFocus ? root.accentColor : root.borderColor; border.width: 1 } }
                                TextField { id: passwordInput; Layout.fillWidth: true; implicitHeight: 46; placeholderText: "Пароль внешнего приложения"; placeholderTextColor: palette.placeholderText; color: root.textColor; maximumLength: 256; echoMode: TextInput.Password; passwordMaskDelay: 0; inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText; enabled: !root.isCloudBusy() && !root.cloudConnected(); Accessible.name: "Пароль внешнего приложения Mail"; background: Rectangle { radius: 12; color: root.canvasColor; border.color: passwordInput.activeFocus ? root.accentColor : root.borderColor; border.width: 1 } }
                                Button { Layout.fillWidth: true; implicitHeight: 46; text: root.cloudConnected() ? "Отключиться" : root.isCloudBusy() ? "Отменить подключение" : "Подключить Cloud Mail"; enabled: root.cloudConnected() || root.isCloudBusy() || (emailInput.text.length > 0 && passwordInput.text.length > 0); onClicked: { if (root.androidPreview) { if (root.cloudConnected() || mailWebDav.busy) mailWebDav.disconnectCloud(); else { mailWebDav.checkConnection(emailInput.text, passwordInput.text); passwordInput.clear() } } else if (root.cloudConnected() || root.isCloudBusy()) mediaController.disconnectCloud(); else { mediaController.connectCloud(emailInput.text, passwordInput.text); passwordInput.clear() } } }
                                Label { Layout.fillWidth: true; text: root.cloudStatus(); color: root.secondaryColor; font.pixelSize: 12; wrapMode: Text.WordWrap; Accessible.role: Accessible.AlertMessage }
                                Label { Layout.fillWidth: true; text: "Реквизиты не сохраняются; пароль очищается после отправки."; color: root.secondaryColor; font.pixelSize: 11; wrapMode: Text.WordWrap }
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true; implicitHeight: appearanceSettings.implicitHeight + 28; radius: 18; color: root.surfaceColor; border.color: root.borderColor
                            ColumnLayout { id: appearanceSettings; anchors.fill: parent; anchors.margins: 16; spacing: 8
                                Label { Layout.fillWidth: true; text: "Внешний вид"; color: root.textColor; font.pixelSize: 17; font.weight: Font.DemiBold }
                                RowLayout { Layout.fillWidth: true; Label { Layout.fillWidth: true; text: "Тема"; color: root.secondaryColor } ComboBox { Layout.preferredWidth: 160; model: ["Системная", "Светлая", "Тёмная"]; currentIndex: appearance.themeMode === "light" ? 1 : appearance.themeMode === "dark" ? 2 : 0; Accessible.name: "Цветовая тема"; onActivated: appearance.setThemeMode(["system", "light", "dark"][index]) } }
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true; implicitHeight: diagnosticsColumn.implicitHeight + 26; radius: 18; color: root.surfaceColor; border.color: root.borderColor
                            ColumnLayout { id: diagnosticsColumn; anchors.fill: parent; anchors.margins: 14; spacing: 8
                                Label { Layout.fillWidth: true; text: "Диагностика"; color: root.textColor; font.pixelSize: 15; font.weight: Font.DemiBold }
                                Label { Layout.fillWidth: true; text: "Состояние вывода: " + mediaController.outputStatusText; color: root.secondaryColor; font.pixelSize: 12; wrapMode: Text.WordWrap }
                                Label { Layout.fillWidth: true; text: "Состояние воспроизведения: " + mediaController.stateText; color: root.secondaryColor; font.pixelSize: 12; wrapMode: Text.WordWrap }
                                Label { Layout.fillWidth: true; text: library.trackCount + " композиций в медиатеке"; color: root.secondaryColor; font.pixelSize: 12 }
                                Label { Layout.fillWidth: true; visible: library.scanRunning; text: library.scanTotal > 0 ? "Читает теги: " + library.scanCompleted + " / " + library.scanTotal : "Сканирует папки…"; color: root.secondaryColor; font.pixelSize: 12; wrapMode: Text.WordWrap }
                                GroupBox {
                                    Layout.fillWidth: true; title: "Локальный тест плеера"
                                    ColumnLayout { anchors.fill: parent
                                        Label { Layout.fillWidth: true; text: mediaController.activeTrackName; color: root.secondaryColor; wrapMode: Text.WordWrap }
                                        ComboBox { id: diagnosticTrackPicker; Layout.fillWidth: true; model: mediaController.trackNames; currentIndex: mediaController.trackIndex >= 0 ? mediaController.trackIndex : 0; Accessible.name: "Тестовая композиция Media3"; onActivated: { queue.clear(); mediaController.selectTrack(index) } }
                                        RowLayout { Layout.fillWidth: true; Button { Layout.fillWidth: true; text: mediaController.pauseEnabled ? "Пауза" : "Воспроизвести"; enabled: mediaController.pauseEnabled || mediaController.playEnabled; onClicked: mediaController.pauseEnabled ? mediaController.pause() : mediaController.play() } Button { Layout.fillWidth: true; text: "Стоп"; enabled: mediaController.stopEnabled; onClicked: mediaController.stop() } }
                                    }
                                }
                                Label { Layout.fillWidth: true; visible: mediaController.errorText.length > 0; text: mediaController.errorText; color: root.darkTheme ? "#ff9a91" : "#a32119"; wrapMode: Text.WordWrap }
                                Label { Layout.fillWidth: true; text: typeof buildLabel === "string" ? "Сборка " + buildLabel : ""; color: root.secondaryColor; font.pixelSize: 11; wrapMode: Text.WordWrap }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            id: miniPlayer
            Layout.fillWidth: true
            Layout.preferredHeight: root.screen === "now" || root.screen === "settings" ? 0 : root.showcaseMode || queue.count > 0 ? 72 : 0
            visible: Layout.preferredHeight > 0
            color: root.surfaceColor
            border.color: root.borderColor
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14; anchors.rightMargin: 12; spacing: 10
                Rectangle { Layout.preferredWidth: 44; Layout.preferredHeight: 44; radius: 12; color: root.accentWash; Label { anchors.centerIn: parent; text: "♪"; color: root.accentColor; font.pixelSize: 21 } }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 1
                    Label { Layout.fillWidth: true; text: root.displayedTitle().length ? root.displayedTitle() : "Выберите композицию"; color: root.textColor; font.pixelSize: 14; font.weight: Font.DemiBold; elide: Text.ElideRight }
                    Label { Layout.fillWidth: true; visible: root.displayedArtist().length > 0; text: root.displayedArtist(); color: root.secondaryColor; font.pixelSize: 11; elide: Text.ElideRight }
                }
                ToolButton {
                    implicitWidth: 44; implicitHeight: 48
                    text: mediaController.pauseEnabled ? "Ⅱ" : "▶"
                    Accessible.name: mediaController.pauseEnabled ? "Пауза" : "Воспроизвести"
                    enabled: root.showcaseMode || queue.count > 0
                    onClicked: root.togglePlayback()
                    background: Rectangle { radius: 14; color: parent.down ? root.raisedColor : "transparent" }
                }
                ToolButton { implicitWidth: 40; implicitHeight: 48; text: "⌃"; Accessible.name: "Открыть экран Сейчас играет"; onClicked: root.setScreen("now"); background: Rectangle { radius: 14; color: parent.down ? root.raisedColor : "transparent" } }
            }
        }

        Rectangle {
            id: miniPlayerSeparator
            Layout.fillWidth: true
            height: root.screen === "now" || root.screen === "settings" ? 0 : 1
            color: root.borderColor
        }

        RowLayout {
            id: navigation
            Layout.fillWidth: true
            Layout.preferredHeight: root.screen === "now" || root.screen === "settings" ? 0 : 62
            visible: Layout.preferredHeight > 0
            spacing: 2
            Repeater {
                model: [
                    { label: "Треки", page: "tracks", icon: "♫" },
                    { label: "Артисты", page: "artists", icon: "•" },
                    { label: "Альбомы", page: "albums", icon: "▣" },
                    { label: "Папки", page: "folders", icon: "▱" }
                ]
                delegate: Button {
                    id: navButton
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    flat: true
                    Accessible.name: modelData.label
                    onClicked: root.setScreen(modelData.page)
                    background: Rectangle { radius: 13; color: root.screen === navButton.modelData.page ? root.accentWash : navButton.down ? root.raisedColor : "transparent" }
                    contentItem: Column {
                        spacing: 1
                        Label { anchors.horizontalCenter: parent.horizontalCenter; text: navButton.modelData.icon; color: root.screen === navButton.modelData.page ? root.accentColor : root.secondaryColor; font.pixelSize: 17; font.weight: Font.DemiBold }
                        Label { anchors.horizontalCenter: parent.horizontalCenter; text: navButton.modelData.label; color: root.screen === navButton.modelData.page ? root.accentColor : root.secondaryColor; font.pixelSize: 10; font.weight: root.screen === navButton.modelData.page ? Font.DemiBold : Font.Normal }
                    }
                }
            }
        }
    }

    Connections {
        target: mailWebDav
        function onEntriesChanged() {
            root.selectedFolderIndex = -1
            root.selectedFolderTrackId = ""
        }
    }
    Component.onDestruction: if (!root.androidPreview) mediaController.disconnectCloud()
}
