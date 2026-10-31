#pragma once
#include <QTcpServer>
#include <QTcpSocket>
#include <QSslSocket>
#include <QSslConfiguration>
#include <QTimer>
#include <QMap>
#include <functional>
#include <memory>
#include <utility>

class MockResponseWriter final : public QObject {
public:
    MockResponseWriter(QTcpSocket *socket, QByteArray body, qint64 limit,
                       qsizetype chunkSize, int chunkIntervalMs)
        : QObject(socket), m_socket(socket), m_body(std::move(body)), m_limit(limit),
          m_chunkSize(chunkSize), m_chunkIntervalMs(chunkIntervalMs) {}

    void start() { writeNext(); }

private:
    void writeNext() {
        if (m_socket->state() != QAbstractSocket::ConnectedState) {
            deleteLater();
            return;
        }
        const qint64 remaining = m_limit - m_offset;
        if (remaining <= 0) {
            m_socket->disconnectFromHost();
            deleteLater();
            return;
        }
        const qint64 count = m_chunkSize > 0 ? qMin<qint64>(m_chunkSize, remaining) : remaining;
        m_socket->write(m_body.mid(m_offset, count));
        m_offset += count;
        if (m_offset >= m_limit) {
            m_socket->disconnectFromHost();
            deleteLater();
            return;
        }
        QTimer::singleShot(m_chunkIntervalMs, this, [this] { writeNext(); });
    }

    QTcpSocket *m_socket;
    QByteArray m_body;
    qint64 m_limit;
    qsizetype m_chunkSize;
    int m_chunkIntervalMs;
    qint64 m_offset = 0;
};

