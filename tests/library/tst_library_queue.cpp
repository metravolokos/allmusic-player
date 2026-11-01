#include "AudioMetadataReader.h"
#include "LibraryModel.h"
#include "PlaybackQueueModel.h"
#include "TrackIdentity.h"

#include <QSignalSpy>
#include <QFile>
#include <QSet>
#include <QStandardPaths>
#include <QtTest>

namespace {
QByteArray id3TextFrame(const QByteArray &id, const QByteArray &value)
{
    QByteArray frame = id;
    const quint32 size = static_cast<quint32>(value.size() + 1);
    frame.append(char(0));
    frame.append(char(0));
    frame.append(char(0));
    frame.append(char(size));
    frame.append(char(0));
    frame.append(char(0));
    frame.append(char(3));
    frame += value;
    return frame;
}

QByteArray mp3WithTitle(const QByteArray &title)
{
    const QByteArray frame = id3TextFrame("TIT2", title);
    QByteArray bytes("ID3", 3);
    bytes.append(char(3));
    bytes.append(char(0));
    bytes.append(char(0));
    const quint32 size = static_cast<quint32>(frame.size());
    bytes.append(char((size >> 21) & 0x7f));
    bytes.append(char((size >> 14) & 0x7f));
    bytes.append(char((size >> 7) & 0x7f));
    bytes.append(char(size & 0x7f));
    bytes += frame;
    return bytes;
}

void appendLittleEndian32(QByteArray &bytes, quint32 value)
{
    bytes.append(char(value & 0xff));
    bytes.append(char((value >> 8) & 0xff));
    bytes.append(char((value >> 16) & 0xff));
    bytes.append(char((value >> 24) & 0xff));
}

QByteArray flacWithComment(const QByteArray &comment)
{
    QByteArray block;
    appendLittleEndian32(block, 0); // Empty vendor string.
    appendLittleEndian32(block, 1);
    appendLittleEndian32(block, static_cast<quint32>(comment.size()));
    block += comment;

    QByteArray bytes("fLaC", 4);
    bytes.append(char(0x84)); // Last metadata block, Vorbis comment.
    bytes.append(char((block.size() >> 16) & 0xff));
    bytes.append(char((block.size() >> 8) & 0xff));
    bytes.append(char(block.size() & 0xff));
    bytes += block;
    return bytes;
}

LibraryModel::Track track(const QString &path, const QString &title, const QString &artist,
                          const QString &album, int trackNumber, int discNumber = 1)
{
    LibraryModel::Track result;
    result.path = path;
    result.stableId = TrackIdentity::stableIdForPath(path);
    result.size = 1000;
    result.etag = QByteArrayLiteral("\"v1\"");
    result.title = title;
    result.artist = artist;
    result.albumArtist = artist;
    result.album = album;
    result.trackNumber = trackNumber;
    result.discNumber = discNumber;
    result.metadataLoaded = true;
    return result;
}

QVector<LibraryModel::Track> sampleTracks()
{
    return {
        track(QStringLiteral("/AllMusic/Artist A/Album One/02-Beta.mp3"),
              QStringLiteral("Beta"), QStringLiteral("Artist A"), QStringLiteral("Album One"), 2),
        track(QStringLiteral("/AllMusic/Artist A/Album One/01-Alpha.mp3"),
              QStringLiteral("Alpha"), QStringLiteral("Artist A"), QStringLiteral("Album One"), 1),
        track(QStringLiteral("/AllMusic/Artist A/Album Two/01-Gamma.flac"),
              QStringLiteral("Gamma"), QStringLiteral("Artist A"), QStringLiteral("Album Two"), 1),
        track(QStringLiteral("/AllMusic/Artist B/Album Three/01-Delta.mp3"),
              QStringLiteral("Delta"), QStringLiteral("Artist B"), QStringLiteral("Album Three"), 1),
    };
}
}

