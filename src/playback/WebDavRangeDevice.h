#pragma once

#include <QIODevice>
#include <QThread>
#include <QUrl>
#include <QByteArray>
#include <QMap>
#include <QQueue>
#include <QMutex>
#include <atomic>

// Bounded, random-access view of one loopback /AllMusic resource. Requests are
// issued by a worker thread; decoder reads may wait there, never on the UI.
class WebDavRangeDevice final : public QIODevice
{
    Q_OBJECT
public:
    WebDavRangeDevice(quint16 port, QString path, qint64 size, QByteArray strongEtag,
                      QObject *parent = nullptr);
    ~WebDavRangeDevice() override;

    bool open(OpenMode mode) override;
    bool isSequential() const override { return false; }
    qint64 size() const override { return m_size; }
    bool seek(qint64 position) override;
    QString errorString() const;
    qint64 cachedBytes() const;
    void cancel();

signals:
    void bytesFetched(qint64 total);
    void requestFailed(const QString &message);

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    bool loadPage(qint64 pageIndex);
    void setFailure(const QString &message);

    static constexpr qint64 PageSize = 64 * 1024;
    static constexpr int CachedPages = 4;
    quint16 m_port;
    QString m_path;
    qint64 m_size;
    QByteArray m_etag;
    QThread m_workerThread;
    QObject *m_fetcher = nullptr;
    QMap<qint64, QByteArray> m_pages;
    QQueue<qint64> m_lru;
    mutable QMutex m_mutex;
    QString m_error;
    std::atomic_bool m_cancelled{false};
};
