#pragma once

#include "AndroidAudioTestState.h"

#include <QAudioBufferOutput>
#include <QAudioOutput>
#include <QAudioSink>
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
    Q_PROPERTY(QString playerAudioStatusText READ playerAudioStatusText NOTIFY changed)
    Q_PROPERTY(QString outputStatusText READ outputStatusText NOTIFY changed)
    Q_PROPERTY(QString pcmStatusText READ pcmStatusText NOTIFY changed)
    Q_PROPERTY(QString syntheticOutputStatusText READ syntheticOutputStatusText NOTIFY changed)
    Q_PROPERTY(QString backendStatusText READ backendStatusText NOTIFY changed)
    Q_PROPERTY(QString trackText READ trackText NOTIFY changed)
    Q_PROPERTY(QString sourceDeviceText READ sourceDeviceText NOTIFY changed)
    Q_PROPERTY(int selectedTrackIndex READ selectedTrackIndex NOTIFY changed)
    Q_PROPERTY(qint64 position READ position NOTIFY changed)
    Q_PROPERTY(qint64 duration READ duration NOTIFY changed)
    Q_PROPERTY(bool seekEnabled READ seekEnabled NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
    Q_PROPERTY(bool playEnabled READ playEnabled NOTIFY changed)
    Q_PROPERTY(bool pauseEnabled READ pauseEnabled NOTIFY changed)
    Q_PROPERTY(bool stopEnabled READ stopEnabled NOTIFY changed)
    Q_PROPERTY(bool restartEnabled READ restartEnabled NOTIFY changed)
    Q_PROPERTY(bool syntheticOutputTestEnabled READ syntheticOutputTestEnabled NOTIFY changed)

public:
    explicit AndroidAudioTestController(QObject *parent = nullptr);
    ~AndroidAudioTestController() override;

    QString stateText() const;
    QString mediaStatusText() const;
    QString playerAudioStatusText() const;
    QString outputStatusText() const;
    QString pcmStatusText() const;
    QString syntheticOutputStatusText() const;
    QString backendStatusText() const;
    QString trackText() const;
    QString sourceDeviceText() const;
    int selectedTrackIndex() const;
    qint64 position() const;
    qint64 duration() const;
    bool seekEnabled() const;
    QString errorText() const;
    bool playEnabled() const;
    bool pauseEnabled() const;
    bool stopEnabled() const;
    bool restartEnabled() const;
    bool syntheticOutputTestEnabled() const;

public slots:
    void selectTrack(int index);
    void play();
    void pause();
    void stop();
    void restart();
    void seekTo(qint64 positionMs);
    void startSyntheticOutputTest();

signals:
    void changed();

private:
    bool loadTone();
    bool loadCompressedFixture();
    bool createSyntheticOutputPcm(const QAudioFormat &format);
    void collectAudioBuffer(const QAudioBuffer &buffer);
    void resetPcmDiagnostics();
    void refreshBackendStatus();
    void refreshAudioOutputDevice();
    void updateSyntheticOutputTestStatus(QAudio::State state);
    void setState(AndroidAudioTestState::Value state);
    void setError(const QString &message);

    QAudioOutput m_audioOutput;
    QAudioBufferOutput m_audioBufferOutput;
    QMediaDevices m_mediaDevices;
    QByteArray m_mp3Data;
    QBuffer m_mp3Buffer;
    QByteArray m_flacData;
    QBuffer m_flacBuffer;
    QByteArray m_syntheticOutputPcm;
    QBuffer m_syntheticOutputBuffer;
    QMediaPlayer m_player;
    QAudioSink *m_syntheticOutputSink = nullptr;
    QTimer m_backendProbeTimer;
    QTimer m_syntheticOutputTimeout;
    QString m_tonePath;
    int m_trackIndex = 0;
    bool m_ffmpegLogDetected = false;
    bool m_syntheticOutputTimedOut = false;
    qsizetype m_pcmBufferCount = 0;
    qsizetype m_pcmFrameCount = 0;
    qsizetype m_pcmSampleCount = 0;
    qsizetype m_pcmNonZeroSampleCount = 0;
    double m_pcmPeak = 0.0;
    double m_pcmSumSquares = 0.0;
    int m_pcmSampleRate = 0;
    int m_pcmChannelCount = 0;
    QAudioFormat::SampleFormat m_pcmSampleFormat = QAudioFormat::Unknown;
    QString m_syntheticOutputStatus;
    AndroidAudioTestState::Value m_state = AndroidAudioTestState::Value::Preparing;
    QString m_error;
};

void installAndroidQtFfmpegLogProbe();
