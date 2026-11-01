#include "LibraryModel.h"

#include "TrackIdentity.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>
#include <utility>

namespace {
QString cacheFilePath()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return directory.isEmpty() ? QString() : directory + QStringLiteral("/library-index-v1.json");
}

QString groupId(const QString &prefix, const QString &value)
{
    const QByteArray digest = QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha256);
    return prefix + QString::fromLatin1(digest.toHex());
}

bool isAudioPath(const QString &path)
{
    const QString lower = path.toLower();
    return lower.endsWith(QStringLiteral(".mp3")) || lower.endsWith(QStringLiteral(".flac"))
        || lower.endsWith(QStringLiteral(".aac")) || lower.endsWith(QStringLiteral(".m4a"))
        || lower.endsWith(QStringLiteral(".mp4"));
}

bool canStreamPath(const QString &path)
{
    const QString lower = path.toLower();
    return lower.endsWith(QStringLiteral(".mp3")) || lower.endsWith(QStringLiteral(".flac"));
}

bool sameVersion(const LibraryModel::Track &left, const LibraryModel::Track &right)
{
    return left.size == right.size && !left.etag.isEmpty() && left.etag == right.etag;
}
}

LibraryModel::LibraryModel(QObject *parent)
    : QAbstractListModel(parent), m_collator(QLocale(QLocale::Russian, QLocale::Russia))
{
    m_collator.setCaseSensitivity(Qt::CaseInsensitive);
    m_collator.setNumericMode(true);
    m_groupRefreshTimer.setSingleShot(true);
    m_groupRefreshTimer.setInterval(1000);
    connect(&m_groupRefreshTimer, &QTimer::timeout, this, &LibraryModel::rebuildRows);
    loadCache();
    rebuildRows();
}

int LibraryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant LibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row &row = m_rows.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case LabelRole: return row.label;
    case StableIdRole: return row.stableId;
    case ArtistRole: return row.artist;
    case AlbumArtistRole: return row.albumArtist;
    case AlbumRole: return row.album;
    case AlbumKeyRole: return row.albumKey;
    case FilePathRole: return row.filePath;
    case ParentPathRole: return row.parentPath;
    case SizeRole: return row.trackIndex >= 0 ? QVariant::fromValue(m_tracks.at(row.trackIndex).size) : QVariant();
    case IsDirectoryRole: return false;
    case IsAudioRole: return row.kind == 0;
    case IsPlayableRole: return row.playable && row.available;
    case IsAvailableRole: return row.available;
    case TrackCountRole: return row.trackCount;
    case DiscNumberRole: return row.discNumber;
    case TrackNumberRole: return row.trackNumber;
    case KindRole: return row.kind == 0 ? QStringLiteral("track")
        : row.kind == 1 ? QStringLiteral("artist") : QStringLiteral("album");
    default: return {};
    }
}

QHash<int, QByteArray> LibraryModel::roleNames() const
{
    return {{StableIdRole, "stableId"}, {LabelRole, "label"}, {ArtistRole, "artist"},
            {AlbumArtistRole, "albumArtist"}, {AlbumRole, "album"},
            {AlbumKeyRole, "albumKey"}, {FilePathRole, "filePath"},
            {ParentPathRole, "parentPath"}, {SizeRole, "size"},
            {IsDirectoryRole, "isDirectory"}, {IsAudioRole, "isAudio"},
            {IsPlayableRole, "isPlayable"}, {IsAvailableRole, "isAvailable"},
            {TrackCountRole, "trackCount"}, {DiscNumberRole, "discNumber"},
            {TrackNumberRole, "trackNumber"}, {KindRole, "kind"}};
}

void LibraryModel::setMode(int mode)
{
    if (mode < Tracks || mode > Albums || m_mode == mode)
        return;
    m_mode = mode;
    if (m_mode == Tracks)
        m_groupRefreshTimer.stop();
    rebuildRows();
    emit viewChanged();
}

void LibraryModel::setSearchText(const QString &text)
{
    const QString normalized = text.normalized(QString::NormalizationForm_C);
    if (m_searchText == normalized)
        return;
    m_searchText = normalized;
    rebuildRows();
    emit viewChanged();
}

void LibraryModel::setSortMode(int mode)
{
    if (mode < NameAscending || mode > SourceOrder || m_sortMode == mode)
        return;
    m_sortMode = mode;
    rebuildRows();
    emit viewChanged();
}

