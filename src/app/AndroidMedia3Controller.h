#pragma once

#include "AndroidCloudTrackListModel.h"
#include "AndroidMedia3State.h"

#include <QObject>
#include <QTimer>

class MailWebDavController;

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
    Q_PROPERTY(bool cloudConnected READ cloudConnected NOTIFY changed)
    Q_PROPERTY(bool cloudBusy READ cloudBusy NOTIFY changed)
    Q_PROPERTY(QString cloudStatus READ cloudStatus NOTIFY changed)
    Q_PROPERTY(QString cloudDirectoryLabel READ cloudDirectoryLabel NOTIFY changed)
    Q_PROPERTY(AndroidCloudTrackListModel *cloudEntriesModel READ cloudEntriesModel CONSTANT)

public:
    explicit AndroidMedia3Controller(MailWebDavController *cloud,
                                     QObject *parent = nullptr);
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
    bool cloudConnected() const;
    bool cloudBusy() const;
    QString cloudStatus() const;
    QString cloudDirectoryLabel() const;
    AndroidCloudTrackListModel *cloudEntriesModel() const;

public slots:
    void selectTrack(int index);
    void play();
    void pause();
    void stop();
    void seekTo(qint64 positionMs);
    void connectCloud(QString email, QString appPassword);
    void disconnectCloud();
    void openCloudDirectory(int index);
    bool canGoToCloudParent() const;
    void goToCloudParent();
    bool canPlayCloud(int index) const;
    void playCloud(int index);
    void release();

signals:
    void changed();
    void cloudSessionEnded();
    void cloudEntriesReset();

private slots:
    void refreshState();

private:
    void setError(const QString &message);
    void releaseCloudSource();
    void stopForDisconnect();
    void invokeTrack(const QString &assetPath, const QString &title);
    void invokeCommand(const char *method);

    QTimer m_refreshTimer;
    MailWebDavController *m_cloud = nullptr;
    AndroidCloudTrackListModel *m_cloudEntriesModel = nullptr;
    qint64 m_cloudHandle = 0;
    int m_cloudTrackIndex = -1;
    bool m_cloudTrackActive = false;
    AndroidMedia3State::Value m_state = AndroidMedia3State::Value::Preparing;
    int m_trackIndex = -1;
    qint64 m_positionMs = 0;
    qint64 m_durationMs = 0;
    QString m_error;
    QString m_cloudTrackName;
    bool m_released = false;
};
