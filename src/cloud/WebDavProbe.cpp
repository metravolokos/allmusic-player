#include "WebDavProbe.h"
#include <QNetworkProxy>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QSslError>
#include <limits>

namespace {
constexpr qint64 BodyLimit = 4 * 1024 * 1024;
bool strong(const QByteArray &tag) {
    if (tag.size() < 2 || tag.front() != '"' || tag.back() != '"') return false;
    for (qsizetype i = 1; i < tag.size() - 1; ++i) {
        const auto c = static_cast<unsigned char>(tag[i]);
        if (c < 0x21 || c == 0x22 || c == 0x7f) return false;
    }
    return true;
}
bool retryable(int code) { return code == 408 || code == 429 || (code >= 500 && code <= 599); }
}

WebDavProbe::WebDavProbe(quint16 port, bool tls, QObject *parent) : QObject(parent) {
    origin.setScheme(tls ? "https" : "http");
    origin.setHost("127.0.0.1");
    origin.setPort(port);
    origin.setPath("/AllMusic/");
    manager.setProxy(QNetworkProxy::NoProxy); // Only literal loopback, never user destinations.
    deadline.setSingleShot(true);
    retry.setSingleShot(true);
    connect(&deadline, &QTimer::timeout, this, [this] { finish(Error::Timeout); });
    connect(&retry, &QTimer::timeout, this, &WebDavProbe::send);
}

std::optional<QString> WebDavProbe::safePath(const QString &ref, const QUrl &base) {
    // Validate the raw reference before QUrl can remove dot segments or repair escapes.
    if (ref.isEmpty() || ref != ref.trimmed()) return {};
    for (const QChar c : ref) if (c.unicode() < 32 || c.unicode() == 127 || c == '\\') return {};
    if (ref.contains('?') || ref.contains('#')) return {};
    for (qsizetype i = 0; i < ref.size(); ++i) {
        if (ref[i] != '%') continue;
        if (i + 2 >= ref.size()) return {};
        const QString hex = ref.mid(i + 1, 2);
        bool ok = false;
        const int byte = hex.toInt(&ok, 16);
        if (!ok || byte == '/' || byte == '\\' || byte < 32 || byte == 127) return {};
        i += 2;
    }
    const QUrl url(ref, QUrl::StrictMode);
    if (!url.isValid() || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment()) return {};
    if (!url.isRelative()) {
        if (url.scheme() != base.scheme() || url.host() != base.host()
            || url.port(url.scheme() == "https" ? 443 : 80) != base.port()) return {};
    } else if (!url.authority().isEmpty()) return {};
    QString path = url.path(QUrl::FullyDecoded);
    if (!path.startsWith('/')) path = base.path() + path;
    if (path != "/AllMusic" && !path.startsWith("/AllMusic/")) return {};
    if (path.contains(QChar::ReplacementCharacter) || path.contains('\\')
        || path.contains(QRegularExpression("%[0-9a-fA-F]{2}"))) return {};
    for (const auto &part : path.split('/')) {
        if (part == "." || part == "..") return {};
        for (const QChar c : part) if (c.unicode() < 32 || c.unicode() == 127) return {};
    }
    if (path.contains("//")) return {};
    return path;
}

bool WebDavProbe::begin(const QString &path, bool isListing) {
    if (active) return false; // No second operation, and no completion belonging to it.
    result = {};
    active = true;
    const auto ticket = ++operation;
    listing = isListing;
    const auto checked = safePath(path, origin);
    if (!checked) {
        QTimer::singleShot(0, this, [this, ticket] { if (active && operation == ticket) finish(Error::UnsafePath); });
        return true;
    }
    requestPath = *checked;
    deadline.start(timeoutMs); // Whole operation, including retries.
    send();
    return true;
}

bool WebDavProbe::listDirectory(const QString &path) {
    return begin(path, true);
}
bool WebDavProbe::readRange(const QString &path, qint64 offset, qint64 length, const QByteArray &tag) {
    if (active) return false;
    if (offset < 0 || length <= 0 || length > 1024 * 1024
        || offset > std::numeric_limits<qint64>::max() - length || (!tag.isEmpty() && !strong(tag))) {
        active = true; result = {};
        const auto ticket = ++operation;
        QTimer::singleShot(0, this, [this, ticket] { if (active && operation == ticket) finish(Error::InvalidRequest); });
        return true;
    }
    start = offset; count = length; expectedEtag = tag;
    return begin(path, false);
}