class LibraryQueueTest final : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void readsId3AndVorbisComments();
    void buildsArtistAlbumAndFolderQueues();
    void queueTracksStayIndependentOfSearchAndReceiveTags();
    void advancesAndRejectsStaleCompletion();
    void repeatModesAndHistoryWork();
    void shuffleDoesNotRepeatWithinPass();
    void handlesEmptyAndSingleTrackQueues();
    void playbackErrorDoesNotSkipTrack();
};

void LibraryQueueTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    const QString cachePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/library-index-v1.json");
    QFile::remove(cachePath);
}

void LibraryQueueTest::readsId3AndVorbisComments()
{
    const AudioTags mp3 = AudioMetadataReader::read(QStringLiteral("song.mp3"),
                                                    mp3WithTitle("Blue Morning"));
    QCOMPARE(mp3.title, QStringLiteral("Blue Morning"));

    const AudioTags flac = AudioMetadataReader::read(QStringLiteral("song.flac"),
                                                     flacWithComment("ARTIST=The Band"));
    QCOMPARE(flac.artist, QStringLiteral("The Band"));
}

void LibraryQueueTest::buildsArtistAlbumAndFolderQueues()
{
    LibraryModel library;
    library.setRemoteTracks(sampleTracks());
    const auto tracks = library.tracksForQueue(LibraryModel::AllTracks, {});
    QCOMPARE(tracks.size(), 4);
    QCOMPARE(tracks.first().title, QStringLiteral("Alpha"));
    library.setSearchText(QStringLiteral("Delta"));
    QCOMPARE(library.rowCount(), 1);
    QCOMPARE(library.tracksForQueue(LibraryModel::AllTracks, {}).size(), 4);
    library.setSearchText({});

    const auto artistTracks = library.tracksForQueue(LibraryModel::ArtistTracks,
                                                      QStringLiteral("Artist A"));
    QCOMPARE(artistTracks.size(), 3);
    const auto albumKey = LibraryModel::albumKeyFor(tracks.first());
    const auto albumTracks = library.tracksForQueue(LibraryModel::AlbumTracks, albumKey);
    QCOMPARE(albumTracks.size(), 2);
    QCOMPARE(albumTracks.at(0).trackNumber, 1);
    QCOMPARE(albumTracks.at(1).trackNumber, 2);
    const auto folderTracks = library.tracksForQueue(LibraryModel::FolderTracks,
        QStringLiteral("/AllMusic/Artist A/Album One/"));
    QCOMPARE(folderTracks.size(), 2);
    const auto parentFolderTracks = library.tracksForQueue(LibraryModel::FolderTracks,
        QStringLiteral("/AllMusic/Artist A"));
    QCOMPARE(parentFolderTracks.size(), 0);

    PlaybackQueueModel queue(&library);
    QVERIFY(queue.startFolder(QStringLiteral("/AllMusic/Artist A/Album One/"),
                              tracks.first().stableId));
    QCOMPARE(queue.count(), 2);

    auto updatedTracks = sampleTracks();
    updatedTracks[0].artist.clear();
    updatedTracks[0].albumArtist = QStringLiteral("Album Artist A");
    library.setRemoteTracks(updatedTracks);
    QCOMPARE(library.tracksForQueue(LibraryModel::ArtistTracks,
                                    QStringLiteral("Album Artist A")).size(), 1);
    QVERIFY(library.firstPlayableStableId(LibraryModel::ArtistTracks,
                                           QStringLiteral("Album Artist A")).length() > 0);
}

