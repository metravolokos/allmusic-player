#include "WebDavRangeDevice.h"
#include "WebDavProbe.h"
#include "MockWebDav.h"

#include <QAudioBufferOutput>
#include <QMediaPlayer>
#include <QFile>
#include <QSignalSpy>
#include <QElapsedTimer>
#include <QtTest>
#include <atomic>
#include <thread>
#include <limits>

class StreamingTests final : public QObject
{
    Q_OBJECT
    MockWebDav server;
    QByteArray audio;
    const QByteArray etag = "\"fixture-v1\"";
    bool ignoreRange = false;
    bool changeEtag = false;
    bool truncateRange = false;
    int delayMs = 0;

    void installHandler()
    {
        server.handler = [this](const MockWebDav::Request &request) {
            MockWebDav::Response response;
            response.delayMs = delayMs;
            if (request.method == "PROPFIND") {
                const QByteArray listing = "<d:multistatus xmlns:d=\"DAV:\"><d:response>"
                    "<d:href>/AllMusic/tone.mp3</d:href><d:propstat><d:prop>"
                    "<d:getcontentlength>" + QByteArray::number(audio.size()) + "</d:getcontentlength>"
                    "<d:getetag>&quot;fixture-v1&quot;</d:getetag></d:prop>"
                    "<d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response></d:multistatus>";
                response.status = 207;
                response.body = listing;
                return response;
            }
            const QByteArray range = request.headers.value("range");
            if (ignoreRange || range.isEmpty()) {
                response.status = 200;
                response.body = audio;
                response.headers["ETag"] = etag;
                return response;
            }
            const auto parts = range.mid(6).split('-');
            bool startOk = false, endOk = false;
            const qint64 first = parts.value(0).toLongLong(&startOk);
            const qint64 last = parts.value(1).toLongLong(&endOk);
            if (!startOk || !endOk || first < 0 || last < first || first >= audio.size()) {
                response.status = 416;
                return response;
            }
            const qint64 boundedLast = qMin(last, audio.size() - 1);
            response.status = 206;
            response.body = audio.mid(first, boundedLast - first + 1);
            response.headers["Content-Range"] = "bytes " + QByteArray::number(first) + "-"
                + QByteArray::number(boundedLast) + "/" + QByteArray::number(audio.size());
            response.headers["ETag"] = changeEtag ? "\"fixture-v2\"" : etag;
            response.truncate = truncateRange;
            return response;
        };
    }

    bool readWithEventLoop(WebDavRangeDevice &device, char *data, qint64 length, qint64 *result,
                           int timeoutMs = 7000)
    {
        std::atomic_bool done{false};
        std::thread reader([&] {
            *result = device.read(data, length);
            done.store(true);
        });
        QElapsedTimer timer;
        timer.start();
        while (!done.load() && timer.elapsed() < timeoutMs) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
            QTest::qWait(2);
        }
        const bool completed = done.load();
        if (!completed)
            device.cancel();
        reader.join();
        return completed;
    }

