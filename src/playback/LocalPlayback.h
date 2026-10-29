#pragma once

#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QUrl>
#include <QStringList>
#include <QVector>

class WebDavProbe;
class WebDavRangeDevice;

// Disposable 001-B1 adapter, not the final IPlaybackEngine contract.
class LocalPlayback final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QMediaPlayer* player READ player CONSTANT)
    Q_PROPERTY(QAudioOutput* audio READ audio CONSTANT)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString deviceWarning READ deviceWarning NOTIFY deviceWarningChanged)
    Q_PROPERTY(QStringList loopbackFiles READ loopbackFiles NOTIFY loopbackFilesChanged)
    Q_PROPERTY(qint64 networkBytes READ networkBytes NOTIFY networkBytesChanged)
public:
    explicit LocalPlayback(QObject *parent = nullptr);
    ~LocalPlayback() override;
    QMediaPlayer *player() { return &m_player; }
    QAudioOutput *audio() { return &m_audio; }
    QString error() const { return m_error; }
    QString deviceWarning() const;
    QStringList loopbackFiles() const { return m_loopbackFiles; }
    qint64 networkBytes() const { return m_networkBytes; }
    Q_INVOKABLE bool openLocal(const QUrl &url);
    Q_INVOKABLE void scanLoopback(int port);
    Q_INVOKABLE bool openLoopback(int index);
signals:
    void errorChanged();
    void deviceWarningChanged();
    void loopbackFilesChanged();
    void networkBytesChanged();
private:
    void fail(const QString &message);
    QMediaPlayer m_player;
    QAudioOutput m_audio;
    QString m_error;
    WebDavProbe *m_probe = nullptr;
    QVector<QString> m_references;
    QVector<qint64> m_sizes;
    QVector<QByteArray> m_etags;
    QStringList m_loopbackFiles;
    WebDavRangeDevice *m_rangeDevice = nullptr;
    qint64 m_networkBytes = 0;
    quint16 m_loopbackPort = 0;
};
