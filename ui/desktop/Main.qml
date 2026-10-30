import QtQuick
import QtQuick.Window
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia

Window {
    id: window
    width: 900
    height: 1020
    minimumWidth: 540
    minimumHeight: 760
    visible: true
    title: "AllMusic Player — этап 002-C"
    color: "#f5f6f8"
    readonly property var media: playback.player
    function time(ms) {
        const seconds = Math.floor(Math.max(0, ms) / 1000)
        return Math.floor(seconds / 60) + ":" + (seconds % 60).toString().padStart(2, "0")
    }
    FileDialog {
        id: picker
        title: "Выберите локальный тестовый аудиофайл"
        nameFilters: ["Аудио (*.mp3 *.flac *.aac *.m4a *.mp4)", "Все файлы (*)"]
        onAccepted: playback.openLocal(selectedFile)
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 16
        Label { text: "AllMusic Player"; font.pixelSize: 28; font.bold: true }
        Label { text: "Desktop-плеер: локальные файлы, loopback-мок и потоковый Cloud Mail"; color: "#526171" }
        Button { text: "Выбрать аудиофайл…"; onClicked: picker.open() }
        GroupBox {
            Layout.fillWidth: true
            title: "Локальный тестовый WebDAV"
            RowLayout {
                anchors.fill: parent
                TextField {
                    id: webdavPort
                    Layout.preferredWidth: 100
                    text: "8765"
                    validator: IntValidator { bottom: 1; top: 65535 }
                    Accessible.name: "Порт loopback WebDAV"
                }
                Button { text: "Найти файлы"; onClicked: playback.scanLoopback(Number(webdavPort.text)) }
                ComboBox {
                    id: remoteFile
                    Layout.fillWidth: true
                    model: playback.loopbackFiles
                    Accessible.name: "Файл из /AllMusic"
                    onActivated: {
                        if (playback.openLoopback(currentIndex)) playback.togglePlayback()
                    }
                }
            }
        }
        Label {
            Layout.fillWidth: true
            text: "Локальная потоковая функция обращается только к 127.0.0.1 и внутри /AllMusic. Range 206 обязателен; 200 отображается как ошибка, файл целиком не скачивается."
            color: "#526171"
            wrapMode: Text.WordWrap
        }
        Label { text: "Получено по Range: " + playback.networkBytes + " байт"; color: "#526171" }
        GroupBox {
            Layout.fillWidth: true
            title: "Cloud Mail — потоковое чтение"
            ColumnLayout {
                anchors.fill: parent
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    text: "Используются только PROPFIND и ограниченные GET Range внутри /AllMusic. Перенаправления и команды записи не выполняются. Выбор строки не отправляет запрос: нажмите отдельную кнопку воспроизведения."
                    color: "#526171"
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    text: "Важно: пароль внешнего приложения Mail может давать права записи на стороне аккаунта. Ограничение методов действует только в этом приложении. Не вводите основной пароль почты."
                    color: "#8a4b00"
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    text: "Адрес и пароль не сохраняются в настройках. Поля очищаются после нажатия, пароль остаётся только во временной памяти сессии до отключения."
                    color: "#526171"
                    wrapMode: Text.WordWrap
                }
                RowLayout {
                    Layout.fillWidth: true
                    TextField {
                        id: mailEmail
                        Layout.fillWidth: true
                        placeholderText: "Адрес почты Mail"
                        maximumLength: 320
                        enabled: !mailWebDav.busy && !mailWebDav.connected
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
                        enabled: !mailWebDav.busy && !mailWebDav.connected
                        Accessible.name: "Пароль внешнего приложения Mail"
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        text: "Проверить подключение"
                        enabled: !mailWebDav.busy && !mailWebDav.connected
                                 && mailEmail.text.length > 0 && mailAppPassword.text.length > 0
                        onClicked: {
                            mailWebDav.checkConnection(mailEmail.text, mailAppPassword.text)
                            mailEmail.text = ""
                            mailAppPassword.text = ""
                        }
                    }
                    Button {
                        text: "Отменить"
                        enabled: mailWebDav.busy
                        onClicked: mailWebDav.cancel()
                    }
                    Button {
                        text: "Отключиться"
                        enabled: mailWebDav.connected || mailWebDav.busy
                        onClicked: {
                            mailWebDav.disconnectCloud()
                            mailEmail.text = ""
                            mailAppPassword.text = ""
                            rangeConsent.checked = false
                        }
                    }
                    Label {
                        text: mailWebDav.entryCount + " элементов"
                        color: "#526171"
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: "Текущая папка: " + mailWebDav.currentDirectory
                    color: "#526171"
                    elide: Text.ElideMiddle
                }
                RowLayout {
                    Layout.fillWidth: true
                    ComboBox {
                        id: mailEntry
                        Layout.fillWidth: true
                        model: mailWebDav.entries
                        textRole: "label"
                        enabled: mailWebDav.connected && !mailWebDav.busy
                        Accessible.name: "Непосредственные папки и аудиофайлы в /AllMusic"
                    }
                    Button {
                        text: "Открыть папку"
                        enabled: mailWebDav.canOpenDirectory(mailEntry.currentIndex)
                        onClicked: mailWebDav.openDirectory(mailEntry.currentIndex)
                    }
                    Button {
                        text: "Наверх"
                        enabled: mailWebDav.canGoToParent()
                        onClicked: mailWebDav.goToParent()
                    }
                    Button {
                        text: "Воспроизвести MP3/FLAC"
                        enabled: mailWebDav.canPlay(mailEntry.currentIndex)
                        onClicked: mailWebDav.play(mailEntry.currentIndex)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    CheckBox {
                        id: rangeConsent
                        text: "Разрешаю отдельную диагностическую проверку Range до 64 КиБ"
                        enabled: mailWebDav.canCheckRange(mailEntry.currentIndex)
                        onCheckedChanged: if (!enabled) checked = false
                        Accessible.name: "Подтверждение отдельной проверки HTTP Range"
                    }
                    Button {
                        text: "Проверить Range"
                        enabled: rangeConsent.checked
                                 && mailWebDav.canCheckRange(mailEntry.currentIndex)
                        onClicked: {
                            mailWebDav.checkRange(mailEntry.currentIndex)
                            rangeConsent.checked = false
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: mailWebDav.status
                    color: "#34445a"
                    wrapMode: Text.WordWrap
                    Accessible.role: Accessible.AlertMessage
                }
                Label {
                    Layout.fillWidth: true
                    text: "Создание пароля внешнего приложения и описание Cloud Mail WebDAV: https://help.mail.ru/cloud/desktop/webdav/"
                    color: "#526171"
                    wrapMode: Text.WordWrap
                }
            }
        }
        Label {
            Layout.fillWidth: true
            text: playback.activeSourceLabel || window.media.source.toString()
                  || remoteFile.currentText || "Файл ещё не выбран"
            elide: Text.ElideMiddle
            Accessible.name: "Выбранный файл"
        }
        RowLayout {
            Button {
                text: window.media.playbackState === MediaPlayer.PlayingState ? "Пауза" : "Воспроизвести"
                enabled: (window.media.hasAudio || window.media.source.toString().length > 0
                          || playback.currentLoopbackIndex >= 0 || playback.cloudMailActive
                          || (mailWebDav.connected && mailWebDav.canPlay(mailEntry.currentIndex)))
                         && playback.error.length === 0
                onClicked: {
                    if (window.media.playbackState !== MediaPlayer.StoppedState) {
                        playback.togglePlayback()
                    } else if (window.media.source.toString().length === 0
                               && playback.currentLoopbackIndex < 0
                               && mailWebDav.connected
                               && mailWebDav.canPlay(mailEntry.currentIndex)) {
                        // Stop cancels and destroys the old credential-bearing source.
                        // A new explicit Play starts a fresh bounded stream for this row.
                        mailWebDav.play(mailEntry.currentIndex)
                    } else {
                        playback.togglePlayback()
                    }
                }
            }
            Button {
                text: "Стоп"
                enabled: window.media.hasAudio || window.media.source.toString().length > 0
                         || playback.currentLoopbackIndex >= 0 || playback.cloudMailActive
                onClicked: playback.stop()
            }
            Label {
                text: window.media.playbackState === MediaPlayer.PlayingState ? "Воспроизведение"
                    : window.media.playbackState === MediaPlayer.PausedState ? "Пауза"
                    : playback.buffering ? "Буферизация…"
                    : window.media.mediaStatus === MediaPlayer.LoadingMedia ? "Загрузка…" : "Остановлено"
            }
        }
        Slider {
            id: seek
            Layout.fillWidth: true
            from: 0
            to: Math.max(1, window.media.duration)
            value: window.media.position
            enabled: window.media.seekable && window.media.duration > 0
            Accessible.name: "Позиция воспроизведения"
            onMoved: window.media.setPosition(value)
        }
        Label { text: window.time(seek.pressed ? seek.value : window.media.position) + " / " + window.time(window.media.duration) }
        RowLayout {
            Label { text: "Громкость" }
            Slider {
                id: volume
                Layout.fillWidth: true
                from: 0; to: 1; value: 0.25
                Accessible.name: "Громкость"
                onMoved: playback.audio.volume = value
            }
            Label { text: Math.round(volume.value * 100) + "%" }
        }
        Label {
            Layout.fillWidth: true
            text: playback.error || playback.deviceWarning
            color: "#a52020"
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.AlertMessage
        }
        Item { Layout.fillHeight: true }
    }
    Component.onDestruction: mailWebDav.disconnectCloud()
}
