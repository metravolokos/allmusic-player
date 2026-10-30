#include "MailWebDavProbe.h"

#include "WebDavProbe.h"

#include <QNetworkRequest>
#include <QNetworkProxy>
#include <QRegularExpression>
#include <QSslError>
#include <QStringList>
#include <QXmlStreamReader>
#include <limits>

namespace {
const QUrl MailOrigin(QStringLiteral("https://webdav.cloud.mail.ru"));
const QString MusicRoot = QStringLiteral("/AllMusic/");
constexpr qint64 ListingLimit = 4 * 1024 * 1024;
constexpr qint64 RangeLimit = 64 * 1024;
constexpr int EntryLimit = 5000;
constexpr qsizetype PathLimit = 8192;
constexpr qsizetype EtagLimit = 1024;

bool isStrongEtag(const QByteArray &tag)
{
    if (tag.size() < 2 || tag.size() > EtagLimit
        || tag.front() != '"' || tag.back() != '"')
        return false;
    for (qsizetype i = 1; i < tag.size() - 1; ++i) {
        const auto c = static_cast<unsigned char>(tag[i]);
        if (c < 0x21 || c == 0x22 || c == 0x7f)
            return false;
    }
    return true;
}

std::optional<qint64> decimal(const QByteArray &raw)
{
    static const QRegularExpression digits(QStringLiteral("^[0-9]+$"));
    const QByteArray value = raw.trimmed();
    if (!digits.match(QString::fromLatin1(value)).hasMatch())
        return {};
    bool ok = false;
    const qint64 parsed = value.toLongLong(&ok);
    if (!ok || parsed < 0)
        return {};
    return parsed;
}

struct ParsedContentRange {
    qint64 first = 0;
    qint64 last = 0;
    std::optional<qint64> total;
};

// RFC 9110 14.4: bytes SP first-last/(complete-length / "*").
// Diagnostics use fixed labels only, bounded before regex/conversion. They are
// deliberately independent of acceptance and cannot authorize a Range response.
QString classifyContentRange(const QByteArray &raw, bool repeated)
{
    if (repeated)
        return QStringLiteral("повторённое поле");
    if (raw.size() > 256)
        return QStringLiteral("длина: более 256 байт");
    const QByteArray value = raw.trimmed();
    for (const unsigned char c : raw) {
        if ((c < 0x20 && c != '\t') || c >= 0x7f)
            return QStringLiteral("непечатаемые или не-ASCII символы");
    }
    if (value.contains(','))
        return QStringLiteral("список/объединённые поля (запятая)");
    QStringList labels;
    labels << QStringLiteral("длина: до 256 байт");
    const bool bytes = value.left(5).compare("bytes", Qt::CaseInsensitive) == 0;
    labels << (bytes ? QStringLiteral("unit: bytes") : QStringLiteral("unit: другой/неизвестный"));
    if (!bytes)
        return labels.join(QStringLiteral("; "));
    const QByteArray tail = value.mid(5);
    labels << (tail.startsWith(' ') ? QStringLiteral("разделитель: пробел")
        : tail.startsWith('\t') ? QStringLiteral("разделитель: табуляция")
        : tail.startsWith('=') ? QStringLiteral("разделитель: равно")
        : QStringLiteral("разделитель: отсутствует/другой"));
    static const QRegularExpression shape(
        QStringLiteral("^[ \t=]*([0-9]+)-([0-9]+)/([0-9]+|\\*)$"));
    static const QRegularExpression unsatisfied(QStringLiteral("^ +\\*/([0-9]+)$"));
    const auto match = shape.match(QString::fromLatin1(tail));
    if (match.hasMatch()) {
        bool overflow = false;
        for (int i = 1; i <= 3; ++i) {
            if (match.captured(i) != QStringLiteral("*")
                && !decimal(match.captured(i).toLatin1()))
                overflow = true;
        }
        labels << (overflow ? QStringLiteral("числа: переполнение qint64")
            : (match.captured(3) == QStringLiteral("*")
                ? QStringLiteral("границы: числовые; знаменатель: звёздочка")
                : QStringLiteral("границы: числовые; знаменатель: числовой")));
    } else if (unsatisfied.match(QString::fromLatin1(tail)).hasMatch()) {
        labels << QStringLiteral("конструкция: неудовлетворимый диапазон */число");
    } else {
        labels << QStringLiteral("конструкция: иная/нечисловая");
    }
    return labels.join(QStringLiteral("; "));
}

std::optional<ParsedContentRange> parseContentRange(const QByteArray &raw)
{
    if (raw.size() > 256)
        return {};
    // Only HTTP OWS at the field edges. Never trim CR/LF or other controls.
    for (const unsigned char c : raw) {
        if ((c < 0x20 && c != '\t') || c >= 0x7f)
            return {};
    }
    static const QRegularExpression pattern(
        QStringLiteral("\\Abytes +([0-9]+)-([0-9]+)/([0-9]+|\\*)\\z"),
        QRegularExpression::CaseInsensitiveOption);
    const auto match = pattern.match(QString::fromLatin1(raw.trimmed()));
    if (!match.hasMatch())
        return {};
    const auto first = decimal(match.captured(1).toLatin1());
    const auto last = decimal(match.captured(2).toLatin1());
    if (!first || !last)
        return {};
    ParsedContentRange parsed;
    parsed.first = *first;
    parsed.last = *last;
    const QString total = match.captured(3);
    if (total != QStringLiteral("*")) {
        parsed.total = decimal(total.toLatin1());
        if (!parsed.total)
            return {};
    }
    return parsed;
}

bool isAudioPath(const QString &path)
{
    const QString lower = path.toLower();
    return lower.endsWith(QStringLiteral(".mp3"))
        || lower.endsWith(QStringLiteral(".flac"))
        || lower.endsWith(QStringLiteral(".aac"))
        || lower.endsWith(QStringLiteral(".m4a"))
        || lower.endsWith(QStringLiteral(".mp4"));
}

MailWebDavProbe::Error httpError(int status)
{
    switch (status) {
    case 401: return MailWebDavProbe::Error::Auth;
    case 403: return MailWebDavProbe::Error::Forbidden;
    case 404: return MailWebDavProbe::Error::NotFound;
    case 408: return MailWebDavProbe::Error::Timeout;
    case 416: return MailWebDavProbe::Error::RangeUnsupported;
    case 429: return MailWebDavProbe::Error::RateLimited;
    default:
        if (status >= 500 && status <= 599)
            return MailWebDavProbe::Error::Server;
        return MailWebDavProbe::Error::Http;
    }
}

QString encodedReference(const QString &path)
{
    QUrl url;
    url.setPath(path, QUrl::DecodedMode);
    return url.path(QUrl::FullyEncoded);
}
} // namespace

