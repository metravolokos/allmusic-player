#pragma once

#include "MailWebDavProbe.h"

#include <QObject>
#include <QVariantList>
#include <QVector>

class MailWebDavRangeDevice;

class MailWebDavController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(int entryCount READ entryCount NOTIFY entriesChanged)
    Q_PROPERTY(QString currentDirectory READ currentDirectory NOTIFY stateChanged)
public:
    explicit MailWebDavController(QObject *parent = nullptr,
                                  bool streamableFormatsOnly = false);
    ~MailWebDavController() override;

    QString status() const { return m_status; }
    bool busy() const { return m_busy; }
    bool connected() const { return m_connected; }
    QString currentDirectory() const { return m_currentDirectory; }
    QVariantList entries() const;
    int entryCount() const { return m_entries.size(); }

    Q_INVOKABLE bool checkConnection(QString email, QString appPassword);
    Q_INVOKABLE bool canCheckRange(int index) const;
    Q_INVOKABLE void checkRange(int index);
    Q_INVOKABLE bool canOpenDirectory(int index) const;
    Q_INVOKABLE void openDirectory(int index);
    Q_INVOKABLE bool canGoToParent() const;
    Q_INVOKABLE void goToParent();
    Q_INVOKABLE bool canPlay(int index) const;
    Q_INVOKABLE void play(int index);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void disconnectCloud();
    void setPlaybackActive(bool active);
    MailWebDavRangeDevice *takePlaybackDevice(int index, QString *displayLabel,
                                               QObject *parent);

signals:
    void stateChanged();
    void entriesChanged();
    void playbackRequested(int index);
    void disconnectPlayback();
    void sessionEnded();

private:
    void handleCompleted(const MailWebDavProbe::Result &result);
    void dropSession();
    void setStatus(const QString &status);
    static QString rangeDiagnosticText(const MailWebDavProbe::Result &result);
    static bool isAudio(const QString &path);
    static bool isStreamable(const QString &path);
    static QString displayName(const QString &path, bool directory);
    static QString errorText(const MailWebDavProbe::Result &result);
    static QString networkDiagnosticText(const MailWebDavProbe::Result &result);

    MailWebDavProbe *m_probe = nullptr;
    QVector<MailWebDavProbe::Entry> m_entries;
    QString m_pendingEmail;
    QString m_pendingAppPassword;
    QString m_currentDirectory = QStringLiteral("/AllMusic/");
    QString m_status = QStringLiteral("Проверка Cloud Mail ещё не запускалась.");
    bool m_busy = false;
    bool m_connected = false;
    bool m_playbackActive = false;
    bool m_streamableFormatsOnly = false;
};
