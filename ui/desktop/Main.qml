import QtQuick
import QtQuick.Window
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia

Window {
    id: window
    width: 800
    height: 680
    minimumWidth: 540
    minimumHeight: 600
    visible: true
    title: "AllMusic Player — прототип 001-B2"
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
        Label { text: "Потоковый прототип через локальный WebDAV · 001-B2"; color: "#526171" }
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
            text: "Сеть разрешена только к 127.0.0.1 и внутри /AllMusic. Range 206 обязателен; 200 отображается как ошибка, файл целиком не скачивается."
            color: "#526171"
            wrapMode: Text.WordWrap
        }
        Label { text: "Получено по Range: " + playback.networkBytes + " байт"; color: "#526171" }
        Label {
            Layout.fillWidth: true
            text: window.media.source.toString() || remoteFile.currentText || "Файл ещё не выбран"
            elide: Text.ElideMiddle
            Accessible.name: "Выбранный файл"
        }
        RowLayout {
            Button {
                text: window.media.playbackState === MediaPlayer.PlayingState ? "Пауза" : "Воспроизвести"
                enabled: (window.media.hasAudio || window.media.source.toString().length > 0
                          || playback.currentLoopbackIndex >= 0) && playback.error.length === 0
                onClicked: playback.togglePlayback()
            }
            Button {
                text: "Стоп"
                enabled: window.media.hasAudio || window.media.source.toString().length > 0
                         || playback.currentLoopbackIndex >= 0
                onClicked: playback.stop()
            }
            Label {
                text: window.media.playbackState === MediaPlayer.PlayingState ? "Воспроизведение"
                    : window.media.playbackState === MediaPlayer.PausedState ? "Пауза"
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
}