MailWebDavProbe::MailWebDavProbe(QObject *parent)
    : QObject(parent), origin(MailOrigin)
{
    initialize();
}

MailWebDavProbe::MailWebDavProbe(const QUrl &testUrl, TestOriginTag, QObject *parent)
    : QObject(parent), origin(testUrl), testOrigin(true)
{
    initialize();
    manager.setProxy(QNetworkProxy::NoProxy);
}

void MailWebDavProbe::initialize()
{
    deadline.setSingleShot(true);
    connect(&deadline, &QTimer::timeout, this, [this] { finish(Error::Timeout); });
}

MailWebDavProbe::~MailWebDavProbe()
{
    cancel();
    clearCredentials();
}

void MailWebDavProbe::clearCredentials()
{
    email.fill(QChar(u'\0'));
    email.clear();
    password.fill(QChar(u'\0'));
    password.clear();
}

bool MailWebDavProbe::listAllMusic(const QString &userEmail, const QString &appPassword)
{
    if (active)
        return false;
    if (userEmail.isEmpty() || userEmail.size() > 320 || appPassword.size() > 256
        || userEmail != userEmail.trimmed()
        || appPassword.isEmpty() || userEmail.contains(QChar(u'\r'))
        || userEmail.contains(QChar(u'\n')) || appPassword.contains(QChar(u'\r'))
        || appPassword.contains(QChar(u'\n'))) {
        result = {};
        result.listing = true;
        active = true;
        const quint64 ticket = ++operationId;
        QTimer::singleShot(0, this, [this, ticket] {
            if (active && operationId == ticket)
                finish(Error::InvalidCredentials);
        });
        return true;
    }
    email = userEmail;
    password = appPassword;
    return begin(Operation::List, MusicRoot, -1, {});
}

