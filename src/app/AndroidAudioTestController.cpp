#include "AndroidAudioTestController.h"

#include <QAudioDevice>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

#include <atomic>
#include <cstdio>

namespace {
const QString kToneResource = QStringLiteral(":/audio-test/android-test-tone.wav");
const QString kMp3Resource = QStringLiteral(":/audio-test/android-spike.mp3");
const QString kFlacResource = QStringLiteral(":/audio-test/android-spike.flac");

std::atomic_bool qtFfmpegVersionLogSeen{false};
QtMessageHandler previousMessageHandler = nullptr;

void captureQtFfmpegMessage(QtMsgType type, const QMessageLogContext &context,
                            const QString &message)
{
    const QString category = QString::fromLatin1(context.category ? context.category : "");
    if (category.startsWith(QLatin1String("qt.multimedia.ffmpeg"), Qt::CaseInsensitive)
        && message.contains(QLatin1String("FFmpeg version"), Qt::CaseInsensitive)) {
        qtFfmpegVersionLogSeen.store(true, std::memory_order_release);
    }

    if (previousMessageHandler) {
        previousMessageHandler(type, context, message);
        return;
    }

    // Keep Qt's log visible if no handler was installed by the platform plugin.
    const QByteArray formatted = qFormatLogMessage(type, context, message).toLocal8Bit();
    std::fprintf(stderr, "%s\n", formatted.constData());
}
} // namespace

void installAndroidQtFfmpegLogProbe()
{
    QLoggingCategory::setFilterRules(QStringLiteral("qt.multimedia.ffmpeg.info=true"));
    previousMessageHandler = qInstallMessageHandler(captureQtFfmpegMessage);
}

AndroidAudioTestController::AndroidAudioTestController(QObject *parent)
    : QObject(parent), m_mp3Buffer(&m_mp3Data), m_flacBuffer(&m_flacData)
{
    m_audioOutput.setVolume(0.5);
    m_player.setAudioOutput(&m_audioOutput);
    connect(&m_mediaDevices, &QMediaDevices::audioOutputsChanged,
            this, &AndroidAudioTestController::changed);

    connect(&m_player, &QMediaPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState state) {
        if (state == QMediaPlayer::PlayingState) {
            setState(AndroidAudioTestState::Value::Playing);
        } else if (state == QMediaPlayer::PausedState) {
            setState(AndroidAudioTestState::Value::Paused);
        } else if (m_player.mediaStatus() == QMediaPlayer::EndOfMedia) {
            setState(AndroidAudioTestState::Value::Finished);
        } else if (m_state != AndroidAudioTestState::Value::Error
                   && m_state != AndroidAudioTestState::Value::Preparing
                   && m_state != AndroidAudioTestState::Value::Ready) {
            setState(AndroidAudioTestState::Value::Stopped);
        }
    });
    connect(&m_player, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia) {
            setState(AndroidAudioTestState::Value::Finished);
        } else if (status == QMediaPlayer::InvalidMedia) {
            const QString backendError = m_player.errorString();
            setError(backendError.isEmpty()
                         ? tr("Аудиодвижок не смог прочитать выбранный тестовый файл.")
                         : backendError);
        } else if ((status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia)
                   && m_state == AndroidAudioTestState::Value::Preparing) {
            setState(AndroidAudioTestState::Value::Ready);
        }
        emit changed();
    });
    connect(&m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &message) {
        setError(message.isEmpty()
                     ? tr("Ошибка Qt Multimedia при воспроизведении тестового файла.")
                     : message);
    });
    connect(&m_player, &QMediaPlayer::positionChanged, this,
            [this](qint64) { emit changed(); });
    connect(&m_player, &QMediaPlayer::durationChanged, this,
            [this](qint64) { emit changed(); });
    connect(&m_player, &QMediaPlayer::seekableChanged, this,
            [this](bool) { emit changed(); });

    QFile mp3Resource(kMp3Resource);
    QFile flacResource(kFlacResource);
    if (mp3Resource.open(QIODevice::ReadOnly))
        m_mp3Data = mp3Resource.readAll();
    if (flacResource.open(QIODevice::ReadOnly))
        m_flacData = flacResource.readAll();
    if (!m_mp3Data.isEmpty())
        m_mp3Buffer.open(QIODevice::ReadOnly);
    if (!m_flacData.isEmpty())
        m_flacBuffer.open(QIODevice::ReadOnly);

    m_ffmpegLogDetected = qtFfmpegVersionLogSeen.load(std::memory_order_acquire);
    if (!m_ffmpegLogDetected) {
        m_backendProbeTimer.setInterval(200);
        connect(&m_backendProbeTimer, &QTimer::timeout,
                this, &AndroidAudioTestController::refreshBackendStatus);
        m_backendProbeTimer.start();
    }

    if (loadTone())
        setState(AndroidAudioTestState::Value::Ready);
}

