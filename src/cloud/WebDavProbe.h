#pragma once
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QVector>
#include <optional>

// Research-only, loopback-only client. No production ICloudProvider contract yet.
class WebDavProbe final : public QObject {
    Q_OBJECT
public:
    enum class Error { None, Busy, UnsafePath, InvalidRequest, Redirect, Auth,
        Forbidden, NotFound, RangeUnavailable, RateLimited, Server, Http,
        Network, Timeout, Cancelled, TooLarge, Protocol, VersionChanged, Tls };
    Q_ENUM(Error)
    struct Entry {
        QString path;
        QString reference; // Encoded path suitable for a subsequent request; path is for display.
        bool directory = false;
        std::optional<qint64> size;
        QByteArray etag;
        QString lastModified;
    };
    struct Result {
        Error error = Error::None;
        int status = 0;
        int attempts = 0;
        QByteArray bytes;
        QByteArray etag;
        bool strongEtag = false;
        std::optional<qint64> totalSize;
        QVector<Entry> entries;
        int rejectedEntries = 0;
    };
    explicit WebDavProbe(quint16 loopbackPort, bool tls = false, QObject *parent = nullptr);
    bool listDirectory(const QString &path);
    bool readRange(const QString &path, qint64 offset, qint64 length,
                   const QByteArray &expectedStrongEtag = {});
    void cancel();
    bool busy() const { return active; }
    void requireB2MockMarker(bool required = true) { requireMockMarker = required; }
    // Smaller limits may be used by tests; defaults bound all operations.
    void setTimeoutMs(int ms) { timeoutMs = qBound(10, ms, 30000); }
    static std::optional<QString> safePath(const QString &reference, const QUrl &origin);
signals:
    void completed(const WebDavProbe::Result &result);
private:
    bool begin(const QString &path, bool listing);
    void send();
    void finish(Error error);
    void parseListing();
    QNetworkAccessManager manager;
    QUrl origin;
    QNetworkReply *reply = nullptr;
    QTimer deadline, retry;
    Result result;
    QByteArray body, expectedEtag;
    QString requestPath;
    qint64 start = 0, count = 0;
    int timeoutMs = 3000;
    quint64 operation = 0;
    bool active = false, listing = false, tlsFailure = false;
    bool requireMockMarker = false;
};
Q_DECLARE_METATYPE(WebDavProbe::Result)