bool MailWebDavProbe::checkRange(const Entry &entry)
{
    if (active)
        return false;
    if (entry.directory || !isAudioPath(entry.path)
        || (entry.size && *entry.size <= 0))
        return false;
    return begin(Operation::Range, entry.reference, entry.size.value_or(-1), entry.etag);
}

bool MailWebDavProbe::begin(Operation operation, const QString &path, qint64 fileSize,
                            const QByteArray &etag)
{
    if (active)
        return false;
    result = {};
    result.listing = operation == Operation::List;
    result.rangeCheck = operation == Operation::Range;
    result.requestedRangeBytes = result.rangeCheck
        ? qMin(RangeLimit, fileSize >= 0 ? fileSize : RangeLimit) : 0;
    active = true;
    const quint64 ticket = ++operationId;

    const bool pinned = origin.scheme() == QStringLiteral("https")
        && origin.host() == QStringLiteral("webdav.cloud.mail.ru")
        && origin.port(443) == 443 && origin.userInfo().isEmpty()
        && !origin.hasQuery() && !origin.hasFragment();
    if ((!testOrigin && !pinned) || (testOrigin && !origin.isValid())) {
        QTimer::singleShot(0, this, [this, ticket] {
            if (active && operationId == ticket)
                finish(Error::UnsafePath);
        });
        return true;
    }

    const auto checked = WebDavProbe::safePath(path, origin);
    if (!checked || checked->size() > PathLimit
        || (operation == Operation::List && *checked != MusicRoot)
        || (operation == Operation::Range && !checked->startsWith(MusicRoot))) {
        QTimer::singleShot(0, this, [this, ticket] {
            if (active && operationId == ticket)
                finish(Error::UnsafePath);
        });
        return true;
    }

    requestPath = *checked;
    knownSize = fileSize;
    expectedEtag = isStrongEtag(etag) ? etag : QByteArray();
    requestedLength = operation == Operation::Range
        ? qMin(RangeLimit, fileSize >= 0 ? fileSize : RangeLimit) : 0;
    authChallenges = 0;
    authorizationSent = false;
    result.ifRangeSent = operation == Operation::Range && !expectedEtag.isEmpty();
    body.clear();
    tlsFailure = false;
    deadline.start(timeoutMs);
    send();
    return true;
}

bool MailWebDavProbe::isCurrent(QNetworkReply *candidate) const
{
    return candidate && candidate == reply;
}

