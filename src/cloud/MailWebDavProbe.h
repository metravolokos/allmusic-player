#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>
#include <QVector>
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

    struct Entry {
        QString path;
        QString reference;
        bool directory = false;
        std::optional<qint64> size;
        QByteArray etag;
    };

    struct Result {
        Error error = Error::None;
        RangeIssue rangeIssue = RangeIssue::None;
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
    };

    explicit MailWebDavProbe(QObject *parent = nullptr);
    ~MailWebDavProbe() override;

    bool listAllMusic(const QString &email, const QString &appPassword);
    bool checkRange(const Entry &entry);
    void cancel();
    bool busy() const { return active; }

signals:
    void completed(const MailWebDavProbe::Result &result);

private:
    // The alternate origin exists only for a friend test fixture. Normal callers
    // can construct only the pinned HTTPS client above.
    struct TestOriginTag {};
    MailWebDavProbe(const QUrl &origin, TestOriginTag, QObject *parent = nullptr);
    friend class MailWebDavProbeTests;
    friend class MailWebDavController;

    enum class Operation { List, Range };
    bool begin(Operation operation, const QString &path, qint64 knownSize,
               const QByteArray &etag);
    void send();
    void finish(Error error);
    void captureResponseMetadata(QNetworkReply *reply);
    void clearResponseMetadata();
    void parseListing();
    void clearCredentials();
    void initialize();
    bool isCurrent(QNetworkReply *candidate) const;

    QNetworkAccessManager manager;
    QUrl origin;
    QNetworkReply *reply = nullptr;
    QTimer deadline;
    Result result;
    QByteArray body;
    QString email;
    QString password;
    QString requestPath;
    QByteArray expectedEtag;
    bool authorizationSent = false;
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
