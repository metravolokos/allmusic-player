import QtQuick
import QtQuick.Window
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia

Window {
    id: window
    width: 760
    height: 480
    minimumWidth: 540
    minimumHeight: 420
    visible: true
    title: "AllMusic Player — локальный прототип 001-B1"
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
        Label { text: "Локальный прототип 001-B1"; color: "#526171" }
        Button { text: "Выбрать аудиофайл…"; onClicked: picker.open() }
        Label {
            Layout.fillWidth: true
            text: window.media.source.toString() || "Файл ещё не выбран"
            elide: Text.ElideMiddle
            Accessible.name: "Выбранный файл"
        }
        RowLayout {
            Button {
                text: window.media.playbackState === MediaPlayer.PlayingState ? "Пауза" : "Воспроизвести"
                enabled: window.media.hasAudio && playback.error.length === 0
                onClicked: window.media.playbackState === MediaPlayer.PlayingState ? window.media.pause() : window.media.play()
            }
            Button { text: "Стоп"; enabled: window.media.hasAudio; onClicked: window.media.stop() }
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
