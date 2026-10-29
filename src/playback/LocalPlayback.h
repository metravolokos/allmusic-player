#pragma once

#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QUrl>

// Disposable 001-B1 adapter, not the final IPlaybackEngine contract.
class LocalPlayback final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QMediaPlayer* player READ player CONSTANT)
    Q_PROPERTY(QAudioOutput* audio READ audio CONSTANT)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString deviceWarning READ deviceWarning NOTIFY deviceWarningChanged)
public:
    explicit LocalPlayback(QObject *parent = nullptr);
    QMediaPlayer *player() { return &m_player; }
    QAudioOutput *audio() { return &m_audio; }
    QString error() const { return m_error; }
    QString deviceWarning() const;
    Q_INVOKABLE bool openLocal(const QUrl &url);
signals:
    void errorChanged();
    void deviceWarningChanged();
private:
    void fail(const QString &message);
    QMediaPlayer m_player;
    QAudioOutput m_audio;
    QString m_error;
};
