#include "WebDavRangeDevice.h"
#include "WebDavProbe.h"

#include <QEventLoop>
#include <QMetaObject>
#include <QMutexLocker>
#include <QTimer>
#include <QVector>
#include <cstring>
#include <limits>
#include <utility>

namespace {
class RangeFetcher final : public QObject
{
public:
    WebDavProbe::Result fetch(quint16 port, const QString &path, qint64 offset,
                              qint64 length, const QByteArray &etag)
    {
        WebDavProbe probe(port);
        probe.requireB2MockMarker();
        activeProbe = &probe;
        QEventLoop loop;
        QObject::connect(&probe, &WebDavProbe::completed, &loop,
                         [this, &loop](const WebDavProbe::Result &result) {
            lastResult = result;
            loop.quit();
        });
        probe.setTimeoutMs(5000);
        probe.readRange(path, offset, length, etag);
        loop.exec();
        activeProbe = nullptr;
        auto completed = std::move(lastResult);
        lastResult = {};
        return completed;
    }

    void cancelActive()
    {
        if (activeProbe) activeProbe->cancel();
    }

    WebDavProbe *activeProbe = nullptr;
    WebDavProbe::Result lastResult;
};
}

WebDavRangeDevice::WebDavRangeDevice(quint16 port, QString path, qint64 size,
                                     QByteArray strongEtag, QObject *parent)
    : QIODevice(parent), m_port(port), m_path(std::move(path)), m_size(size),
      m_etag(std::move(strongEtag)), m_fetcher(new RangeFetcher)
{
    m_fetcher->moveToThread(&m_workerThread);
    connect(&m_workerThread, &QThread::finished, m_fetcher, &QObject::deleteLater);
    m_workerThread.start();
}

WebDavRangeDevice::~WebDavRangeDevice()
{
    cancel();
    m_workerThread.quit();
    m_workerThread.wait(6000);
}

bool WebDavRangeDevice::open(OpenMode mode)
{
    if (mode != QIODevice::ReadOnly || m_port == 0 || m_size <= 0
        || m_size > std::numeric_limits<qint64>::max() / 2
        || !m_etag.startsWith('"') || !m_etag.endsWith('"')
        || !WebDavProbe::safePath(m_path, QUrl(QStringLiteral("http://127.0.0.1:%1/AllMusic/").arg(m_port)))) {
        setFailure(QStringLiteral("Источник WebDAV для прототипа недопустим."));
        return false;
    }
    return QIODevice::open(mode);
}

bool WebDavRangeDevice::seek(qint64 position)
{
    if (position < 0 || position > m_size || m_cancelled.load())
        return false;
    return QIODevice::seek(position);
}

qint64 WebDavRangeDevice::cachedBytes() const
{
    QMutexLocker lock(&m_mutex);
    qint64 bytes = 0;
    for (const auto &page : m_pages)
        bytes += page.size();
    return bytes;
}

QString WebDavRangeDevice::errorString() const
{
    QMutexLocker lock(&m_mutex);
    return m_error;
}

void WebDavRangeDevice::cancel()
{
    if (m_cancelled.exchange(true))
        return;
    if (m_fetcher && m_workerThread.isRunning()) {
        QMetaObject::invokeMethod(m_fetcher, [fetcher = m_fetcher] {
            static_cast<RangeFetcher *>(fetcher)->cancelActive();
        }, Qt::QueuedConnection);
    }
}

void WebDavRangeDevice::setFailure(const QString &message)
{
    {
        QMutexLocker lock(&m_mutex);
        if (!m_error.isEmpty())
            return;
        m_error = message;
    }
    emit requestFailed(message);
}

bool WebDavRangeDevice::loadPage(qint64 pageIndex)
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_pages.contains(pageIndex)) {
            m_lru.removeAll(pageIndex);
            m_lru.enqueue(pageIndex);
            return true;
        }
        if (m_cancelled.load())
            return false;
    }

    const qint64 offset = pageIndex * PageSize;
    const qint64 length = qMin(PageSize, m_size - offset);
    if (length <= 0) {
        setFailure(QStringLiteral("Попытка чтения за концом файла."));
        return false;
    }
    auto *fetcher = static_cast<RangeFetcher *>(m_fetcher);
    WebDavProbe::Result result;
    const bool invoked = QMetaObject::invokeMethod(fetcher, [fetcher, this, offset, length, &result] {
        if (m_cancelled.load()) {
            result.error = WebDavProbe::Error::Cancelled;
            return;
        }
        result = fetcher->fetch(m_port, m_path, offset, length, m_etag);
    }, Qt::BlockingQueuedConnection);
    if (m_cancelled.load())
        return false;
    if (!invoked || result.error != WebDavProbe::Error::None
        || result.bytes.size() != length || result.etag != m_etag || !result.totalSize
        || *result.totalSize != m_size) {
        const QString reason = result.status == 200
            ? QStringLiteral("Сервер ответил HTTP 200 вместо Range 206. Возможен игнор Range или изменение файла; полная загрузка не используется.")
            : result.error == WebDavProbe::Error::VersionChanged
                ? QStringLiteral("Версия файла изменилась во время воспроизведения (ETag).")
                : QStringLiteral("Ошибка чтения диапазона WebDAV: %1 (HTTP %2).")
                    .arg(static_cast<int>(result.error)).arg(result.status);
        setFailure(reason);
        return false;
    }

    const qint64 total = result.bytes.size();
    {
        QMutexLocker lock(&m_mutex);
        if (m_cancelled.load())
            return false;
        m_pages.insert(pageIndex, std::move(result.bytes));
        m_lru.enqueue(pageIndex);
        while (m_lru.size() > CachedPages)
            m_pages.remove(m_lru.dequeue());
    }
    emit bytesFetched(total);
    return true;
}

qint64 WebDavRangeDevice::readData(char *data, qint64 maxSize)
{
    if (maxSize <= 0)
        return 0;
    if (m_cancelled.load())
        return -1;
    qint64 offset = QIODevice::pos();
    if (offset < 0 || offset >= m_size)
        return 0;

    qint64 copied = 0;
    while (copied < maxSize && offset < m_size) {
        const qint64 pageIndex = offset / PageSize;
        if (!loadPage(pageIndex))
            return copied > 0 ? copied : -1;
        const qint64 withinPage = offset % PageSize;
        QMutexLocker lock(&m_mutex);
        const auto it = m_pages.constFind(pageIndex);
        if (it == m_pages.cend())
            return copied > 0 ? copied : -1;
        const qint64 available = it->size() - withinPage;
        const qint64 amount = qMin(qMin(maxSize - copied, available), m_size - offset);
        if (amount <= 0)
            return copied;
        memcpy(data + copied, it->constData() + withinPage, static_cast<size_t>(amount));
        m_lru.removeAll(pageIndex);
        m_lru.enqueue(pageIndex);
        copied += amount;
        offset += amount;
    }
    return copied;
}