void LibraryModel::setFilterArtist(const QString &artist)
{
    if (m_filterArtist == artist)
        return;
    m_filterArtist = artist;
    rebuildRows();
    emit viewChanged();
}

void LibraryModel::setFilterAlbum(const QString &albumKey)
{
    if (m_filterAlbum == albumKey)
        return;
    m_filterAlbum = albumKey;
    rebuildRows();
    emit viewChanged();
}

void LibraryModel::setFilterFolder(const QString &folder)
{
    if (m_filterFolder == folder)
        return;
    m_filterFolder = folder;
    rebuildRows();
    emit viewChanged();
}

void LibraryModel::setRemoteTracks(const QVector<Track> &tracks)
{
    QHash<QString, Track> cached;
    cached.reserve(m_tracks.size());
    for (const Track &track : std::as_const(m_tracks))
        cached.insert(track.stableId, track);

    QVector<Track> next;
    next.reserve(tracks.size());
    for (Track remote : tracks) {
        if (remote.stableId.isEmpty() || !TrackIdentity::isValidStableId(remote.stableId)
            || !TrackIdentity::isValidPath(remote.path) || !isAudioPath(remote.path))
            continue;
        remote.available = true;
        remote.playable = canStreamPath(remote.path) && remote.size > 0;
        remote.title = fallbackTitle(remote.path);
        const auto cachedIt = cached.constFind(remote.stableId);
        if (cachedIt != cached.cend() && sameVersion(cachedIt.value(), remote)
            && cachedIt->metadataLoaded) {
            remote.title = cachedIt->title;
            remote.artist = cachedIt->artist;
            remote.albumArtist = cachedIt->albumArtist;
            remote.album = cachedIt->album;
            remote.discNumber = cachedIt->discNumber;
            remote.trackNumber = cachedIt->trackNumber;
            remote.metadataLoaded = true;
        }
        next.push_back(std::move(remote));
    }

    beginResetModel();
    m_tracks = std::move(next);
    m_trackIndices.clear();
    for (int i = 0; i < m_tracks.size(); ++i)
        m_trackIndices.insert(m_tracks.at(i).stableId, i);
    m_rows.clear();
    endResetModel();
    rebuildRows();
    emit trackCountChanged();
    emit viewChanged();
}

void LibraryModel::applyTags(const QString &stableId, const QString &title,
                             const QString &artist, const QString &albumArtist,
                             const QString &album, int discNumber, int trackNumber)
{
    const auto it = m_trackIndices.constFind(stableId);
    if (it == m_trackIndices.cend())
        return;
    Track &track = m_tracks[it.value()];
    track.title = title.trimmed().isEmpty() ? fallbackTitle(track.path) : title.trimmed();
    track.artist = artist.trimmed();
    track.albumArtist = albumArtist.trimmed();
    track.album = album.trimmed();
    track.discNumber = qMax(0, discNumber);
    track.trackNumber = qMax(0, trackNumber);
    track.metadataLoaded = true;
    updateTrackRow(stableId);
    if (m_mode != Tracks && !m_groupRefreshTimer.isActive())
        m_groupRefreshTimer.start();
    emit trackMetadataChanged(stableId);
}

bool LibraryModel::needsTags(const QString &stableId) const
{
    const int index = m_trackIndices.value(stableId, -1);
    return index >= 0 && !m_tracks.at(index).metadataLoaded
        && (m_tracks.at(index).path.endsWith(QStringLiteral(".mp3"), Qt::CaseInsensitive)
            || m_tracks.at(index).path.endsWith(QStringLiteral(".flac"), Qt::CaseInsensitive));
}

void LibraryModel::updateScanProgress(bool running, int completed, int total)
{
    if (m_scanRunning == running && m_scanCompleted == completed && m_scanTotal == total)
        return;
    m_scanRunning = running;
    m_scanCompleted = qBound(0, completed, qMax(0, total));
    m_scanTotal = qMax(0, total);
    emit scanProgressChanged();
}

void LibraryModel::finishScan()
{
    updateScanProgress(false, m_scanTotal, m_scanTotal);
    m_groupRefreshTimer.stop();
    rebuildRows();
    saveCache();
}