void LibraryQueueTest::queueTracksStayIndependentOfSearchAndReceiveTags()
{
    LibraryModel library;
    QVector<LibraryModel::Track> tracks = sampleTracks();
    tracks[0].title.clear();
    tracks[0].artist.clear();
    tracks[0].albumArtist.clear();
    tracks[0].album.clear();
    tracks[0].metadataLoaded = false;
    const QString stableId = tracks[0].stableId;
    library.setRemoteTracks(tracks);

    PlaybackQueueModel queue(&library);
    QVERIFY(queue.startAll(stableId));
    const int allTracksCount = queue.count();
    QCOMPARE(queue.currentTitle(), QStringLiteral("02-Beta"));
    library.setMode(LibraryModel::Artists);
    library.setSearchText(QStringLiteral("Artist B"));
    QCOMPARE(queue.count(), allTracksCount);
    QCOMPARE(queue.currentTrackId(), stableId);
    library.setMode(LibraryModel::Tracks);
    library.applyTags(stableId, QStringLiteral("Tagged Title"), QStringLiteral("Tagged Artist"),
                      QStringLiteral("Tagged Artist"), QStringLiteral("Tagged Album"), 1, 2);
    QCOMPARE(queue.currentTitle(), QStringLiteral("Tagged Title"));
    QCOMPARE(queue.currentArtist(), QStringLiteral("Tagged Artist"));
}

void LibraryQueueTest::advancesAndRejectsStaleCompletion()
{
    LibraryModel library;
    library.setRemoteTracks(sampleTracks());
    PlaybackQueueModel queue(&library);
    const auto ordered = library.tracksForQueue(LibraryModel::AllTracks, {});
    QVERIFY(queue.startAll(ordered.at(0).stableId));
    QCOMPARE(queue.currentTrackId(), ordered.at(0).stableId);

    const quint64 staleGeneration = queue.generation();
    queue.next();
    QCOMPARE(queue.currentTrackId(), ordered.at(1).stableId);
    const QString current = queue.currentTrackId();
    queue.advance(staleGeneration);
    QCOMPARE(queue.currentTrackId(), current);

    QVERIFY(queue.canPrevious());
    queue.previous();
    QCOMPARE(queue.currentTrackId(), ordered.at(0).stableId);
    queue.next();
    QCOMPARE(queue.currentTrackId(), ordered.at(1).stableId);

    PlaybackQueueModel previousFromStart(&library);
    QVERIFY(previousFromStart.startAll(ordered.at(2).stableId));
    QVERIFY(previousFromStart.canPrevious());
    previousFromStart.previous();
    QCOMPARE(previousFromStart.currentTrackId(), ordered.at(1).stableId);
    previousFromStart.next();
    QCOMPARE(previousFromStart.currentTrackId(), ordered.at(2).stableId);
}

void LibraryQueueTest::repeatModesAndHistoryWork()
{
    LibraryModel library;
    library.setRemoteTracks(sampleTracks());
    const auto ordered = library.tracksForQueue(LibraryModel::AllTracks, {});

    PlaybackQueueModel repeatAll(&library);
    repeatAll.setRepeatMode(PlaybackQueueModel::RepeatAll);
    QVERIFY(repeatAll.startAll(ordered.last().stableId));
    QCOMPARE(repeatAll.count(), ordered.size());
    repeatAll.next();
    QCOMPARE(repeatAll.currentTrackId(), ordered.first().stableId);
    for (int i = 0; i < ordered.size() - 1; ++i)
        repeatAll.next();
    QCOMPARE(repeatAll.currentTrackId(), ordered.last().stableId);
    QVERIFY(repeatAll.active());

    PlaybackQueueModel repeatOne(&library);
    repeatOne.setRepeatMode(PlaybackQueueModel::RepeatOne);
    QVERIFY(repeatOne.startAll(ordered.at(0).stableId));
    const QString first = repeatOne.currentTrackId();
    const quint64 generation = repeatOne.generation();
    repeatOne.advance(generation);
    QCOMPARE(repeatOne.currentTrackId(), first);
    QVERIFY(repeatOne.generation() > generation);
    repeatOne.next();
    QCOMPARE(repeatOne.currentTrackId(), ordered.at(1).stableId);

    PlaybackQueueModel shuffledRepeat(&library);
    shuffledRepeat.setRepeatMode(PlaybackQueueModel::RepeatAll);
    shuffledRepeat.setShuffleEnabled(true);
    QVERIFY(shuffledRepeat.startAll(ordered.at(1).stableId));
    shuffledRepeat.previous();
    QVERIFY(shuffledRepeat.currentTrackId() != ordered.at(1).stableId);
    shuffledRepeat.next();
    QCOMPARE(shuffledRepeat.currentTrackId(), ordered.at(1).stableId);
}

