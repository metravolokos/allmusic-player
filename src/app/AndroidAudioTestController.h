#pragma once

#include "AndroidAudioTestState.h"

#include <QObject>
#include <QAudioOutput>
#include <QMediaDevices>
#include <QMediaPlayer>

class AndroidAudioTestController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString stateText READ stateText NOTIFY changed)
    Q_PROPERTY(QString mediaStatusText READ mediaStatusText NOTIFY changed)
    Q_PROPERTY(QString outputStatusText READ outputStatusText NOTIFY changed)
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
    QString errorText() const;
    bool playEnabled() const;
    bool pauseEnabled() const;
    bool stopEnabled() const;
    bool restartEnabled() const;

public slots:
    void play();
    void pause();
    void stop();
    void restart();

signals:
    void changed();

private:
    bool loadTone();
    void setState(AndroidAudioTestState::Value state);
    void setError(const QString &message);

    QAudioOutput m_audioOutput;
    QMediaDevices m_mediaDevices;
    QMediaPlayer m_player;
    QString m_tonePath;
    AndroidAudioTestState::Value m_state = AndroidAudioTestState::Value::Preparing;
    QString m_error;
};