void MailWebDavProbe::captureResponseMetadata(QNetworkReply *source)
{
    if (!source)
        return;
    result.status = source->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    const QByteArray contentRange = source->rawHeader("Content-Range");
    result.contentRangePresent = source->hasRawHeader("Content-Range");
    int rangeFields = 0;
    for (const auto &pair : source->rawHeaderPairs()) {
        if (pair.first.compare("Content-Range", Qt::CaseInsensitive) == 0)
            ++rangeFields;
    }
    // Qt may coalesce duplicate fields into a comma-separated rawHeader value.
    result.contentRangeRepeated = rangeFields > 1 || contentRange.contains(',');
    result.contentRangeSyntax = result.contentRangePresent
        ? classifyContentRange(contentRange, rangeFields > 1) : QStringLiteral("поле отсутствует");
    result.rangeFirst.reset();
    result.rangeLast.reset();
    result.rangeExpectedBytes.reset();
    result.totalSize.reset();
    result.contentRangeUnsatisfied = false;
    if (const auto parsed = result.contentRangeRepeated
            ? std::optional<ParsedContentRange>() : parseContentRange(contentRange)) {
        result.rangeFirst = parsed->first;
        result.rangeLast = parsed->last;
        if (parsed->last >= parsed->first
            && parsed->last - parsed->first < std::numeric_limits<qint64>::max()) {
            result.rangeExpectedBytes = parsed->last - parsed->first + 1;
        }
        result.totalSize = parsed->total;
    } else if (result.contentRangePresent) {
        static const QRegularExpression unsatisfiedPattern(
            QStringLiteral("^bytes\\s+\\*/([0-9]+)$"),
            QRegularExpression::CaseInsensitiveOption);
        const auto unsatisfied = unsatisfiedPattern.match(
            QString::fromLatin1(contentRange.trimmed()));
        if (unsatisfied.hasMatch()) {
            result.totalSize = decimal(unsatisfied.captured(1).toLatin1());
            result.contentRangeUnsatisfied = result.totalSize.has_value();
        }
    }

    const QByteArray contentLength = source->rawHeader("Content-Length");
    result.contentLengthPresent = source->hasRawHeader("Content-Length");
    result.declaredContentLength = result.contentLengthPresent
        ? decimal(contentLength) : std::optional<qint64>();

    const QByteArray transferEncoding = source->rawHeader("Transfer-Encoding").trimmed();
    result.transferEncodingPresent = source->hasRawHeader("Transfer-Encoding");
    result.transferEncodingChunked = result.transferEncodingPresent
        && transferEncoding.compare("chunked", Qt::CaseInsensitive) == 0;

    const QByteArray contentEncoding = source->rawHeader("Content-Encoding").trimmed();
    result.contentEncodingPresent = source->hasRawHeader("Content-Encoding");
    result.contentEncodingIdentity = !result.contentEncodingPresent
        || contentEncoding.compare("identity", Qt::CaseInsensitive) == 0;

    const QByteArray responseEtag = source->rawHeader("ETag");
    result.etagPresent = source->hasRawHeader("ETag");
    result.strongEtag = result.etagPresent && isStrongEtag(responseEtag);
    result.etagMatchesIfRange = !result.ifRangeSent || !result.etagPresent
        || responseEtag == expectedEtag;
}

void MailWebDavProbe::clearResponseMetadata()
{
    result.status = 0;
    result.bytesReceived = 0;
    result.bytesReceivedIsLowerBound = false;
    result.rangeFirst.reset();
    result.rangeLast.reset();
    result.rangeExpectedBytes.reset();
    result.totalSize.reset();
    result.declaredContentLength.reset();
    result.contentRangeSyntax.clear();
    result.contentRangeRepeated = false;
    result.contentRangePresent = false;
    result.contentRangeUnsatisfied = false;
    result.contentLengthPresent = false;
    result.transferEncodingPresent = false;
    result.transferEncodingChunked = false;
    result.contentEncodingPresent = false;
    result.contentEncodingIdentity = true;
    result.etagPresent = false;
    result.strongEtag = false;
    result.etagMatchesIfRange = true;
    result.rangeIssue = RangeIssue::None;
}

