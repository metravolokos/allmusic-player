import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Fusion
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia

ApplicationWindow {
    id: root
    width: 1260
    height: 820
    minimumWidth: 850
    minimumHeight: 620
    visible: true
    title: "AllMusic Player"

    readonly property var media: playback.player
    property string screen: "tracks"
    property string selectedArtist: ""
    property string selectedAlbumKey: ""
    property string selectedAlbumName: ""
    property bool albumFromArtist: false
    property int selectedEntry: -1
    property string selectedFolderTrackId: ""

    function pageTitle() {
        if (screen === "artistTracks") return selectedArtist
        if (screen === "artistAlbums") return "Альбомы · " + selectedArtist
        if (screen === "albumTracks") return selectedAlbumName
        if (screen === "now") return "Сейчас играет"
        if (screen === "settings") return "Настройки"
        if (screen === "folders") return "Папки Cloud Mail"
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

    function openArtistAlbums() {
        setScreen("artistAlbums")
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

    function togglePlayback() {
        if (queue.count > 0) {
            if (queue.active && media.playbackState !== MediaPlayer.StoppedState)
                playback.togglePlayback()
            else
                queue.play()
        } else if (media.hasAudio || media.source.toString().length > 0) {
            playback.togglePlayback()
        }
    }

    function formatTime(milliseconds) {
        const seconds = Math.max(0, Math.floor(milliseconds / 1000))
        return Math.floor(seconds / 60) + ":" + ("0" + (seconds % 60)).slice(-2)
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

    Component.onCompleted: {
        if (typeof smokePage === "string" && smokePage.length > 0)
            root.setScreen(smokePage)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ToolBar {
            Layout.fillWidth: true
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                Label { text: "AllMusic Player"; font.pixelSize: 22; font.bold: true }
                Label { Layout.fillWidth: true; text: root.pageTitle(); leftPadding: 12; elide: Text.ElideRight; opacity: 0.8 }
                Button { text: "Открыть файл…"; implicitHeight: 42; onClicked: picker.open() }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.preferredWidth: 208
                Layout.fillHeight: true
                color: palette.alternateBase
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6
                    Label { text: "Медиатека"; font.bold: true; leftPadding: 8; bottomPadding: 4 }
                    Repeater {
                        model: [
                            { label: "Все композиции", page: "tracks" },
                            { label: "Исполнители", page: "artists" },
                            { label: "Альбомы", page: "albums" },
                            { label: "Папки", page: "folders" },
                            { label: "Сейчас играет", page: "now" },
                            { label: "Настройки", page: "settings" }
                        ]
                        delegate: Button {
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: 46
                            text: modelData.label
                            flat: true
                            highlighted: root.screen === modelData.page
                            Accessible.name: text
                            onClicked: root.setScreen(modelData.page)
                        }
                    }
                    Item { Layout.fillHeight: true }
                    Label { Layout.fillWidth: true; text: media3StateText(); wrapMode: Text.WordWrap; font.pixelSize: 12; opacity: 0.75 }
                }
            }

            Rectangle { Layout.fillHeight: true; width: 1; color: palette.midlight }

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
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 10
                        RowLayout {
                            Layout.fillWidth: true
                            TextField {
                                Layout.fillWidth: true
                                placeholderText: "Поиск по названию, исполнителю или альбому"
                                placeholderTextColor: palette.placeholderText
                                Accessible.name: "Поиск по композициям"
                                onTextChanged: library.searchText = text
                            }
                            ComboBox {
                                Layout.preferredWidth: 170
                                model: ["Название ↑", "Название ↓", "Путь"]
                                currentIndex: library.sortMode
                                Accessible.name: "Сортировка"
                                onActivated: library.sortMode = index
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { Layout.fillWidth: true; text: library.trackCount + " композиций" }
                            Label {
                                visible: library.scanRunning
                                text: library.scanTotal > 0 ? "Индекс: " + library.scanCompleted + " / " + library.scanTotal : "Сканирование папок…"
                                color: palette.highlight
                            }
                        }
                        ListView {
                            id: trackList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 2
                            model: library
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
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: library.trackCount === 0 && !library.scanRunning
                            text: mailWebDav.connected ? "Подождите, пока медиатека завершит сканирование." : "Подключите Cloud Mail в настройках."
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        Label { Layout.fillWidth: true; text: "Исполнители, найденные в тегах аудиофайлов"; opacity: 0.8 }
                        TextField { Layout.fillWidth: true; placeholderText: "Поиск исполнителя"; placeholderTextColor: palette.placeholderText; onTextChanged: library.searchText = text }
                        ListView {
                            id: artistList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: library
                            delegate: ItemDelegate {
                                required property string label
                                required property int trackCount
                                width: artistList.width
                                implicitHeight: 56
                                text: label + "\n" + trackCount + " композиций"
                                onClicked: root.openArtist(label)
                            }
                        }
                        Label { Layout.fillWidth: true; visible: artistList.count === 0; text: "Исполнители не найдены."; horizontalAlignment: Text.AlignHCenter }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        RowLayout {
                            Layout.fillWidth: true
                            Button { text: "← Исполнители"; onClicked: root.setScreen("artists") }
                            Label { Layout.fillWidth: true; text: root.selectedArtist; font.pixelSize: 20; font.bold: true; elide: Text.ElideRight }
                            Button { text: "Альбомы"; implicitHeight: 44; onClicked: root.openArtistAlbums() }
                            Button {
                                text: "Воспроизвести исполнителя"
                                implicitHeight: 44
                                enabled: library.firstPlayableStableId(1, root.selectedArtist).length > 0
                                onClicked: queue.startArtist(root.selectedArtist, library.firstPlayableStableId(1, root.selectedArtist))
                            }
                        }
                        TextField { Layout.fillWidth: true; placeholderText: "Поиск композиции исполнителя"; placeholderTextColor: palette.placeholderText; onTextChanged: library.searchText = text }
                        ListView {
                            id: artistTrackList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: library
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
                        anchors.margins: 18
                        RowLayout {
                            Layout.fillWidth: true
                            Button { text: "← Композиции"; onClicked: { root.screen = "artistTracks"; library.mode = 0; library.filterArtist = root.selectedArtist; library.filterAlbum = ""; library.searchText = "" } }
                            Label { Layout.fillWidth: true; text: root.selectedArtist; elide: Text.ElideRight }
                        }
                        TextField { Layout.fillWidth: true; placeholderText: "Поиск альбома исполнителя"; placeholderTextColor: palette.placeholderText; onTextChanged: library.searchText = text }
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
                        Label { Layout.fillWidth: true; visible: artistAlbumList.count === 0; text: "У исполнителя нет альбомов с тегами."; horizontalAlignment: Text.AlignHCenter }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        Label { Layout.fillWidth: true; text: "Альбомы сгруппированы по альбомному исполнителю"; opacity: 0.8 }
                        TextField { Layout.fillWidth: true; placeholderText: "Поиск альбома"; placeholderTextColor: palette.placeholderText; onTextChanged: library.searchText = text }
                        ListView {
                            id: albumList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: library
                            delegate: ItemDelegate {
                                required property string label
                                required property string artist
                                required property string albumKey
                                required property int trackCount
                                width: albumList.width
                                implicitHeight: 62
                                text: label + (artist.length ? "\n" + artist : "") + " · " + trackCount + " тр."
                                onClicked: root.openAlbum(albumKey, label)
                            }
                        }
                        Label { Layout.fillWidth: true; visible: albumList.count === 0; text: "Альбомы не найдены."; horizontalAlignment: Text.AlignHCenter }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        RowLayout {
                            Layout.fillWidth: true
                            Button { text: "← Альбомы"; implicitHeight: 42; onClicked: root.setScreen(root.albumFromArtist ? "artistAlbums" : "albums") }
                            Label { Layout.fillWidth: true; text: root.selectedAlbumName; font.pixelSize: 20; font.bold: true; elide: Text.ElideRight }
                            Button {
                                text: "Воспроизвести альбом"
                                implicitHeight: 44
                                enabled: library.firstPlayableStableId(2, root.selectedAlbumKey).length > 0
                                onClicked: queue.startAlbum(root.selectedAlbumKey, library.firstPlayableStableId(2, root.selectedAlbumKey))
                            }
                        }
                        TextField { Layout.fillWidth: true; placeholderText: "Поиск композиции в альбоме"; placeholderTextColor: palette.placeholderText; onTextChanged: library.searchText = text }
                        ListView {
                            id: albumTrackList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: library
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
                        anchors.margins: 18
                        RowLayout {
                            Layout.fillWidth: true
                            Label { Layout.fillWidth: true; text: mailWebDav.currentDirectory; elide: Text.ElideMiddle; Accessible.name: "Текущая папка" }
                            Button { text: "Вверх"; implicitHeight: 42; enabled: mailWebDav.canGoToParent() && !mailWebDav.busy; onClicked: { mailWebDav.goToParent(); root.selectedEntry = -1; root.selectedFolderTrackId = "" } }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { Layout.fillWidth: true; text: mailWebDav.status; wrapMode: Text.WordWrap; Accessible.role: Accessible.AlertMessage }
                            Button { text: "Отключиться"; enabled: mailWebDav.connected || mailWebDav.busy; onClicked: { mailWebDav.disconnectCloud(); root.selectedEntry = -1 } }
                        }
                        ListView {
                            id: folderList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 2
                            model: mailWebDav.entries
                            Accessible.name: "Папки и композиции текущего каталога"
                            delegate: ItemDelegate {
                                required property int index
                                required property var modelData
                                width: folderList.width
                                implicitHeight: 54
                                highlighted: root.selectedEntry === index
                                text: (modelData.isDirectory ? "▸  " : "♫  ") + modelData.label
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
                            Button {
                                text: "Открыть папку"
                                enabled: mailWebDav.canOpenDirectory(root.selectedEntry)
                                onClicked: { mailWebDav.openDirectory(root.selectedEntry); root.selectedEntry = -1; root.selectedFolderTrackId = "" }
                            }
                            Button {
                                text: "Папка в очередь"
                                enabled: root.selectedFolderTrackId.length > 0 && mailWebDav.canPlayStableId(root.selectedFolderTrackId)
                                onClicked: queue.startFolder(mailWebDav.currentDirectory, root.selectedFolderTrackId)
                            }
                            Item { Layout.fillWidth: true }
                            Label { text: library.scanRunning ? "Индексируется…" : library.trackCount + " треков"; opacity: 0.75 }
                        }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        Label { Layout.fillWidth: true; text: queue.currentTitle.length ? queue.currentTitle : playback.activeSourceLabel.length ? playback.activeSourceLabel : "Ничего не играет"; font.pixelSize: 24; font.bold: true; wrapMode: Text.WordWrap }
                        Label { Layout.fillWidth: true; text: queue.count > 0 ? queue.currentArtist : media.source.toString(); opacity: 0.8; elide: Text.ElideMiddle }
                        Label { Layout.fillWidth: true; text: queue.contextText; opacity: 0.7 }
                        Slider {
                            id: seek
                            Layout.fillWidth: true
                            from: 0
                            to: Math.max(1, media.duration)
                            value: media.position
                            enabled: media.seekable && media.duration > 0
                            Accessible.name: "Позиция воспроизведения"
                            onMoved: media.setPosition(value)
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { Layout.fillWidth: true; text: root.formatTime(seek.pressed ? seek.value : media.position) }
                            Label { text: media.duration > 0 ? root.formatTime(media.duration) : "--:--" }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Button { Layout.fillWidth: true; implicitHeight: 48; text: "Предыдущая"; enabled: queue.canPrevious; onClicked: queue.previous() }
                            Button { Layout.fillWidth: true; implicitHeight: 48; text: media.playbackState === MediaPlayer.PlayingState ? "Пауза" : "Воспроизвести"; enabled: queue.count > 0 || media.hasAudio || media.source.toString().length > 0; onClicked: root.togglePlayback() }
                            Button { Layout.fillWidth: true; implicitHeight: 48; text: "Следующая"; enabled: queue.canNext; onClicked: queue.next() }
                            Button { implicitHeight: 48; text: "Стоп"; enabled: queue.active || media.playbackState !== MediaPlayer.StoppedState; onClicked: queue.count > 0 ? queue.stop() : playback.stop() }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Button { text: queue.repeatMode === 1 ? "Повтор: всё" : queue.repeatMode === 2 ? "Повтор: трек" : "Повтор: выкл."; enabled: queue.count > 0; onClicked: queue.repeatMode = (queue.repeatMode + 1) % 3 }
                            Button { text: queue.shuffleEnabled ? "Перемешано" : "Перемешать"; enabled: queue.count > 0; checkable: true; checked: queue.shuffleEnabled; onToggled: queue.shuffleEnabled = checked }
                            Item { Layout.fillWidth: true }
                            Label { text: playback.buffering ? "Буферизация…" : media.playbackState === MediaPlayer.PlayingState ? "Воспроизведение" : media.playbackState === MediaPlayer.PausedState ? "Пауза" : "Остановлено"; opacity: 0.75 }
                        }
                        Label { Layout.fillWidth: true; visible: queue.error.length > 0 || playback.error.length > 0; text: queue.error.length ? queue.error : playback.error; color: palette.text; wrapMode: Text.WordWrap; Accessible.role: Accessible.AlertMessage }
                        ListView {
                            id: queueList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
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
                    }
                }

                Item {
                    Flickable {
                        anchors.fill: parent
                        contentHeight: settingsColumn.implicitHeight + 36
                        clip: true
                        ColumnLayout {
                            id: settingsColumn
                            width: parent.width
                            anchors.top: parent.top
                            anchors.margins: 20
                            spacing: 12
                            Label { Layout.fillWidth: true; text: "Cloud Mail"; font.pixelSize: 21; font.bold: true }
                            Label { Layout.fillWidth: true; text: "Используется пароль внешнего приложения. Реквизиты не сохраняются; пароль очищается после отправки."; wrapMode: Text.WordWrap; opacity: 0.8 }
                            RowLayout {
                                Layout.fillWidth: true
                                TextField {
                                    id: mailEmail
                                    Layout.fillWidth: true
                                    implicitHeight: 46
                                    placeholderText: "Адрес почты Mail"
                                    placeholderTextColor: palette.placeholderText
                                    maximumLength: 320
                                    enabled: !mailWebDav.busy && !mailWebDav.connected
                                    inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                                    Accessible.name: "Адрес почты Mail"
                                }
                                TextField {
                                    id: mailAppPassword
                                    Layout.fillWidth: true
                                    implicitHeight: 46
                                    placeholderText: "Пароль внешнего приложения"
                                    placeholderTextColor: palette.placeholderText
                                    maximumLength: 256
                                    echoMode: TextInput.Password
                                    passwordMaskDelay: 0
                                    enabled: !mailWebDav.busy && !mailWebDav.connected
                                    Accessible.name: "Пароль внешнего приложения Mail"
                                }
                                Button {
                                    text: mailWebDav.connected ? "Отключиться" : mailWebDav.busy ? "Отменить" : "Подключиться"
                                    implicitHeight: 46
                                    enabled: mailWebDav.connected || mailWebDav.busy || (mailEmail.text.length > 0 && mailAppPassword.text.length > 0)
                                    onClicked: {
                                        if (mailWebDav.connected || mailWebDav.busy) {
                                            mailWebDav.disconnectCloud()
                                        } else {
                                            mailWebDav.checkConnection(mailEmail.text, mailAppPassword.text)
                                            mailAppPassword.clear()
                                        }
                                    }
                                }
                            }
                            Label { Layout.fillWidth: true; text: mailWebDav.status; wrapMode: Text.WordWrap; Accessible.role: Accessible.AlertMessage }
                            Label { Layout.fillWidth: true; text: "Медиатека"; font.pixelSize: 21; font.bold: true; topPadding: 14 }
                            Label { Layout.fillWidth: true; text: library.trackCount + " аудиофайлов"; wrapMode: Text.WordWrap }
                            Label {
                                Layout.fillWidth: true
                                visible: library.scanRunning
                                text: library.scanTotal > 0 ? "Чтение тегов: " + library.scanCompleted + " / " + library.scanTotal : "Сканирование вложенных папок…"
                                wrapMode: Text.WordWrap
                            }
                            Label { Layout.fillWidth: true; text: "Звук"; font.pixelSize: 21; font.bold: true; topPadding: 14 }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: "Громкость" }
                                Slider { id: volume; Layout.fillWidth: true; from: 0; to: 1; value: playback.audio.volume; onMoved: playback.audio.volume = value; Accessible.name: "Громкость" }
                                Label { text: Math.round(volume.value * 100) + "%" }
                            }
                            Label { Layout.fillWidth: true; text: playback.deviceWarning; visible: playback.deviceWarning.length > 0; wrapMode: Text.WordWrap }
                            Label { Layout.fillWidth: true; text: "Внешний вид"; font.pixelSize: 21; font.bold: true; topPadding: 14 }
                            ComboBox {
                                Layout.preferredWidth: 180
                                model: ["Системная", "Светлая", "Тёмная"]
                                currentIndex: appearance.themeMode === "light" ? 1 : appearance.themeMode === "dark" ? 2 : 0
                                Accessible.name: "Цветовая тема"
                                onActivated: appearance.setThemeMode(["system", "light", "dark"][index])
                            }
                        }
                    }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: palette.midlight }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.topMargin: 6
            Layout.bottomMargin: 6
            spacing: 8
            Button { implicitWidth: 46; implicitHeight: 44; text: "‹"; enabled: queue.canPrevious; Accessible.name: "Предыдущая композиция"; onClicked: queue.previous() }
            Button { implicitWidth: 46; implicitHeight: 44; text: media.playbackState === MediaPlayer.PlayingState ? "Ⅱ" : "▶"; enabled: queue.count > 0 || media.hasAudio || media.source.toString().length > 0; Accessible.name: "Воспроизведение и пауза"; onClicked: root.togglePlayback() }
            Button { implicitWidth: 46; implicitHeight: 44; text: "›"; enabled: queue.canNext; Accessible.name: "Следующая композиция"; onClicked: queue.next() }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Label { Layout.fillWidth: true; text: queue.currentTitle.length ? queue.currentTitle : playback.activeSourceLabel.length ? playback.activeSourceLabel : "AllMusic Player"; font.bold: true; elide: Text.ElideRight }
                Label { Layout.fillWidth: true; text: queue.count > 0 ? queue.currentArtist : playback.buffering ? "Буферизация…" : "Готов к воспроизведению"; font.pixelSize: 12; opacity: 0.75; elide: Text.ElideRight }
            }
            Slider {
                id: miniSeek
                Layout.preferredWidth: 180
                from: 0
                to: Math.max(1, media.duration)
                value: media.position
                enabled: media.seekable && media.duration > 0
                onMoved: media.setPosition(value)
                Accessible.name: "Позиция воспроизведения"
            }
            Label { text: media.duration > 0 ? root.formatTime(media.position) + " / " + root.formatTime(media.duration) : "--:--" }
            Button { implicitHeight: 42; text: "Очередь"; onClicked: root.setScreen("now") }
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

    function media3StateText() {
        return mailWebDav.connected ? "Cloud Mail подключён" : "Cloud Mail отключён"
    }
}
