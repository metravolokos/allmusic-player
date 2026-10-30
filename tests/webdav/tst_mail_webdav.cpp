#include "MailWebDavProbe.h"
#include "MockWebDav.h"

#include <QFile>
#include <QSignalSpy>
#include <QSslKey>
#include <QtTest>

using Error = MailWebDavProbe::Error;
using Result = MailWebDavProbe::Result;
using Entry = MailWebDavProbe::Entry;
using Response = MockWebDav::Response;

class MailWebDavProbeTests final : public QObject {
    Q_OBJECT
    MockWebDav *server = nullptr;
    MailWebDavProbe *client = nullptr;

    Result list(const QString &email = QStringLiteral("owner@example.invalid"),
                const QString &password = QStringLiteral("mock-only-secret"))
    {
        QSignalSpy spy(client, &MailWebDavProbe::completed);
        if (!client->listAllMusic(email, password) || !spy.wait(5000)) {
            QTest::qFail("PROPFIND did not emit a result", __FILE__, __LINE__);
            return Result{Error::Network};
        }
        return qvariant_cast<Result>(spy.takeFirst().at(0));
    }

    Result range(const Entry &entry)
    {
        QSignalSpy spy(client, &MailWebDavProbe::completed);
        if (!client->checkRange(entry) || !spy.wait(5000)) {
            QTest::qFail("Range request did not emit a result", __FILE__, __LINE__);
            return Result{Error::Network};
        }
        return qvariant_cast<Result>(spy.takeFirst().at(0));
    }

    static QByteArray listing(const QByteArray &extra = {})
    {
        return "<d:multistatus xmlns:d=\"DAV:\">"
            "<d:response><d:href>/AllMusic/</d:href><d:propstat><d:prop>"
            "<d:resourcetype><d:collection/></d:resourcetype>"
            "</d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>"
            + extra + "</d:multistatus>";
    }

    static QByteArray child(const QByteArray &href, const QByteArray &properties = {})
    {
        return "<d:response><d:href>" + href + "</d:href><d:propstat><d:prop>"
            + properties + "</d:prop><d:status>HTTP/1.1 200 OK</d:status>"
            "</d:propstat></d:response>";
    }

    Entry audio(qint64 size = 100, QByteArray etag = "\"v1\"") const
    {
        Entry entry;
        entry.path = QStringLiteral("/AllMusic/sample.mp3");
        entry.reference = QStringLiteral("/AllMusic/sample.mp3");
        entry.size = size;
        entry.etag = std::move(etag);
        return entry;
    }

private slots:
    void initTestCase()
    {
        qRegisterMetaType<MailWebDavProbe::Error>();
        qRegisterMetaType<MailWebDavProbe::RangeIssue>();
        qRegisterMetaType<MailWebDavProbe::Result>();
    }

    void init()
    {
        server = new MockWebDav;
        QVERIFY(server->start());
        const QUrl localOrigin(QStringLiteral("http://127.0.0.1:%1").arg(server->serverPort()));
        client = new MailWebDavProbe(localOrigin, MailWebDavProbe::TestOriginTag{}, this);
    }

    void cleanup()
    {
        // The real client exposes only PROPFIND and GET; every target stays below /AllMusic.
        for (const auto &request : server->requests) {
            QVERIFY(request.method == "PROPFIND" || request.method == "GET");
            QVERIFY(request.target == "/AllMusic" || request.target.startsWith("/AllMusic/"));
            QVERIFY(!request.target.contains('?'));
            QVERIFY(!request.target.contains('#'));
            QVERIFY(!request.headers.contains("cookie"));
        }
        delete client;
        client = nullptr;
        delete server;
        server = nullptr;
    }