void LibraryModel::markUnavailable()
{
    bool changed = false;
    for (Track &track : m_tracks) {
        if (track.available) {
            track.available = false;
            changed = true;
        }
    }
    if (changed) {
        for (int row = 0; row < m_rows.size(); ++row) {
            if (m_rows[row].kind == 0) {
                m_rows[row].available = false;
                emit dataChanged(index(row, 0), index(row, 0), {IsAvailableRole, IsPlayableRole});
            }
        }
    }
}

void LibraryModel::markTrackUnavailable(const QString &stableId)
{
    const int trackIndex = m_trackIndices.value(stableId, -1);
    if (trackIndex < 0 || !m_tracks[trackIndex].available)
        return;
    m_tracks[trackIndex].available = false;
    updateTrackRow(stableId);
    for (int row = 0; row < m_rows.size(); ++row) {
        if (m_rows.at(row).stableId == stableId) {
            m_rows[row].available = false;
            emit dataChanged(index(row, 0), index(row, 0), {IsAvailableRole, IsPlayableRole});
            break;
        }
    }
}

LibraryModel::Track LibraryModel::trackForStableId(const QString &stableId) const
{
    const int index = m_trackIndices.value(stableId, -1);
    return index >= 0 ? m_tracks.at(index) : Track{};
}

QVector<LibraryModel::Track> LibraryModel::tracksForQueue(QueueScope scope,
                                                           const QString &context) const
{
    QVector<Track> result;
    for (const Track &track : m_tracks) {
        if (!track.available || !track.playable)
            continue;
        bool include = false;
        switch (scope) {
        case AllTracks: include = true; break;
        case ArtistTracks:
            include = track.artist == context
                || albumArtistFor(track) == context;
            break;
        case AlbumTracks: include = albumKeyFor(track) == context; break;
        case FolderTracks: {
            QString folder = context;
            if (!folder.isEmpty() && !folder.endsWith(u'/'))
                folder += u'/';
            include = !folder.isEmpty() && parentDirectory(track.path) == folder;
            break;
        }
        }
        if (include)
            result.push_back(track);
    }
    const auto naturalOrder = [](const Track &a, const Track &b) {
        if (a.discNumber != b.discNumber) {
            if (a.discNumber == 0) return false;
            if (b.discNumber == 0) return true;
            return a.discNumber < b.discNumber;
        }
        if (a.trackNumber != b.trackNumber) {
            if (a.trackNumber == 0) return false;
            if (b.trackNumber == 0) return true;
            return a.trackNumber < b.trackNumber;
        }
        return QString::compare(a.path, b.path, Qt::CaseInsensitive) < 0;
    };
    if (scope == AlbumTracks) {
        std::stable_sort(result.begin(), result.end(), naturalOrder);
    } else if (m_sortMode == SourceOrder) {
        std::stable_sort(result.begin(), result.end(), [](const Track &a, const Track &b) {
            return QString::compare(a.path, b.path, Qt::CaseInsensitive) < 0;
        });
    } else {
        std::stable_sort(result.begin(), result.end(), [this](const Track &a, const Track &b) {
            const int compared = m_collator.compare(a.title, b.title);
            if (compared != 0)
                return m_sortMode == NameDescending ? compared > 0 : compared < 0;
            return QString::compare(a.path, b.path, Qt::CaseInsensitive) < 0;
        });
    }
    return result;
}

QString LibraryModel::firstPlayableStableId(int scope, const QString &context) const
{
    if (scope < AllTracks || scope > FolderTracks)
        return {};
    const QVector<Track> tracks = tracksForQueue(static_cast<QueueScope>(scope), context);
    return tracks.isEmpty() ? QString() : tracks.first().stableId;
}

QString LibraryModel::fallbackTitle(const QString &path)
{
    QString name = path.mid(path.lastIndexOf(u'/') + 1);
    const qsizetype dot = name.lastIndexOf(u'.');
    if (dot > 0)
        name.truncate(dot);
    return name;
}

QString LibraryModel::albumKeyFor(const Track &track)
{
    if (track.album.trimmed().isEmpty())
        return {};
    const QString artist = albumArtistFor(track).normalized(QString::NormalizationForm_C).toCaseFolded();
    const QString album = track.album.normalized(QString::NormalizationForm_C).toCaseFolded();
    return QString::fromLatin1(QCryptographicHash::hash(
        (artist + QChar(u'\n') + album).toUtf8(), QCryptographicHash::Sha256).toHex());
}

