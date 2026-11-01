#pragma once

#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QUrl>
#include <QStringList>
#include <QVector>

class WebDavProbe;
class WebDavRangeDevice;
class MailWebDavRangeDevice;

// Disposable 001-B1 adapter, not the final IPlaybackEngine contract.
class LocalPlayback final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QMediaPlayer* player READ player CONSTANT)
    Q_PROPERTY(QAudioOutput* audio READ audio CONSTANT)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString deviceWarning READ deviceWarning NOTIFY deviceWarningChanged)
    Q_PROPERTY(bool cloudMailActive READ cloudMailActive NOTIFY cloudMailActiveChanged)
    Q_PROPERTY(bool buffering READ buffering NOTIFY bufferingChanged)
    Q_PROPERTY(QString activeSourceLabel READ activeSourceLabel NOTIFY activeSourceLabelChanged)
    Q_PROPERTY(QStringList loopbackFiles READ loopbackFiles NOTIFY loopbackFilesChanged)
    Q_PROPERTY(qint64 networkBytes READ networkBytes NOTIFY networkBytesChanged)
    Q_PROPERTY(int currentLoopbackIndex READ currentLoopbackIndex NOTIFY currentLoopbackIndexChanged)
public:
    explicit LocalPlayback(QObject *parent = nullptr);
    ~LocalPlayback() override;
    QMediaPlayer *player() { return &m_player; }
    QAudioOutput *audio() { return &m_audio; }
    QString error() const { return m_error; }
    QString deviceWarning() const;
    bool cloudMailActive() const { return m_cloudMailActive; }
    bool buffering() const { return m_buffering; }
    QString activeSourceLabel() const { return m_activeSourceLabel; }
    QStringList loopbackFiles() const { return m_loopbackFiles; }
    qint64 networkBytes() const { return m_networkBytes; }
    int currentLoopbackIndex() const { return m_currentLoopbackIndex; }
    Q_INVOKABLE bool openLocal(const QUrl &url);
    Q_INVOKABLE void scanLoopback(int port);
    Q_INVOKABLE bool openLoopback(int index);
    bool openMailDevice(MailWebDavRangeDevice *device, const QString &label,
                        quint64 playbackGeneration = 0);
    void stopMail();
    Q_INVOKABLE void togglePlayback();
    Q_INVOKABLE void stop();
signals:
    void errorChanged();
    void deviceWarningChanged();
    void cloudMailActiveChanged();
    void bufferingChanged();
    void activeSourceLabelChanged();
    void cloudMailSessionInvalid();
    void loopbackFilesChanged();
    void networkBytesChanged();
    void currentLoopbackIndexChanged();
    void trackFinished(quint64 playbackGeneration);
private:
    void fail(const QString &message);
    void resetPlayerSource();
    void detachNetworkSource();
    void detachMailSource();
    void setCurrentLoopbackIndex(int index);
    QMediaPlayer m_player;
    QAudioOutput m_audio;
    QString m_error;
    WebDavProbe *m_probe = nullptr;
    QVector<QString> m_references;
    QVector<qint64> m_sizes;
    QVector<QByteArray> m_etags;
    QStringList m_loopbackFiles;
    WebDavRangeDevice *m_rangeDevice = nullptr;
    MailWebDavRangeDevice *m_mailRangeDevice = nullptr;
    qint64 m_networkBytes = 0;
    quint16 m_loopbackPort = 0;
    int m_currentLoopbackIndex = -1;
    quint64 m_playbackGeneration = 0;
    bool m_cloudMailActive = false;
    bool m_buffering = false;
    QString m_activeSourceLabel;
};
