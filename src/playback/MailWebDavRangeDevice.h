#pragma once

#include "MailWebDavProbe.h"

#include <QIODevice>
#include <QMap>
#include <QMutex>
#include <QQueue>
#include <QThread>
#include <QUrl>
#include <atomic>

class MailWebDavRangeFetcher;

// Read-only, bounded random-access stream for one listed Cloud Mail item.
// Credentials are copied only into private C++ state and never into a URL/QML.
class MailWebDavRangeDevice final : public QIODevice
{
    Q_OBJECT
    Q_PROPERTY(bool buffering READ buffering NOTIFY bufferingChanged)
public:
    MailWebDavRangeDevice(MailWebDavProbe::Entry entry, QString email,
                          QString appPassword, QObject *parent = nullptr);
    ~MailWebDavRangeDevice() override;

    bool open(OpenMode mode) override;
    bool isSequential() const override { return false; }
    qint64 size() const override { return m_entry.size.value_or(-1); }
    qint64 pos() const override;
    qint64 bytesAvailable() const override;
    bool seek(qint64 position) override;
    QString errorString() const;
    qint64 cachedBytes() const;
    bool buffering() const;
    void cancel();
    void cancelAsync();
    void interruptPendingRead();
    void retire();

signals:
    void bytesFetched(qint64 count);
    void bufferingChanged();
    void requestFailed(const QString &message);
    void sessionInvalid();

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    struct TestOriginTag {};
    MailWebDavRangeDevice(const QUrl &origin, TestOriginTag,
                          MailWebDavProbe::Entry entry, QString email,
                          QString appPassword, QObject *parent = nullptr);
    MailWebDavRangeDevice(const QUrl &origin, bool testOrigin,
                          MailWebDavProbe::Entry entry, QString email,
                          QString appPassword, QObject *parent);
    friend class MailWebDavRangeDeviceTests;

    bool loadPage(qint64 pageIndex, quint64 generation);
    void cancelSource(bool waitForFetcher);
    void setFailure(const QString &message);
    void setBuffering(bool value);

    static constexpr qint64 PageSize = 64 * 1024;
    static constexpr int CachedPages = 4;

    MailWebDavProbe::Entry m_entry;
    QUrl m_testOrigin;
    bool m_testOriginEnabled = false;
    QThread m_workerThread;
    MailWebDavRangeFetcher *m_fetcher = nullptr;
    QMap<qint64, QByteArray> m_pages;
    QQueue<qint64> m_lru;
    mutable QMutex m_mutex;
    QString m_error;
    bool m_buffering = false;
    std::atomic_bool m_cancelled{false};
    std::atomic_bool m_sourceOpen{false};
    std::atomic_bool m_retiring{false};
    std::atomic_bool m_interruptRequested{false};
    std::atomic_bool m_readInProgress{false};
    std::atomic<quint64> m_generation{0};
    std::atomic<qint64> m_pendingSeek{-1};
    std::atomic<qint64> m_visiblePosition{0};
};
