#include "AndroidAudioTestController.h"

#include <QAudioDevice>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QMediaDevices>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

namespace {
const QString kToneResource = QStringLiteral(":/audio-test/android-test-tone.wav");
}

AndroidAudioTestController::AndroidAudioTestController(QObject *parent)
    : QObject(parent)
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
                         ? tr("Аудиодвижок не смог прочитать тестовый сигнал.")
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
                     ? tr("Ошибка Qt Multimedia при воспроизведении тестового сигнала.")
                     : message);
    });

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
    case AndroidAudioTestState::Value::Finished: return tr("Сигнал завершён");
    case AndroidAudioTestState::Value::Error: return tr("Ошибка");
    }
    return tr("Неизвестно");
}

QString AndroidAudioTestController::mediaStatusText() const
{
    switch (m_player.mediaStatus()) {
    case QMediaPlayer::NoMedia: return tr("Нет источника");
    case QMediaPlayer::LoadingMedia: return tr("Загрузка локального сигнала");
    case QMediaPlayer::LoadedMedia: return tr("Сигнал загружен");
    case QMediaPlayer::StalledMedia: return tr("Аудиодвижок остановился во время чтения");
    case QMediaPlayer::BufferingMedia: return tr("Подготовка аудиовыхода");
    case QMediaPlayer::BufferedMedia: return tr("Сигнал подготовлен");
    case QMediaPlayer::EndOfMedia: return tr("Сигнал завершён");
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

void AndroidAudioTestController::play()
{
    if (!playEnabled())
        return;
    if (m_state == AndroidAudioTestState::Value::Error && !loadTone())
        return;
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
    if (m_state == AndroidAudioTestState::Value::Error && !loadTone())
        return;
    m_error.clear();
    m_player.setPosition(0);
    setState(AndroidAudioTestState::Value::Loading);
    m_player.play();
}

bool AndroidAudioTestController::loadTone()
{
    m_player.stop();
    m_player.setSource(QUrl());
    QFile resource(kToneResource);
    if (!resource.open(QIODevice::ReadOnly)) {
        setError(tr("Не удалось открыть встроенный синтетический сигнал."));
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
