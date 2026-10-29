#include "WebDavProbe.h"
#include "MockWebDav.h"
#include <QFile>
#include <QSignalSpy>
#include <QRegularExpression>
#include <QSslKey>
#include <QtTest>

using Error = WebDavProbe::Error;
using Result = WebDavProbe::Result;
using Response = MockWebDav::Response;

class WebDavTests : public QObject {
    Q_OBJECT
    MockWebDav *server = nullptr;
    WebDavProbe *client = nullptr;
    Result range(const QString &path = "/AllMusic/test.bin", qint64 offset = 2,
                 qint64 length = 4, const QByteArray &tag = {}) {
        QSignalSpy spy(client, &WebDavProbe::completed);
        if (!client->readRange(path, offset, length, tag) || !spy.wait(4000)) return Result{Error::Timeout};
        return qvariant_cast<Result>(spy.takeFirst().at(0));
    }
    Result list(const QString &path = "/AllMusic/") {
        QSignalSpy spy(client, &WebDavProbe::completed);
        if (!client->listDirectory(path) || !spy.wait(4000)) return Result{Error::Timeout};
        return qvariant_cast<Result>(spy.takeFirst().at(0));
    }
    static QByteArray response(const QByteArray &href, const QByteArray &props = "", int propStatus = 200) {
        return "<d:response><d:href>" + href + "</d:href><d:propstat><d:prop>" + props
            + "</d:prop><d:status>HTTP/1.1 " + QByteArray::number(propStatus)
            + " Fixture</d:status></d:propstat></d:response>";
    }
    static QByteArray multistatus(const QByteArray &entries) {
        return "<d:multistatus xmlns:d=\"DAV:\">" + entries + "</d:multistatus>";
    }
    static Response partial() {
        return {206, "2345", {{"Content-Range", "bytes 2-5/10"}, {"ETag", "\"v1\""}}};
    }
private slots:
    void init() {
        server = new MockWebDav;
        QVERIFY(server->start());
        client = new WebDavProbe(server->serverPort());
    }
    void cleanup() {
        // Wire audit on EVERY scenario, including all error and attack cases.
        for (const auto &r : server->requests) {
            QVERIFY2(r.method == "GET" || r.method == "PROPFIND", r.method.constData());
            QVERIFY2(r.target == "/AllMusic" || r.target.startsWith("/AllMusic/"), r.target.constData());
            QVERIFY(!r.target.contains('?')); QVERIFY(!r.target.contains('#'));
            const auto decoded = QUrl::fromPercentEncoding(r.target);
            QVERIFY(!decoded.contains('\\'));
            QVERIFY(!decoded.contains(QRegularExpression("%[0-9a-fA-F]{2}")));
            for (const auto &segment : decoded.split('/')) {
                QVERIFY(segment != "."); QVERIFY(segment != "..");
            }
            QVERIFY(!r.headers.contains("authorization"));
            QVERIFY(!r.headers.contains("cookie"));
        }
        delete client; client = nullptr;
        delete server; server = nullptr;
    }
    void listingProperties() {
        server->handler = [](const auto &) {
            Response r;
            r.body = multistatus(response("/AllMusic/", "<d:resourcetype><d:collection/></d:resourcetype>")
                + response("/AllMusic/%D0%A2%D0%B5%D1%81%D1%82%20%23100%25.bin",
                    "<d:getcontentlength>10</d:getcontentlength><d:getetag>&quot;v1&quot;</d:getetag>"
                    "<d:getlastmodified>Thu, 29 Oct 2026 00:00:00 GMT</d:getlastmodified>")
                + response("relative.bin", "<d:getcontentlength>999</d:getcontentlength>", 404)
                + response("/AllMusic/no-size.bin", "<d:getcontentlength>invalid</d:getcontentlength>"));
            return r;
        };
        const auto r = list(); QCOMPARE(r.error, Error::None); QCOMPARE(r.entries.size(), 4);
        QVERIFY(r.entries[0].directory);
        QCOMPARE(r.entries[1].path, QString::fromUtf8("/AllMusic/Тест #100%.bin"));
        QCOMPARE(r.entries[1].size.value(), 10); QCOMPARE(r.entries[1].etag, "\"v1\"");
        server->handler = [](const auto &) { return partial(); };
        QCOMPARE(range(r.entries[1].reference).error, Error::None);
        QVERIFY(!r.entries[1].lastModified.isEmpty());
        QVERIFY(!r.entries[2].size); QVERIFY(!r.entries[3].size);
        QCOMPARE(server->requests.size(), 2);
        QCOMPARE(server->requests[0].headers.value("depth"), "1");
        QVERIFY(server->requests[0].body.contains("getetag"));
    }
    void namespaceAndMultiplePropstats() {
        server->handler = [](const auto &) {
            Response r; r.body = "<multistatus xmlns=\"DAV:\"><response><href>/AllMusic/a</href>"
                "<propstat><prop><getcontentlength>10</getcontentlength></prop><status>HTTP/1.1 200 OK</status></propstat>"
                "<propstat><prop><getetag>bad</getetag></prop><status>HTTP/1.1 404 Missing</status></propstat>"
                "</response></multistatus>"; return r;
        };
        const auto r = list(); QCOMPARE(r.error, Error::None); QCOMPARE(r.entries.size(), 1);
        QCOMPARE(r.entries[0].size.value(), 10); QVERIFY(r.entries[0].etag.isEmpty());
    }
    void unsafePaths_data() {
        QTest::addColumn<QString>("path");
        const QStringList paths = {"/", "/Other/file", "/AllMusic-other/a", "/allmusic/a",
            "/AllMusic/../secret", "/AllMusic/./a", "/AllMusic/%2e%2E/x", "/AllMusic/%2fsecret",
            "/AllMusic/%5csecret", "/AllMusic/%252e%252e/x", "/AllMusic/%252fsecret", "/AllMusic/%00",
            "/AllMusic/%0a", "/AllMusic/%GG", "/AllMusic/%", "/AllMusic/%ff", "/AllMusic//a",
            "/AllMusic/a?token=value", "/AllMusic/a#fragment", "/AllMusic/\\a", " /AllMusic/a",
            "http://example.invalid/AllMusic/a", "https://127.0.0.1/AllMusic/a", "//example.invalid/AllMusic/a",
            "file:///AllMusic/a", "http://user:password@127.0.0.1/AllMusic/a", "/AllMusic/a\n"};
        for (int i = 0; i < paths.size(); ++i) QTest::newRow(qPrintable(QString::number(i))) << paths[i];
    }
    void unsafePaths() {
        QFETCH(QString, path);
        QCOMPARE(range(path).error, Error::UnsafePath);
        QCOMPARE(list(path).error, Error::UnsafePath);
        QCOMPARE(server->requests.size(), 0);
    }
    void validEncodedRequest() {
        server->handler = [](const auto &) { return partial(); };
        QCOMPARE(range(QString::fromUtf8("/AllMusic/Тест%20%23100%25.bin")).error, Error::None);
        QCOMPARE(server->requests[0].target, "/AllMusic/%D0%A2%D0%B5%D1%81%D1%82%20%23100%25.bin");
    }
    void hostileHrefs() {
        const auto own = QByteArray("http://127.0.0.1:") + QByteArray::number(server->serverPort());
        server->handler = [own](const auto &) {
            Response r; r.body = multistatus(response("/AllMusic/a") + response(own + "/AllMusic/b")
                + response("/AllMusic/../secret") + response("http://example.invalid/AllMusic/a")
                + response("/AllMusic-other/a") + response("/AllMusic/sub/deep.bin")
                + response("/AllMusic/%252e%252e/a") + response("//example.invalid/AllMusic/a")); return r;
        };
        const auto r = list(); QCOMPARE(r.error, Error::None); QCOMPARE(r.entries.size(), 2);
        QCOMPARE(r.rejectedEntries, 6); QCOMPARE(server->requests.size(), 1);
    }
    void invalidXml_data() {
        QTest::addColumn<QByteArray>("xml");
        QTest::newRow("truncated") << QByteArray("<d:multistatus xmlns:d=\"DAV:\"><d:response>");
        QTest::newRow("wrong-root") << QByteArray("<multistatus xmlns=\"wrong\"/>");
        QTest::newRow("entity") << QByteArray("<!DOCTYPE a [<!ENTITY e SYSTEM 'file:///etc/passwd'>]><multistatus xmlns=\"DAV:\"/>");
        QTest::newRow("empty") << QByteArray();
    }
    void invalidXml() {
        QFETCH(QByteArray, xml);
        server->handler = [xml](const auto &) { Response r; r.body = xml; return r; };
        const auto r = list(); QCOMPARE(r.error, Error::Protocol); QVERIFY(r.entries.isEmpty());
    }
    void exactRangeAndResume() {
        server->handler = [](const auto &) { return partial(); };
        const auto r = range("/AllMusic/test.bin", 2, 4, "\"v1\"");
        QCOMPARE(r.error, Error::None); QCOMPARE(r.bytes, "2345"); QCOMPARE(r.totalSize.value(), 10);
        QVERIFY(r.strongEtag); QCOMPARE(server->requests[0].headers.value("range"), "bytes=2-5");
        QCOMPARE(server->requests[0].headers.value("if-range"), "\"v1\"");
        QCOMPARE(server->requests[0].headers.value("accept-encoding"), "identity");
    }
    void endOfFileRange() {
        server->handler = [](const auto &) { return Response{206, "89", {{"Content-Range", "bytes 8-9/10"}}}; };
        const auto r = range("/AllMusic/test.bin", 8, 4);
        QCOMPARE(r.error, Error::None); QCOMPARE(r.bytes, "89");
    }
    void serverCannotEnableCookies() {
        server->handler = [](const auto &) { auto r = partial(); r.headers["Set-Cookie"] = "fixture=value; Path=/"; return r; };
        QCOMPARE(range().error, Error::None); QCOMPARE(range().error, Error::None);
        QCOMPARE(server->requests.size(), 2);
        QVERIFY(!server->requests[1].headers.contains("cookie"));
    }
    void rejectsEncodedBody() {
        server->handler = [](const auto &) { auto r = partial(); r.headers["Content-Encoding"] = "br"; return r; };
        const auto r = range(); QVERIFY(r.error == Error::Protocol || r.error == Error::Network);
        QVERIFY(r.bytes.isEmpty());
    }
    void cancelledValidationCannotFinishNextOperation() {
        server->handler = [](const auto &) { return partial(); };
        QSignalSpy spy(client, &WebDavProbe::completed);
        QVERIFY(client->readRange("/Outside/a", 0, 1));
        client->cancel(); QCOMPARE(spy.size(), 1);
        QVERIFY(client->readRange("/AllMusic/a", 2, 4));
        QVERIFY(spy.wait(1000)); QCOMPARE(spy.size(), 2);
        QCOMPARE(qvariant_cast<Result>(spy[1][0]).error, Error::None);
        QCOMPARE(server->requests.size(), 1);
    }
    void malformedRange_data() {
        QTest::addColumn<QByteArray>("header");
        for (const auto &s : {"bytes 3-6/10", "bytes 2-4/10", "bytes 2-5/5", "bytes 2-5/*", "",
                            "bytes 2-999999999999999999999/10", "bytes 2-5/999999999999999999999"})
            QTest::newRow(s) << QByteArray(s);
    }
    void malformedRange() {
        QFETCH(QByteArray, header);
        server->handler = [header](const auto &) { auto r = partial(); r.headers["Content-Range"] = header; return r; };
        const auto r = range(); QCOMPARE(r.error, Error::Protocol); QVERIFY(r.bytes.isEmpty());
    }
    void noRangeAndVersions() {
        server->handler = [](const auto &) { auto r = partial(); r.status = 200; return r; };
        QCOMPARE(range().error, Error::RangeUnavailable);
        QCOMPARE(range("/AllMusic/a", 2, 4, "\"v1\"").error, Error::VersionChanged);
        server->handler = [](const auto &) { auto r = partial(); r.headers["ETag"] = "\"v2\""; return r; };
        const auto changed = range("/AllMusic/a", 2, 4, "\"v1\"");
        QCOMPARE(changed.error, Error::VersionChanged); QVERIFY(changed.bytes.isEmpty());
        server->handler = [](const auto &) { auto r = partial(); r.headers.remove("ETag"); return r; };
        QCOMPARE(range("/AllMusic/a", 2, 4, "\"v1\"").error, Error::VersionChanged);
        QVERIFY(!range().strongEtag);
        server->handler = [](const auto &) { auto r = partial(); r.headers["ETag"] = "W/\"v1\""; return r; };
        QVERIFY(!range().strongEtag);
        const auto before = server->requests.size();
        QCOMPARE(range("/AllMusic/a", 2, 4, "W/\"v1\"").error, Error::InvalidRequest);
        QCOMPARE(server->requests.size(), before);
    }
    void rangeBounds() {
        for (auto [offset, length] : {std::pair<qint64,qint64>{-1, 1}, {0, 0}, {0, 1048577}, {INT64_MAX, 1}})
            QCOMPARE(range("/AllMusic/a", offset, length).error, Error::InvalidRequest);
        QCOMPARE(server->requests.size(), 0);
    }
    void redirects_data() {
        QTest::addColumn<int>("status"); QTest::addColumn<QByteArray>("location");
        for (int code : {301, 302, 303, 307, 308}) {
            for (const auto &url : {"/Outside/secret", "/AllMusic/../secret", "http://example.invalid/secret", "/AllMusic/safe"})
                QTest::newRow(qPrintable(QString::number(code) + url)) << code << QByteArray(url);
        }
    }
    void redirects() {
        QFETCH(int, status); QFETCH(QByteArray, location);
        MockWebDav trap; QVERIFY(trap.start());
        server->handler = [status, location](const auto &) { return Response{status, {}, {{"Location", location}}}; };
        QCOMPARE(list().error, Error::Redirect); QCOMPARE(server->requests.size(), 1);
        server->handler = [status, &trap](const auto &) { return Response{status, {}, {{"Location",
            "http://127.0.0.1:" + QByteArray::number(trap.serverPort()) + "/AllMusic/a"}}}; };
        QCOMPARE(range().error, Error::Redirect); QCOMPARE(trap.requests.size(), 0);
        QCOMPARE(server->requests.size(), 2);
    }
    void httpErrors_data() {
        QTest::addColumn<int>("status"); QTest::addColumn<Error>("error"); QTest::addColumn<int>("attempts");
        QTest::newRow("401") << 401 << Error::Auth << 1;
        QTest::newRow("403") << 403 << Error::Forbidden << 1;
        QTest::newRow("404") << 404 << Error::NotFound << 1;
        QTest::newRow("416") << 416 << Error::RangeUnavailable << 1;
        QTest::newRow("429") << 429 << Error::RateLimited << 3;
        QTest::newRow("500") << 500 << Error::Server << 3;
        QTest::newRow("503") << 503 << Error::Server << 3;
        QTest::newRow("408") << 408 << Error::Timeout << 3;
    }
    void httpErrors() {
        QFETCH(int, status); QFETCH(Error, error); QFETCH(int, attempts);
        server->handler = [status](const auto &) { return Response{status, {}, {{"Retry-After", "0"}}}; };
        const auto r = range(); QCOMPARE(r.error, error); QCOMPARE(r.attempts, attempts);
        QCOMPARE(server->requests.size(), attempts); QVERIFY(r.bytes.isEmpty());
    }
    void retryThenSuccess() {
        int n = 0;
        server->handler = [&n](const auto &) { if (++n < 3) return Response{503, {}}; return partial(); };
        const auto r = range(); QCOMPARE(r.error, Error::None); QCOMPARE(r.attempts, 3);
        QCOMPARE(r.bytes, "2345");
    }
    void retryAfterPolicy() {
        server->handler = [](const auto &) { return Response{429, {}, {{"Retry-After", "120"}}}; };
        auto r = range(); QCOMPARE(r.error, Error::RateLimited); QCOMPARE(r.attempts, 1);
        server->handler = [](const auto &) { return Response{429, {}, {{"Retry-After", "Thu, 29 Oct 2026 12:00:00 GMT"}}}; };
        r = range(); QCOMPARE(r.error, Error::RateLimited); QCOMPARE(r.attempts, 1);
    }
    void timeoutCancelAndResponsiveEventLoop() {
        server->handler = [](const auto &) { auto r = partial(); r.delayMs = 500; return r; };
        client->setTimeoutMs(100);
        int ticks = 0; QTimer ticker; connect(&ticker, &QTimer::timeout, this, [&ticks] { ++ticks; }); ticker.start(5);
        QCOMPARE(range().error, Error::Timeout); QVERIFY(ticks > 2);
        client->setTimeoutMs(3000);
        QSignalSpy spy(client, &WebDavProbe::completed);
        QVERIFY(client->readRange("/AllMusic/a", 2, 4));
        QVERIFY(!client->listDirectory("/AllMusic/"));
        QTimer::singleShot(20, client, &WebDavProbe::cancel);
        QVERIFY(spy.wait(1000)); QCOMPARE(qvariant_cast<Result>(spy[0][0]).error, Error::Cancelled);
        QTest::qWait(550); QCOMPARE(spy.size(), 1); QVERIFY(!client->busy());
    }
    void cancelBackoff() {
        server->handler = [](const auto &) { return Response{429, {}, {{"Retry-After", "1"}}}; };
        QSignalSpy spy(client, &WebDavProbe::completed);
        QVERIFY(client->readRange("/AllMusic/a", 2, 4));
        QTRY_COMPARE(server->requests.size(), 1);
        QTimer::singleShot(30, client, &WebDavProbe::cancel);
        QVERIFY(spy.wait(1000)); QCOMPARE(qvariant_cast<Result>(spy[0][0]).error, Error::Cancelled);
        QTest::qWait(1100); QCOMPARE(server->requests.size(), 1); QCOMPARE(spy.size(), 1);
    }
    void truncatedAndOversized() {
        server->handler = [](const auto &) { auto r = partial(); r.truncate = true; return r; };
        auto r = range(); QVERIFY(r.error == Error::Network || r.error == Error::Protocol); QVERIFY(r.bytes.isEmpty());
        server->handler = [](const auto &) { auto r = partial(); r.body = "23456"; return r; };
        r = range(); QCOMPARE(r.error, Error::TooLarge); QVERIFY(r.bytes.isEmpty());
        server->handler = [](const auto &) { Response big; big.body = QByteArray(4 * 1024 * 1024 + 1, 'x'); return big; };
        QCOMPARE(list().error, Error::TooLarge);
    }
    void forbiddenMethodsOnServer() {
        // Independent raw socket: prove fixture cannot mutate anything either.
        // These deliberate negative requests go to a SECOND local server only.
        MockWebDav isolated; QVERIFY(isolated.start());
        for (const auto &method : {"PUT", "DELETE", "MOVE", "COPY", "MKCOL", "PROPPATCH", "POST", "PATCH"}) {
            QTcpSocket socket;
            socket.connectToHost(QHostAddress::LocalHost, isolated.serverPort());
            QVERIFY(socket.waitForConnected(1000));
            socket.write(QByteArray(method) + " /AllMusic/a HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n");
            socket.flush();
            QTRY_VERIFY(socket.bytesAvailable() > 0);
            QVERIFY(socket.readAll().startsWith("HTTP/1.1 405"));
        }
        QCOMPARE(isolated.requests.size(), 8);
        QCOMPARE(list().error, Error::None);
    }
    void tlsRejectsUntrustedCertificate() {
        QVERIFY2(QSslSocket::supportsSsl(), "TLS backend must be present; cannot claim TLS test passed without it");
        MockWebDav secure; secure.tls = true;
        QFile cert(QStringLiteral(":/webdav/localhost.crt"));
        QFile key(QStringLiteral(":/webdav/localhost.testkey"));
        QVERIFY(cert.open(QIODevice::ReadOnly)); QVERIFY(key.open(QIODevice::ReadOnly));
        secure.ssl = QSslConfiguration::defaultConfiguration();
        secure.ssl.setLocalCertificate(QSslCertificate(cert.readAll()));
        secure.ssl.setPrivateKey(QSslKey(key.readAll(), QSsl::Rsa));
        QVERIFY(!secure.ssl.localCertificate().isNull()); QVERIFY(!secure.ssl.privateKey().isNull());
        secure.ssl.setPeerVerifyMode(QSslSocket::VerifyNone); // server only; client keeps strict default verification.
        QVERIFY(secure.start());
        WebDavProbe strict(secure.serverPort(), true); QSignalSpy spy(&strict, &WebDavProbe::completed);
        QVERIFY(strict.listDirectory("/AllMusic/")); QVERIFY(spy.wait(4000));
        QCOMPARE(qvariant_cast<Result>(spy[0][0]).error, Error::Tls);
        QCOMPARE(secure.requests.size(), 0);
    }
};
QTEST_GUILESS_MAIN(WebDavTests)
#include "tst_webdav.moc"
