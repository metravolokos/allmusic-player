#include "LocalPlayback.h"
#include <QAudioBufferOutput>
#include <QAudioBuffer>
#include <QFile>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QtTest>
#include <cmath>

class PlaybackTest : public QObject
{
    Q_OBJECT
private slots:
    void rejectsNonLocal()
    {
        LocalPlayback engine;
        QVERIFY(!engine.openLocal(QUrl("https://example.invalid/music.mp3")));
        QVERIFY(engine.player()->source().isEmpty());
        QVERIFY(!engine.error().isEmpty());
        QVERIFY(!engine.openLocal(QUrl("file://server/share/music.mp3")));
        QVERIFY(!engine.openLocal(QUrl::fromLocalFile("/not-present-001b1.mp3")));
        QTemporaryDir dir;
        QFile fake(dir.filePath("fake.mp3"));
        QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("#EXTM3U\nhttps://example.invalid/music.mp3\n");
        fake.close();
        QVERIFY(!engine.openLocal(QUrl::fromLocalFile(fake.fileName())));
    }

    void decode_data()
    {
        QTest::addColumn<QString>("name");
        for (const auto &name : {"tone-mp3.mp3", "tone-flac.flac", "tone-aac-adts.aac",
                                 "tone-aac-m4a.m4a", "tone-alac-m4a.m4a"})
            QTest::newRow(name) << QString::fromLatin1(name);
    }

    void decode()
    {
        QFETCH(QString, name);
        const QString root = qEnvironmentVariable("ALLMUSIC_FIXTURES");
        QVERIFY2(!root.isEmpty(), "Set ALLMUSIC_FIXTURES to generated fixture directory");
        LocalPlayback engine;
        auto *player = engine.player();
        // No sound device dependency: this test proves decoded PCM, not audibility.
        player->setAudioOutput(nullptr);
        QAudioBufferOutput output;
        player->setAudioBufferOutput(&output);
        qint64 frames = 0;
        float peak = 0;
        connect(&output, &QAudioBufferOutput::audioBufferReceived, this,
                [&](const QAudioBuffer &buffer) {
            if (!buffer.isValid()) return;
            frames += buffer.frameCount();
            for (int i = 0; i < buffer.sampleCount(); ++i)
                peak = std::max(peak, std::abs(buffer.format().normalizedSampleValue(
                    buffer.constData<char>() + i * buffer.format().bytesPerSample())));
        });
        QSignalSpy errors(player, &QMediaPlayer::errorOccurred);
        QVERIFY(engine.openLocal(QUrl::fromLocalFile(root + "/" + name)));
        QTRY_VERIFY_WITH_TIMEOUT(player->hasAudio() && player->duration() > 7500, 10000);
        player->play();
        QTRY_VERIFY_WITH_TIMEOUT(frames > 22050 && peak > 0.01f, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(player->position() > 300, 5000);
        player->pause();
        QTRY_COMPARE(player->playbackState(), QMediaPlayer::PausedState);
        const qint64 paused = player->position();
        QTest::qWait(200);
        QVERIFY(std::abs(player->position() - paused) < 150);
        QVERIFY(player->isSeekable());
        player->setPosition(5000);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(player->position() - 5000) < 600, 5000);
        const qint64 before = frames;
        player->play();
        QTRY_VERIFY_WITH_TIMEOUT(frames > before + 4096 && player->position() > 5300, 5000);
        player->stop();
        QCOMPARE(player->playbackState(), QMediaPlayer::StoppedState);
        QCOMPARE(player->position(), 0);
        engine.audio()->setVolume(0.4f);
        QVERIFY(std::abs(engine.audio()->volume() - 0.4f) < 0.001f);
        // Decode again after stop, through EOF; guard against state-only success.
        player->play();
        QTRY_COMPARE_WITH_TIMEOUT(player->mediaStatus(), QMediaPlayer::EndOfMedia, 15000);
        QVERIFY(frames > 7 * 44100);
        QVERIFY2(errors.isEmpty(), qPrintable(engine.error()));
        qInfo() << name << "decoded frames" << frames << "peak" << peak
                << "; physical audio output NOT TESTED";
    }

    void corruptAndRecover()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath("broken.flac"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("fLaCthis is not a valid audio stream");
        file.close();
        LocalPlayback engine;
        engine.player()->setAudioOutput(nullptr);
        QVERIFY(engine.openLocal(QUrl::fromLocalFile(file.fileName())));
        engine.player()->play();
        QTRY_VERIFY_WITH_TIMEOUT(!engine.error().isEmpty(), 10000);
        const QString root = qEnvironmentVariable("ALLMUSIC_FIXTURES");
        // Misleading extension must not determine decoder choice.
        const QString renamed = dir.filePath("actually-flac.mp3");
        QVERIFY(QFile::copy(root + "/tone-flac.flac", renamed));
        QVERIFY(engine.openLocal(QUrl::fromLocalFile(renamed)));
        QTRY_VERIFY_WITH_TIMEOUT(engine.player()->hasAudio(), 10000);
        QVERIFY(engine.error().isEmpty());
    }
};
QTEST_GUILESS_MAIN(PlaybackTest)
#include "tst_playback.moc"