QString AndroidAudioTestController::stateText() const
{
    switch (m_state) {
    case AndroidAudioTestState::Value::Preparing: return tr("Подготовка");
    case AndroidAudioTestState::Value::Ready: return tr("Готово к запуску");
    case AndroidAudioTestState::Value::Loading: return tr("Запуск");
    case AndroidAudioTestState::Value::Playing: return tr("Воспроизведение");
    case AndroidAudioTestState::Value::Paused: return tr("Пауза");
    case AndroidAudioTestState::Value::Stopped: return tr("Остановлено");
    case AndroidAudioTestState::Value::Finished: return tr("Файл завершён");
    case AndroidAudioTestState::Value::Error: return tr("Ошибка");
    }
    return tr("Неизвестно");
}

QString AndroidAudioTestController::mediaStatusText() const
{
    switch (m_player.mediaStatus()) {
    case QMediaPlayer::NoMedia: return tr("Нет источника");
    case QMediaPlayer::LoadingMedia: return tr("Загрузка тестового файла");
    case QMediaPlayer::LoadedMedia: return tr("Тестовый файл загружен");
    case QMediaPlayer::StalledMedia: return tr("Аудиодвижок остановился во время чтения");
    case QMediaPlayer::BufferingMedia: return tr("Подготовка аудиовыхода");
    case QMediaPlayer::BufferedMedia: return tr("Тестовый файл подготовлен");
    case QMediaPlayer::EndOfMedia: return tr("Тестовый файл завершён");
    case QMediaPlayer::InvalidMedia: return tr("Источник недоступен");
    }
    return tr("Состояние источника неизвестно");
}

QString AndroidAudioTestController::outputStatusText() const
{
    return QMediaDevices::defaultAudioOutput().isNull()
        ? tr("Аудиовыход недоступен")
        : tr("Аудиовыход доступен");
}

QString AndroidAudioTestController::backendStatusText() const
{
    return m_ffmpegLogDetected
        ? tr("Qt FFmpeg подтверждён runtime-сообщением")
        : tr("Ожидание сообщения Qt FFmpeg; Android media plugin исключён из сборки");
}

QString AndroidAudioTestController::trackText() const
{
    switch (m_trackIndex) {
    case 0: return tr("WAV — регрессионный сигнал 002-A");
    case 1: return tr("MP3 — синтетический QBuffer");
    case 2: return tr("FLAC — синтетический QBuffer");
    }
    return tr("Тестовый файл");
}

QString AndroidAudioTestController::sourceDeviceText() const
{
    if (m_trackIndex == 1) {
        return m_mp3Buffer.isOpen() && !m_mp3Buffer.isSequential()
            ? tr("QBuffer открыт для чтения; источник seekable")
            : tr("QBuffer MP3 недоступен");
    }
    if (m_trackIndex == 2) {
        return m_flacBuffer.isOpen() && !m_flacBuffer.isSequential()
            ? tr("QBuffer открыт для чтения; источник seekable")
            : tr("QBuffer FLAC недоступен");
    }
    return tr("Встроенный WAV через локальную копию 002-A");
}

qint64 AndroidAudioTestController::position() const
{
    return m_player.position();
}

qint64 AndroidAudioTestController::duration() const
{
    return m_player.duration();
}

bool AndroidAudioTestController::seekEnabled() const
{
    return m_player.isSeekable() && m_player.duration() > 0;
}

QString AndroidAudioTestController::errorText() const
{
    return m_error;
}

bool AndroidAudioTestController::playEnabled() const
{
    return AndroidAudioTestState::actions(m_state).play;
}

bool AndroidAudioTestController::pauseEnabled() const
{
    return AndroidAudioTestState::actions(m_state).pause;
}

bool AndroidAudioTestController::stopEnabled() const
{
    return AndroidAudioTestState::actions(m_state).stop;
}

bool AndroidAudioTestController::restartEnabled() const
{
    return AndroidAudioTestState::actions(m_state).restart;
}

void AndroidAudioTestController::selectTrack(int index)
{
    if (index < 0 || index > 2 || index == m_trackIndex)
        return;

    m_player.stop();
    m_player.setSource(QUrl());
    m_trackIndex = index;
    m_error.clear();
    setState(AndroidAudioTestState::Value::Preparing);

    const bool loaded = index == 0 ? loadTone() : loadCompressedFixture();
    if (loaded)
        setState(AndroidAudioTestState::Value::Ready);
    emit changed();
}

