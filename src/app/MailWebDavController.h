#pragma once

#include "MailWebDavProbe.h"

#include <QObject>
#include <QVariantList>
#include <QVector>

class MailWebDavController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(int entryCount READ entryCount NOTIFY entriesChanged)
public:
    explicit MailWebDavController(QObject *parent = nullptr);
    ~MailWebDavController() override;

    QString status() const { return m_status; }
    bool busy() const { return m_busy; }
    bool connected() const { return m_connected; }
    QVariantList entries() const;
    int entryCount() const { return m_entries.size(); }

    Q_INVOKABLE bool checkConnection(QString email, QString appPassword);
    Q_INVOKABLE bool canCheckRange(int index) const;
    Q_INVOKABLE void checkRange(int index);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void disconnectCloud();

signals:
    void stateChanged();
    void entriesChanged();

private:
    void handleCompleted(const MailWebDavProbe::Result &result);
    void dropSession();
    void setStatus(const QString &status);
    static QString rangeDiagnosticText(const MailWebDavProbe::Result &result);
    static bool isAudio(const QString &path);
    static QString displayName(const QString &path, bool directory);
    static QString errorText(MailWebDavProbe::Error error, int status);

    MailWebDavProbe *m_probe = nullptr;
    QVector<MailWebDavProbe::Entry> m_entries;
    QString m_status = QStringLiteral("Проверка Cloud Mail ещё не запускалась.");
    bool m_busy = false;
    bool m_connected = false;
};