    void propfindListsOnlyImmediateEntriesAndAuthenticatesLocally()
    {
        const QByteArray expectedAuthorization = "Basic "
            + QByteArray("owner@example.invalid:mock-only-secret").toBase64();
        server->handler = [expectedAuthorization](const MockWebDav::Request &request) {
            if (request.headers.value("authorization") != expectedAuthorization)
                return Response{401, {}, {{"WWW-Authenticate", "Basic realm=\"fixture\""}}};
            const QByteArray rows = child("/AllMusic/%D0%A2%D0%B5%D1%81%D1%82%20%231.mp3",
                    "<d:getcontentlength>100</d:getcontentlength><d:getetag>&quot;v1&quot;</d:getetag>")
                + child("/AllMusic/%D0%90%D0%BB%D1%8C%D0%B1%D0%BE%D0%BC/",
                    "<d:resourcetype><d:collection/></d:resourcetype>")
                + child("/AllMusic/readme.txt");
            return Response{207, listing(rows), {{"Content-Type", "application/xml"}}};
        };

        const Result result = list();
        QCOMPARE(result.error, Error::None);
        QVERIFY(result.listing);
        QCOMPARE(result.entries.size(), 3);
        QCOMPARE(result.entries.at(0).path, QString::fromUtf8("/AllMusic/Тест #1.mp3"));
        QCOMPARE(result.entries.at(0).size.value(), 100);
        QCOMPARE(result.entries.at(0).etag, "\"v1\"");
        QVERIFY(result.entries.at(1).directory);
        QCOMPARE(server->requests.size(), 2);
        for (const auto &request : server->requests) {
            QVERIFY(request.method == "PROPFIND");
            QCOMPARE(request.target, "/AllMusic/");
            QCOMPARE(request.headers.value("depth"), "1");
            QVERIFY(request.body.contains("getcontentlength"));
        }
        // Avoid QCOMPARE on the synthetic secret/header: its failure output could reveal it.
        const bool authorized = server->requests.at(1).headers.value("authorization")
            == expectedAuthorization;
        QVERIFY2(authorized, "mock Basic challenge was not answered with the synthetic account");
    }

    void repeatedUnauthorizedChallengeStops()
    {
        server->handler = [](const MockWebDav::Request &) {
            return Response{401, {}, {{"WWW-Authenticate", "Basic realm=\"fixture\""}}};
        };
        const Result result = list();
        QCOMPARE(result.error, Error::Auth);
        QCOMPARE(result.status, 401);
        QVERIFY(server->requests.size() <= 2);
    }