// Entirely in-memory, loopback-only fixture. No filesystem and no cloud proxying.
class MockWebDav : public QTcpServer {
public:
    struct Request { QByteArray method, target, body; QMap<QByteArray, QByteArray> headers; };
    struct Response {
        int status = 207;
        QByteArray body;
        QMap<QByteArray, QByteArray> headers;
        int delayMs = 0;
        bool truncate = false;
        bool disconnectBeforeResponse = false;
        qint64 disconnectAfterBytes = -1;
        qsizetype chunkSize = 0;
        int chunkIntervalMs = 0;
        bool omitContentLength = false;
        bool chunked = false;
    };
    QVector<Request> requests;
    std::function<Response(const Request &)> handler;
    std::function<void(const Request &)> onRequest;
    int activeResponses = 0;
    int activeRangeResponses = 0;
    bool includeB2Marker = true;
    bool tls = false;
    QSslConfiguration ssl;
    bool start() { return listen(QHostAddress::LocalHost, 0); }
    // QTcpServer::close() only stops listening. Accepted sockets belong to the
    // fixture, and their delayed readyRead callbacks must not cross test cases.
    void stop()
    {
        close();
        for (auto *socket : findChildren<QTcpSocket *>(QString(), Qt::FindDirectChildrenOnly)) {
            socket->abort();
            socket->deleteLater();
        }
    }
    static QByteArray listing() {
        return "<x:multistatus xmlns:x=\"DAV:\"><x:response><x:href>/AllMusic/</x:href>"
            "<x:propstat><x:prop><x:resourcetype><x:collection/></x:resourcetype></x:prop>"
            "<x:status>HTTP/1.1 200 OK</x:status></x:propstat></x:response></x:multistatus>";
    }
protected:
    void incomingConnection(qintptr descriptor) override {
        QTcpSocket *socket;
        if (tls) {
            auto *secure = new QSslSocket(this);
            secure->setSslConfiguration(ssl);
            socket = secure;
        } else socket = new QTcpSocket(this);
        if (!socket->setSocketDescriptor(descriptor)) { socket->deleteLater(); return; }
        auto buffer = std::make_shared<QByteArray>();
        auto handled = std::make_shared<bool>(false);
        connect(socket, &QTcpSocket::disconnected, socket, [this, socket] {
            if (socket->property("mock-response-active").toBool()) {
                --activeResponses;
                if (socket->property("mock-range-response-active").toBool())
                    --activeRangeResponses;
            }
            socket->deleteLater();
        });
        QTimer::singleShot(5000, socket, &QTcpSocket::abort);
        connect(socket, &QTcpSocket::readyRead, socket, [this, socket, buffer, handled] {
            if (*handled) return;
            buffer->append(socket->readAll());
            if (buffer->size() > 65536) { socket->abort(); return; }
            const auto end = buffer->indexOf("\r\n\r\n");
            if (end < 0) return;
            const auto lines = buffer->left(end).split('\n');
            const auto first = lines.value(0).trimmed().split(' ');
            if (first.size() != 3) { socket->abort(); return; }
            Request request; request.method = first[0]; request.target = first[1];
            for (qsizetype i = 1; i < lines.size(); ++i) {
                const auto colon = lines[i].indexOf(':');
                if (colon < 1) { socket->abort(); return; }
                request.headers[lines[i].left(colon).trimmed().toLower()] = lines[i].mid(colon + 1).trimmed();
            }
            bool validLength = true;
            const auto length = request.headers.contains("content-length")
                ? request.headers["content-length"].toLongLong(&validLength) : 0;
            if (!validLength || length < 0 || length > 32768) { socket->abort(); return; }
            if (buffer->size() - end - 4 < length) return;
            *handled = true; request.body = buffer->mid(end + 4, length); requests.push_back(request);
            if (onRequest) onRequest(request);
            Response response;
            if (request.method != "GET" && request.method != "PROPFIND" && request.method != "HEAD") {
                response.status = 405;
            } else if ((request.method == "HEAD" && request.target != "/")
                       || (request.method != "HEAD" && request.target != "/AllMusic"
                           && !request.target.startsWith("/AllMusic/"))) {
                response.status = 403;
            } else if (handler) response = handler(request);
            else response.body = listing();
            if (includeB2Marker)
                response.headers["X-AllMusic-Mock"] = "001-B2-loopback";
            socket->setProperty("mock-response-active", true);
            const bool rangeResponse = request.method == "GET" && request.headers.contains("range");
            socket->setProperty("mock-range-response-active", rangeResponse);
            ++activeResponses;
            if (rangeResponse)
                ++activeRangeResponses;
            QTimer::singleShot(response.delayMs, socket, [socket, response] {
                if (socket->state() != QAbstractSocket::ConnectedState) return;
                if (response.disconnectBeforeResponse) { socket->abort(); return; }
                QByteArray bytes = "HTTP/1.1 " + QByteArray::number(response.status) + " Fixture\r\nConnection: close\r\n";
                auto headers = response.headers;
                const bool chunked = response.chunked
                    || headers.value("Transfer-Encoding").toLower().contains("chunked");
                if (response.chunked && !headers.contains("Transfer-Encoding"))
                    headers["Transfer-Encoding"] = "chunked";
                if (!response.omitContentLength && !headers.contains("Content-Length")
                    && !headers.contains("Transfer-Encoding"))
                    headers["Content-Length"] = QByteArray::number(response.body.size());
                for (auto it = headers.cbegin(); it != headers.cend(); ++it) bytes += it.key() + ": " + it.value() + "\r\n";
                bytes += "\r\n";
                socket->write(bytes);
                qint64 bodyLimit = response.body.size();
                if (response.truncate) bodyLimit /= 2;
                if (response.disconnectAfterBytes >= 0)
                    bodyLimit = qMin(bodyLimit, response.disconnectAfterBytes);
                if (chunked) {
                    QByteArray framed;
                    if (bodyLimit > 0) {
                        framed += QByteArray::number(bodyLimit, 16) + "\r\n";
                        framed += response.body.left(bodyLimit);
                        framed += "\r\n";
                    }
                    framed += "0\r\n\r\n";
                    socket->write(framed);
                    socket->disconnectFromHost();
                } else if (response.chunkSize > 0) {
                    auto *writer = new MockResponseWriter(socket, response.body, bodyLimit,
                                                          response.chunkSize, response.chunkIntervalMs);
                    writer->start();
                } else {
                    socket->write(response.body.left(bodyLimit));
                    socket->disconnectFromHost();
                }
            });
        });
        if (tls) static_cast<QSslSocket *>(socket)->startServerEncryption();
    }
};
