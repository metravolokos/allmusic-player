import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Fusion
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia

ApplicationWindow {
    id: root
    width: 1320
    height: 840
    minimumWidth: 940
    minimumHeight: 620
    visible: true
    title: "AllMusic Player"
    color: root.canvasColor

    readonly property var media: playback.player
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
    property int selectedEntry: -1
    property string selectedFolderTrackId: ""

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

    function setScreen(name) {
        screen = name
        selectedEntry = -1
        selectedFolderTrackId = ""
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
        if (root.showcaseMode)
            return
        if (queue.count > 0) {
            if (queue.active && media.playbackState !== MediaPlayer.StoppedState)
                playback.togglePlayback()
            else
                queue.play()
        } else if (media.hasAudio || media.source.toString().length > 0) {
            playback.togglePlayback()
        }
    }

    function displayedTitle() {
        if (root.showcaseMode)
            return "Тихие улицы"
        return queue.currentTitle.length ? queue.currentTitle : playback.activeSourceLabel
    }

    function displayedArtist() {
        return root.showcaseMode ? "Мира Северова" : queue.currentArtist
    }

    function displayedPosition() {
        return root.showcaseMode ? 84000 : media.position
    }

    function displayedDuration() {
        return root.showcaseMode ? 225000 : media.duration
    }

    function formatTime(milliseconds) {
        const seconds = Math.max(0, Math.floor(milliseconds / 1000))
        return Math.floor(seconds / 60) + ":" + ("0" + (seconds % 60)).slice(-2)
    }

    function repeatLabel() {
        if (queue.repeatMode === 1) return "Повтор всех"
        if (queue.repeatMode === 2) return "Повтор трека"
        return "Повтор выкл."
    }

    FileDialog {
        id: picker
        title: "Открыть локальную композицию"
        nameFilters: ["Аудиофайлы (*.mp3 *.flac *.aac *.m4a *.mp4)", "Все файлы (*)"]
        onAccepted: {
            queue.clear()
            playback.openLocal(selectedFile)
        }
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

    Component.onCompleted: {
        if (typeof smokePage === "string" && smokePage.length > 0)
            root.setScreen(smokePage)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: root.surfaceColor
            border.color: root.borderColor
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 22
                anchors.rightMargin: 22
                spacing: 12
                Rectangle {
                    Layout.preferredWidth: 36; Layout.preferredHeight: 36; radius: 11; color: root.accentWash
                    Label { anchors.centerIn: parent; text: "♫"; color: root.accentColor; font.pixelSize: 19 }
                }
                ColumnLayout {
                    spacing: 0
                    Label { text: "AllMusic"; color: root.textColor; font.pixelSize: 17; font.weight: Font.DemiBold }
                    Label { text: "PLAYER"; color: root.secondaryColor; font.pixelSize: 9; font.letterSpacing: 1.2 }
                }
                Rectangle { Layout.fillHeight: true; width: 1; Layout.leftMargin: 8; Layout.rightMargin: 8; color: root.borderColor }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 0
                    Label { Layout.fillWidth: true; text: root.pageTitle(); color: root.textColor; font.pixelSize: 19; font.weight: Font.DemiBold; elide: Text.ElideRight }
                    Label { Layout.fillWidth: true; visible: root.showcaseMode; text: "Предпросмотр интерфейса"; color: root.secondaryColor; font.pixelSize: 10 }
                }
                Button {
                    text: "Открыть файл…"
                    implicitHeight: 38
                    Accessible.name: "Открыть локальный аудиофайл"
                    onClicked: picker.open()
                    background: Rectangle { radius: 11; color: parent.down ? root.raisedColor : root.accentWash }
                    contentItem: Label { text: parent.text; color: root.accentColor; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: 12; rightPadding: 12 }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.preferredWidth: 220
                Layout.fillHeight: true
                color: root.surfaceColor
                border.color: root.borderColor
                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    anchors.topMargin: 20
                    anchors.bottomMargin: 14
                    spacing: 4
                    Label { Layout.leftMargin: 10; Layout.bottomMargin: 10; text: "МЕДИАТЕКА"; color: root.secondaryColor; font.pixelSize: 10; font.weight: Font.DemiBold; font.letterSpacing: 1.2 }
                    Repeater {
                        model: [
                            { label: "Композиции", page: "tracks", icon: "♫" },
                            { label: "Исполнители", page: "artists", icon: "•" },
                            { label: "Альбомы", page: "albums", icon: "▣" },
                            { label: "Папки", page: "folders", icon: "▱" }
                        ]
                        delegate: Button {
                            id: libraryNavigationButton
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: 42
                            flat: true
                            Accessible.name: modelData.label
                            onClicked: root.setScreen(modelData.page)
                            background: Rectangle { radius: 11; color: root.screen === libraryNavigationButton.modelData.page ? root.accentWash : libraryNavigationButton.hovered ? root.raisedColor : "transparent" }
                            contentItem: RowLayout {
                                spacing: 12
                                Label { Layout.leftMargin: 8; text: libraryNavigationButton.modelData.icon; color: root.screen === libraryNavigationButton.modelData.page ? root.accentColor : root.secondaryColor; font.pixelSize: 18; horizontalAlignment: Text.AlignHCenter; Layout.preferredWidth: 22 }
                                Label { Layout.fillWidth: true; text: libraryNavigationButton.modelData.label; color: root.screen === libraryNavigationButton.modelData.page ? root.accentColor : root.textColor; font.pixelSize: 14; font.weight: root.screen === libraryNavigationButton.modelData.page ? Font.DemiBold : Font.Normal }
                            }
                        }
                    }
                    Item { Layout.fillHeight: true }
                    Rectangle { Layout.fillWidth: true; height: 1; Layout.bottomMargin: 8; color: root.borderColor }
                    Button {
                        Layout.fillWidth: true; implicitHeight: 42; flat: true; text: "Настройки"; Accessible.name: text; onClicked: root.setScreen("settings")
                        background: Rectangle { radius: 11; color: root.screen === "settings" ? root.accentWash : parent.hovered ? root.raisedColor : "transparent" }
                        contentItem: RowLayout { spacing: 12; Label { Layout.leftMargin: 8; Layout.preferredWidth: 22; text: "⚙"; color: root.screen === "settings" ? root.accentColor : root.secondaryColor; font.pixelSize: 17; horizontalAlignment: Text.AlignHCenter } Label { Layout.fillWidth: true; text: "Настройки"; color: root.screen === "settings" ? root.accentColor : root.textColor; font.pixelSize: 14 } }
                    }
                    RowLayout {
                        Layout.fillWidth: true; Layout.topMargin: 8; Layout.leftMargin: 8; Layout.rightMargin: 8; spacing: 7
                        Rectangle { width: 7; height: 7; radius: 4; color: mailWebDav.connected ? root.accentColor : root.secondaryColor }
                        Label { Layout.fillWidth: true; text: mailWebDav.connected ? "Cloud Mail подключён" : "Cloud Mail отключён"; color: root.secondaryColor; font.pixelSize: 11; elide: Text.ElideRight }
                    }
                }
            }

            Rectangle { Layout.fillHeight: true; width: 1; color: root.borderColor }

            StackLayout {
                id: pages
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.screen === "artists" ? 1
                    : root.screen === "artistTracks" ? 2
                    : root.screen === "artistAlbums" ? 3
                    : root.screen === "albums" ? 4
                    : root.screen === "albumTracks" ? 5
                    : root.screen === "folders" ? 6
                    : root.screen === "now" ? 7
                    : root.screen === "settings" ? 8 : 0

                Item {
                    ColumnLayout {
                        anchors.fill: parent; anchors.leftMargin: 28; anchors.rightMargin: 28; anchors.topMargin: 24; anchors.bottomMargin: 18; spacing: 13
                        RowLayout { Layout.fillWidth: true; Label { Layout.fillWidth: true; text: "Композиции"; color: root.textColor; font.pixelSize: 26; font.weight: Font.DemiBold } Label { text: (root.showcaseMode ? showcaseTracks.count : library.trackCount) + " треков"; color: root.secondaryColor; font.pixelSize: 13 } }
                        RowLayout {
                            Layout.fillWidth: true; spacing: 10
                            TextField {
                                id: trackSearch
                                Layout.fillWidth: true; implicitHeight: 42; leftPadding: 38
                                placeholderText: "Поиск по названию, исполнителю или альбому"; placeholderTextColor: palette.placeholderText; color: root.textColor
                                Accessible.name: "Поиск по композициям"
                                onTextChanged: if (!root.showcaseMode) library.searchText = text
                                background: Rectangle { radius: 12; color: root.surfaceColor; border.color: trackSearch.activeFocus ? root.accentColor : root.borderColor; border.width: trackSearch.activeFocus ? 2 : 1 }
                                Label { anchors.left: parent.left; anchors.leftMargin: 13; anchors.verticalCenter: parent.verticalCenter; text: "⌕"; color: root.secondaryColor; font.pixelSize: 21 }
                            }
                            ComboBox {
                                id: trackSortBox
                                Layout.preferredWidth: 154; implicitHeight: 42
                                model: ["Название ↑", "Название ↓", "Путь"]
                                currentIndex: library.sortMode
                                Accessible.name: "Сортировка композиций"
                                onActivated: library.sortMode = index
                                contentItem: Label { leftPadding: 12; rightPadding: 26; text: trackSortBox.displayText; color: root.textColor; font: trackSortBox.font; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                                indicator: Label { x: trackSortBox.width - width - 10; anchors.verticalCenter: parent.verticalCenter; text: "⌄"; color: root.secondaryColor; font.pixelSize: 15 }
                                background: Rectangle { radius: 10; color: root.surfaceColor; border.color: trackSortBox.activeFocus ? root.accentColor : root.borderColor; border.width: trackSortBox.activeFocus ? 2 : 1 }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { Layout.fillWidth: true; text: library.scanRunning ? "Обновление медиатеки…" : "НАЗВАНИЕ"; color: root.secondaryColor; font.pixelSize: 10; font.weight: Font.DemiBold; font.letterSpacing: 1 }
                            Label { Layout.preferredWidth: 220; text: "АЛЬБОМ"; color: root.secondaryColor; font.pixelSize: 10; font.weight: Font.DemiBold; font.letterSpacing: 1 }
                        }
                        ListView {
                            id: trackList; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 2
                            model: root.showcaseMode ? showcaseTracks : library
                            Accessible.name: "Все композиции"
                            delegate: ItemDelegate {
                                id: trackRow
                                required property string stableId; required property string label; required property string artist; required property string album; required property bool isPlayable
                                property bool isCurrent: stableId === queue.currentTrackId
                                width: trackList.width; implicitHeight: 48; enabled: isPlayable; highlighted: isCurrent; Accessible.name: label + (artist.length ? ", " + artist : "")
                                onClicked: root.playTrack(stableId, "all")
                                background: Rectangle { radius: 9; color: trackRow.highlighted ? root.accentWash : trackRow.down ? root.raisedColor : trackRow.hovered ? root.raisedColor : "transparent"; border.color: trackRow.visualFocus ? root.accentColor : "transparent"; border.width: trackRow.visualFocus ? 1 : 0 }
                                contentItem: RowLayout {
                                    spacing: 12
                                    Rectangle { Layout.preferredWidth: 32; Layout.preferredHeight: 32; radius: 9; color: root.raisedColor; Label { anchors.centerIn: parent; text: "♪"; color: root.accentColor; font.pixelSize: 16 } }
                                    ColumnLayout { Layout.fillWidth: true; spacing: 0; Label { Layout.fillWidth: true; text: trackRow.label; color: root.textColor; font.pixelSize: 13; font.weight: trackRow.highlighted ? Font.DemiBold : Font.Normal; elide: Text.ElideRight } Label { Layout.fillWidth: true; text: trackRow.artist; visible: trackRow.artist.length > 0; color: root.secondaryColor; font.pixelSize: 11; elide: Text.ElideRight } }
                                    Label { Layout.preferredWidth: 220; text: trackRow.album; visible: trackRow.album.length > 0; color: root.secondaryColor; font.pixelSize: 12; elide: Text.ElideRight }
                                    Label { Layout.preferredWidth: 18; text: trackRow.highlighted ? "♫" : ""; color: root.accentColor; font.pixelSize: 15 }
                                }
                            }
                        }
                        Label { Layout.fillWidth: true; visible: trackList.count === 0 && !library.scanRunning; text: mailWebDav.connected ? "Подождите, пока медиатека завершит сканирование." : "Подключите Cloud Mail в настройках."; color: root.secondaryColor; horizontalAlignment: Text.AlignHCenter }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 28; spacing: 13
                        RowLayout { Layout.fillWidth: true; Label { Layout.fillWidth: true; text: "Исполнители"; color: root.textColor; font.pixelSize: 26; font.weight: Font.DemiBold } Label { text: "Из тегов композиций"; color: root.secondaryColor; font.pixelSize: 13 } }
                        TextField { Layout.fillWidth: true; implicitHeight: 42; placeholderText: "Поиск исполнителя"; placeholderTextColor: palette.placeholderText; color: root.textColor; onTextChanged: if (!root.showcaseMode) library.searchText = text; background: Rectangle { radius: 12; color: root.surfaceColor; border.color: parent.activeFocus ? root.accentColor : root.borderColor; border.width: 1 } }
                        ListView {
                            id: artistList; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 2; model: library; Accessible.name: "Исполнители"
                            delegate: ItemDelegate {
                                id: artistRow; required property string label; required property int trackCount
                                width: artistList.width; implicitHeight: 54; Accessible.name: label + ", " + trackCount + " композиций"; onClicked: root.openArtist(label)
                                background: Rectangle { radius: 9; color: artistRow.hovered ? root.raisedColor : "transparent" }
                                contentItem: RowLayout { spacing: 12; Rectangle { Layout.preferredWidth: 34; Layout.preferredHeight: 34; radius: 17; color: root.accentWash; Label { anchors.centerIn: parent; text: "•"; color: root.accentColor; font.pixelSize: 23 } } ColumnLayout { Layout.fillWidth: true; spacing: 1; Label { Layout.fillWidth: true; text: artistRow.label; color: root.textColor; font.pixelSize: 14; elide: Text.ElideRight } Label { Layout.fillWidth: true; text: artistRow.trackCount + " композиций"; color: root.secondaryColor; font.pixelSize: 11 } } Label { text: "›"; color: root.secondaryColor; font.pixelSize: 20 } }
                            }
                        }
                        Label { Layout.fillWidth: true; visible: artistList.count === 0; text: library.scanRunning ? "Исполнители появятся после чтения тегов." : "Исполнители не найдены."; color: root.secondaryColor; horizontalAlignment: Text.AlignHCenter }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 28; spacing: 12
                        RowLayout { Layout.fillWidth: true; ToolButton { text: "‹"; Accessible.name: "Назад к исполнителям"; onClicked: root.setScreen("artists") } Label { Layout.fillWidth: true; text: root.selectedArtist; color: root.textColor; font.pixelSize: 23; font.weight: Font.DemiBold; elide: Text.ElideRight } ToolButton { text: "Альбомы"; Accessible.name: "Альбомы исполнителя"; onClicked: root.setScreen("artistAlbums") } ToolButton { text: "▶"; Accessible.name: "Воспроизвести исполнителя"; enabled: library.firstPlayableStableId(1, root.selectedArtist).length > 0; onClicked: queue.startArtist(root.selectedArtist, library.firstPlayableStableId(1, root.selectedArtist)) } }
                        TextField { Layout.fillWidth: true; implicitHeight: 42; placeholderText: "Поиск композиции исполнителя"; placeholderTextColor: palette.placeholderText; color: root.textColor; onTextChanged: if (!root.showcaseMode) library.searchText = text; background: Rectangle { radius: 12; color: root.surfaceColor; border.color: parent.activeFocus ? root.accentColor : root.borderColor; border.width: 1 } }
                        ListView {
                            id: artistTrackList; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 2; model: library; Accessible.name: "Композиции исполнителя"
                            delegate: ItemDelegate {
                                id: artistTrackRow; required property string stableId; required property string label; required property string album; required property bool isPlayable
                                width: artistTrackList.width; implicitHeight: 48; enabled: isPlayable; onClicked: root.playTrack(stableId, "artist")
                                background: Rectangle { radius: 9; color: artistTrackRow.highlighted ? root.accentWash : artistTrackRow.hovered ? root.raisedColor : "transparent" }
                                contentItem: RowLayout { Rectangle { Layout.preferredWidth: 30; Layout.preferredHeight: 30; radius: 8; color: root.raisedColor; Label { anchors.centerIn: parent; text: "♪"; color: root.accentColor; font.pixelSize: 15 } } ColumnLayout { Layout.fillWidth: true; spacing: 0; Label { Layout.fillWidth: true; text: artistTrackRow.label; color: root.textColor; font.pixelSize: 13; elide: Text.ElideRight } Label { Layout.fillWidth: true; visible: artistTrackRow.album.length > 0; text: artistTrackRow.album; color: root.secondaryColor; font.pixelSize: 11; elide: Text.ElideRight } } }
                            }
                        }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 28; spacing: 12
                        RowLayout { Layout.fillWidth: true; ToolButton { text: "‹"; Accessible.name: "Назад к исполнителю"; onClicked: root.setScreen("artistTracks") } Label { Layout.fillWidth: true; text: "Альбомы · " + root.selectedArtist; color: root.textColor; font.pixelSize: 23; font.weight: Font.DemiBold; elide: Text.ElideRight } }
                        ListView {
                            id: artistAlbumList; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 2; model: library; Accessible.name: "Альбомы исполнителя"
                            delegate: ItemDelegate {
                                id: artistAlbumRow; required property string label; required property string artist; required property string albumKey; required property int trackCount
                                width: artistAlbumList.width; implicitHeight: 58; onClicked: root.openAlbum(albumKey, label)
                                background: Rectangle { radius: 10; color: artistAlbumRow.hovered ? root.raisedColor : "transparent" }
                                contentItem: RowLayout { spacing: 12; Rectangle { Layout.preferredWidth: 38; Layout.preferredHeight: 38; radius: 10; color: root.accentWash; Label { anchors.centerIn: parent; text: "▣"; color: root.accentColor } } ColumnLayout { Layout.fillWidth: true; spacing: 1; Label { Layout.fillWidth: true; text: artistAlbumRow.label; color: root.textColor; font.pixelSize: 14; elide: Text.ElideRight } Label { Layout.fillWidth: true; text: artistAlbumRow.artist + " · " + artistAlbumRow.trackCount + " тр."; color: root.secondaryColor; font.pixelSize: 11; elide: Text.ElideRight } } Label { text: "›"; color: root.secondaryColor; font.pixelSize: 20 } }
                            }
                        }
                        Label { Layout.fillWidth: true; visible: artistAlbumList.count === 0; text: "У исполнителя нет альбомов с тегами."; color: root.secondaryColor; horizontalAlignment: Text.AlignHCenter }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 28; spacing: 12
                        RowLayout { Layout.fillWidth: true; Label { Layout.fillWidth: true; text: "Альбомы"; color: root.textColor; font.pixelSize: 26; font.weight: Font.DemiBold } Label { text: "По альбомному исполнителю"; color: root.secondaryColor; font.pixelSize: 13 } }
                        TextField { Layout.fillWidth: true; implicitHeight: 42; placeholderText: "Поиск альбома"; placeholderTextColor: palette.placeholderText; color: root.textColor; onTextChanged: if (!root.showcaseMode) library.searchText = text; background: Rectangle { radius: 12; color: root.surfaceColor; border.color: parent.activeFocus ? root.accentColor : root.borderColor; border.width: 1 } }
                        ListView {
                            id: albumList; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 2; model: library; Accessible.name: "Альбомы"
                            delegate: ItemDelegate {
                                id: albumRow; required property string label; required property string artist; required property string albumKey; required property int trackCount
                                width: albumList.width; implicitHeight: 58; onClicked: root.openAlbum(albumKey, label)
                                background: Rectangle { radius: 10; color: albumRow.hovered ? root.raisedColor : "transparent" }
                                contentItem: RowLayout { spacing: 12; Rectangle { Layout.preferredWidth: 38; Layout.preferredHeight: 38; radius: 10; color: root.accentWash; Label { anchors.centerIn: parent; text: "▣"; color: root.accentColor } } ColumnLayout { Layout.fillWidth: true; spacing: 1; Label { Layout.fillWidth: true; text: albumRow.label; color: root.textColor; font.pixelSize: 14; elide: Text.ElideRight } Label { Layout.fillWidth: true; text: albumRow.artist + " · " + albumRow.trackCount + " тр."; color: root.secondaryColor; font.pixelSize: 11; elide: Text.ElideRight } } Label { text: "›"; color: root.secondaryColor; font.pixelSize: 20 } }
                            }
                        }
                        Label { Layout.fillWidth: true; visible: albumList.count === 0; text: "Альбомы появятся после чтения тегов."; color: root.secondaryColor; horizontalAlignment: Text.AlignHCenter }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 28; spacing: 12
                        RowLayout { Layout.fillWidth: true; ToolButton { text: "‹"; Accessible.name: "Назад к альбомам"; onClicked: root.setScreen(root.albumFromArtist ? "artistAlbums" : "albums") } Label { Layout.fillWidth: true; text: root.selectedAlbumName; color: root.textColor; font.pixelSize: 23; font.weight: Font.DemiBold; elide: Text.ElideRight } ToolButton { text: "▶  Воспроизвести"; Accessible.name: "Воспроизвести альбом"; enabled: library.firstPlayableStableId(2, root.selectedAlbumKey).length > 0; onClicked: queue.startAlbum(root.selectedAlbumKey, library.firstPlayableStableId(2, root.selectedAlbumKey)) } }
                        RowLayout { Layout.fillWidth: true; Label { Layout.fillWidth: true; text: "КОМПОЗИЦИИ"; color: root.secondaryColor; font.pixelSize: 10; font.weight: Font.DemiBold; font.letterSpacing: 1 } Label { Layout.preferredWidth: 220; text: "ТРЕК"; color: root.secondaryColor; font.pixelSize: 10; font.weight: Font.DemiBold; font.letterSpacing: 1 } }
                        ListView {
                            id: albumTrackList; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 2; model: library; Accessible.name: "Композиции альбома"
                            delegate: ItemDelegate {
                                id: albumTrackRow; required property string stableId; required property string label; required property int trackNumber; required property bool isPlayable
                                width: albumTrackList.width; implicitHeight: 48; enabled: isPlayable; onClicked: root.playTrack(stableId, "album")
                                background: Rectangle { radius: 9; color: albumTrackRow.highlighted ? root.accentWash : albumTrackRow.hovered ? root.raisedColor : "transparent" }
                                contentItem: RowLayout { spacing: 12; Label { Layout.preferredWidth: 28; text: albumTrackRow.trackNumber > 0 ? ("0" + albumTrackRow.trackNumber).slice(-2) : "–"; color: root.secondaryColor; font.pixelSize: 11 } Label { Layout.fillWidth: true; text: albumTrackRow.label; color: root.textColor; font.pixelSize: 13; elide: Text.ElideRight } Label { text: albumTrackRow.highlighted ? "♫" : ""; color: root.accentColor } }
                            }
                        }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 28; spacing: 12
                        RowLayout { Layout.fillWidth: true; Label { Layout.fillWidth: true; text: "Папки Cloud Mail"; color: root.textColor; font.pixelSize: 26; font.weight: Font.DemiBold } RowLayout { spacing: 6; Rectangle { width: 7; height: 7; radius: 4; color: mailWebDav.connected ? root.accentColor : root.secondaryColor } Label { text: mailWebDav.connected ? "Подключено" : "Не подключено"; color: root.secondaryColor; font.pixelSize: 12 } } }
                        RowLayout {
                            Layout.fillWidth: true; spacing: 8
                            Rectangle { Layout.fillWidth: true; implicitHeight: 42; radius: 11; color: root.surfaceColor; border.color: root.borderColor; RowLayout { anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 8; Label { Layout.fillWidth: true; text: mailWebDav.currentDirectory; color: root.textColor; elide: Text.ElideMiddle; Accessible.name: "Текущая папка" } } }
                            ToolButton { text: "↑  Вверх"; implicitHeight: 42; enabled: mailWebDav.canGoToParent() && !mailWebDav.busy; onClicked: { mailWebDav.goToParent(); root.selectedEntry = -1; root.selectedFolderTrackId = "" } }
                        }
                        Label { Layout.fillWidth: true; visible: !mailWebDav.connected || mailWebDav.status.length > 0; text: mailWebDav.status; color: mailWebDav.connected ? root.secondaryColor : root.secondaryColor; font.pixelSize: 12; wrapMode: Text.WordWrap; Accessible.role: Accessible.AlertMessage }
                        ListView {
                            id: folderList; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 2; model: mailWebDav.entries; Accessible.name: "Папки и композиции текущего каталога"
                            delegate: ItemDelegate {
                                id: folderRow; required property int index; required property var modelData
                                width: folderList.width; implicitHeight: 46; highlighted: root.selectedEntry === index
                                background: Rectangle { radius: 8; color: folderRow.highlighted ? root.accentWash : folderRow.hovered ? root.raisedColor : "transparent" }
                                contentItem: RowLayout { spacing: 12; Label { Layout.preferredWidth: 24; text: folderRow.modelData.isDirectory ? "▱" : "♪"; color: root.accentColor; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter } Label { Layout.fillWidth: true; text: folderRow.modelData.label; color: root.textColor; font.pixelSize: 13; elide: Text.ElideRight } Label { text: folderRow.modelData.isDirectory ? "›" : ""; color: root.secondaryColor; font.pixelSize: 19 } }
                                onClicked: {
                                    if (modelData.isDirectory) {
                                        mailWebDav.openDirectory(index)
                                        root.selectedEntry = -1
                                        root.selectedFolderTrackId = ""
                                    } else {
                                        root.selectedEntry = index
                                        root.selectedFolderTrackId = modelData.stableId
                                    }
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { Layout.fillWidth: true; text: library.scanRunning ? "Индексируется…" : library.trackCount + " треков"; color: root.secondaryColor; font.pixelSize: 12 }
                            Button { text: "Открыть папку"; enabled: mailWebDav.canOpenDirectory(root.selectedEntry); onClicked: { mailWebDav.openDirectory(root.selectedEntry); root.selectedEntry = -1; root.selectedFolderTrackId = "" } }
                            Button { text: "Добавить в очередь"; enabled: root.selectedFolderTrackId.length > 0 && mailWebDav.canPlayStableId(root.selectedFolderTrackId); onClicked: queue.startFolder(mailWebDav.currentDirectory, root.selectedFolderTrackId) }
                        }
                    }
                }

                Item {
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 28; spacing: 28
                        ColumnLayout {
                            Layout.preferredWidth: 350; Layout.fillHeight: true; spacing: 13
                            Item { Layout.fillHeight: true }
                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 246; Layout.preferredHeight: 246; radius: 22; color: root.accentWash; border.color: root.borderColor; border.width: 1
                                Label { anchors.centerIn: parent; text: "♫"; color: root.accentColor; font.pixelSize: 90 }
                            }
                            Label { Layout.fillWidth: true; text: root.displayedTitle().length ? root.displayedTitle() : "Нет выбранной композиции"; color: root.textColor; font.pixelSize: 22; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; maximumLineCount: 2; wrapMode: Text.Wrap; elide: Text.ElideRight }
                            Label { Layout.fillWidth: true; visible: root.displayedArtist().length > 0; text: root.displayedArtist(); color: root.secondaryColor; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }
                            Label { Layout.fillWidth: true; visible: queue.contextText.length > 0 && !root.showcaseMode; text: queue.contextText; color: root.secondaryColor; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }
                            RowLayout {
                                Layout.alignment: Qt.AlignHCenter; spacing: 8
                                ToolButton {
                                    implicitWidth: 46; implicitHeight: 46
                                    text: "|‹"; Accessible.name: "Предыдущая композиция"
                                    enabled: root.showcaseMode || queue.canPrevious
                                    onClicked: if (!root.showcaseMode) queue.previous()
                                    contentItem: Label { text: parent.text; color: root.secondaryColor; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { radius: 12; color: parent.down ? root.raisedColor : "transparent" }
                                }
                                ToolButton {
                                    implicitWidth: 54; implicitHeight: 54
                                    text: media.playbackState === MediaPlayer.PlayingState || root.showcaseMode ? "Ⅱ" : "▶"
                                    Accessible.name: media.playbackState === MediaPlayer.PlayingState ? "Пауза" : "Воспроизвести"
                                    enabled: root.showcaseMode || queue.count > 0 || media.hasAudio
                                    onClicked: root.togglePlayback()
                                    contentItem: Label { text: parent.text; color: root.accentTextColor; font.pixelSize: 20; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { radius: 27; color: root.accentColor }
                                }
                                ToolButton {
                                    implicitWidth: 46; implicitHeight: 46
                                    text: "›|"; Accessible.name: "Следующая композиция"
                                    enabled: root.showcaseMode || queue.canNext
                                    onClicked: if (!root.showcaseMode) queue.next()
                                    contentItem: Label { text: parent.text; color: root.secondaryColor; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { radius: 12; color: parent.down ? root.raisedColor : "transparent" }
                                }
                                ToolButton {
                                    implicitWidth: 42; implicitHeight: 46
                                    text: "■"; Accessible.name: "Стоп"
                                    enabled: root.showcaseMode || queue.active || media.playbackState !== MediaPlayer.StoppedState
                                    onClicked: if (!root.showcaseMode) { if (queue.count > 0) queue.stop(); else playback.stop() }
                                    contentItem: Label { text: parent.text; color: root.secondaryColor; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { radius: 12; color: parent.down ? root.raisedColor : "transparent" }
                                }
                            }
                            Item { Layout.fillHeight: true }
                        }
                        Rectangle { Layout.fillHeight: true; width: 1; color: root.borderColor }
                        ColumnLayout {
                            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 12
                            RowLayout { Layout.fillWidth: true; Label { Layout.fillWidth: true; text: "Очередь"; color: root.textColor; font.pixelSize: 21; font.weight: Font.DemiBold } Label { text: (root.showcaseMode ? showcaseTracks.count : queue.count) + " треков"; color: root.secondaryColor; font.pixelSize: 12 } }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { Layout.fillWidth: true; text: root.showcaseMode ? "Из композиции · После дождя" : queue.contextText; color: root.secondaryColor; font.pixelSize: 12; elide: Text.ElideRight }
                                ToolButton {
                                    implicitHeight: 34; text: queue.shuffleEnabled ? "⇄ Вкл." : "⇄"
                                    Accessible.name: "Перемешать очередь"
                                    checkable: true; checked: queue.shuffleEnabled
                                    enabled: root.showcaseMode || queue.count > 0
                                    onToggled: if (!root.showcaseMode) queue.shuffleEnabled = checked
                                    contentItem: Label { text: parent.text; color: parent.checked ? root.accentColor : root.textColor; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { radius: 9; color: parent.checked ? root.accentWash : root.surfaceColor; border.color: parent.checked ? root.accentColor : root.borderColor }
                                }
                                ToolButton {
                                    implicitHeight: 34; text: root.repeatLabel(); Accessible.name: root.repeatLabel()
                                    enabled: root.showcaseMode || queue.count > 0
                                    onClicked: if (!root.showcaseMode) queue.repeatMode = (queue.repeatMode + 1) % 3
                                    contentItem: Label { text: parent.text; color: root.textColor; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { radius: 9; color: root.surfaceColor; border.color: root.borderColor }
                                }
                            }
                            ListView {
                                id: queueList; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 2; model: root.showcaseMode ? showcaseTracks : queue; Accessible.name: "Очередь воспроизведения"
                                delegate: ItemDelegate {
                                    id: queueRow; required property string stableId; required property string label; required property string artist; required property bool isCurrent
                                    width: queueList.width; implicitHeight: 48; highlighted: isCurrent || stableId === queue.currentTrackId; onClicked: if (!root.showcaseMode) queue.jumpTo(stableId)
                                    background: Rectangle { radius: 9; color: queueRow.highlighted ? root.accentWash : queueRow.hovered ? root.raisedColor : "transparent" }
                                    contentItem: RowLayout { spacing: 12; Label { Layout.preferredWidth: 22; text: queueRow.highlighted ? "♫" : ""; color: root.accentColor } ColumnLayout { Layout.fillWidth: true; spacing: 0; Label { Layout.fillWidth: true; text: queueRow.label; color: root.textColor; font.pixelSize: 13; elide: Text.ElideRight } Label { Layout.fillWidth: true; text: queueRow.artist; visible: queueRow.artist.length > 0; color: root.secondaryColor; font.pixelSize: 11; elide: Text.ElideRight } } }
                                }
                            }
                            Label { Layout.fillWidth: true; visible: queue.error.length > 0 || playback.error.length > 0; text: queue.error.length ? queue.error : playback.error; color: root.darkTheme ? "#ff9a91" : "#a32119"; wrapMode: Text.WordWrap; Accessible.role: Accessible.AlertMessage }
                        }
                    }
                }

                Item {
                    Flickable {
                        anchors.fill: parent; contentHeight: settingsColumn.implicitHeight + 40; clip: true
                        ColumnLayout {
                            id: settingsColumn; width: parent.width; anchors.top: parent.top; anchors.margins: 28; spacing: 16
                            Label { Layout.fillWidth: true; text: "Настройки"; color: root.textColor; font.pixelSize: 26; font.weight: Font.DemiBold }
                            Rectangle {
                                Layout.fillWidth: true; implicitHeight: cloudSettings.implicitHeight + 30; radius: 16; color: root.surfaceColor; border.color: root.borderColor
                                ColumnLayout { id: cloudSettings; anchors.fill: parent; anchors.margins: 18; spacing: 10
                                    Label { Layout.fillWidth: true; text: "Cloud Mail"; color: root.textColor; font.pixelSize: 17; font.weight: Font.DemiBold }
                                    Label { Layout.fillWidth: true; text: "Подключение к музыкальной папке. Реквизиты не сохраняются; пароль очищается после отправки."; color: root.secondaryColor; font.pixelSize: 12; wrapMode: Text.WordWrap }
                                    RowLayout { Layout.fillWidth: true
                                        TextField { id: mailEmail; Layout.fillWidth: true; implicitHeight: 40; placeholderText: "Адрес почты Mail"; placeholderTextColor: palette.placeholderText; color: root.textColor; maximumLength: 320; enabled: !mailWebDav.busy && !mailWebDav.connected; inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase; Accessible.name: "Адрес почты Mail"; background: Rectangle { radius: 10; color: root.canvasColor; border.color: mailEmail.activeFocus ? root.accentColor : root.borderColor; border.width: 1 } }
                                        TextField { id: mailAppPassword; Layout.fillWidth: true; implicitHeight: 40; placeholderText: "Пароль приложения"; placeholderTextColor: palette.placeholderText; color: root.textColor; maximumLength: 256; echoMode: TextInput.Password; passwordMaskDelay: 0; enabled: !mailWebDav.busy && !mailWebDav.connected; Accessible.name: "Пароль внешнего приложения Mail"; background: Rectangle { radius: 10; color: root.canvasColor; border.color: mailAppPassword.activeFocus ? root.accentColor : root.borderColor; border.width: 1 } }
                                        Button { implicitHeight: 40; text: mailWebDav.connected ? "Отключиться" : mailWebDav.busy ? "Отменить" : "Подключиться"; enabled: mailWebDav.connected || mailWebDav.busy || (mailEmail.text.length > 0 && mailAppPassword.text.length > 0); onClicked: { if (mailWebDav.connected || mailWebDav.busy) mailWebDav.disconnectCloud(); else { mailWebDav.checkConnection(mailEmail.text, mailAppPassword.text); mailAppPassword.clear() } } }
                                    }
                                    Label { Layout.fillWidth: true; text: mailWebDav.status; color: root.secondaryColor; font.pixelSize: 12; wrapMode: Text.WordWrap; Accessible.role: Accessible.AlertMessage }
                                    Label { Layout.fillWidth: true; visible: library.scanRunning; text: library.scanTotal > 0 ? "Чтение тегов: " + library.scanCompleted + " / " + library.scanTotal : "Сканирование папок…"; color: root.secondaryColor; font.pixelSize: 12; wrapMode: Text.WordWrap }
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true; implicitHeight: appearanceSettings.implicitHeight + 28; radius: 16; color: root.surfaceColor; border.color: root.borderColor
                                ColumnLayout { id: appearanceSettings; anchors.fill: parent; anchors.margins: 18; spacing: 8
                                    Label { Layout.fillWidth: true; text: "Внешний вид"; color: root.textColor; font.pixelSize: 17; font.weight: Font.DemiBold }
                                    RowLayout { Layout.fillWidth: true; Label { Layout.fillWidth: true; text: "Тема"; color: root.secondaryColor } ComboBox { Layout.preferredWidth: 170; model: ["Системная", "Светлая", "Тёмная"]; currentIndex: appearance.themeMode === "light" ? 1 : appearance.themeMode === "dark" ? 2 : 0; Accessible.name: "Цветовая тема"; onActivated: appearance.setThemeMode(["system", "light", "dark"][index]) } }
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true; implicitHeight: playbackSettings.implicitHeight + 28; radius: 16; color: root.surfaceColor; border.color: root.borderColor
                                ColumnLayout { id: playbackSettings; anchors.fill: parent; anchors.margins: 18; spacing: 8
                                    Label { Layout.fillWidth: true; text: "Воспроизведение"; color: root.textColor; font.pixelSize: 17; font.weight: Font.DemiBold }
                                    Label { Layout.fillWidth: true; text: "Устройство вывода: " + (playback.deviceWarning.length > 0 ? playback.deviceWarning : "по умолчанию"); color: root.secondaryColor; font.pixelSize: 12; wrapMode: Text.WordWrap }
                                    Label { Layout.fillWidth: true; text: library.trackCount + " композиций в медиатеке"; color: root.secondaryColor; font.pixelSize: 12 }
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 94
            color: root.surfaceColor
            border.color: root.borderColor
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 22; anchors.rightMargin: 22; spacing: 14
                Rectangle { Layout.preferredWidth: 46; Layout.preferredHeight: 46; radius: 12; color: root.accentWash; Label { anchors.centerIn: parent; text: "♪"; color: root.accentColor; font.pixelSize: 22 } }
                RowLayout {
                    Layout.preferredWidth: 142; spacing: 2
                    ToolButton {
                        implicitWidth: 40; implicitHeight: 46
                        text: "|‹"; Accessible.name: "Предыдущая композиция"
                        enabled: root.showcaseMode || queue.canPrevious
                        onClicked: if (!root.showcaseMode) queue.previous()
                        contentItem: Label { text: parent.text; color: root.secondaryColor; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { radius: 10; color: parent.down ? root.raisedColor : "transparent" }
                    }
                    ToolButton {
                        implicitWidth: 46; implicitHeight: 46
                        text: media.playbackState === MediaPlayer.PlayingState || root.showcaseMode ? "Ⅱ" : "▶"
                        Accessible.name: media.playbackState === MediaPlayer.PlayingState ? "Пауза" : "Воспроизвести"
                        enabled: root.showcaseMode || queue.count > 0 || media.hasAudio
                        onClicked: root.togglePlayback()
                        contentItem: Label { text: parent.text; color: root.accentTextColor; font.pixelSize: 17; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { radius: 23; color: root.accentColor }
                    }
                    ToolButton {
                        implicitWidth: 40; implicitHeight: 46
                        text: "›|"; Accessible.name: "Следующая композиция"
                        enabled: root.showcaseMode || queue.canNext
                        onClicked: if (!root.showcaseMode) queue.next()
                        contentItem: Label { text: parent.text; color: root.secondaryColor; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { radius: 10; color: parent.down ? root.raisedColor : "transparent" }
                    }
                }
                ColumnLayout {
                    Layout.preferredWidth: 220; spacing: 1
                    Label { Layout.fillWidth: true; text: root.displayedTitle().length ? root.displayedTitle() : "Нет выбранной композиции"; color: root.textColor; font.pixelSize: 13; font.weight: Font.DemiBold; elide: Text.ElideRight }
                    Label { Layout.fillWidth: true; visible: root.displayedArtist().length > 0; text: root.displayedArtist(); color: root.secondaryColor; font.pixelSize: 11; elide: Text.ElideRight }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 0
                    Slider { id: miniSeek; Layout.fillWidth: true; from: 0; to: Math.max(1, root.displayedDuration()); value: root.displayedPosition(); enabled: root.showcaseMode || (media.seekable && media.duration > 0); onMoved: if (!root.showcaseMode) media.setPosition(value); Accessible.name: "Позиция воспроизведения" }
                    RowLayout { Layout.fillWidth: true; Label { Layout.fillWidth: true; text: root.formatTime(miniSeek.pressed ? miniSeek.value : root.displayedPosition()); color: root.secondaryColor; font.pixelSize: 10 } Label { text: root.displayedDuration() > 0 ? root.formatTime(root.displayedDuration()) : "--:--"; color: root.secondaryColor; font.pixelSize: 10 } }
                }
                ToolButton {
                    implicitWidth: 40; implicitHeight: 42; text: "⇄"
                    Accessible.name: "Перемешать очередь"
                    checkable: true; checked: queue.shuffleEnabled
                    enabled: root.showcaseMode || queue.count > 0
                    onToggled: if (!root.showcaseMode) queue.shuffleEnabled = checked
                    contentItem: Label { text: parent.text; color: parent.checked ? root.accentColor : root.textColor; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { radius: 10; color: parent.checked ? root.accentWash : parent.hovered ? root.raisedColor : "transparent" }
                }
                ToolButton {
                    implicitWidth: 98; implicitHeight: 42; text: root.repeatLabel()
                    Accessible.name: root.repeatLabel(); enabled: root.showcaseMode || queue.count > 0
                    onClicked: if (!root.showcaseMode) queue.repeatMode = (queue.repeatMode + 1) % 3
                    contentItem: Label { text: parent.text; color: root.textColor; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                    background: Rectangle { radius: 10; color: parent.hovered ? root.raisedColor : "transparent" }
                }
                RowLayout { Layout.preferredWidth: 142; spacing: 6; Label { text: "◖"; color: root.secondaryColor; font.pixelSize: 15 } Slider { id: volume; Layout.fillWidth: true; from: 0; to: 1; value: playback.audio.volume; onMoved: playback.audio.volume = value; Accessible.name: "Громкость" } }
                ToolButton {
                    implicitWidth: 40; implicitHeight: 42; text: "■"; Accessible.name: "Стоп"
                    enabled: root.showcaseMode || queue.active || media.playbackState !== MediaPlayer.StoppedState
                    onClicked: if (!root.showcaseMode) { if (queue.count > 0) queue.stop(); else playback.stop() }
                    contentItem: Label { text: parent.text; color: root.secondaryColor; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { radius: 10; color: parent.down ? root.raisedColor : "transparent" }
                }
            }
        }
    }

    Connections {
        target: mailWebDav
        function onEntriesChanged() {
            root.selectedEntry = -1
            root.selectedFolderTrackId = ""
        }
    }
    Component.onDestruction: mailWebDav.disconnectCloud()
}