QString LibraryModel::parentDirectory(const QString &path)
{
    const qsizetype slash = path.lastIndexOf(u'/');
    return slash < 0 ? QString() : path.left(slash + 1);
}

void LibraryModel::rebuildRows()
{
    QVector<Row> rows;
    if (m_mode == Tracks) {
        rows.reserve(m_tracks.size());
        for (int i = 0; i < m_tracks.size(); ++i) {
            const Track &track = m_tracks.at(i);
            if (!m_filterArtist.isEmpty() && track.artist != m_filterArtist
                && albumArtistFor(track) != m_filterArtist)
                continue;
            if (!m_filterAlbum.isEmpty() && albumKeyFor(track) != m_filterAlbum)
                continue;
            if (!m_filterFolder.isEmpty() && parentDirectory(track.path) != m_filterFolder)
                continue;
            if (!contains(track.title, m_searchText) && !contains(track.artist, m_searchText)
                && !contains(track.album, m_searchText))
                continue;
            Row row;
            row.trackIndex = i;
            row.stableId = track.stableId;
            row.label = track.title;
            row.artist = track.artist.trimmed().isEmpty() ? albumArtistFor(track) : track.artist;
            row.albumArtist = track.albumArtist;
            row.album = track.album;
            row.albumKey = albumKeyFor(track);
            row.filePath = track.path;
            row.parentPath = parentDirectory(track.path);
            row.playable = track.playable;
            row.available = track.available;
            row.discNumber = track.discNumber;
            row.trackNumber = track.trackNumber;
            row.kind = 0;
            rows.push_back(std::move(row));
        }
        std::stable_sort(rows.begin(), rows.end(), [this](const Row &a, const Row &b) {
            if (m_sortMode == SourceOrder)
                return a.trackIndex < b.trackIndex;
            const int compared = m_collator.compare(a.label, b.label);
            if (compared != 0)
                return m_sortMode == NameDescending ? compared > 0 : compared < 0;
            return a.trackIndex < b.trackIndex;
        });
    } else if (m_mode == Artists) {
        struct Group { QString artist; int count = 0; };
        QHash<QString, Group> groups;
        for (const Track &track : std::as_const(m_tracks)) {
            const QString trackArtist = track.artist.trimmed();
            const QString albumArtist = track.albumArtist.trimmed();
            const auto addArtist = [&groups, this](const QString &artist) {
                if (artist.isEmpty() || !LibraryModel::contains(artist, m_searchText))
                    return;
                auto &group = groups[artist];
                group.artist = artist;
                ++group.count;
            };
            addArtist(trackArtist);
            if (!albumArtist.isEmpty() && albumArtist != trackArtist)
                addArtist(albumArtist);
        }
        rows.reserve(groups.size());
        for (const Group &group : std::as_const(groups)) {
            Row row;
            row.stableId = groupId(QStringLiteral("artist:"), group.artist);
            row.label = group.artist;
            row.artist = group.artist;
            row.trackCount = group.count;
            row.kind = 1;
            rows.push_back(std::move(row));
        }
        std::stable_sort(rows.begin(), rows.end(), [this](const Row &a, const Row &b) {
            return m_collator.compare(a.label, b.label) < 0;
        });
    } else {
        struct Group {
            QString album;
            QString artist;
            QString key;
            QSet<QString> artists;
            int count = 0;
            bool matchesSearch = false;
        };
        QHash<QString, Group> groups;
        for (const Track &track : std::as_const(m_tracks)) {
            if (track.album.trimmed().isEmpty())
                continue;
            const QString key = albumKeyFor(track);
            auto &group = groups[key];
            group.album = track.album;
            group.artist = albumArtistFor(track);
            group.key = key;
            ++group.count;
            if (!track.artist.trimmed().isEmpty())
                group.artists.insert(track.artist.trimmed());
            if (!group.artist.isEmpty())
                group.artists.insert(group.artist);
            group.matchesSearch = group.matchesSearch
                || contains(track.album, m_searchText) || contains(track.artist, m_searchText)
                || contains(group.artist, m_searchText);
        }
        rows.reserve(groups.size());
        for (const Group &group : std::as_const(groups)) {
            if (!m_filterArtist.isEmpty() && !group.artists.contains(m_filterArtist))
                continue;
            if (!group.matchesSearch)
                continue;
            Row row;
            row.stableId = groupId(QStringLiteral("album:"), group.key);
            row.label = group.album;
            row.artist = group.artist;
            row.album = group.album;
            row.albumKey = group.key;
            row.trackCount = group.count;
            row.kind = 2;
            rows.push_back(std::move(row));
        }
        std::stable_sort(rows.begin(), rows.end(), [this](const Row &a, const Row &b) {
            const int album = m_collator.compare(a.label, b.label);
            return album == 0 ? m_collator.compare(a.artist, b.artist) < 0 : album < 0;
        });
    }
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}

