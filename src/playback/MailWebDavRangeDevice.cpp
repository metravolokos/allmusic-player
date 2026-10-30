#include "MailWebDavRangeDevice.h"

#include "WebDavProbe.h"

#include <QEventLoop>
#include <QFileInfo>
#include <QMetaObject>
#include <QMutexLocker>
#include <QPointer>
#include <QRegularExpression>
#include <QTimer>
#include <cstring>
#include <limits>
#include <utility>

namespace {
const QUrl PinnedMailOrigin(QStringLiteral("https://webdav.cloud.mail.ru"));
}

// Runs every Cloud Mail GET on its own Qt network thread. It reuses the same
// pinned-origin request validation as the diagnostic client, but has no QML
// surface and keeps its credential copy only for the lifetime of the source.
class MailWebDavRangeFetcher final : public QObject
{
public:
    MailWebDavRangeFetcher(QString email, QString password,
                           MailWebDavProbe::Entry entry, QUrl testOrigin,
                           bool testOriginEnabled)
        : m_email(std::move(email)), m_password(std::move(password)),
          m_entry(std::move(entry)), m_testOrigin(std::move(testOrigin)),
          m_testOriginEnabled(testOriginEnabled)
    {
    }

    ~MailWebDavRangeFetcher() override
    {
        cancelAndClear();
    }

    MailWebDavProbe::Result fetch(qint64 offset, qint64 length)
    {
        using Error = MailWebDavProbe::Error;
        if (m_terminalCancelled || m_email.isEmpty() || m_password.isEmpty())
            return cancelledResult();

        ensureProbe();
        if (!m_probe)
            return failedResult(Error::InvalidCredentials);

        const quint64 cancellationGeneration = m_cancellationGeneration.load();
        for (int attempt = 0; attempt < 2; ++attempt) {
            if (m_terminalCancelled
                || cancellationGeneration != m_cancellationGeneration.load())
                return cancelledResult();

            MailWebDavProbe::Result completed;
            QEventLoop loop;
            const auto connection = QObject::connect(
                m_probe, &MailWebDavProbe::completed, &loop,
                [&completed, &loop](const MailWebDavProbe::Result &result) {
                    completed = result;
                    loop.quit();
                });
            const bool started = m_probe->readRange(m_entry, offset, length);
            if (!started) {
                QObject::disconnect(connection);
                return failedResult(Error::Protocol);
            }
            loop.exec();
            QObject::disconnect(connection);

            if (m_terminalCancelled
                || cancellationGeneration != m_cancellationGeneration.load())
                return cancelledResult();
            if (completed.error == Error::Auth || completed.error == Error::Tls) {
                cancelAndClear();
                return completed;
            }
            if (completed.error != Error::RateLimited || attempt != 0
                || completed.retryAfterSeconds < 0 || completed.retryAfterSeconds > 5)
                return completed;

            QEventLoop retryLoop;
            m_retryLoop = &retryLoop;
            QTimer::singleShot(completed.retryAfterSeconds * 1000, &retryLoop,
                               &QEventLoop::quit);
            retryLoop.exec();
            m_retryLoop = nullptr;
            if (m_terminalCancelled
                || cancellationGeneration != m_cancellationGeneration.load())
                return cancelledResult();
        }
        return failedResult(Error::RateLimited);
    }

    void cancelCurrentRequest()
    {
        ++m_cancellationGeneration;
        if (m_probe && m_probe->busy())
            m_probe->cancel();
        if (m_retryLoop)
            m_retryLoop->quit();
    }

    void cancelAndClear()
    {
        if (m_terminalCancelled)
            return;
        m_terminalCancelled = true;
        cancelCurrentRequest();
        if (m_probe) {
            m_probe->clearCredentials();
            delete m_probe;
            m_probe = nullptr;
        }
        m_email.fill(QChar(u'\0'));
        m_email.clear();
        m_password.fill(QChar(u'\0'));
        m_password.clear();
        m_entry.etag.fill('\0');
        m_entry.etag.clear();
        m_entry.reference.fill(QChar(u'\0'));
        m_entry.reference.clear();
        m_entry.path.fill(QChar(u'\0'));
        m_entry.path.clear();
    }

private:
    void ensureProbe()
    {
        if (m_probe)
            return;
        if (m_testOriginEnabled) {
            m_probe = new MailWebDavProbe(m_testOrigin,
                MailWebDavProbe::TestOriginTag{}, this);
        } else {
            m_probe = new MailWebDavProbe(this);
        }
        if (!m_probe->setSessionCredentials(m_email, m_password)) {
            delete m_probe;
            m_probe = nullptr;
        }
    }

    static MailWebDavProbe::Result cancelledResult()
    {
        MailWebDavProbe::Result result;
        result.error = MailWebDavProbe::Error::Cancelled;
        return result;
    }

    static MailWebDavProbe::Result failedResult(MailWebDavProbe::Error error)
    {
        MailWebDavProbe::Result result;
        result.error = error;
        return result;
    }

