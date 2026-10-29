#include "LocalPlayback.h"
#include "WebDavRangeDevice.h"
#include "WebDavProbe.h"
#include "MockWebDav.h"

#include <QAudioBufferOutput>
#include <QAudioBuffer>
#include <QMediaPlayer>
#include <QFile>
#include <QSignalSpy>
#include <QElapsedTimer>
#include <QtTest>
#include <atomic>
#include <cmath>
#include <thread>
#include <limits>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#elif defined(Q_OS_LINUX)
#include <sys/resource.h>
#endif

class StreamingTests final : public QObject
{
    Q_OBJECT
    MockWebDav server;
    QByteArray audio;
    QByteArray etag = "\"fixture-v1\"";
    QString fixtureName = QStringLiteral("tone-mp3.mp3");
    QString secondFixtureName = QStringLiteral("tone-next.mp3");
    bool exposeSecondFixture = false;
    bool ignoreRange = false;
    bool changeEtag = false;
    bool truncateRange = false;
    bool force416 = false;
    bool disconnectBeforeResponse = false;
    int retry429Responses = 0;
    QByteArray rangeReceiving429;
    bool sameRangeRetried = false;
    int delayMs = 0;
    qint64 disconnectAfterBytes = -1;
    qsizetype chunkSize = 0;
    int chunkIntervalMs = 0;

