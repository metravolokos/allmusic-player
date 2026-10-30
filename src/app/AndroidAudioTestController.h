#pragma once

#include "AndroidAudioTestState.h"

#include <QObject>
#include <QAudioOutput>
#include <QBuffer>
#include <QByteArray>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QTimer>

class AndroidAudioTestController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString stateText READ stateText NOTIFY changed)
    Q_PROPERTY(QString mediaStatusText READ mediaStatusText NOTIFY changed)
    Q_PROPERTY(QString outputStatusText READ outputStatusText NOTIFY changed)
    Q_PROPERTY(QString backendStatusText READ backendStatusText NOTIFY changed)
    Q_PROPERTY(QString trackText READ trackText NOTIFY changed)
    Q_PROPERTY(QString sourceDeviceText READ sourceDeviceText NOTIFY changed)
    Q_PROPERTY(qint64 position READ position NOTIFY changed)
    Q_PROPERTY(qint64 duration READ duration NOTIFY changed)
    Q_PROPERTY(bool seekEnabled READ seekEnabled NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
    Q_PROPERTY(bool playEnabled READ playEnabled NOTIFY changed)
    Q_PROPERTY(bool pauseEnabled READ pauseEnabled NOTIFY changed)
    Q_PROPERTY(bool stopEnabled READ stopEnabled NOTIFY changed)
    Q_PROPERTY(bool restartEnabled READ restartEnabled NOTIFY changed)

public:
    explicit AndroidAudioTestController(QObject *parent = nullptr);

    QString stateText() const;
    QString mediaStatusText() const;
    QString outputStatusText() const;
    QString backendStatusText() const;
    QString trackText() const;
    QString sourceDeviceText() const;
    qint64 position() const;
    qint64 duration() const;
    bool seekEnabled() const;
    QString errorText() const;
    bool playEnabled() const;
    bool pauseEnabled() const;
    bool stopEnabled() const;
    bool restartEnabled() const;

public slots:
    void selectTrack(int index);
    void play();
    void pause();
    void stop();
    void restart();
    void seekTo(qint64 positionMs);

signals:
    void changed();

private:
    bool loadTone();
    bool loadCompressedFixture();
    void refreshBackendStatus();
    void setState(AndroidAudioTestState::Value state);
    void setError(const QString &message);

    QAudioOutput m_audioOutput;
    QMediaDevices m_mediaDevices;
    QMediaPlayer m_player;
    QByteArray m_mp3Data;
    QBuffer m_mp3Buffer;
    QByteArray m_flacData;
    QBuffer m_flacBuffer;
    QTimer m_backendProbeTimer;
    QString m_tonePath;
    int m_trackIndex = 0;
    bool m_ffmpegLogDetected = false;
    AndroidAudioTestState::Value m_state = AndroidAudioTestState::Value::Preparing;
    QString m_error;
};

void installAndroidQtFfmpegLogProbe();