void LibraryModel::updateTrackRow(const QString &stableId)
{
    for (int row = 0; row < m_rows.size(); ++row) {
        if (m_rows.at(row).stableId == stableId) {
            const int trackIndex = m_rows.at(row).trackIndex;
            if (trackIndex < 0)
                return;
            const Track &track = m_tracks.at(trackIndex);
            Row &item = m_rows[row];
            item.label = track.title;
            item.artist = track.artist.trimmed().isEmpty()
                ? albumArtistFor(track) : track.artist;
            item.albumArtist = track.albumArtist;
            item.album = track.album;
            item.albumKey = albumKeyFor(track);
            item.discNumber = track.discNumber;
            item.trackNumber = track.trackNumber;
            emit dataChanged(index(row, 0), index(row, 0),
                             {LabelRole, ArtistRole, AlbumArtistRole, AlbumRole,
                              AlbumKeyRole, DiscNumberRole, TrackNumberRole});
            return;
        }
    }
}

void LibraryModel::loadCache()
{
    const QString path = cacheFilePath();
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject() || doc.object().value(QStringLiteral("version")).toInt() != 1)
        return;
    for (const QJsonValue &value : doc.object().value(QStringLiteral("tracks")).toArray()) {
        if (!value.isObject())
            continue;
        const QJsonObject row = value.toObject();
        Track track;
        track.path = row.value(QStringLiteral("path")).toString();
        if (!TrackIdentity::isValidPath(track.path) || !isAudioPath(track.path))
            continue;
        track.stableId = TrackIdentity::stableIdForPath(track.path);
        track.title = row.value(QStringLiteral("title")).toString(fallbackTitle(track.path));
        track.artist = row.value(QStringLiteral("artist")).toString();
        track.albumArtist = row.value(QStringLiteral("albumArtist")).toString();
        track.album = row.value(QStringLiteral("album")).toString();
        track.size = row.value(QStringLiteral("size")).toVariant().toLongLong();
        track.etag = QByteArray::fromBase64(row.value(QStringLiteral("etag")).toString().toLatin1());
        track.discNumber = row.value(QStringLiteral("discNumber")).toInt();
        track.trackNumber = row.value(QStringLiteral("trackNumber")).toInt();
        track.metadataLoaded = row.value(QStringLiteral("metadataLoaded")).toBool();
        track.playable = canStreamPath(track.path) && track.size > 0;
        track.available = false;
        m_trackIndices.insert(track.stableId, m_tracks.size());
        m_tracks.push_back(std::move(track));
    }
}

void LibraryModel::saveCache() const
{
    const QString path = cacheFilePath();
    if (path.isEmpty())
        return;
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath()))
        return;
    QJsonArray tracks;
    for (const Track &track : m_tracks) {
        QJsonObject row;
        row.insert(QStringLiteral("path"), track.path);
        row.insert(QStringLiteral("title"), track.title);
        row.insert(QStringLiteral("artist"), track.artist);
        row.insert(QStringLiteral("albumArtist"), track.albumArtist);
        row.insert(QStringLiteral("album"), track.album);
        row.insert(QStringLiteral("size"), QString::number(track.size));
        row.insert(QStringLiteral("etag"), QString::fromLatin1(track.etag.toBase64()));
        row.insert(QStringLiteral("discNumber"), track.discNumber);
        row.insert(QStringLiteral("trackNumber"), track.trackNumber);
        row.insert(QStringLiteral("metadataLoaded"), track.metadataLoaded);
        tracks.append(row);
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("tracks"), tracks);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.commit();
}

QString LibraryModel::albumArtistFor(const Track &track)
{
    return track.albumArtist.trimmed().isEmpty() ? track.artist : track.albumArtist;
}

bool LibraryModel::contains(const QString &value, const QString &query)
{
    return query.isEmpty()
        || value.normalized(QString::NormalizationForm_C).contains(
            query.normalized(QString::NormalizationForm_C), Qt::CaseInsensitive);
}
