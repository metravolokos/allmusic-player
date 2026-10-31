#include "AndroidAudioTestController.h"

#include <QAudioBuffer>
#include <QAudioDevice>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
const QString kToneResource = QStringLiteral(":/audio-test/android-test-tone.wav");
const QString kMp3Resource = QStringLiteral(":/audio-test/android-spike.mp3");
const QString kFlacResource = QStringLiteral(":/audio-test/android-spike.flac");
constexpr double kPcmNonZeroThreshold = 0.00001;
constexpr int kSyntheticToneDurationMs = 750;
constexpr int kSyntheticToneFrequencyHz = 440;
constexpr int kSyntheticToneAmplitudePercent = 25;
constexpr int kSyntheticOutputTimeoutMs = 3000;
constexpr double kPi = 3.14159265358979323846;

std::atomic_bool qtFfmpegVersionLogSeen{false};
QtMessageHandler previousMessageHandler = nullptr;

QString deviceDescription(const QAudioDevice &device)
{
    return device.isNull() ? QObject::tr("не выбрано")
                           : (device.description().isEmpty()
                                  ? QObject::tr("устройство без названия")
                                  : device.description());
}

QString sampleFormatName(QAudioFormat::SampleFormat format)
{
    switch (format) {
    case QAudioFormat::UInt8: return QStringLiteral("UInt8");
    case QAudioFormat::Int16: return QStringLiteral("Int16");
    case QAudioFormat::Int32: return QStringLiteral("Int32");
    case QAudioFormat::Float: return QStringLiteral("Float");
    case QAudioFormat::Unknown:
    case QAudioFormat::NSampleFormats:
        return QStringLiteral("Unknown");
    }
    return QStringLiteral("Unknown");
}

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
    : QObject(parent),
      m_mp3Buffer(&m_mp3Data),
      m_flacBuffer(&m_flacData),
      m_syntheticOutputBuffer(&m_syntheticOutputPcm)
{
    m_audioOutput.setVolume(0.5f);
    m_player.setAudioOutput(&m_audioOutput);
    m_player.setAudioBufferOutput(&m_audioBufferOutput);

    connect(&m_mediaDevices, &QMediaDevices::audioOutputsChanged,
            this, &AndroidAudioTestController::refreshAudioOutputDevice);
    connect(&m_audioOutput, &QAudioOutput::deviceChanged,
            this, &AndroidAudioTestController::changed);
    connect(&m_audioOutput, &QAudioOutput::volumeChanged,
            this, &AndroidAudioTestController::changed);
    connect(&m_audioOutput, &QAudioOutput::mutedChanged,
            this, &AndroidAudioTestController::changed);
    connect(&m_audioBufferOutput, &QAudioBufferOutput::audioBufferReceived,
            this, &AndroidAudioTestController::collectAudioBuffer);

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
        emit changed();
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
    connect(&m_player, &QMediaPlayer::hasAudioChanged, this,
            [this](bool) { emit changed(); });
    connect(&m_player, &QMediaPlayer::tracksChanged,
            this, &AndroidAudioTestController::changed);
    connect(&m_player, &QMediaPlayer::activeTracksChanged,
            this, &AndroidAudioTestController::changed);
    connect(&m_player, &QMediaPlayer::errorChanged,
            this, &AndroidAudioTestController::changed);

    m_syntheticOutputTimeout.setSingleShot(true);
    connect(&m_syntheticOutputTimeout, &QTimer::timeout, this, [this] {
        if (!m_syntheticOutputSink
            || m_syntheticOutputSink->state() != QAudio::ActiveState) {
            return;
        }
        m_syntheticOutputTimedOut = true;
        m_syntheticOutputStatus = tr("Тайм-аут: QAudioSink оставался Active дольше 3 с; "
                                     "ошибка %1.")
                                     .arg(static_cast<int>(m_syntheticOutputSink->error()));
        emit changed();
        m_syntheticOutputSink->stop();
    });

    refreshAudioOutputDevice();

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

AndroidAudioTestController::~AndroidAudioTestController()
{
    m_syntheticOutputTimeout.stop();
    if (m_syntheticOutputSink) {
        m_syntheticOutputSink->stop();
        delete m_syntheticOutputSink;
        m_syntheticOutputSink = nullptr;
    }
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

QString AndroidAudioTestController::playerAudioStatusText() const
{
    const int activeIndex = m_player.activeAudioTrack();
    const QString activeTrack = activeIndex < 0
        ? tr("нет")
        : QString::number(activeIndex + 1);
    const QString trackCount = QString::number(
        static_cast<qlonglong>(m_player.audioTracks().size()));
    const QString error = m_player.error() == QMediaPlayer::NoError
        ? tr("нет")
        : QStringLiteral("%1 (%2)")
              .arg(static_cast<int>(m_player.error()))
              .arg(m_player.errorString());
    const QString playbackState = m_player.playbackState() == QMediaPlayer::PlayingState
        ? tr("воспроизведение")
        : (m_player.playbackState() == QMediaPlayer::PausedState ? tr("пауза") : tr("стоп"));

    return tr("QMediaPlayer: hasAudio=%1; активная дорожка %2 из %3; состояние %4; ошибка %5")
        .arg(m_player.hasAudio() ? tr("да") : tr("нет"))
        .arg(activeTrack)
        .arg(trackCount)
        .arg(playbackState)
        .arg(error);
}

QString AndroidAudioTestController::outputStatusText() const
{
    const QAudioDevice selected = m_audioOutput.device();
    const QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();
    if (selected.isNull() && defaultDevice.isNull())
        return tr("QAudioOutput: аудиовыход недоступен");

    return tr("QAudioOutput: выбрано «%1»; системный default «%2»; громкость %3%; mute %4")
        .arg(deviceDescription(selected))
        .arg(deviceDescription(defaultDevice))
        .arg(qRound(m_audioOutput.volume() * 100.0f))
        .arg(m_audioOutput.isMuted() ? tr("да") : tr("нет"));
}

QString AndroidAudioTestController::pcmStatusText() const
{
    if (m_pcmBufferCount == 0)
        return tr("Qt FFmpeg PCM: пока нет аудиобуферов; выбор формата сбрасывает счётчики.");

    const double rms = m_pcmSampleCount == 0
        ? 0.0
        : std::sqrt(m_pcmSumSquares / static_cast<double>(m_pcmSampleCount));
    return tr("Qt FFmpeg PCM: буферов %1; кадров %2; samples %3; ненулевых %4; "
              "peak %5; RMS %6; %7 Гц, %8 кан., %9")
        .arg(static_cast<qlonglong>(m_pcmBufferCount))
        .arg(static_cast<qlonglong>(m_pcmFrameCount))
        .arg(static_cast<qlonglong>(m_pcmSampleCount))
        .arg(static_cast<qlonglong>(m_pcmNonZeroSampleCount))
        .arg(m_pcmPeak, 0, 'f', 4)
        .arg(rms, 0, 'f', 4)
        .arg(m_pcmSampleRate)
        .arg(m_pcmChannelCount)
        .arg(sampleFormatName(m_pcmSampleFormat));
}

QString AndroidAudioTestController::syntheticOutputStatusText() const
{
    return m_syntheticOutputStatus.isEmpty()
        ? tr("QAudioSink: короткий тест ещё не запускался.")
        : m_syntheticOutputStatus;
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

int AndroidAudioTestController::selectedTrackIndex() const
{
    return m_trackIndex;
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

bool AndroidAudioTestController::syntheticOutputTestEnabled() const
{
    return m_player.playbackState() == QMediaPlayer::StoppedState;
}

void AndroidAudioTestController::selectTrack(int index)
{
    if (!AndroidAudioTestState::isValidTrackIndex(index) || index == m_trackIndex)
        return;

    m_player.stop();
    m_player.setSource(QUrl());
    m_trackIndex = index;
    resetPcmDiagnostics();
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
        || m_state == AndroidAudioTestState::Value::Finished) {
        m_player.setPosition(0);
    }
    if (m_state != AndroidAudioTestState::Value::Paused)
        resetPcmDiagnostics();
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
    resetPcmDiagnostics();
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

void AndroidAudioTestController::startSyntheticOutputTest()
{
    if (!syntheticOutputTestEnabled()) {
        m_syntheticOutputStatus = tr("Для отдельного теста QAudioSink сначала остановите QMediaPlayer.");
        emit changed();
        return;
    }

    if (m_syntheticOutputSink
        && m_syntheticOutputSink->state() == QAudio::ActiveState) {
        m_syntheticOutputStatus = tr("QAudioSink уже передаёт короткий сигнал.");
        emit changed();
        return;
    }

    m_syntheticOutputTimeout.stop();
    if (m_syntheticOutputSink) {
        m_syntheticOutputSink->stop();
        delete m_syntheticOutputSink;
        m_syntheticOutputSink = nullptr;
    }
    m_syntheticOutputBuffer.close();
    m_syntheticOutputPcm.clear();

    QAudioDevice device = m_audioOutput.device();
    if (device.isNull())
        device = QMediaDevices::defaultAudioOutput();
    if (device.isNull()) {
        m_syntheticOutputStatus = tr("QAudioSink: не найдено устройство вывода.");
        emit changed();
        return;
    }

    const QAudioFormat format = device.preferredFormat();
    if (!format.isValid() || !createSyntheticOutputPcm(format)) {
        m_syntheticOutputStatus = tr("QAudioSink: неподдерживаемый PCM-формат устройства.");
        emit changed();
        return;
    }
    if (!m_syntheticOutputBuffer.open(QIODevice::ReadOnly)) {
        m_syntheticOutputStatus = tr("QAudioSink: не удалось открыть синтетический PCM-буфер.");
        emit changed();
        return;
    }

    m_syntheticOutputTimedOut = false;
    m_syntheticOutputSink = new QAudioSink(device, format, this);
    m_syntheticOutputSink->setVolume(0.5f);
    connect(m_syntheticOutputSink, &QAudioSink::stateChanged, this,
            [this](QAudio::State state) {
        updateSyntheticOutputTestStatus(state);
    });

    m_syntheticOutputStatus = tr("QAudioSink: запуск тона 440 Гц, 0.75 с, громкость 50%; "
                                 "устройство «%1», %2 Гц, %3 кан., %4.")
                                 .arg(deviceDescription(device))
                                 .arg(format.sampleRate())
                                 .arg(format.channelCount())
                                 .arg(sampleFormatName(format.sampleFormat()));
    m_syntheticOutputSink->start(&m_syntheticOutputBuffer);
    updateSyntheticOutputTestStatus(m_syntheticOutputSink->state());
    m_syntheticOutputTimeout.start(kSyntheticOutputTimeoutMs);
    emit changed();
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

bool AndroidAudioTestController::createSyntheticOutputPcm(const QAudioFormat &format)
{
    const int bytesPerSample = format.bytesPerSample();
    const int channelCount = format.channelCount();
    const qsizetype frameCount =
        (static_cast<qsizetype>(format.sampleRate()) * kSyntheticToneDurationMs) / 1000;
    const qsizetype sampleCount = frameCount * channelCount;
    const qsizetype byteCount = sampleCount * bytesPerSample;

    if (bytesPerSample <= 0 || channelCount <= 0 || channelCount > 8
        || format.sampleRate() < 8000 || format.sampleRate() > 192000
        || frameCount <= 0 || byteCount <= 0 || byteCount > 16 * 1024 * 1024) {
        return false;
    }
    if (format.sampleFormat() != QAudioFormat::UInt8
        && format.sampleFormat() != QAudioFormat::Int16
        && format.sampleFormat() != QAudioFormat::Int32
        && format.sampleFormat() != QAudioFormat::Float) {
        return false;
    }

    m_syntheticOutputPcm.resize(byteCount);
    char *pcm = m_syntheticOutputPcm.data();
    qsizetype sampleIndex = 0;

    for (qsizetype frame = 0; frame < frameCount; ++frame) {
        const double phase = 2.0 * kPi * kSyntheticToneFrequencyHz
            * static_cast<double>(frame) / format.sampleRate();
        const double value = std::sin(phase) * kSyntheticToneAmplitudePercent / 100.0;
        for (int channel = 0; channel < channelCount; ++channel, ++sampleIndex) {
            char *sample = pcm + sampleIndex * bytesPerSample;
            switch (format.sampleFormat()) {
            case QAudioFormat::UInt8: {
                const auto encoded = static_cast<quint8>(128 + std::lround(value * 127.0));
                std::memcpy(sample, &encoded, sizeof(encoded));
                break;
            }
            case QAudioFormat::Int16: {
                const auto encoded = static_cast<qint16>(std::lround(value * 32767.0));
                std::memcpy(sample, &encoded, sizeof(encoded));
                break;
            }
            case QAudioFormat::Int32: {
                const auto encoded = static_cast<qint32>(std::llround(value * 2147483647.0));
                std::memcpy(sample, &encoded, sizeof(encoded));
                break;
            }
            case QAudioFormat::Float: {
                const float encoded = static_cast<float>(value);
                std::memcpy(sample, &encoded, sizeof(encoded));
                break;
            }
            case QAudioFormat::Unknown:
            case QAudioFormat::NSampleFormats:
                return false;
            }
        }
    }
    return true;
}

void AndroidAudioTestController::collectAudioBuffer(const QAudioBuffer &buffer)
{
    if (!buffer.isValid())
        return;

    const QAudioFormat format = buffer.format();
    const int bytesPerSample = format.bytesPerSample();
    const qsizetype sampleCount = buffer.sampleCount();
    if (!format.isValid() || bytesPerSample <= 0 || sampleCount <= 0
        || buffer.byteCount() / bytesPerSample < sampleCount) {
        return;
    }

    const char *data = buffer.constData<char>();
    if (!data)
        return;

    ++m_pcmBufferCount;
    m_pcmFrameCount += buffer.frameCount();
    m_pcmSampleCount += sampleCount;
    m_pcmSampleRate = format.sampleRate();
    m_pcmChannelCount = format.channelCount();
    m_pcmSampleFormat = format.sampleFormat();

    for (qsizetype i = 0; i < sampleCount; ++i) {
        const double sample = format.normalizedSampleValue(data + i * bytesPerSample);
        const double magnitude = std::abs(sample);
        m_pcmPeak = std::max(m_pcmPeak, magnitude);
        m_pcmSumSquares += sample * sample;
        if (magnitude > kPcmNonZeroThreshold)
            ++m_pcmNonZeroSampleCount;
    }
    emit changed();
}

void AndroidAudioTestController::resetPcmDiagnostics()
{
    m_pcmBufferCount = 0;
    m_pcmFrameCount = 0;
    m_pcmSampleCount = 0;
    m_pcmNonZeroSampleCount = 0;
    m_pcmPeak = 0.0;
    m_pcmSumSquares = 0.0;
    m_pcmSampleRate = 0;
    m_pcmChannelCount = 0;
    m_pcmSampleFormat = QAudioFormat::Unknown;
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

void AndroidAudioTestController::refreshAudioOutputDevice()
{
    const QList<QAudioDevice> outputs = QMediaDevices::audioOutputs();
    if (!outputs.isEmpty()) {
        const QAudioDevice current = m_audioOutput.device();
        const bool currentStillAvailable = !current.isNull()
            && std::any_of(outputs.cbegin(), outputs.cend(),
                           [&current](const QAudioDevice &device) {
                return device.id() == current.id();
            });
        if (!currentStillAvailable)
            m_audioOutput.setDevice(QMediaDevices::defaultAudioOutput());
    }
    emit changed();
}

void AndroidAudioTestController::updateSyntheticOutputTestStatus(QAudio::State state)
{
    if (!m_syntheticOutputSink)
        return;

    if (m_syntheticOutputTimedOut && state == QAudio::StoppedState)
        return;

    const QAudio::Error error = m_syntheticOutputSink->error();
    const QString stateName = state == QAudio::ActiveState
        ? tr("Active")
        : (state == QAudio::IdleState ? tr("Idle")
           : (state == QAudio::SuspendedState ? tr("Suspended") : tr("Stopped")));
    if (error != QAudio::NoError) {
        m_syntheticOutputStatus =
            tr("QAudioSink %1; ошибка %2; processed %3 мкс.")
                .arg(stateName)
                .arg(static_cast<int>(error))
                .arg(m_syntheticOutputSink->processedUSecs());
    } else if (state == QAudio::ActiveState) {
        m_syntheticOutputStatus =
            tr("QAudioSink Active; поток передаётся; processed %1 мкс. "
               "Слышимость подтвердить на устройстве.")
                .arg(m_syntheticOutputSink->processedUSecs());
    } else if (state == QAudio::IdleState) {
        m_syntheticOutputTimeout.stop();
        m_syntheticOutputStatus =
            tr("QAudioSink Idle; PCM передан до конца; processed %1 мкс. "
               "Слышимость подтвердить на устройстве.")
                .arg(m_syntheticOutputSink->processedUSecs());
    } else if (state == QAudio::SuspendedState) {
        m_syntheticOutputStatus =
            tr("QAudioSink Suspended; processed %1 мкс.")
                .arg(m_syntheticOutputSink->processedUSecs());
    } else {
        m_syntheticOutputStatus =
            tr("QAudioSink Stopped; processed %1 мкс.")
                .arg(m_syntheticOutputSink->processedUSecs());
    }
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