private slots:
    void initTestCase()
    {
        qRegisterMetaType<WebDavProbe::Result>();
        QFile file(qEnvironmentVariable("ALLMUSIC_FIXTURES") + "/tone-mp3.mp3");
        QVERIFY2(file.open(QIODevice::ReadOnly), "synthetic MP3 fixture missing");
        audio = file.readAll();
        QVERIFY(audio.size() > 65536);
    }
    void init()
    {
        QVERIFY(server.start());
        QFile file(qEnvironmentVariable("ALLMUSIC_FIXTURES") + "/tone-mp3.mp3");
        QVERIFY(file.open(QIODevice::ReadOnly));
        audio = file.readAll();
        ignoreRange = false;
        changeEtag = false;
        truncateRange = false;
        delayMs = 0;
        installHandler();
    }
    void cleanup()
    {
        for (const auto &request : server.requests) {
            QVERIFY(request.method == "GET" || request.method == "PROPFIND");
            QVERIFY(request.target == "/AllMusic" || request.target.startsWith("/AllMusic/"));
            QVERIFY(!request.headers.contains("authorization"));
            QVERIFY(!request.headers.contains("cookie"));
        }
        server.requests.clear();
        server.close();
    }
    void randomReadUsesValidatedRanges()
    {
        WebDavRangeDevice device(server.serverPort(), "/AllMusic/tone.mp3", audio.size(), etag);
        QVERIFY(device.open(QIODevice::ReadOnly));
        QByteArray head(32, Qt::Uninitialized);
        qint64 read = -1;
        QVERIFY(readWithEventLoop(device, head.data(), head.size(), &read));
        QCOMPARE(read, head.size());
        QCOMPARE(head, audio.left(head.size()));
        QVERIFY(device.seek(70000));
        QByteArray later(32, Qt::Uninitialized);
        QVERIFY(readWithEventLoop(device, later.data(), later.size(), &read));
        QCOMPARE(read, later.size());
        QCOMPARE(later, audio.mid(70000, later.size()));
        QCOMPARE(server.requests.size(), 2);
        QCOMPARE(server.requests[0].headers.value("range"), "bytes=0-65535");
        QCOMPARE(server.requests[1].headers.value("range"), "bytes=65536-131071");
        QVERIFY(device.cachedBytes() <= 4 * 64 * 1024);
        QVERIFY(server.requests[1].headers.contains("if-range"));
    }
    void propfindVersionIsPinnedAcrossGet()
    {
        WebDavProbe listing(server.serverPort());
        QSignalSpy completed(&listing, &WebDavProbe::completed);
        QVERIFY(listing.listDirectory("/AllMusic/"));
        QVERIFY(completed.wait(3000));
        const auto result = qvariant_cast<WebDavProbe::Result>(completed.takeFirst().at(0));
        QCOMPARE(result.error, WebDavProbe::Error::None);
        QCOMPARE(result.entries.size(), 1);
        QCOMPARE(result.entries[0].etag, etag);
        QCOMPARE(result.entries[0].size.value(), audio.size());
        WebDavRangeDevice device(server.serverPort(), result.entries[0].reference,
                                 result.entries[0].size.value(), result.entries[0].etag);
        QVERIFY(device.open(QIODevice::ReadOnly));
        char byte = 0;
        qint64 read = -1;
        QVERIFY(readWithEventLoop(device, &byte, 1, &read));
        QCOMPARE(read, 1);
        QCOMPARE(uchar(byte), uchar(audio[0]));
        QCOMPARE(server.requests.size(), 2);
        QCOMPARE(server.requests[1].headers.value("if-range"), etag);
    }
    void ignoredRangeFailsClosed()
    {
        ignoreRange = true;
        WebDavRangeDevice device(server.serverPort(), "/AllMusic/tone.mp3", audio.size(), etag);
        QVERIFY(device.open(QIODevice::ReadOnly));
        QSignalSpy failed(&device, &WebDavRangeDevice::requestFailed);
        char byte = 0;
        qint64 read = 0;
        QVERIFY(readWithEventLoop(device, &byte, 1, &read));
        QCOMPARE(read, -1);
        QTRY_COMPARE(failed.size(), 1);
        QVERIFY(failed.first().first().toString().contains("200"));
        QVERIFY(server.requests.first().headers.contains("range"));
        QVERIFY(device.cachedBytes() == 0);
    }
    void changedEtagFailsClosed()
    {
        changeEtag = true;
        WebDavRangeDevice device(server.serverPort(), "/AllMusic/tone.mp3", audio.size(), etag);
        QVERIFY(device.open(QIODevice::ReadOnly));
        QSignalSpy failed(&device, &WebDavRangeDevice::requestFailed);
        char byte = 0;
        qint64 read = 0;
        QVERIFY(readWithEventLoop(device, &byte, 1, &read));
        QCOMPARE(read, -1);
        QTRY_COMPARE(failed.size(), 1);
        QVERIFY(failed.first().first().toString().contains("ETag"));
        QCOMPARE(device.cachedBytes(), 0);
        WebDavRangeDevice recovered(server.serverPort(), "/AllMusic/tone.mp3", audio.size(), "\"fixture-v2\"");
        QVERIFY(recovered.open(QIODevice::ReadOnly));
        char next = 0;
        qint64 recoveredRead = -1;
        QVERIFY(readWithEventLoop(recovered, &next, 1, &recoveredRead));
        QCOMPARE(recoveredRead, 1);
        QCOMPARE(uchar(next), uchar(audio[0]));
    }
    void truncatedConnectionFailsWithoutCachingPartialData()
    {
        truncateRange = true;
        WebDavRangeDevice device(server.serverPort(), "/AllMusic/tone.mp3", audio.size(), etag);
        QVERIFY(device.open(QIODevice::ReadOnly));
        char byte = 0;
        qint64 read = 0;
        QVERIFY(readWithEventLoop(device, &byte, 1, &read));
        QCOMPARE(read, -1);
        QCOMPARE(device.cachedBytes(), 0);
    }
    void decoderStartsBeforeWholeFileArrives()
    {
        QFile flacFile(qEnvironmentVariable("ALLMUSIC_FIXTURES") + "/tone-stream-flac.flac");
        QVERIFY(flacFile.open(QIODevice::ReadOnly));
        audio = flacFile.readAll();
        QVERIFY(audio.size() > 2 * 64 * 1024);
        WebDavRangeDevice device(server.serverPort(), "/AllMusic/tone-stream-flac.flac", audio.size(), etag);
        QVERIFY(device.open(QIODevice::ReadOnly));
        QMediaPlayer player;
        QAudioBufferOutput output;
        player.setAudioOutput(nullptr);
        player.setAudioBufferOutput(&output);
        player.setSourceDevice(&device);
        std::atomic<qint64> decodedFrames{0};
        std::atomic<qint64> bytesAtFirstPcm{-1};
        std::atomic<qint64> transferred{0};
        std::atomic<qint64> firstPcmMs{-1};
        QElapsedTimer startup;
        startup.start();
        connect(&device, &WebDavRangeDevice::bytesFetched, this,
                [&](qint64 count) { transferred.fetch_add(count); }, Qt::DirectConnection);
        connect(&output, &QAudioBufferOutput::audioBufferReceived, this,
                [&](const QAudioBuffer &buffer) {
            if (!buffer.isValid()) return;
            decodedFrames.fetch_add(buffer.frameCount());
            if (bytesAtFirstPcm.load() < 0) {
                bytesAtFirstPcm.store(transferred.load());
                firstPcmMs.store(startup.elapsed());
            }
        }, Qt::DirectConnection);
        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(decodedFrames.load() > 4096, 12000);
        QVERIFY(bytesAtFirstPcm.load() > 0);
        QVERIFY2(bytesAtFirstPcm.load() < audio.size(), "decoder consumed the entire file before first PCM");
        QVERIFY2(device.cachedBytes() <= 4 * 64 * 1024, "page cache exceeded its fixed bound");
        QTRY_VERIFY_WITH_TIMEOUT(player.duration() > 30000 && player.isSeekable(), 5000);
        player.pause();
        const qint64 beforeSeekFrames = decodedFrames.load();
        player.setPosition(25000);
        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(decodedFrames.load() > beforeSeekFrames + 4096, 7000);
        QVERIFY(qAbs(player.position() - 25000) < 1000);
        QVERIFY2(device.cachedBytes() <= 4 * 64 * 1024, "seek exceeded the fixed cache bound");
        bool requestedUnseenPage = false;
        for (const auto &request : server.requests)
            requestedUnseenPage |= request.headers.value("range").startsWith("bytes=262144-");
        QVERIFY2(requestedUnseenPage, "seek did not issue a Range request for the later file region");
        player.stop();
        qInfo() << "time to PCM ms" << firstPcmMs.load() << "bytes fetched at PCM"
                << bytesAtFirstPcm.load() << "of file" << audio.size()
                << "; cached bytes" << device.cachedBytes() << "; decoded frames" << decodedFrames.load()
                << "; HTTP requests" << server.requests.size();
        player.setSource({});
        disconnect(&output, nullptr, this, nullptr);
        disconnect(&device, nullptr, this, nullptr);
    }
    void cancelInterruptsActiveRequest()
    {
        delayMs = 3000;
        installHandler();
        WebDavRangeDevice device(server.serverPort(), "/AllMusic/tone.mp3", audio.size(), etag);
        QVERIFY(device.open(QIODevice::ReadOnly));
        std::atomic<qint64> result{0};
        std::thread reader([&] { char byte = 0; result = device.read(&byte, 1); });
        QTRY_VERIFY_WITH_TIMEOUT(!server.requests.isEmpty(), 2000);
        QElapsedTimer timer; timer.start();
        device.cancel();
        reader.join();
        QVERIFY(timer.elapsed() < 1500);
        QCOMPARE(result.load(), -1);
    }

};

QTEST_GUILESS_MAIN(StreamingTests)
#include "tst_streaming.moc"
