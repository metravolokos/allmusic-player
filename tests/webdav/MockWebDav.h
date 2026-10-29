#pragma once
#include <QTcpServer>
#include <QTcpSocket>
#include <QSslSocket>
#include <QSslConfiguration>
#include <QTimer>
#include <QMap>
#include <functional>
#include <memory>

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
    };
    QVector<Request> requests;
    std::function<Response(const Request &)> handler;
    std::function<void(const Request &)> onRequest;
    bool tls = false;
    QSslConfiguration ssl;
    bool start() { return listen(QHostAddress::LocalHost, 0); }
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
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
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
            } else if (request.target != "/AllMusic" && !request.target.startsWith("/AllMusic/")) {
                response.status = 403;
            } else if (handler) response = handler(request);
            else response.body = listing();
            QTimer::singleShot(response.delayMs, socket, [socket, response] {
                if (socket->state() != QAbstractSocket::ConnectedState) return;
                QByteArray bytes = "HTTP/1.1 " + QByteArray::number(response.status) + " Fixture\r\nConnection: close\r\n";
                auto headers = response.headers;
                if (!headers.contains("Content-Length")) headers["Content-Length"] = QByteArray::number(response.body.size());
                for (auto it = headers.cbegin(); it != headers.cend(); ++it) bytes += it.key() + ": " + it.value() + "\r\n";
                bytes += "\r\n";
                bytes += response.truncate ? response.body.left(response.body.size() / 2) : response.body;
                socket->write(bytes); socket->disconnectFromHost();
            });
        });
        if (tls) static_cast<QSslSocket *>(socket)->startServerEncryption();
    }
};