    QString m_email;
    QString m_password;
    MailWebDavProbe::Entry m_entry;
    QUrl m_testOrigin;
    bool m_testOriginEnabled = false;
    MailWebDavProbe *m_probe = nullptr;
    QPointer<QEventLoop> m_retryLoop;
    std::atomic<quint64> m_cancellationGeneration{0};
    bool m_terminalCancelled = false;
};

MailWebDavRangeDevice::MailWebDavRangeDevice(MailWebDavProbe::Entry entry,
                                             QString email, QString appPassword,
                                             QObject *parent)
    : MailWebDavRangeDevice(PinnedMailOrigin, false, std::move(entry),
                            std::move(email), std::move(appPassword), parent)
{
}

MailWebDavRangeDevice::MailWebDavRangeDevice(const QUrl &origin, TestOriginTag,
                                             MailWebDavProbe::Entry entry,
                                             QString email, QString appPassword,
                                             QObject *parent)
    : MailWebDavRangeDevice(origin, true, std::move(entry), std::move(email),
                            std::move(appPassword), parent)
{
}

MailWebDavRangeDevice::MailWebDavRangeDevice(const QUrl &origin, bool testOrigin,
                                             MailWebDavProbe::Entry entry,
                                             QString email, QString appPassword,
                                             QObject *parent)
    : QIODevice(parent), m_entry(std::move(entry)), m_testOrigin(origin),
      m_testOriginEnabled(testOrigin),
      m_fetcher(new MailWebDavRangeFetcher(std::move(email), std::move(appPassword),
                                           m_entry, origin, testOrigin))
{
    m_fetcher->moveToThread(&m_workerThread);
    connect(&m_workerThread, &QThread::finished, m_fetcher, &QObject::deleteLater);
    m_workerThread.start();
}

MailWebDavRangeDevice::~MailWebDavRangeDevice()
{
    cancel();
    m_workerThread.quit();
    if (!m_workerThread.wait(7000))
        m_workerThread.wait();
}

bool MailWebDavRangeDevice::open(OpenMode mode)
{
    const QString suffix = m_entry.path.section(u'.', -1).toLower();
    const QUrl origin = m_testOriginEnabled ? m_testOrigin : PinnedMailOrigin;
    const auto checked = WebDavProbe::safePath(m_entry.reference, origin);
    if (isOpen() || m_cancelled.load() || mode != QIODevice::ReadOnly
        || !m_entry.size || *m_entry.size <= 0
        || *m_entry.size > std::numeric_limits<qint64>::max() / 2
        || (suffix != QStringLiteral("mp3") && suffix != QStringLiteral("flac"))
        || !checked || !checked->startsWith(QStringLiteral("/AllMusic/"))) {
        setFailure(QStringLiteral("Источник Cloud Mail недопустим или не поддерживает этот формат."));
        return false;
    }
    if (!QIODevice::open(mode))
        return false;

    // The fetcher owns a separate private copy. The QIODevice no longer needs
    // the remote path or validator once the source has been validated/opened.
    m_entry.path.fill(QChar(u'\0'));
    m_entry.path.clear();
    m_entry.reference.fill(QChar(u'\0'));
    m_entry.reference.clear();
    m_entry.etag.fill('\0');
    m_entry.etag.clear();
    return true;
}

bool MailWebDavRangeDevice::seek(qint64 position)
{
    if (position < 0 || position > size() || m_cancelled.load())
        return false;
    if (!QIODevice::seek(position))
        return false;
    const quint64 generation = ++m_generation;
    Q_UNUSED(generation);
    if (m_fetcher && m_workerThread.isRunning()) {
        QMetaObject::invokeMethod(m_fetcher, [fetcher = m_fetcher] {
            fetcher->cancelCurrentRequest();
        }, Qt::QueuedConnection);
    }
    return true;
}

QString MailWebDavRangeDevice::errorString() const
{
    QMutexLocker lock(&m_mutex);
    return m_error;
}

qint64 MailWebDavRangeDevice::cachedBytes() const
{
    QMutexLocker lock(&m_mutex);
    qint64 bytes = 0;
    for (const auto &page : m_pages)
        bytes += page.size();
    return bytes;
}

bool MailWebDavRangeDevice::buffering() const
{
    QMutexLocker lock(&m_mutex);
    return m_buffering;
}

void MailWebDavRangeDevice::setBuffering(bool value)
{
    bool changed = false;
    {
        QMutexLocker lock(&m_mutex);
        if (m_buffering != value) {
            m_buffering = value;
            changed = true;
        }
    }
    if (changed)
        emit bufferingChanged();
}

void MailWebDavRangeDevice::cancel()
{
    if (m_cancelled.exchange(true))
        return;
    ++m_generation;
    setBuffering(false);
    {
        QMutexLocker lock(&m_mutex);
        for (auto it = m_pages.begin(); it != m_pages.end(); ++it)
            it.value().fill('\0');
        m_pages.clear();
        m_lru.clear();
    }
    if (m_fetcher && m_workerThread.isRunning()) {
        QMetaObject::invokeMethod(m_fetcher, [fetcher = m_fetcher] {
            fetcher->cancelAndClear();
        }, Qt::BlockingQueuedConnection);
    }
}