void MailWebDavProbe::send()
{
    if (!active)
        return;

    QUrl url = origin;
    url.setPath(requestPath, QUrl::DecodedMode);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute,
                         QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute,
                         QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute,
                         QNetworkRequest::Manual);
    request.setRawHeader("Accept-Encoding", "identity");
    if (authorizationSent) {
        QByteArray userBytes = email.toUtf8();
        QByteArray passwordBytes = password.toUtf8();
        QByteArray credentials = userBytes;
        credentials.append(':');
        credentials.append(passwordBytes);
        userBytes.fill('\0');
        userBytes.clear();
        passwordBytes.fill('\0');
        passwordBytes.clear();
        QByteArray encoded = credentials.toBase64();
        credentials.fill('\0');
        credentials.clear();
        QByteArray authorization = "Basic " + encoded;
        encoded.fill('\0');
        encoded.clear();
        request.setRawHeader("Authorization", authorization);
        authorization.fill('\0');
        authorization.clear();
    }

    if (result.listing) {
        request.setRawHeader("Depth", "1");
        request.setRawHeader("Accept", "application/xml");
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          QStringLiteral("application/xml; charset=utf-8"));
        reply = manager.sendCustomRequest(request, "PROPFIND",
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
            "<d:propfind xmlns:d=\"DAV:\"><d:prop>"
            "<d:resourcetype/><d:getcontentlength/><d:getetag/>"
            "</d:prop></d:propfind>");
    } else {
        request.setRawHeader("Range", "bytes=0-"
            + QByteArray::number(requestedLength - 1));
        if (!expectedEtag.isEmpty())
            request.setRawHeader("If-Range", expectedEtag);
        reply = manager.get(request);
    }

    QNetworkReply *const sentReply = reply;
    reply->setReadBufferSize((result.listing ? ListingLimit : requestedLength) + 1);
    connect(sentReply, &QNetworkReply::sslErrors, this,
            [this, sentReply](const QList<QSslError> &) {
        if (!isCurrent(sentReply))
            return;
        tlsFailure = true;
        // Deliberately do not call ignoreSslErrors; the secret is never sent on
        // a connection whose certificate did not pass normal Qt validation.
        finish(Error::Tls);
    });

    connect(sentReply, &QNetworkReply::metaDataChanged, this, [this, sentReply] {
        if (!active || !isCurrent(sentReply))
            return;
        captureResponseMetadata(sentReply);
        const int status = result.status;

        if ((status >= 300 && status < 400)) {
            finish(Error::Redirect);
            return;
        }
        if (status == 200 && !result.listing) {
            result.rangeIssue = RangeIssue::RangeIgnored;
            finish(expectedEtag.isEmpty() ? Error::RangeUnsupported : Error::VersionChanged);
            return;
        }
        if (status == 416 && !result.listing)
            result.rangeIssue = RangeIssue::RangeUnsatisfiable;
        if (status != 401 && status != (result.listing ? 207 : 206)) {
            finish(httpError(status));
            return;
        }
        if (status == 401)
            return; // The explicit one-time retry is handled after this response completes.

        if (!result.contentEncodingIdentity) {
            if (result.rangeCheck)
                result.rangeIssue = RangeIssue::UnsupportedContentEncoding;
            finish(Error::Protocol);
            return;
        }
        if (result.contentLengthPresent && !result.declaredContentLength) {
            if (result.rangeCheck)
                result.rangeIssue = RangeIssue::InvalidContentLength;
            finish(Error::Protocol);
            return;
        }
        if (result.rangeCheck && result.transferEncodingPresent
            && result.contentLengthPresent) {
            result.rangeIssue = RangeIssue::ConflictingMessageFraming;
            finish(Error::Protocol);
            return;
        }
        if (result.rangeCheck && result.transferEncodingPresent
            && !result.transferEncodingChunked) {
            result.rangeIssue = RangeIssue::UnsupportedTransferEncoding;
            finish(Error::Protocol);
            return;
        }
        const qint64 cap = result.listing ? ListingLimit : requestedLength;
        if (result.declaredContentLength && *result.declaredContentLength > cap) {
            if (result.rangeCheck)
                result.rangeIssue = RangeIssue::TooLarge;
            finish(Error::TooLarge);
            return;
        }
    });

    connect(sentReply, &QNetworkReply::readyRead, this, [this, sentReply] {
        if (!active || !isCurrent(sentReply))
            return;
        const int status = sentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const int expectedStatus = result.listing ? 207 : 206;
        if (status != expectedStatus) {
            sentReply->readAll(); // Discard challenge/error bodies; never retain them.
            return;
        }
        const qint64 limit = result.listing ? ListingLimit : requestedLength;
        if (sentReply->bytesAvailable() > limit - body.size()) {
            if (result.rangeCheck) {
                const qint64 observedLimit = limit + 1;
                result.bytesReceived = qMin(observedLimit,
                    body.size() + sentReply->bytesAvailable());
                result.bytesReceivedIsLowerBound = true;
                result.rangeIssue = RangeIssue::TooLarge;
            }
            finish(Error::TooLarge);
            return;
        }
        body += sentReply->readAll();
        if (result.rangeCheck)
            result.bytesReceived = body.size();
        if (body.size() > limit)
            finish(Error::TooLarge);
    });

    connect(sentReply, &QNetworkReply::finished, this, [this, sentReply] {
        if (!active || !isCurrent(sentReply))
            return;

        captureResponseMetadata(sentReply);
        const QByteArray contentRangeValue = sentReply->rawHeader("Content-Range");
        const QByteArray responseEtag = sentReply->rawHeader("ETag");
        const int status = result.status;
        if (result.rangeCheck && status == 206) {
            const qint64 available = sentReply->bytesAvailable();
            if (available > requestedLength - body.size()) {
                result.bytesReceived = qMin(requestedLength + 1, body.size() + available);
                result.bytesReceivedIsLowerBound = true;
                result.rangeIssue = RangeIssue::TooLarge;
                finish(Error::TooLarge);
                return;
            }
            body += sentReply->readAll();
            result.bytesReceived = body.size();
        }
        const auto networkError = sentReply->error();
        if (status == 401) {
            const QByteArray challenge = sentReply->rawHeader("WWW-Authenticate");
            static const QRegularExpression basicChallenge(
                QStringLiteral("(?:^|,)\\s*basic(?:\\s|$)"),
                QRegularExpression::CaseInsensitiveOption);
            const bool supportsBasic = basicChallenge.match(
                QString::fromLatin1(challenge)).hasMatch();
            if (!authorizationSent && authChallenges == 0 && supportsBasic
                && !email.isEmpty() && !password.isEmpty()) {
                ++authChallenges;
                authorizationSent = true;
                body.fill('\0');
                body.clear();
                QNetworkReply *old = reply;
                reply = nullptr;
                old->disconnect(this);
                old->deleteLater();
                clearResponseMetadata();
                send();
                return;
            }
            finish(Error::Auth);
            return;
        }
        if (status >= 300 && status < 400) {
            finish(Error::Redirect);
            return;
        }
        if (status == 416 && result.rangeCheck)
            result.rangeIssue = RangeIssue::RangeUnsatisfiable;
        if (status == 403 || status == 404 || status == 408
            || status == 429 || status >= 500) {
            finish(httpError(status));
            return;
        }
        if (networkError != QNetworkReply::NoError) {
            if (tlsFailure || networkError == QNetworkReply::SslHandshakeFailedError) {
                finish(Error::Tls);
            } else if (networkError == QNetworkReply::TimeoutError) {
                finish(Error::Timeout);
            } else if (result.rangeCheck && status == 206) {
                result.rangeIssue = RangeIssue::IncompleteTransfer;
                finish(Error::Protocol);
            } else {
                finish(Error::Network);
            }
            return;
        }
        if (!result.contentEncodingIdentity) {
            if (result.rangeCheck)
                result.rangeIssue = RangeIssue::UnsupportedContentEncoding;
            finish(Error::Protocol);
            return;
        }

        if (result.listing) {
            if (status != 207) {
                finish(Error::Http);
                return;
            }
            if (result.contentLengthPresent
                && (!result.declaredContentLength
                    || *result.declaredContentLength != body.size())) {
                finish(Error::Protocol);
                return;
            }
            parseListing();
            return;
        }

        if (status == 200) {
            result.rangeIssue = RangeIssue::RangeIgnored;
            finish(expectedEtag.isEmpty() ? Error::RangeUnsupported : Error::VersionChanged);
            return;
        }
        if (status != 206) {
            finish(Error::Http);
            return;
        }
        if (!result.contentRangePresent) {
            result.rangeIssue = RangeIssue::MissingContentRange;
            finish(Error::Protocol);
            return;
        }
        const auto parsedRange = parseContentRange(contentRangeValue);
        if (!parsedRange || result.contentRangeRepeated) {
            result.rangeIssue = RangeIssue::InvalidContentRange;
            finish(Error::Protocol);
            return;
        }
        if (parsedRange->last < parsedRange->first
            || parsedRange->last == std::numeric_limits<qint64>::max()
            || (parsedRange->total
                && (*parsedRange->total <= 0 || parsedRange->last >= *parsedRange->total))) {
            result.rangeIssue = RangeIssue::InvalidRangeBounds;
            finish(Error::Protocol);
            return;
        }
        if (parsedRange->first != 0) {
            result.rangeIssue = RangeIssue::RangeStartMismatch;
            finish(Error::Protocol);
            return;
        }
        if (parsedRange->total && knownSize >= 0 && *parsedRange->total != knownSize) {
            result.rangeIssue = RangeIssue::TotalSizeMismatch;
            finish(Error::VersionChanged);
            return;
        }
        const qint64 completeSize = parsedRange->total.value_or(knownSize);
        const qint64 expectedLast = completeSize >= 0
            ? qMin(requestedLength - 1, completeSize - 1)
            : requestedLength - 1;
        if (parsedRange->last != expectedLast) {
            result.rangeIssue = RangeIssue::RangeEndMismatch;
            finish(Error::Protocol);
            return;
        }

        result.rangeFirst = parsedRange->first;
        result.rangeLast = parsedRange->last;
        const qint64 expectedBytes = expectedLast + 1;
        result.rangeExpectedBytes = expectedBytes;
        if (result.ifRangeSent && result.etagPresent && !result.etagMatchesIfRange) {
            result.rangeIssue = RangeIssue::EtagMismatch;
            finish(Error::VersionChanged);
            return;
        }
        if (result.contentLengthPresent) {
            if (!result.declaredContentLength) {
                result.rangeIssue = RangeIssue::InvalidContentLength;
                finish(Error::Protocol);
                return;
            }
            if (*result.declaredContentLength != expectedBytes) {
                result.rangeIssue = RangeIssue::ContentLengthMismatch;
                finish(Error::Protocol);
                return;
            }
        }
        if (body.size() != expectedBytes) {
            result.rangeIssue = RangeIssue::BodyLengthMismatch;
            finish(Error::Protocol);
            return;
        }
        result.bytesReceived = body.size();
        // RFC 9110 does not require Content-Length on a 206. QNetworkReply
        // completes only after the active HTTP framing (length, chunked coding,
        // close-delimiting, or protocol frames) has ended; Content-Range and
        // the delivered identity bytes provide the independent size check.
        finish(Error::None);
    });
}