void LibraryQueueTest::shuffleDoesNotRepeatWithinPass()
{
    LibraryModel library;
    library.setRemoteTracks(sampleTracks());
    PlaybackQueueModel queue(&library);
    const auto ordered = library.tracksForQueue(LibraryModel::AllTracks, {});
    QVERIFY(queue.startAll(ordered.first().stableId));
    queue.setShuffleEnabled(true);
    QVERIFY(queue.shuffleEnabled());
    QSignalSpy stopped(&queue, &PlaybackQueueModel::stopRequested);

    QSet<QString> seen;
    while (queue.active()) {
        seen.insert(queue.currentTrackId());
        const quint64 generation = queue.generation();
        queue.advance(generation);
    }
    QCOMPARE(seen.size(), ordered.size());
    QVERIFY(!queue.canNext());
    QCOMPARE(stopped.count(), 1);
}

void LibraryQueueTest::handlesEmptyAndSingleTrackQueues()
{
    LibraryModel library;
    PlaybackQueueModel emptyQueue(&library);
    QVERIFY(!emptyQueue.startAll(QStringLiteral("missing")));
    emptyQueue.next();
    QVERIFY(!emptyQueue.active());

    library.setRemoteTracks({sampleTracks().first()});
    const auto onlyTrack = library.tracksForQueue(LibraryModel::AllTracks, {}).first();
    PlaybackQueueModel oneTrack(&library);
    oneTrack.setShuffleEnabled(true);
    QVERIFY(oneTrack.startAll(onlyTrack.stableId));
    QCOMPARE(oneTrack.count(), 1);
    oneTrack.next();
    QVERIFY(!oneTrack.active());
    QCOMPARE(oneTrack.currentTrackId(), onlyTrack.stableId);
    QVERIFY(!oneTrack.canNext());

    oneTrack.setRepeatMode(PlaybackQueueModel::RepeatAll);
    oneTrack.next();
    QVERIFY(oneTrack.active());
    QCOMPARE(oneTrack.currentTrackId(), onlyTrack.stableId);
    QVERIFY(oneTrack.canPrevious());
    oneTrack.previous();
    QCOMPARE(oneTrack.currentTrackId(), onlyTrack.stableId);

    oneTrack.setRepeatMode(PlaybackQueueModel::RepeatOne);
    const quint64 generation = oneTrack.generation();
    oneTrack.advance(generation);
    QCOMPARE(oneTrack.currentTrackId(), onlyTrack.stableId);
    QVERIFY(oneTrack.active());
}

void LibraryQueueTest::playbackErrorDoesNotSkipTrack()
{
    LibraryModel library;
    library.setRemoteTracks(sampleTracks());
    PlaybackQueueModel queue(&library);
    const auto ordered = library.tracksForQueue(LibraryModel::AllTracks, {});
    QVERIFY(queue.startAll(ordered.first().stableId));
    const quint64 generation = queue.generation();
    QSignalSpy stopped(&queue, &PlaybackQueueModel::stopRequested);
    queue.reportError(generation, QStringLiteral("Range request failed"));
    QCOMPARE(queue.currentTrackId(), ordered.first().stableId);
    QVERIFY(!queue.active());
    QCOMPARE(queue.error(), QStringLiteral("Range request failed"));
    QCOMPARE(stopped.count(), 1);

    QVERIFY(queue.startAlbum(LibraryModel::albumKeyFor(ordered.first()),
                             ordered.first().stableId));
    QCOMPARE(queue.contextText(), QStringLiteral("Альбом · Album One"));
    QCOMPARE(queue.count(), 2);
}

QTEST_GUILESS_MAIN(LibraryQueueTest)
#include "tst_library_queue.moc"
