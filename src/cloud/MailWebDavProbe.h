#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>
#include <QVector>
#include <QStringList>
#include <optional>

// Read-only diagnostic client for the fixed Cloud Mail WebDAV origin.
// Unlike WebDavProbe, this client never accepts a destination from the UI.
class MailWebDavProbe final : public QObject {
    Q_OBJECT
public:
    enum class Error {
        None, Busy, InvalidCredentials, UnsafePath, Auth, Forbidden, NotFound,
        Redirect, RangeUnsupported, VersionChanged, RateLimited, Server,
        Network, Timeout, Cancelled, TooLarge, Protocol, Tls, Http
    };
    Q_ENUM(Error)

    enum class RangeIssue {
        None, RangeIgnored, RangeUnsatisfiable, MissingContentRange,
        InvalidContentRange, InvalidRangeBounds, RangeStartMismatch,
        RangeEndMismatch, TotalSizeMismatch, InvalidContentLength,
        ContentLengthMismatch, BodyLengthMismatch, UnsupportedContentEncoding,
        UnsupportedTransferEncoding, ConflictingMessageFraming,
        EtagMismatch, IncompleteTransfer, TooLarge
    };
    Q_ENUM(RangeIssue)

    enum class NetworkCategory {
        None, DnsLookup, ConnectionRefused, RemoteHostClosed, TemporaryFailure,
        Proxy, TlsBackendUnavailable, TlsHandshake, Timeout, Cancelled, Unknown
    };
    Q_ENUM(NetworkCategory)

    struct TlsCapabilities {
        bool available = false;
        QString activeBackend;
        QStringList availableBackends;
        QString runtimeVersion;
    };

    struct Entry {
        QString path;
        QString reference;
        bool directory = false;
        std::optional<qint64> size;
        QByteArray etag;
    };

    struct Result {
        Error error = Error::None;
        bool preflight = false;
        RangeIssue rangeIssue = RangeIssue::None;
        NetworkCategory networkCategory = NetworkCategory::None;
        int networkErrorCode = -1;
        bool sslBackendAvailable = false;
        QString activeSslBackend;
        QStringList availableSslBackends;
        QString sslRuntimeVersion;
        int status = 0;
        bool listing = false;
        bool rangeCheck = false;
        QVector<Entry> entries;
        int rejectedEntries = 0;
        qint64 bytesReceived = 0;
        bool bytesReceivedIsLowerBound = false;
        qint64 requestedRangeBytes = 0;
        std::optional<qint64> rangeFirst;
        std::optional<qint64> rangeLast;
        std::optional<qint64> rangeExpectedBytes;
        std::optional<qint64> totalSize;
        std::optional<qint64> declaredContentLength;
        QString contentRangeSyntax; // Fixed vocabulary only; never contains header fragments.
        bool contentRangeRepeated = false;
        bool contentRangePresent = false;
        bool contentRangeUnsatisfied = false;
        bool contentLengthPresent = false;
        bool transferEncodingPresent = false;
        bool transferEncodingChunked = false;
        bool contentEncodingPresent = false;
        bool contentEncodingIdentity = true;
        bool etagPresent = false;
        bool strongEtag = false;
        bool etagMatchesIfRange = true;
        bool ifRangeSent = false;
        int retryAfterSeconds = -1;
        QByteArray rangeBytes;
    };

    explicit MailWebDavProbe(QObject *parent = nullptr);
    ~MailWebDavProbe() override;

    bool listAllMusic(const QString &email, const QString &appPassword);
    bool preflight();
    bool checkRange(const Entry &entry);
    void cancel();
    bool busy() const { return active; }

    static TlsCapabilities tlsCapabilities();
    static NetworkCategory networkCategoryForError(QNetworkReply::NetworkError error);

signals:
    void completed(const MailWebDavProbe::Result &result);

private:
    // The alternate origin exists only for a friend test fixture. Normal callers
    // can construct only the pinned HTTPS client above.
    struct TestOriginTag {};
    MailWebDavProbe(const QUrl &origin, TestOriginTag, QObject *parent = nullptr);
    friend class MailWebDavProbeTests;
    friend class MailWebDavController;
    friend class MailWebDavRangeFetcher;

    enum class Operation { Preflight, List, Range };
    bool begin(Operation operation, const QString &path, qint64 knownSize,
               const QByteArray &etag, qint64 offset = 0, qint64 length = 0,
               bool returnRangeBytes = false);
    bool beginPreflight();
    bool listDirectory(const QString &path);
    bool readRange(const Entry &entry, qint64 offset, qint64 length);
    bool setSessionCredentials(QString userEmail, QString appPassword);
    void send();
    void finish(Error error);
    void captureResponseMetadata(QNetworkReply *reply);
    void clearResponseMetadata();
    void parseListing();
    void clearCredentials();
    void initialize();
    bool isCurrent(QNetworkReply *candidate) const;
    void captureTlsCapabilities();
    void captureNetworkError(QNetworkReply *reply);

    QNetworkAccessManager manager;
    QUrl origin;
    QNetworkReply *reply = nullptr;
    QTimer deadline;
    Result result;
    QByteArray body;
    QString email;
    QString password;
    QString requestPath;
    QString listingPath;
    QByteArray expectedEtag;
    qint64 rangeOffset = 0;
    bool returnRangeBytes = false;
    bool authorizationSent = false;
    bool basicChallengeConfirmed = false;
    qint64 knownSize = -1;
    qint64 requestedLength = 0;
    quint64 operationId = 0;
    int authChallenges = 0;
    int timeoutMs = 15000;
    bool active = false;
    bool tlsFailure = false;
    bool testOrigin = false;
};

Q_DECLARE_METATYPE(MailWebDavProbe::Result)