    void installHandler()
    {
        server.handler = [this](const MockWebDav::Request &request) {
            MockWebDav::Response response;
            if (request.method == "PROPFIND") {
                QStringList names{fixtureName};
                if (exposeSecondFixture)
                    names.push_back(secondFixtureName);
                QByteArray listing = "<d:multistatus xmlns:d=\"DAV:\"><d:response>"
                    "<d:href>/AllMusic/</d:href><d:propstat><d:prop>"
                    "<d:resourcetype><d:collection/></d:resourcetype></d:prop>"
                    "<d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>";
                for (const auto &name : names) {
                    listing += "<d:response><d:href>/AllMusic/" + name.toUtf8()
                        + "</d:href><d:propstat><d:prop><d:getcontentlength>"
                        + QByteArray::number(audio.size())
                        + "</d:getcontentlength><d:getetag>&quot;fixture-v1&quot;</d:getetag></d:prop>"
                          "<d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>";
                }
                listing += "</d:multistatus>";
                response.status = 207;
                response.body = listing;
                return response;
            }

            response.delayMs = delayMs;
            response.disconnectBeforeResponse = disconnectBeforeResponse;
            response.disconnectAfterBytes = disconnectAfterBytes;
            response.chunkSize = chunkSize;
            response.chunkIntervalMs = chunkIntervalMs;
            if (retry429Responses > 0) {
                --retry429Responses;
                rangeReceiving429 = request.headers.value("range");
                response.status = 429;
                response.headers["Retry-After"] = "0";
                return response;
            }
            if (!rangeReceiving429.isEmpty() && request.method == "GET"
                && request.headers.value("range") == rangeReceiving429)
                sameRangeRetried = true;
            const QByteArray expectedTarget = "/AllMusic/" + fixtureName.toUtf8();
            const QByteArray secondTarget = "/AllMusic/" + secondFixtureName.toUtf8();
            if (request.method != "GET"
                || (request.target != expectedTarget
                    && !(exposeSecondFixture && request.target == secondTarget))) {
                response.status = 404;
                return response;
            }
            if (force416) {
                response.status = 416;
                response.headers["Content-Range"] = "bytes */" + QByteArray::number(audio.size());
                return response;
            }
            const QByteArray range = request.headers.value("range");
            if (ignoreRange || range.isEmpty()) {
                response.status = 200;
                response.body = audio;
                response.headers["ETag"] = etag;
                return response;
            }
            const auto parts = range.startsWith("bytes=") ? range.mid(6).split('-') : QList<QByteArray>{};
            bool startOk = false, endOk = false;
            const qint64 first = parts.value(0).toLongLong(&startOk);
            const qint64 last = parts.value(1).toLongLong(&endOk);
            if (!startOk || !endOk || first < 0 || last < first || first >= audio.size()) {
                response.status = 416;
                response.headers["Content-Range"] = "bytes */" + QByteArray::number(audio.size());
                return response;
            }
            const qint64 boundedLast = qMin(last, audio.size() - 1);
            response.status = 206;
            response.body = audio.mid(first, boundedLast - first + 1);
            response.headers["Content-Range"] = "bytes " + QByteArray::number(first) + "-"
                + QByteArray::number(boundedLast) + "/" + QByteArray::number(audio.size());
            response.headers["ETag"] = changeEtag ? QByteArray("\"fixture-v2\"") : etag;
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

    static qint64 processPeakRssBytes()
    {
#if defined(Q_OS_WIN)
        PROCESS_MEMORY_COUNTERS_EX counters{};
        if (GetProcessMemoryInfo(GetCurrentProcess(),
                reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters), sizeof(counters)))
            return static_cast<qint64>(counters.PeakWorkingSetSize);
#elif defined(Q_OS_LINUX)
        struct rusage usage{};
        if (getrusage(RUSAGE_SELF, &usage) == 0)
            return static_cast<qint64>(usage.ru_maxrss) * 1024;
#endif
        return -1;
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
        fixtureName = QStringLiteral("tone-mp3.mp3");
        secondFixtureName = QStringLiteral("tone-next.mp3");
        exposeSecondFixture = false;
        ignoreRange = false;
        changeEtag = false;
        truncateRange = false;
        force416 = false;
        disconnectBeforeResponse = false;
        retry429Responses = 0;
        rangeReceiving429.clear();
        sameRangeRetried = false;
        delayMs = 0;
        disconnectAfterBytes = -1;
        chunkSize = 0;
        chunkIntervalMs = 0;
        server.includeB2Marker = true;
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
        QTRY_COMPARE_WITH_TIMEOUT(server.activeResponses, 0, 6000);
        server.requests.clear();
        server.close();
    }
    void randomReadUsesValidatedRanges()
    {
        WebDavRangeDevice device(server.serverPort(), "/AllMusic/tone-mp3.mp3", audio.size(), etag);
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
        QCOMPARE(server.requests[1].headers.value("range"),
                 "bytes=65536-" + QByteArray::number(qMin<qint64>(131071, audio.size() - 1)));
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
        QVector<WebDavProbe::Entry> files;
        for (const auto &entry : result.entries) {
            if (!entry.directory)
                files.push_back(entry);
        }
        QCOMPARE(files.size(), 1);
        QCOMPARE(files[0].etag, etag);
        QCOMPARE(files[0].size.value(), audio.size());
        WebDavRangeDevice device(server.serverPort(), files[0].reference,
                                 files[0].size.value(), files[0].etag);
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
        WebDavRangeDevice device(server.serverPort(), "/AllMusic/tone-mp3.mp3", audio.size(), etag);
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
    void range416FailsClosed()
    {
        force416 = true;
        WebDavRangeDevice device(server.serverPort(), "/AllMusic/tone-mp3.mp3", audio.size(), etag);
        QVERIFY(device.open(QIODevice::ReadOnly));
        char byte = 0;
        qint64 read = 0;
        QVERIFY(readWithEventLoop(device, &byte, 1, &read));
        QCOMPARE(read, -1);
        QVERIFY(device.errorString().contains("416"));
        QCOMPARE(device.cachedBytes(), 0);
        QCOMPARE(server.requests.first().headers.value("range"), "bytes=0-65535");
    }
    void unexpectedLoopbackServiceIsRejected()
    {
        server.includeB2Marker = false;
        WebDavRangeDevice device(server.serverPort(), "/AllMusic/tone-mp3.mp3", audio.size(), etag);
        QVERIFY(device.open(QIODevice::ReadOnly));
        char byte = 0;
        qint64 read = 0;
        QVERIFY(readWithEventLoop(device, &byte, 1, &read));
        QCOMPARE(read, -1);
        QVERIFY(device.errorString().contains("HTTP"));
        QCOMPARE(device.cachedBytes(), 0);
    }
    void changedEtagFailsClosed()
    {
        changeEtag = true;
        WebDavRangeDevice device(server.serverPort(), "/AllMusic/tone-mp3.mp3", audio.size(), etag);
        QVERIFY(device.open(QIODevice::ReadOnly));
        QSignalSpy failed(&device, &WebDavRangeDevice::requestFailed);
        char byte = 0;
        qint64 read = 0;
        QVERIFY(readWithEventLoop(device, &byte, 1, &read));
        QCOMPARE(read, -1);
        QTRY_COMPARE(failed.size(), 1);
        QVERIFY(failed.first().first().toString().contains("ETag"));
        QCOMPARE(device.cachedBytes(), 0);
        WebDavRangeDevice recovered(server.serverPort(), "/AllMusic/tone-mp3.mp3",
                                    audio.size(), "\"fixture-v2\"");
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
        WebDavRangeDevice device(server.serverPort(), "/AllMusic/tone-mp3.mp3", audio.size(), etag);
        QVERIFY(device.open(QIODevice::ReadOnly));
        char byte = 0;
        qint64 read = 0;
        QVERIFY(readWithEventLoop(device, &byte, 1, &read));
        QCOMPARE(read, -1);
        QCOMPARE(device.cachedBytes(), 0);
    }
    void delayedAndRateLimitedResponse()
    {
        delayMs = 150;
        chunkSize = 8192;
        chunkIntervalMs = 25;
        WebDavRangeDevice device(server.serverPort(), "/AllMusic/tone-mp3.mp3", audio.size(), etag);
        QVERIFY(device.open(QIODevice::ReadOnly));
        QByteArray page(64 * 1024, Qt::Uninitialized);
        qint64 read = -1;
        QElapsedTimer elapsed;
        elapsed.start();
        QVERIFY(readWithEventLoop(device, page.data(), page.size(), &read));
        const qint64 elapsedMs = elapsed.elapsed();
        QCOMPARE(read, page.size());
        QCOMPARE(page, audio.left(page.size()));
        QVERIFY2(elapsedMs >= 250, qPrintable(QStringLiteral("controlled delay/limit took %1 ms").arg(elapsedMs)));
        qInfo() << "delayed/rate-limited 64 KiB Range elapsed ms" << elapsedMs;
    }
    void slowNetworkRecordsPcmTimingAndGaps()
    {
        QFile file(qEnvironmentVariable("ALLMUSIC_FIXTURES") + "/tone-stream-alac-m4a.m4a");
        QVERIFY(file.open(QIODevice::ReadOnly));
        audio = file.readAll();
        fixtureName = QStringLiteral("tone-stream-alac-m4a.m4a");
        delayMs = 200;
        chunkSize = 4096;
        chunkIntervalMs = 125; // About 32 KiB/s, slower than this ALAC fixture.
        installHandler();
        WebDavRangeDevice device(server.serverPort(), "/AllMusic/tone-stream-alac-m4a.m4a",
                                 audio.size(), etag);
        QVERIFY(device.open(QIODevice::ReadOnly));
        QMediaPlayer player;
        QAudioBufferOutput output;
        player.setAudioOutput(nullptr);
        player.setAudioBufferOutput(&output);
        std::atomic<qint64> decodedFrames{0};
        std::atomic<qint64> transferred{0};
        std::atomic<qint64> firstPcmMs{-1};
        std::atomic<qint64> lastCallbackMs{-1};
        std::atomic<qint64> maximumPcmGapMs{0};
        std::atomic<qint64> gapsOver250Ms{0};
        QElapsedTimer clock;
        connect(&device, &WebDavRangeDevice::bytesFetched, this,
                [&](qint64 count) { transferred.fetch_add(count); }, Qt::DirectConnection);
        connect(&output, &QAudioBufferOutput::audioBufferReceived, this,
                [&](const QAudioBuffer &buffer) {
            if (!buffer.isValid()) return;
            decodedFrames.fetch_add(buffer.frameCount());
            const qint64 now = clock.elapsed();
            const qint64 previous = lastCallbackMs.exchange(now);
            if (previous >= 0) {
                const qint64 gap = now - previous;
                qint64 maximum = maximumPcmGapMs.load();
                while (gap > maximum && !maximumPcmGapMs.compare_exchange_weak(maximum, gap)) { }
                if (gap > 250) gapsOver250Ms.fetch_add(1);
            } else {
                firstPcmMs.store(now);
            }
        }, Qt::DirectConnection);
        clock.start();
        player.setSourceDevice(&device);
        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(decodedFrames.load() > 44100, 30000);
        QVERIFY(firstPcmMs.load() > 500);
        QVERIFY(device.cachedBytes() <= 4 * 64 * 1024);
        qInfo() << "slow ALAC mock ~32 KiB/s: first PCM ms" << firstPcmMs.load()
                << "Range payload bytes" << transferred.load() << "of file" << audio.size()
                << "max PCM callback gap ms" << maximumPcmGapMs.load()
                << "gaps >250 ms" << gapsOver250Ms.load()
                << "process peak RSS bytes" << processPeakRssBytes()
                << "; PCM callback gaps are not an audible-interruption test";
        player.stop();
        player.setSource({});
        device.cancel();
        disconnect(&output, nullptr, this, nullptr);
        disconnect(&device, nullptr, this, nullptr);
    }
    void transient429HonorsRetryAfter()
    {
        retry429Responses = 1;
        WebDavRangeDevice device(server.serverPort(), "/AllMusic/tone-mp3.mp3", audio.size(), etag);
        QVERIFY(device.open(QIODevice::ReadOnly));
        char byte = 0;
        qint64 read = -1;
        QVERIFY(readWithEventLoop(device, &byte, 1, &read));
        QCOMPARE(read, 1);
        QCOMPARE(uchar(byte), uchar(audio[0]));
        QCOMPARE(server.requests.size(), 2);
        QCOMPARE(rangeReceiving429, QByteArray("bytes=0-65535"));
        QVERIFY(sameRangeRetried);
        QVERIFY(device.cachedBytes() > 0);
    }
    void temporaryDisconnectFailsThenNewSourceReconnects()
    {
        disconnectAfterBytes = 2048;
        WebDavRangeDevice first(server.serverPort(), "/AllMusic/tone-mp3.mp3", audio.size(), etag);
        QVERIFY(first.open(QIODevice::ReadOnly));
        char byte = 0;
        qint64 read = 0;
        QVERIFY(readWithEventLoop(first, &byte, 1, &read));
        QCOMPARE(read, -1);
        QCOMPARE(first.cachedBytes(), 0);

        disconnectAfterBytes = -1;
        WebDavRangeDevice reopened(server.serverPort(), "/AllMusic/tone-mp3.mp3", audio.size(), etag);
        QVERIFY(reopened.open(QIODevice::ReadOnly));
        qint64 recoveredRead = -1;
        QVERIFY(readWithEventLoop(reopened, &byte, 1, &recoveredRead));
        QCOMPARE(recoveredRead, 1);
        QCOMPARE(uchar(byte), uchar(audio[0]));
        QCOMPARE(server.requests.size(), 2);
        qInfo() << "after a dropped response, recovery required opening a new source";
    }
    void repeatedSeekUsesValidatedPages()
    {
        QFile file(qEnvironmentVariable("ALLMUSIC_FIXTURES") + "/tone-stream-flac.flac");
        QVERIFY(file.open(QIODevice::ReadOnly));
        audio = file.readAll();
        fixtureName = QStringLiteral("tone-stream-flac.flac");
        installHandler();
        WebDavRangeDevice device(server.serverPort(), "/AllMusic/tone-stream-flac.flac", audio.size(), etag);
        QVERIFY(device.open(QIODevice::ReadOnly));
        const qint64 positions[] = {0, 70000, 131072, 42000, 262144};
        for (qint64 position : positions) {
            QVERIFY(device.seek(position));
            char bytes[32];
            qint64 read = -1;
            QVERIFY(readWithEventLoop(device, bytes, sizeof(bytes), &read));
            QCOMPARE(read, qint64(sizeof(bytes)));
            QCOMPARE(QByteArray(bytes, sizeof(bytes)), audio.mid(position, sizeof(bytes)));
            QVERIFY(device.cachedBytes() <= 4 * 64 * 1024);
        }
        // The requested offsets visit four distinct 64 KiB pages; returning
        // to 42,000 bytes must reuse the still-cached first page.
        QVERIFY(server.requests.size() >= 4);
        qInfo() << "repeated seeks GET count" << server.requests.size()
                << "adapter cache bytes" << device.cachedBytes();
    }
    void allStreamingFormatsDecodeSeekAndFinish_data()
    {
        QTest::addColumn<QString>("name");
        for (const char *name : {"tone-stream-mp3.mp3", "tone-stream-flac.flac",
                                 "tone-stream-aac-adts.aac", "tone-stream-aac-m4a.m4a",
                                 "tone-stream-alac-m4a.m4a"})
            QTest::newRow(name) << QString::fromLatin1(name);
    }
    void allStreamingFormatsDecodeSeekAndFinish()
    {
        QFETCH(QString, name);
        QFile file(qEnvironmentVariable("ALLMUSIC_FIXTURES") + "/" + name);
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(name));
        audio = file.readAll();
        QVERIFY(audio.size() > 64 * 1024);
        fixtureName = name;
        installHandler();
        WebDavRangeDevice device(server.serverPort(), "/AllMusic/" + name, audio.size(), etag);
        QVERIFY(device.open(QIODevice::ReadOnly));
        QMediaPlayer player;
        QAudioBufferOutput output;
        player.setAudioOutput(nullptr);
        player.setAudioBufferOutput(&output);
        std::atomic<qint64> decodedFrames{0};
        std::atomic<qint64> bytesAtFirstPcm{-1};
        std::atomic<qint64> transferred{0};
        std::atomic<qint64> firstPcmMs{-1};
        std::atomic<float> peak{0.0f};
        QElapsedTimer startup;
        connect(&device, &WebDavRangeDevice::bytesFetched, this,
                [&](qint64 count) { transferred.fetch_add(count); }, Qt::DirectConnection);
        connect(&output, &QAudioBufferOutput::audioBufferReceived, this,
                [&](const QAudioBuffer &buffer) {
            if (!buffer.isValid()) return;
            decodedFrames.fetch_add(buffer.frameCount());
            for (int i = 0; i < buffer.sampleCount(); ++i) {
                const float sample = std::abs(buffer.format().normalizedSampleValue(
                    buffer.constData<char>() + i * buffer.format().bytesPerSample()));
                float current = peak.load();
                while (sample > current && !peak.compare_exchange_weak(current, sample)) { }
            }
            if (bytesAtFirstPcm.load() < 0) {
                bytesAtFirstPcm.store(transferred.load());
                firstPcmMs.store(startup.elapsed());
            }
        }, Qt::DirectConnection);
        QSignalSpy errors(&player, &QMediaPlayer::errorOccurred);
        startup.start();
        player.setSourceDevice(&device);
        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(player.hasAudio() && player.duration() > 30000, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(decodedFrames.load() > 4096 && peak.load() > 0.01f, 15000);
        QVERIFY(bytesAtFirstPcm.load() > 0);
        QVERIFY2(bytesAtFirstPcm.load() < audio.size(), "streaming decoder received the whole file before first PCM");
        QVERIFY(player.isSeekable());
        QVERIFY(device.cachedBytes() <= 4 * 64 * 1024);
        qInfo() << name << "first PCM ms" << firstPcmMs.load()
                << "validated Range payload bytes" << bytesAtFirstPcm.load()
                << "file bytes" << audio.size() << "adapter cache" << device.cachedBytes()
                << "process peak RSS bytes" << processPeakRssBytes();

        for (qint64 target : {25000, 5000, 28000}) {
            player.pause();
            const qint64 framesBeforeSeek = decodedFrames.load();
            QElapsedTimer seekTimer;
            seekTimer.start();
            player.setPosition(target);
            player.play();
            QTRY_VERIFY_WITH_TIMEOUT(qAbs(player.position() - target) < 1000
                                     && decodedFrames.load() > framesBeforeSeek + 512, 12000);
            qInfo() << name << "seek to" << target << "ms took" << seekTimer.elapsed() << "ms";
            QVERIFY(device.cachedBytes() <= 4 * 64 * 1024);
        }
        player.pause();
        player.setPosition(30000);
        player.play();
        QTRY_COMPARE_WITH_TIMEOUT(player.mediaStatus(), QMediaPlayer::EndOfMedia, 12000);
        QVERIFY(qAbs(player.position() - player.duration()) < 1500);
        QVERIFY(decodedFrames.load() > 8192);
        QVERIFY2(errors.isEmpty(), qPrintable(name + QStringLiteral(": ") + player.errorString()));
        QVERIFY(device.cachedBytes() <= 4 * 64 * 1024);
        qInfo() << name << "finished at" << player.position() << "of" << player.duration()
                << "ms; fetched payload bytes" << transferred.load()
                << "requests" << server.requests.size() << "adapter cache" << device.cachedBytes();
        player.stop();
        player.setSource({});
        device.cancel();
        disconnect(&output, nullptr, this, nullptr);
        disconnect(&device, nullptr, this, nullptr);
    }
    void decoderStartsBeforeWholeFlacArrives()
    {
        QFile flacFile(qEnvironmentVariable("ALLMUSIC_FIXTURES") + "/tone-stream-flac.flac");
        QVERIFY(flacFile.open(QIODevice::ReadOnly));
        audio = flacFile.readAll();
        fixtureName = QStringLiteral("tone-stream-flac.flac");
        installHandler();
        QVERIFY(audio.size() > 2 * 64 * 1024);
        WebDavRangeDevice device(server.serverPort(), "/AllMusic/tone-stream-flac.flac", audio.size(), etag);
        QVERIFY(device.open(QIODevice::ReadOnly));
        QMediaPlayer player;
        QAudioBufferOutput output;
        player.setAudioOutput(nullptr);
        player.setAudioBufferOutput(&output);
        std::atomic<qint64> decodedFrames{0};
        std::atomic<qint64> bytesAtFirstPcm{-1};
        std::atomic<qint64> transferred{0};
        std::atomic<qint64> firstPcmMs{-1};
        std::atomic<float> peak{0.0f};
        QElapsedTimer startup;
        connect(&device, &WebDavRangeDevice::bytesFetched, this,
                [&](qint64 count) { transferred.fetch_add(count); }, Qt::DirectConnection);
        connect(&output, &QAudioBufferOutput::audioBufferReceived, this,
                [&](const QAudioBuffer &buffer) {
            if (!buffer.isValid()) return;
            decodedFrames.fetch_add(buffer.frameCount());
            for (int i = 0; i < buffer.sampleCount(); ++i) {
                const float sample = std::abs(buffer.format().normalizedSampleValue(
                    buffer.constData<char>() + i * buffer.format().bytesPerSample()));
                float current = peak.load();
                while (sample > current && !peak.compare_exchange_weak(current, sample)) { }
            }
            if (bytesAtFirstPcm.load() < 0) {
                bytesAtFirstPcm.store(transferred.load());
                firstPcmMs.store(startup.elapsed());
            }
        }, Qt::DirectConnection);
        startup.start();
        player.setSourceDevice(&device);
        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(decodedFrames.load() > 4096 && peak.load() > 0.01f, 12000);
        QVERIFY(bytesAtFirstPcm.load() > 0);
        QVERIFY2(bytesAtFirstPcm.load() < audio.size(), "decoder consumed the entire file before first PCM");
        QVERIFY2(device.cachedBytes() <= 4 * 64 * 1024, "page cache exceeded its fixed bound");
        QTRY_VERIFY_WITH_TIMEOUT(player.duration() > 30000 && player.isSeekable(), 5000);
        player.pause();
        const qint64 beforeSeekFrames = decodedFrames.load();
        QElapsedTimer seekTimer;
        seekTimer.start();
        player.setPosition(25000);
        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(decodedFrames.load() > beforeSeekFrames + 4096, 7000);
        QVERIFY(qAbs(player.position() - 25000) < 1000);
        QVERIFY(device.cachedBytes() <= 4 * 64 * 1024);
        bool requestedUnseenPage = false;
        for (const auto &request : server.requests)
            requestedUnseenPage |= request.headers.value("range").startsWith("bytes=262144-");
        QVERIFY2(requestedUnseenPage, "seek did not issue a Range request for the later file region");
        player.stop();
        qInfo() << "FLAC measurement (separate detailed baseline): PCM ms" << firstPcmMs.load()
                << "seek ms" << seekTimer.elapsed() << "payload bytes at PCM" << bytesAtFirstPcm.load()
                << "of" << audio.size() << "; adapter cache" << device.cachedBytes()
                << "; process peak RSS bytes" << processPeakRssBytes()
                << "; decoded frames" << decodedFrames.load() << "; requests" << server.requests.size()
                << "; this is PCM, not audible-output evidence";
        player.setSource({});
        device.cancel();
        disconnect(&output, nullptr, this, nullptr);
        disconnect(&device, nullptr, this, nullptr);
    }
    void stopDuringActiveLoadReturnsPromptly()
    {
        delayMs = 2500;
        LocalPlayback playback;
        playback.scanLoopback(server.serverPort());
        QTRY_COMPARE_WITH_TIMEOUT(playback.loopbackFiles().size(), 1, 5000);
        QVERIFY(playback.openLoopback(0));
        playback.player()->play();
        QTRY_VERIFY_WITH_TIMEOUT(server.requests.size() >= 2, 5000);
        QElapsedTimer timer;
        timer.start();
        playback.player()->stop();
        const qint64 stopMs = timer.elapsed();
        QTest::qWait(100);
        qInfo() << "QMediaPlayer::stop during delayed GET returned in" << stopMs
                << "ms; active mock responses after 100 ms" << server.activeResponses
                << "(nonzero means the adapter request was not cancelled by stop)";
        QVERIFY(stopMs < 1000);
    }
    void trackSwitchCancelsOldLoadAndContinues()
    {
        exposeSecondFixture = true;
        delayMs = 2500;
        LocalPlayback playback;
        playback.scanLoopback(server.serverPort());
        QTRY_COMPARE_WITH_TIMEOUT(playback.loopbackFiles().size(), 2, 5000);
        QVERIFY(playback.openLoopback(0));
        playback.player()->play();
        QTRY_VERIFY_WITH_TIMEOUT(server.requests.size() >= 2, 5000);
        QElapsedTimer timer;
        timer.start();
        QVERIFY(playback.openLoopback(1));
        const qint64 switchMs = timer.elapsed();
        QVERIFY(switchMs < 1500);
        playback.player()->play();
        QTRY_VERIFY_WITH_TIMEOUT(server.requests.size() >= 3, 3000);
        bool requestedSecond = false;
        for (const auto &request : server.requests)
            requestedSecond |= request.method == "GET" && request.target == "/AllMusic/tone-next.mp3";
        QVERIFY(requestedSecond);
        QTRY_COMPARE_WITH_TIMEOUT(server.activeResponses, 1, 1000);
        qInfo() << "track switch cancelled/replaced the source in" << switchMs << "ms";
    }
    void destroyingPlayerDuringActiveLoadReturnsPromptly()
    {
        delayMs = 2500;
        auto *playback = new LocalPlayback;
        playback->scanLoopback(server.serverPort());
        QTRY_COMPARE_WITH_TIMEOUT(playback->loopbackFiles().size(), 1, 5000);
        QVERIFY(playback->openLoopback(0));
        playback->player()->play();
        QTRY_VERIFY_WITH_TIMEOUT(server.requests.size() >= 2, 5000);
        QElapsedTimer timer;
        timer.start();
        delete playback;
        const qint64 destroyMs = timer.elapsed();
        QVERIFY2(destroyMs < 2000, qPrintable(QStringLiteral("destroy took %1 ms").arg(destroyMs)));
        QTRY_COMPARE_WITH_TIMEOUT(server.activeResponses, 0, 1000);
        qInfo() << "destroying LocalPlayback with a pending GET completed in" << destroyMs << "ms";
    }
    void cancelInterruptsActiveRequest()
    {
        delayMs = 3000;
        WebDavRangeDevice device(server.serverPort(), "/AllMusic/tone-mp3.mp3", audio.size(), etag);
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