void MailWebDavRangeDevice::setFailure(const QString &message)
{
    {
        QMutexLocker lock(&m_mutex);
        if (!m_error.isEmpty())
            return;
        m_error = message;
    }
    emit requestFailed(message);
}

bool MailWebDavRangeDevice::loadPage(qint64 pageIndex, quint64 generation)
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
    const qint64 length = qMin(PageSize, size() - offset);
    if (length <= 0) {
        setFailure(QStringLiteral("Попытка чтения за границей файла Cloud Mail."));
        return false;
    }

    setBuffering(true);
    MailWebDavProbe::Result result;
    const bool invoked = QMetaObject::invokeMethod(m_fetcher,
        [fetcher = m_fetcher, offset, length, &result] {
            result = fetcher->fetch(offset, length);
        }, Qt::BlockingQueuedConnection);
    setBuffering(false);
    if (m_cancelled.load() || generation != m_generation.load())
        return false;

    using Error = MailWebDavProbe::Error;
    if (!invoked || result.error != Error::None
        || result.rangeBytes.size() != length || !result.totalSize
        || *result.totalSize != size() || !result.contentRangePresent
        || (result.ifRangeSent && !result.etagMatchesIfRange)) {
        QString message;
        if (result.status == 200 || result.error == Error::RangeUnsupported)
            message = QStringLiteral("Cloud Mail не подтвердил HTTP 206. Полный файл не загружался.");
        else if (result.error == Error::VersionChanged
                 || result.rangeIssue == MailWebDavProbe::RangeIssue::EtagMismatch
                 || (result.totalSize && *result.totalSize != size()))
            message = QStringLiteral("Размер или версия файла Cloud Mail изменились во время воспроизведения.");
        else if (result.error == Error::Auth)
            message = QStringLiteral("Cloud Mail отклонил временную сессию. Отключитесь и подключитесь снова.");
        else if (result.error == Error::RateLimited)
            message = QStringLiteral("Cloud Mail ограничил запросы. Повторите воспроизведение позже.");
        else if (result.error == Error::Timeout)
            message = QStringLiteral("Истекло время ожидания диапазона Cloud Mail.");
        else if (result.error == Error::Tls)
            message = QStringLiteral("Не пройдена проверка TLS Cloud Mail. Сертификаты не игнорировались.");
        else if (result.error == Error::Cancelled)
            return false;
        else
            message = QStringLiteral("Ответ Cloud Mail не прошёл проверку диапазона или передачи данных.");
        if (result.error == Error::Auth || result.error == Error::Tls)
            emit sessionInvalid();
        setFailure(message);
        return false;
    }

    const qint64 received = result.rangeBytes.size();
    {
        QMutexLocker lock(&m_mutex);
        if (m_cancelled.load() || generation != m_generation.load())
            return false;
        m_pages.insert(pageIndex, std::move(result.rangeBytes));
        m_lru.enqueue(pageIndex);
        while (m_lru.size() > CachedPages) {
            const qint64 expired = m_lru.dequeue();
            auto it = m_pages.find(expired);
            if (it != m_pages.end()) {
                it.value().fill('\0');
                m_pages.erase(it);
            }
        }
    }
    emit bytesFetched(received);
    return true;
}

qint64 MailWebDavRangeDevice::readData(char *data, qint64 maxSize)
{
    if (maxSize <= 0)
        return 0;
    if (QThread::currentThread() == thread()) {
        setFailure(QStringLiteral("Чтение Cloud Mail остановлено, чтобы не блокировать интерфейс."));
        return -1;
    }
    if (m_cancelled.load())
        return -1;

    qint64 offset = QIODevice::pos();
    if (offset < 0 || offset >= size())
        return 0;
    quint64 generation = m_generation.load();
    qint64 copied = 0;
    while (copied < maxSize && offset < size()) {
        if (generation != m_generation.load()) {
            generation = m_generation.load();
            offset = QIODevice::pos();
            copied = 0;
            if (offset < 0 || offset >= size())
                return 0;
        }
        const qint64 pageIndex = offset / PageSize;
        if (!loadPage(pageIndex, generation)) {
            if (generation != m_generation.load())
                continue;
            return copied > 0 ? copied : -1;
        }
        QMutexLocker lock(&m_mutex);
        if (generation != m_generation.load()) {
            lock.unlock();
            continue;
        }
        const auto it = m_pages.constFind(pageIndex);
        if (it == m_pages.cend())
            return copied > 0 ? copied : -1;
        const qint64 withinPage = offset % PageSize;
        const qint64 available = it->size() - withinPage;
        const qint64 amount = qMin(qMin(maxSize - copied, available), size() - offset);
        if (amount <= 0)
            return copied;
        std::memcpy(data + copied, it->constData() + withinPage,
                    static_cast<size_t>(amount));
        m_lru.removeAll(pageIndex);
        m_lru.enqueue(pageIndex);
        copied += amount;
        offset += amount;
    }
    return copied;
}