void AndroidAudioTestController::play()
{
    if (!playEnabled())
        return;
    if (m_state == AndroidAudioTestState::Value::Error) {
        const bool loaded = m_trackIndex == 0 ? loadTone() : loadCompressedFixture();
        if (!loaded)
            return;
    }
    if (m_state == AndroidAudioTestState::Value::Stopped
        || m_state == AndroidAudioTestState::Value::Finished)
        m_player.setPosition(0);
    m_error.clear();
    setState(AndroidAudioTestState::Value::Loading);
    m_player.play();
}

void AndroidAudioTestController::pause()
{
    if (!pauseEnabled())
        return;
    m_player.pause();
    setState(AndroidAudioTestState::Value::Paused);
}

void AndroidAudioTestController::stop()
{
    if (!stopEnabled())
        return;
    m_player.stop();
    m_player.setPosition(0);
    setState(AndroidAudioTestState::Value::Stopped);
}

void AndroidAudioTestController::restart()
{
    if (!restartEnabled())
        return;
    if (m_state == AndroidAudioTestState::Value::Error) {
        const bool loaded = m_trackIndex == 0 ? loadTone() : loadCompressedFixture();
        if (!loaded)
            return;
    }
    m_error.clear();
    m_player.setPosition(0);
    setState(AndroidAudioTestState::Value::Loading);
    m_player.play();
}

void AndroidAudioTestController::seekTo(qint64 positionMs)
{
    if (!seekEnabled())
        return;
    m_player.setPosition(qBound<qint64>(0, positionMs, m_player.duration()));
}

bool AndroidAudioTestController::loadTone()
{
    m_player.stop();
    m_player.setSource(QUrl());
    QFile resource(kToneResource);
    if (!resource.open(QIODevice::ReadOnly)) {
        setError(tr("Не удалось открыть встроенный регрессионный сигнал 002-A."));
        return false;
    }

    const QString cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cachePath.isEmpty() || !QDir().mkpath(cachePath)) {
        setError(tr("Не удалось подготовить локальный кэш приложения для сигнала."));
        return false;
    }
    m_tonePath = QDir(cachePath).filePath(QStringLiteral("android-test-tone.wav"));
    QSaveFile localCopy(m_tonePath);
    if (!localCopy.open(QIODevice::WriteOnly)) {
        setError(tr("Не удалось создать локальную копию тестового сигнала."));
        return false;
    }
    const QByteArray tone = resource.readAll();
    if (tone.isEmpty() || localCopy.write(tone) != tone.size() || !localCopy.commit()) {
        setError(tr("Не удалось записать локальную копию тестового сигнала."));
        return false;
    }

    m_player.setSource(QUrl::fromLocalFile(m_tonePath));
    m_error.clear();
    return true;
}

bool AndroidAudioTestController::loadCompressedFixture()
{
    QBuffer *device = nullptr;
    QString suffix;
    if (m_trackIndex == 1) {
        device = &m_mp3Buffer;
        suffix = QStringLiteral("mp3");
    } else if (m_trackIndex == 2) {
        device = &m_flacBuffer;
        suffix = QStringLiteral("flac");
    }
    if (!device || !device->isOpen() || !device->isReadable() || device->isSequential()) {
        setError(tr("Синтетический файл недоступен как seekable QIODevice."));
        return false;
    }
    if (!device->seek(0)) {
        setError(tr("Не удалось переместить QIODevice в начало тестового файла."));
        return false;
    }

    // The URL is a format hint only. The bytes are read from the open QBuffer above.
    const QUrl formatHint(QStringLiteral("file:///synthetic/android-spike.%1").arg(suffix));
    m_player.setSourceDevice(device, formatHint);
    m_error.clear();
    return true;
}

void AndroidAudioTestController::refreshBackendStatus()
{
    if (m_ffmpegLogDetected
        || !qtFfmpegVersionLogSeen.load(std::memory_order_acquire))
        return;
    m_ffmpegLogDetected = true;
    m_backendProbeTimer.stop();
    emit changed();
}

void AndroidAudioTestController::setState(AndroidAudioTestState::Value state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit changed();
}

void AndroidAudioTestController::setError(const QString &message)
{
    m_error = message.trimmed().isEmpty()
        ? tr("Неизвестная ошибка аудиодвижка.")
        : message.trimmed();
    setState(AndroidAudioTestState::Value::Error);
}
