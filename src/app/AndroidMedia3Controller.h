#pragma once

#include "AndroidMedia3State.h"

#include <QObject>
#include <QTimer>

class AndroidMedia3Controller final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList trackNames READ trackNames CONSTANT)
    Q_PROPERTY(int trackIndex READ trackIndex NOTIFY changed)
    Q_PROPERTY(QString activeTrackName READ activeTrackName NOTIFY changed)
    Q_PROPERTY(QString stateText READ stateText NOTIFY changed)
    Q_PROPERTY(QString outputStatusText READ outputStatusText CONSTANT)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
    Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY changed)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY changed)
    Q_PROPERTY(bool playEnabled READ playEnabled NOTIFY changed)
    Q_PROPERTY(bool pauseEnabled READ pauseEnabled NOTIFY changed)
    Q_PROPERTY(bool stopEnabled READ stopEnabled NOTIFY changed)
    Q_PROPERTY(bool seekEnabled READ seekEnabled NOTIFY changed)

public:
    explicit AndroidMedia3Controller(QObject *parent = nullptr);
    ~AndroidMedia3Controller() override;

    QStringList trackNames() const;
    int trackIndex() const;
    QString activeTrackName() const;
    QString stateText() const;
    QString outputStatusText() const;
    QString errorText() const;
    qint64 positionMs() const;
    qint64 durationMs() const;
    bool playEnabled() const;
    bool pauseEnabled() const;
    bool stopEnabled() const;
    bool seekEnabled() const;

public slots:
    void selectTrack(int index);
    void play();
    void pause();
    void stop();
    void seekTo(qint64 positionMs);
    void release();

signals:
    void changed();

private slots:
    void refreshState();

private:
    void setError(const QString &message);
    void invokeTrack(const QString &assetPath, const QString &title);
    void invokeCommand(const char *method);

    QTimer m_refreshTimer;
    AndroidMedia3State::Value m_state = AndroidMedia3State::Value::Preparing;
    int m_trackIndex = -1;
    qint64 m_positionMs = 0;
    qint64 m_durationMs = 0;
    QString m_error;
    bool m_released = false;
};
