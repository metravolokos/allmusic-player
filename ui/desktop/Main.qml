import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Fusion
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia

ApplicationWindow {
    id: window
    width: 1120
    height: 760
    minimumWidth: 760
    minimumHeight: 560
    visible: true
    title: "AllMusic Player"

    readonly property var media: playback.player
    property int selectedEntry: -1
    property bool diagnosticsExpanded: false

    function time(milliseconds) {
        const seconds = Math.floor(Math.max(0, milliseconds) / 1000)
        return Math.floor(seconds / 60) + ":" + (seconds % 60).toString().padStart(2, "0")
    }

    function playSelected() {
        if (selectedEntry >= 0 && mailWebDav.connected && mailWebDav.canPlay(selectedEntry)
                && !playback.cloudMailActive) {
            mailWebDav.play(selectedEntry)
            return
        }
        if (media.hasAudio || media.source.toString().length > 0
                || playback.currentLoopbackIndex >= 0 || playback.cloudMailActive)
            playback.togglePlayback()
    }

    FileDialog {
        id: picker
        title: "Открыть локальный аудиофайл"
        nameFilters: ["Аудиофайлы (*.mp3 *.flac *.aac *.m4a *.mp4)", "Все файлы (*)"]
        onAccepted: playback.openLocal(selectedFile)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Label { text: "AllMusic Player"; font.pixelSize: 24; font.bold: true }
            Item { Layout.fillWidth: true }
            Label { text: "Тема"; Accessible.name: "Настройка темы" }
            ComboBox {
                id: themeSelector
                Layout.preferredWidth: 150
                model: ["Системная", "Светлая", "Тёмная"]
                currentIndex: appearance.themeMode === "light" ? 1
                    : appearance.themeMode === "dark" ? 2 : 0
                Accessible.name: "Цветовая тема"
                onActivated: appearance.setThemeMode(["system", "light", "dark"][index])
            }
            Button { text: "Открыть файл…"; onClicked: picker.open() }
        }

        GroupBox {
            id: cloudPanel
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 220
            title: "Cloud Mail · /AllMusic"

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    visible: !mailWebDav.connected
                    text: "Используйте пароль внешнего приложения Mail. Данные не сохраняются и поля очищаются после отправки."
                    wrapMode: Text.WordWrap
                    opacity: 0.78
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: !mailWebDav.connected
                    TextField {
                        id: mailEmail
                        Layout.fillWidth: true
                        placeholderText: "Адрес почты Mail"
                        maximumLength: 320
                        enabled: !mailWebDav.busy
                        inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                        Accessible.name: "Адрес почты Mail"
                    }
                    TextField {
                        id: mailAppPassword
                        Layout.fillWidth: true
                        placeholderText: "Пароль внешнего приложения"
                        maximumLength: 256
                        echoMode: TextInput.Password
                        passwordMaskDelay: 0
                        enabled: !mailWebDav.busy
                        Accessible.name: "Пароль внешнего приложения Mail"
                    }
                    Button {
                        text: mailWebDav.busy ? "Отменить" : "Подключиться"
                        enabled: mailWebDav.busy || (mailEmail.text.length > 0
                                 && mailAppPassword.text.length > 0)
                        onClicked: {
                            if (mailWebDav.busy) {
                                mailWebDav.cancel()
                            } else {
                                mailWebDav.checkConnection(mailEmail.text, mailAppPassword.text)
                                mailEmail.clear()
                                mailAppPassword.clear()
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: mailWebDav.connected
                    Label {
                        Layout.fillWidth: true
                        text: mailWebDav.currentDirectory
                        elide: Text.ElideMiddle
                        Accessible.name: "Текущая папка"
                    }
                    Label { text: mailWebDav.entryCount + " элементов" }
                    Button {
                        text: "Назад"
                        enabled: mailWebDav.canGoToParent() && !mailWebDav.busy
                        onClicked: { mailWebDav.goToParent(); window.selectedEntry = -1 }
                    }
                    Button {
                        text: "Отключиться"
                        enabled: !mailWebDav.busy
                        onClicked: { mailWebDav.disconnectCloud(); window.selectedEntry = -1 }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: mailWebDav.status
                    visible: mailWebDav.status.length > 0
                    wrapMode: Text.WordWrap
                    Accessible.role: Accessible.AlertMessage
                }

                ListView {
                    id: libraryList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 120
                    clip: true
                    spacing: 2
                    model: mailWebDav.entries
                    Accessible.name: "Папки и аудиофайлы текущей папки"
                    delegate: ItemDelegate {
                        required property int index
                        required property var modelData
                        width: libraryList.width
                        highlighted: window.selectedEntry === index
                        text: (modelData.isDirectory ? "▸  " : "♫  ") + modelData.label
                        onClicked: window.selectedEntry = index
                    }
                }
                Label {
                    Layout.fillWidth: true
                    visible: mailWebDav.connected && !mailWebDav.busy
                             && mailWebDav.entries.length === 0
                    text: "В этой папке нет элементов"
                    horizontalAlignment: Text.AlignHCenter
                    opacity: 0.7
                }

                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        text: "Открыть папку"
                        enabled: mailWebDav.canOpenDirectory(window.selectedEntry)
                        onClicked: { mailWebDav.openDirectory(window.selectedEntry); window.selectedEntry = -1 }
                    }
                    Button {
                        text: "Воспроизвести выбранное"
                        enabled: mailWebDav.canPlay(window.selectedEntry)
                        onClicked: mailWebDav.play(window.selectedEntry)
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: window.selectedEntry >= 0
                              && window.selectedEntry < mailWebDav.entries.length
                              ? mailWebDav.entries[window.selectedEntry].label
                              : "Выберите папку или композицию"
                        elide: Text.ElideMiddle
                        Layout.maximumWidth: 360
                    }
                }
            }
        }

        Button {
            Layout.fillWidth: true
            text: window.diagnosticsExpanded ? "Скрыть диагностику" : "Диагностика"
            onClicked: window.diagnosticsExpanded = !window.diagnosticsExpanded
        }

        GroupBox {
            Layout.fillWidth: true
            visible: window.diagnosticsExpanded
            title: "Локальная диагностика"
            ColumnLayout {
                anchors.fill: parent
                RowLayout {
                    Layout.fillWidth: true
                    TextField {
                        id: webdavPort
                        Layout.preferredWidth: 100
                        text: "8765"
                        validator: IntValidator { bottom: 1; top: 65535 }
                        Accessible.name: "Порт loopback WebDAV"
                    }
                    Button { text: "Найти loopback-файлы"; onClicked: playback.scanLoopback(Number(webdavPort.text)) }
                    ComboBox {
                        id: remoteFile
                        Layout.fillWidth: true
                        model: playback.loopbackFiles
                        Accessible.name: "Файл из тестового /AllMusic"
                    }
                    Button {
                        text: "Открыть"
                        enabled: remoteFile.currentIndex >= 0
                        onClicked: {
                            if (playback.openLoopback(remoteFile.currentIndex)) playback.togglePlayback()
                        }
                    }
                    Label { text: "Range: " + playback.networkBytes + " байт" }
                }
                RowLayout {
                    Layout.fillWidth: true
                    CheckBox {
                        id: rangeConsent
                        Layout.fillWidth: true
                        text: "Разрешаю Range до 64 КиБ для выбранной композиции"
                        enabled: mailWebDav.canCheckRange(window.selectedEntry)
                        onCheckedChanged: if (!enabled) checked = false
                    }
                    Button {
                        text: "Проверить Range"
                        enabled: rangeConsent.checked && mailWebDav.canCheckRange(window.selectedEntry)
                        onClicked: { mailWebDav.checkRange(window.selectedEntry); rangeConsent.checked = false }
                    }
                }
            }
        }

        GroupBox {
            id: playerPanel
            Layout.fillWidth: true
            title: playback.activeSourceLabel || media.source.toString() || "Сейчас играет"

            ColumnLayout {
                anchors.fill: parent
                spacing: 6
                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        text: media.playbackState === MediaPlayer.PlayingState ? "Пауза" : "Воспроизвести"
                        enabled: (media.hasAudio || media.source.toString().length > 0
                                  || playback.currentLoopbackIndex >= 0 || playback.cloudMailActive
                                  || mailWebDav.canPlay(window.selectedEntry))
                                 && playback.error.length === 0
                        onClicked: {
                            if (media.playbackState === MediaPlayer.PlayingState)
                                playback.togglePlayback()
                            else
                                window.playSelected()
                        }
                    }
                    Button {
                        text: "Стоп"
                        enabled: media.hasAudio || media.source.toString().length > 0
                                 || playback.currentLoopbackIndex >= 0 || playback.cloudMailActive
                        onClicked: playback.stop()
                    }
                    Label {
                        Layout.fillWidth: true
                        text: playback.buffering ? "Буферизация…"
                              : media.playbackState === MediaPlayer.PlayingState ? "Воспроизведение"
                              : media.playbackState === MediaPlayer.PausedState ? "Пауза"
                              : "Остановлено"
                    }
                    Label {
                        text: media.duration > 0
                              ? window.time(seek.pressed ? seek.value : media.position)
                                + " / " + window.time(media.duration)
                              : "Длительность неизвестна"
                    }
                }
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
                    Label { text: "Громкость" }
                    Slider {
                        id: volume
                        Layout.fillWidth: true
                        from: 0; to: 1; value: playback.audio.volume
                        Accessible.name: "Громкость"
                        onMoved: playback.audio.volume = value
                    }
                    Label { text: Math.round(volume.value * 100) + "%" }
                    Label {
                        Layout.fillWidth: true
                        text: playback.error || playback.deviceWarning
                        color: palette.windowText
                        wrapMode: Text.WordWrap
                        Accessible.role: Accessible.AlertMessage
                    }
                }
            }
        }
    }

    Connections {
        target: mailWebDav
        function onEntriesChanged() { window.selectedEntry = -1 }
    }
    Component.onDestruction: mailWebDav.disconnectCloud()
}