void WebDavProbe::send() {
    if (!active) return;
    body.clear(); tlsFailure = false; ++result.attempts;
    QUrl url = origin; url.setPath(requestPath);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);
    request.setRawHeader("Accept-Encoding", "identity");
    if (listing) {
        request.setRawHeader("Depth", "1");
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/xml; charset=utf-8");
        reply = manager.sendCustomRequest(request, "PROPFIND",
            "<?xml version=\"1.0\"?><d:propfind xmlns:d=\"DAV:\"><d:prop>"
            "<d:resourcetype/><d:getcontentlength/><d:getetag/><d:getlastmodified/>"
            "</d:prop></d:propfind>");
    } else {
        request.setRawHeader("Range", "bytes=" + QByteArray::number(start) + "-" + QByteArray::number(start + count - 1));
        if (!expectedEtag.isEmpty()) request.setRawHeader("If-Range", expectedEtag);
        reply = manager.get(request);
    }
    reply->setReadBufferSize(64 * 1024);
    connect(reply, &QNetworkReply::sslErrors, this, [this](const QList<QSslError> &) {
        tlsFailure = true; // Observe only: never ignoreSslErrors or change the trust store.
    });
    connect(reply, &QNetworkReply::metaDataChanged, this, [this] {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (requireMockMarker && reply->rawHeader("X-AllMusic-Mock") != "001-B2-loopback") {
            result.status = status;
            finish(Error::Protocol);
            return;
        }
        if (!listing && status == 200) {
            result.status = status;
            finish(expectedEtag.isEmpty() ? Error::RangeUnavailable : Error::VersionChanged);
        }
    });
    connect(reply, &QNetworkReply::readyRead, this, [this] {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status != (listing ? 207 : 206)) { reply->readAll(); return; }
        const qint64 limit = listing ? BodyLimit : count;
        if (reply->bytesAvailable() > limit - body.size()) { finish(Error::TooLarge); return; }
        body += reply->readAll();
    });
    connect(reply, &QNetworkReply::finished, this, [this] {
        auto *finishedReply = reply;
        result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const int status = result.status;
        const auto networkError = reply->error();
        const auto contentRange = reply->rawHeader("Content-Range");
        const auto contentLength = reply->rawHeader("Content-Length");
        const auto contentEncoding = reply->rawHeader("Content-Encoding");
        const auto mockMarker = reply->rawHeader("X-AllMusic-Mock");
        result.etag = reply->rawHeader("ETag");
        result.strongEtag = strong(result.etag);
        const auto retryAfter = reply->rawHeader("Retry-After");
        if (reply->bytesAvailable() > (listing ? BodyLimit : count) - body.size()) {
            finish(Error::TooLarge); return;
        }
        body += reply->readAll();
        reply = nullptr; finishedReply->deleteLater();
        if (requireMockMarker && mockMarker != "001-B2-loopback") { finish(Error::Protocol); return; }
        if (status >= 300 && status <= 399) { finish(Error::Redirect); return; }
        if (retryable(status) && result.attempts < 3) {
            bool ok = false; const int seconds = retryAfter.toInt(&ok);
            // Long or HTTP-date Retry-After is reported, never retried too early.
            if (!retryAfter.isEmpty() && (!ok || seconds < 0 || seconds > 1)) {
                finish(status == 429 ? Error::RateLimited : Error::Server); return;
            }
            const int delay = ok ? seconds * 1000 : (50 << (result.attempts - 1));
            retry.start(delay + QRandomGenerator::global()->bounded(20)); return;
        }
        if (status == 401) { finish(Error::Auth); return; }
        if (status == 403) { finish(Error::Forbidden); return; }
        if (status == 404) { finish(Error::NotFound); return; }
        if (status == 416) { finish(Error::RangeUnavailable); return; }
        if (status == 429) { finish(Error::RateLimited); return; }
        if (status == 408) { finish(Error::Timeout); return; }
        if (status >= 500) { finish(Error::Server); return; }
        if (networkError != QNetworkReply::NoError) { finish(tlsFailure ? Error::Tls : Error::Network); return; }
        if (!contentEncoding.isEmpty() && contentEncoding != "identity") { finish(Error::Protocol); return; }
        if (listing) {
            if (status != 207) { finish(Error::Http); return; }
            parseListing(); return;
        }
        if (status == 200) { finish(expectedEtag.isEmpty() ? Error::RangeUnavailable : Error::VersionChanged); return; }
        if (status != 206) { finish(Error::Http); return; }
        if (!expectedEtag.isEmpty() && result.etag != expectedEtag) { finish(Error::VersionChanged); return; }
        const auto match = QRegularExpression("^bytes ([0-9]+)-([0-9]+)/([0-9]+)$").match(QString::fromLatin1(contentRange));
        bool a = false, b = false, c = false, d = false;
        const qint64 first = match.captured(1).toLongLong(&a), last = match.captured(2).toLongLong(&b);
        const qint64 total = match.captured(3).toLongLong(&c), declared = contentLength.toLongLong(&d);
        if (!match.hasMatch() || !a || !b || !c || first != start || last < first || total <= last
            || last != qMin(start + count - 1, total - 1) || body.size() != last - first + 1
            || (!contentLength.isEmpty() && (!d || declared != body.size()))) {
            finish(Error::Protocol); return;
        }
        result.totalSize = total; result.bytes = body; finish(Error::None);
    });
}