void MailWebDavProbe::parseListing()
{
    QXmlStreamReader xml(body);
    bool rootSeen = false;
    int responseCount = 0;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isDTD() || xml.isEntityReference()) {
            finish(Error::Protocol);
            return;
        }
        if (!xml.isStartElement())
            continue;
        if (!rootSeen) {
            if (xml.name() != u"multistatus" || xml.namespaceUri() != u"DAV:") {
                finish(Error::Protocol);
                return;
            }
            rootSeen = true;
            continue;
        }
        if (xml.name() != u"response" || xml.namespaceUri() != u"DAV:") {
            xml.skipCurrentElement();
            continue;
        }
        if (++responseCount > EntryLimit) {
            finish(Error::TooLarge);
            return;
        }

        Entry entry;
        QString href;
        bool responseOk = true;
        while (xml.readNextStartElement()) {
            if (xml.namespaceUri() != u"DAV:") {
                xml.skipCurrentElement();
                continue;
            }
            if (xml.name() == u"href") {
                href = xml.readElementText();
                continue;
            }
            if (xml.name() == u"status") {
                responseOk = xml.readElementText().contains(QStringLiteral(" 200 "));
                continue;
            }
            if (xml.name() != u"propstat") {
                xml.skipCurrentElement();
                continue;
            }

            Entry properties;
            QString propStatus;
            while (xml.readNextStartElement()) {
                if (xml.namespaceUri() != u"DAV:") {
                    xml.skipCurrentElement();
                    continue;
                }
                if (xml.name() == u"status") {
                    propStatus = xml.readElementText();
                    continue;
                }
                if (xml.name() != u"prop") {
                    xml.skipCurrentElement();
                    continue;
                }
                while (xml.readNextStartElement()) {
                    if (xml.namespaceUri() != u"DAV:") {
                        xml.skipCurrentElement();
                        continue;
                    }
                    if (xml.name() == u"resourcetype") {
                        while (xml.readNextStartElement()) {
                            if (xml.namespaceUri() == u"DAV:" && xml.name() == u"collection")
                                properties.directory = true;
                            xml.skipCurrentElement();
                        }
                    } else if (xml.name() == u"getcontentlength") {
                        const auto size = decimal(xml.readElementText().toLatin1());
                        if (size)
                            properties.size = *size;
                    } else if (xml.name() == u"getetag") {
                        properties.etag = xml.readElementText().toUtf8();
                    } else {
                        xml.skipCurrentElement();
                    }
                }
            }
            if (propStatus.contains(QStringLiteral(" 200 "))) {
                entry.directory |= properties.directory;
                if (properties.size)
                    entry.size = properties.size;
                if (!properties.etag.isEmpty())
                    entry.etag = properties.etag;
            }
        }

        QUrl base = origin;
        base.setPath(MusicRoot, QUrl::DecodedMode);
        if (href.size() > PathLimit) {
            ++result.rejectedEntries;
            continue;
        }
        const auto checked = WebDavProbe::safePath(href, base);
        if (!responseOk || !checked) {
            ++result.rejectedEntries;
            continue;
        }

        QString candidate = *checked;
        if (candidate.endsWith(u'/'))
            candidate.chop(1);
        const QString root = QStringLiteral("/AllMusic");
        if (candidate == root)
            continue;
        const QString prefix = root + u'/';
        if (!candidate.startsWith(prefix) || candidate.mid(prefix.size()).contains(u'/')) {
            ++result.rejectedEntries;
            continue;
        }
        entry.path = *checked;
        entry.reference = encodedReference(*checked);
        result.entries.push_back(entry);
        if (result.entries.size() > EntryLimit) {
            finish(Error::TooLarge);
            return;
        }
    }
    finish(xml.hasError() || !rootSeen ? Error::Protocol : Error::None);
}

void MailWebDavProbe::finish(Error error)
{
    if (!active)
        return;
    if (result.rangeCheck && result.bytesReceived < body.size())
        result.bytesReceived = body.size();
    active = false;
    deadline.stop();
    if (reply) {
        QNetworkReply *old = reply;
        reply = nullptr;
        old->disconnect(this);
        old->abort();
        old->deleteLater();
    }
    result.error = error;
    if (error != Error::None) {
        result.entries.clear();
        if (!result.rangeCheck) {
            result.bytesReceived = 0;
            result.totalSize.reset();
        }
    }
    body.fill('\0');
    body.clear();
    if (error != Error::None && (result.listing || error == Error::Auth || error == Error::Tls))
        clearCredentials();
    emit completed(result);
}

void MailWebDavProbe::cancel()
{
    if (active)
        finish(Error::Cancelled);
}