    void httpErrorsAreCategorized_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<Error>("error");
        QTest::newRow("forbidden") << 403 << Error::Forbidden;
        QTest::newRow("missing-AllMusic") << 404 << Error::NotFound;
        QTest::newRow("rate-limited") << 429 << Error::RateLimited;
        QTest::newRow("server-error") << 503 << Error::Server;
    }

    void httpErrorsAreCategorized()
    {
        QFETCH(int, status);
        QFETCH(Error, error);
        server->handler = [status](const MockWebDav::Request &) { return Response{status, {}}; };
        const Result result = list();
        QCOMPARE(result.error, error);
        QCOMPARE(result.status, status);
        QCOMPARE(server->requests.size(), 1);
    }

    void redirectIsNotFollowed()
    {
        server->handler = [](const MockWebDav::Request &) {
            return Response{302, {}, {{"Location", "https://example.invalid/secret"}}};
        };
        const Result result = list();
        QCOMPARE(result.error, Error::Redirect);
        QCOMPARE(server->requests.size(), 1);
    }

    void listingLimitStopsOnDeclaredOversize()
    {
        server->handler = [](const MockWebDav::Request &) {
            return Response{207, {}, {{"Content-Length", "4194305"}}};
        };
        const Result result = list();
        QCOMPARE(result.error, Error::TooLarge);
        QCOMPARE(server->requests.size(), 1);
    }

    void hostileHrefsAreRejectedWithoutRequestsOutsideRoot()
    {
        const QByteArray host = "http://127.0.0.1:" + QByteArray::number(server->serverPort());
        const QByteArray rows = child(host + "/AllMusic/safe.mp3")
            + child("http://127.0.0.1.evil:" + QByteArray::number(server->serverPort())
                    + "/AllMusic/host.mp3")
            + child(host + "/Other/secret.mp3")
            + child("/AllMusic/../secret.mp3")
            + child("/AllMusic/%252e%252e/secret.mp3")
            + child("/AllMusic/%2fsecret.mp3")
            + child("/AllMusic/sub/deep.mp3");
        server->handler = [rows](const MockWebDav::Request &) {
            return Response{207, listing(rows)};
        };

        const Result result = list();
        QCOMPARE(result.error, Error::None);
        QCOMPARE(result.entries.size(), 1);
        QCOMPARE(result.entries.first().path, QStringLiteral("/AllMusic/safe.mp3"));
        QCOMPARE(result.rejectedEntries, 6);
        QCOMPARE(server->requests.size(), 1);
    }

    void forgedRangePathIsRejectedBeforeNetwork()
    {
        Entry forged = audio();
        forged.reference = QStringLiteral("https://attacker.invalid/AllMusic/secret.mp3");
        const Result result = range(forged);
        QCOMPARE(result.error, Error::UnsafePath);
        QCOMPARE(server->requests.size(), 0);
    }

    void rangeIsSeparateBoundedAndValidatesHeaders()
    {
        server->handler = [](const MockWebDav::Request &) {
            return Response{206, QByteArray(100, 'x'), {
                {"Content-Range", "bytes 0-99/100"},
                {"Content-Length", "100"},
                {"Content-Encoding", "identity"},
                {"ETag", "\"v1\""}
            }};
        };
        const Result result = range(audio());
        QCOMPARE(result.error, Error::None);
        QCOMPARE(result.status, 206);
        QCOMPARE(result.bytesReceived, 100);
        QCOMPARE(result.totalSize.value(), 100);
        QCOMPARE(result.rangeExpectedBytes.value(), 100);
        QVERIFY(result.etagPresent);
        QVERIFY(result.strongEtag);
        QVERIFY(result.etagMatchesIfRange);
        QVERIFY(result.ifRangeSent);
        QCOMPARE(server->requests.size(), 1);
        const auto &request = server->requests.first();
        QCOMPARE(request.method, "GET");
        QCOMPARE(request.headers.value("range"), "bytes=0-99");
        QCOMPARE(request.headers.value("if-range"), "\"v1\"");
        QCOMPARE(request.headers.value("accept-encoding"), "identity");
    }

    void rangeUsesAtMost64KiBWhenSizeIsUnknown()
    {
        server->handler = [](const MockWebDav::Request &request) {
            const bool bounded = request.headers.value("range") == "bytes=0-65535";
            if (!bounded)
                return Response{400, {}};
            return Response{206, QByteArray(64 * 1024, 'x'), {
                {"Content-Range", "bytes 0-65535/100000"},
                {"Content-Length", "65536"}
            }};
        };
        Entry unknownSize = audio(-1, {});
        unknownSize.size.reset();
        const Result result = range(unknownSize);
        QCOMPARE(result.error, Error::None);
        QCOMPARE(result.bytesReceived, 64 * 1024);
        QVERIFY(!result.etagPresent);
        QVERIFY(!result.ifRangeSent);
    }

    void rangeAcceptsMissingContentLengthWhenQtCompletesTheResponse()
    {
        server->handler = [](const MockWebDav::Request &) {
            auto response = Response{206, QByteArray(100, 'x'), {
                {"Content-Range", "bytes 0-99/100"}
            }};
            response.omitContentLength = true; // HTTP/1.1 close-delimited framing.
            return response;
        };
        const Result result = range(audio());
        QCOMPARE(result.error, Error::None);
        QCOMPARE(result.status, 206);
        QCOMPARE(result.rangeIssue, MailWebDavProbe::RangeIssue::None);
        QVERIFY(result.contentRangePresent);
        QVERIFY(!result.contentLengthPresent);
        QCOMPARE(result.bytesReceived, 100);
        QCOMPARE(result.rangeExpectedBytes.value(), 100);
    }

    void rangeAcceptsChunkedBodyAndUnknownCompleteLength()
    {
        server->handler = [](const MockWebDav::Request &) {
            auto response = Response{206, QByteArray(100, 'x'), {
                {"Content-Range", "bytes 0-99/*"}, {"ETag", "\"v1\""}
            }};
            response.omitContentLength = true;
            response.chunked = true;
            return response;
        };
        const Result result = range(audio());
        QCOMPARE(result.error, Error::None);
        QCOMPARE(result.status, 206);
        QVERIFY(result.transferEncodingPresent);
        QVERIFY(result.transferEncodingChunked);
        QVERIFY(!result.contentLengthPresent);
        QVERIFY(!result.totalSize.has_value());
        QCOMPARE(result.rangeExpectedBytes.value(), 100);
        QCOMPARE(result.bytesReceived, 100);
        QVERIFY(result.ifRangeSent);
        QVERIFY(result.etagMatchesIfRange);
    }

    void rangeRequestUsesEncodedUnicodePath()
    {
        server->handler = [](const MockWebDav::Request &) {
            return Response{206, QByteArray(100, 'x'), {
                {"Content-Range", "bytes 0-99/100"}, {"Content-Length", "100"}
            }};
        };
        Entry unicode = audio();
        unicode.path = QString::fromUtf8("/AllMusic/Тест #1.flac");
        unicode.reference = QStringLiteral("/AllMusic/%D0%A2%D0%B5%D1%81%D1%82%20%231.flac");
        const Result result = range(unicode);
        QCOMPARE(result.error, Error::None);
        QCOMPARE(server->requests.size(), 1);
        QCOMPARE(server->requests.first().target,
                 "/AllMusic/%D0%A2%D0%B5%D1%81%D1%82%20%231.flac");
    }

    void etagMismatchAndWeakEtagAreReportedSafely()
    {
        server->handler = [](const MockWebDav::Request &) {
            return Response{206, QByteArray(100, 'x'), {
                {"Content-Range", "bytes 0-99/100"},
                {"Content-Length", "100"}, {"ETag", "\"v2\""}
            }};
        };
        const Result changed = range(audio());
        QCOMPARE(changed.error, Error::VersionChanged);
        QCOMPARE(changed.rangeIssue, MailWebDavProbe::RangeIssue::EtagMismatch);
        QVERIFY(changed.ifRangeSent);
        QVERIFY(changed.etagPresent);
        QVERIFY(!changed.etagMatchesIfRange);

        server->handler = [](const MockWebDav::Request &) {
            return Response{206, QByteArray(100, 'x'), {
                {"Content-Range", "bytes 0-99/100"},
                {"Content-Length", "100"}, {"ETag", "W/\"v1\""}
            }};
        };
        Entry weak = audio(100, "W/\"v1\"");
        const Result weakResult = range(weak);
        QCOMPARE(weakResult.error, Error::None);
        QVERIFY(!weakResult.ifRangeSent);
        QVERIFY(weakResult.etagPresent);
        QVERIFY(!weakResult.strongEtag);
    }

    void rangeSyntax_data()
    {
        QTest::addColumn<QByteArray>("field");
        QTest::addColumn<QString>("category");
        QTest::addColumn<bool>("accepted");
        QTest::newRow("rfc") << QByteArray("bytes 0-99/100") << QStringLiteral("границы: числовые") << true;
        QTest::newRow("case-and-ows") << QByteArray("  BYTES  0-99/100\t") << QStringLiteral("разделитель: пробел") << true;
        QTest::newRow("unknown-total") << QByteArray("bytes 0-99/*") << QStringLiteral("границы: числовые") << true;
        QTest::newRow("mail-equals") << QByteArray("bytes=0-99/100") << QStringLiteral("разделитель: равно") << true;
        QTest::newRow("mail-equals-case") << QByteArray("BYTES=0-99/100") << QStringLiteral("разделитель: равно") << true;
        QTest::newRow("mail-equals-space") << QByteArray("bytes= 0-99/100") << QStringLiteral("разделитель: равно") << false;
        QTest::newRow("mail-double-equals") << QByteArray("bytes==0-99/100") << QStringLiteral("разделитель: равно") << false;
        QTest::newRow("mail-equals-unknown-total") << QByteArray("bytes=0-99/*") << QStringLiteral("границы: числовые") << false;
        QTest::newRow("mail-equals-overflow") << QByteArray("bytes=0-99/9223372036854775808") << QStringLiteral("переполнение") << false;
        QTest::newRow("missing-space") << QByteArray("bytes0-99/100") << QStringLiteral("разделитель: отсутствует/другой") << false;
        QTest::newRow("tab") << QByteArray("bytes\t0-99/100") << QStringLiteral("разделитель: табуляция") << false;
        QTest::newRow("non-ascii") << QByteArray("bytes 0-99/100") + char(0x80) << QStringLiteral("не-ASCII") << false;
        QTest::newRow("signed") << QByteArray("bytes -1-99/100") << QStringLiteral("иная/нечисловая") << false;
        QTest::newRow("trailing-text") << QByteArray("bytes 0-99/100 private-fixture") << QStringLiteral("иная/нечисловая") << false;
        QTest::newRow("other-unit") << QByteArray("items 0-99/100") << QStringLiteral("unit: другой/неизвестный") << false;
        QTest::newRow("unsatisfied") << QByteArray("bytes */100") << QStringLiteral("неудовлетворимый диапазон") << false;
        QTest::newRow("overflow") << QByteArray("bytes 0-99/9223372036854775808") << QStringLiteral("переполнение") << false;
        QTest::newRow("overflow-first") << QByteArray("bytes 9223372036854775808-99/100") << QStringLiteral("переполнение") << false;
        QTest::newRow("overflow-last") << QByteArray("bytes 0-9223372036854775808/100") << QStringLiteral("переполнение") << false;
        QTest::newRow("private-text") << QByteArray("bytes private-fixture/100") << QStringLiteral("иная/нечисловая") << false;
        QTest::newRow("non-numeric-total") << QByteArray("bytes 0-99/private-fixture") << QStringLiteral("иная/нечисловая") << false;
        QTest::newRow("long") << QByteArray(257, 'x') << QStringLiteral("более 256") << false;
        QTest::newRow("control") << QByteArray("bytes 0-99/100") + char(1) << QStringLiteral("непечатаемые") << false;
        QTest::newRow("combined") << QByteArray("bytes 0-99/100, bytes 0-99/100") << QStringLiteral("запятая") << false;
        QTest::newRow("duplicate") << QByteArray("bytes 0-99/100\r\ncontent-range: bytes 0-99/100") << QStringLiteral("пол") << false;
    }

    void rangeSyntax()
    {
        QFETCH(QByteArray, field);
        QFETCH(QString, category);
        QFETCH(bool, accepted);
        server->handler = [field](const MockWebDav::Request &) {
            return Response{206, QByteArray(100, 'x'), {{"Content-Range", field}}};
        };
        const Result result = range(audio());
        QCOMPARE(result.error, accepted ? Error::None : Error::Protocol);
        // Qt Network can discard an HTTP field containing illegal control
        // characters before the application sees it. Either safe outcome is
        // acceptable: reject the observed syntax or reject the missing field.
        if (category == QStringLiteral("непечатаемые") && !result.contentRangePresent)
            QCOMPARE(result.contentRangeSyntax, QStringLiteral("поле отсутствует"));
        else
            QVERIFY(result.contentRangeSyntax.contains(category));
        QVERIFY(!result.contentRangeSyntax.contains(QStringLiteral("private-fixture")));
        QVERIFY(result.contentRangeSyntax.size() < 256);
        if (!accepted) {
            // If Qt discarded the malformed field, the failure is correctly
            // categorized as missing rather than invalid Content-Range.
            const auto expectedIssue = category == QStringLiteral("непечатаемые")
                    && !result.contentRangePresent
                ? MailWebDavProbe::RangeIssue::MissingContentRange
                : MailWebDavProbe::RangeIssue::InvalidContentRange;
            QCOMPARE(result.rangeIssue, expectedIssue);
        }
    }

    void rangeProtocolFailuresIdentifySafeCause_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<QByteArray>("contentRange");
        QTest::addColumn<QByteArray>("contentLength");
        QTest::addColumn<int>("bodyLength");
        QTest::addColumn<MailWebDavProbe::RangeIssue>("issue");
        QTest::newRow("missing-content-range") << 206 << QByteArray() << QByteArray("100") << 100
            << MailWebDavProbe::RangeIssue::MissingContentRange;
        QTest::newRow("malformed-content-range") << 206 << QByteArray("bytes invalid/100")
            << QByteArray("100") << 100 << MailWebDavProbe::RangeIssue::InvalidContentRange;
        QTest::newRow("wrong-start") << 206 << QByteArray("bytes 1-99/100") << QByteArray("99") << 99
            << MailWebDavProbe::RangeIssue::RangeStartMismatch;
        QTest::newRow("wrong-end") << 206 << QByteArray("bytes 0-98/100") << QByteArray("99") << 99
            << MailWebDavProbe::RangeIssue::RangeEndMismatch;
        QTest::newRow("mail-equals-wrong-end") << 206 << QByteArray("bytes=0-98/100") << QByteArray("99") << 99
            << MailWebDavProbe::RangeIssue::RangeEndMismatch;
        QTest::newRow("mail-equals-wrong-start") << 206 << QByteArray("bytes=1-99/100") << QByteArray("99") << 99
            << MailWebDavProbe::RangeIssue::RangeStartMismatch;
        QTest::newRow("content-length-conflicts-with-range") << 206
            << QByteArray("bytes 0-99/100") << QByteArray("99") << 99
            << MailWebDavProbe::RangeIssue::ContentLengthMismatch;
    }

    void rangeProtocolFailuresIdentifySafeCause()
    {
        QFETCH(int, status);
        QFETCH(QByteArray, contentRange);
        QFETCH(QByteArray, contentLength);
        QFETCH(int, bodyLength);
        QFETCH(MailWebDavProbe::RangeIssue, issue);
        server->handler = [=](const MockWebDav::Request &) {
            QMap<QByteArray, QByteArray> headers;
            if (!contentRange.isNull())
                headers.insert("Content-Range", contentRange);
            headers.insert("Content-Length", contentLength);
            return Response{status, QByteArray(bodyLength, 'x'), headers};
        };
        const Result result = range(audio());
        QCOMPARE(result.error, Error::Protocol);
        QCOMPARE(result.rangeIssue, issue);
        QCOMPARE(result.status, status);
        QCOMPARE(result.bytesReceived, bodyLength);
    }

    void bodyLengthIsCheckedWhenContentLengthIsAbsent()
    {
        server->handler = [](const MockWebDav::Request &) {
            auto response = Response{206, QByteArray(99, 'x'), {
                {"Content-Range", "bytes 0-99/100"}
            }};
            response.omitContentLength = true;
            return response;
        };
        const Result result = range(audio());
        QCOMPARE(result.error, Error::Protocol);
        QCOMPARE(result.rangeIssue, MailWebDavProbe::RangeIssue::BodyLengthMismatch);
        QVERIFY(!result.contentLengthPresent);
        QCOMPARE(result.bytesReceived, 99);
        QCOMPARE(result.rangeExpectedBytes.value(), 100);
    }

    void rangeBodyIsStoppedAtRequestedSizeLimit()
    {
        server->handler = [](const MockWebDav::Request &) {
            auto response = Response{206, QByteArray(101, 'x'), {
                {"Content-Range", "bytes 0-99/100"}
            }};
            response.omitContentLength = true;
            return response;
        };
        const Result result = range(audio());
        QCOMPARE(result.error, Error::TooLarge);
        QCOMPARE(result.rangeIssue, MailWebDavProbe::RangeIssue::TooLarge);
        QVERIFY(result.bytesReceivedIsLowerBound);
        QVERIFY(result.bytesReceived >= 101);
        QCOMPARE(result.requestedRangeBytes, 100);
    }

    void totalSizeAndContentEncodingFailuresAreCategorized()
    {
        server->handler = [](const MockWebDav::Request &) {
            return Response{206, QByteArray(100, 'x'), {
                {"Content-Range", "bytes 0-99/101"},
                {"Content-Length", "100"}
            }};
        };
        const Result sizeChanged = range(audio());
        QCOMPARE(sizeChanged.error, Error::VersionChanged);
        QCOMPARE(sizeChanged.rangeIssue, MailWebDavProbe::RangeIssue::TotalSizeMismatch);

        server->handler = [](const MockWebDav::Request &) {
            return Response{206, QByteArray(100, 'x'), {
                {"Content-Range", "bytes 0-99/100"},
                {"Content-Length", "100"}, {"Content-Encoding", "gzip"}
            }};
        };
        const Result encoded = range(audio());
        QCOMPARE(encoded.error, Error::Protocol);
        QCOMPARE(encoded.rangeIssue, MailWebDavProbe::RangeIssue::UnsupportedContentEncoding);
        QCOMPARE(encoded.bytesReceived, 0);
    }

    void range200IsStoppedWithoutReadingFullBody()
    {
        server->handler = [](const MockWebDav::Request &) {
            return Response{200, QByteArray(2 * 1024 * 1024, 'x')};
        };
        const Result result = range(audio());
        QCOMPARE(result.error, Error::VersionChanged);
        QCOMPARE(result.status, 200);
        QCOMPARE(result.rangeIssue, MailWebDavProbe::RangeIssue::RangeIgnored);
        QCOMPARE(result.bytesReceived, 0);
        QCOMPARE(server->requests.size(), 1);
    }

    void range416IsReportedAsUnsupported()
    {
        server->handler = [](const MockWebDav::Request &) { return Response{416, {}}; };
        const Result result = range(audio());
        QCOMPARE(result.error, Error::RangeUnsupported);
        QCOMPARE(result.status, 416);
        QCOMPARE(result.rangeIssue, MailWebDavProbe::RangeIssue::RangeUnsatisfiable);
    }

    void malformedRangeAndEncodedBodyAreRejected()
    {
        server->handler = [](const MockWebDav::Request &) {
            return Response{206, QByteArray(100, 'x'), {
                {"Content-Range", "bytes 0-98/100"}, {"Content-Length", "100"}
            }};
        };
        QCOMPARE(range(audio()).rangeIssue, MailWebDavProbe::RangeIssue::RangeEndMismatch);
    }

    void cancellationStopsPendingRequest()
    {
        server->handler = [](const MockWebDav::Request &) {
            auto response = Response{207, listing()};
            response.delayMs = 500;
            return response;
        };
        QSignalSpy spy(client, &MailWebDavProbe::completed);
        QVERIFY(client->listAllMusic(QStringLiteral("owner@example.invalid"),
                                     QStringLiteral("mock-only-secret")));
        QTRY_COMPARE(server->requests.size(), 1);
        client->cancel();
        QVERIFY(spy.size() == 1);
        const Result result = qvariant_cast<Result>(spy.first().at(0));
        QCOMPARE(result.error, Error::Cancelled);
        QTest::qWait(550);
        QCOMPARE(spy.size(), 1);
    }

    void tlsFailureDoesNotReachHttpOrSendCredentials()
    {
        QVERIFY2(QSslSocket::supportsSsl(), "TLS backend must be present to verify certificate rejection");
        MockWebDav secure;
        secure.tls = true;
        QFile cert(QStringLiteral(":/webdav/localhost.crt"));
        QFile key(QStringLiteral(":/webdav/localhost.testkey"));
        QVERIFY(cert.open(QIODevice::ReadOnly));
        QVERIFY(key.open(QIODevice::ReadOnly));
        auto ssl = QSslConfiguration::defaultConfiguration();
        ssl.setLocalCertificate(QSslCertificate(cert.readAll()));
        ssl.setPrivateKey(QSslKey(key.readAll(), QSsl::Rsa));
        QVERIFY(!ssl.localCertificate().isNull());
        QVERIFY(!ssl.privateKey().isNull());
        ssl.setPeerVerifyMode(QSslSocket::VerifyNone); // Server side only.
        secure.ssl = ssl;
        QVERIFY(secure.start());

        delete client;
        const QUrl tlsOrigin(QStringLiteral("https://127.0.0.1:%1").arg(secure.serverPort()));
        client = new MailWebDavProbe(tlsOrigin, MailWebDavProbe::TestOriginTag{}, this);
        const Result result = list();
        QCOMPARE(result.error, Error::Tls);
        QCOMPARE(secure.requests.size(), 0);
    }
};

QTEST_GUILESS_MAIN(MailWebDavProbeTests)
#include "tst_mail_webdav.moc"