void WebDavProbe::finish(Error error) {
    if (!active) return;
    active = false; deadline.stop(); retry.stop();
    if (reply) {
        auto *old = reply; reply = nullptr;
        old->disconnect(this); old->abort(); old->deleteLater();
    }
    result.error = error;
    if (error != Error::None) { result.bytes.clear(); result.entries.clear(); }
    body.clear(); emit completed(result);
}
void WebDavProbe::cancel() { finish(Error::Cancelled); }

void WebDavProbe::parseListing() {
    QXmlStreamReader xml(body);
    bool root = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isDTD() || xml.isEntityReference()) { finish(Error::Protocol); return; }
        if (!xml.isStartElement()) continue;
        if (!root) {
            if (xml.name() != u"multistatus" || xml.namespaceUri() != u"DAV:") { finish(Error::Protocol); return; }
            root = true; continue;
        }
        if (xml.name() != u"response" || xml.namespaceUri() != u"DAV:") { xml.skipCurrentElement(); continue; }
        Entry entry; QString href; bool responseOk = true;
        while (xml.readNextStartElement()) {
            if (xml.namespaceUri() != u"DAV:") { xml.skipCurrentElement(); continue; }
            if (xml.name() == u"href") { href = xml.readElementText(); continue; }
            if (xml.name() == u"status") { responseOk = xml.readElementText().contains(" 200 "); continue; }
            if (xml.name() != u"propstat") { xml.skipCurrentElement(); continue; }
            Entry properties; QString status;
            while (xml.readNextStartElement()) {
                if (xml.namespaceUri() != u"DAV:") { xml.skipCurrentElement(); continue; }
                if (xml.name() == u"status") { status = xml.readElementText(); continue; }
                if (xml.name() != u"prop") { xml.skipCurrentElement(); continue; }
                while (xml.readNextStartElement()) {
                    if (xml.namespaceUri() != u"DAV:") { xml.skipCurrentElement(); continue; }
                    const auto name = xml.name().toString();
                    if (name == "resourcetype") {
                        while (xml.readNextStartElement()) {
                            if (xml.namespaceUri() == u"DAV:" && xml.name() == u"collection") properties.directory = true;
                            xml.skipCurrentElement();
                        }
                    } else if (name == "getcontentlength") {
                        bool ok = false; const auto size = xml.readElementText().toLongLong(&ok);
                        if (ok && size >= 0) properties.size = size;
                    } else if (name == "getetag") properties.etag = xml.readElementText().toUtf8();
                    else if (name == "getlastmodified") properties.lastModified = xml.readElementText();
                    else xml.skipCurrentElement();
                }
            }
            if (status.contains(" 200 ")) {
                entry.directory |= properties.directory;
                if (properties.size) entry.size = properties.size;
                if (!properties.etag.isEmpty()) entry.etag = properties.etag;
                if (!properties.lastModified.isEmpty()) entry.lastModified = properties.lastModified;
            }
        }
        QUrl base = origin; base.setPath(requestPath.endsWith('/') ? requestPath : requestPath + '/');
        const auto path = safePath(href, base);
        if (!responseOk || !path) { ++result.rejectedEntries; continue; }
        // Depth:1 may return self or immediate children, never unrelated descendants.
        QString directory = requestPath; if (directory.endsWith('/')) directory.chop(1);
        QString candidate = *path; if (candidate.endsWith('/')) candidate.chop(1);
        const QString prefix = directory + '/';
        if (candidate != directory && (!candidate.startsWith(prefix) || candidate.mid(prefix.size()).contains('/'))) {
            ++result.rejectedEntries; continue;
        }
        entry.path = *path;
        QUrl encoded; encoded.setPath(*path); entry.reference = encoded.path(QUrl::FullyEncoded);
        result.entries.push_back(entry);
        if (result.entries.size() > 10000) { finish(Error::TooLarge); return; }
    }
    finish(xml.hasError() || !root ? Error::Protocol : Error::None);
}
