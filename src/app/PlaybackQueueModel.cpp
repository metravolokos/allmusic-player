#include "PlaybackQueueModel.h"

#include <QRandomGenerator>
#include <QSet>

#include <algorithm>
#include <numeric>

namespace {
constexpr int MaximumHistoryEntries = 4096;
constexpr int HistoryTrimBatch = 1024;
}

PlaybackQueueModel::PlaybackQueueModel(LibraryModel *library, QObject *parent)
    : QAbstractListModel(parent), m_library(library)
{
    if (m_library) {
        connect(m_library, &LibraryModel::trackMetadataChanged, this,
                [this](const QString &stableId) {
            const int index = m_trackIndices.value(stableId, -1);
            if (index < 0 || index >= m_tracks.size())
                return;
            const LibraryModel::Track updated = m_library->trackForStableId(stableId);
            m_tracks[index].title = updated.title;
            m_tracks[index].artist = updated.artist;
            m_tracks[index].albumArtist = updated.albumArtist;
            m_tracks[index].album = updated.album;
            m_tracks[index].discNumber = updated.discNumber;
            m_tracks[index].trackNumber = updated.trackNumber;
            emit dataChanged(this->index(index, 0), this->index(index, 0),
                             {LabelRole, ArtistRole, AlbumRole});
            if (index == m_currentTrackIndex)
                emit currentTrackChanged();
        });
    }
}

int PlaybackQueueModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_tracks.size();
}

QVariant PlaybackQueueModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tracks.size())
        return {};
    const auto &track = m_tracks.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case LabelRole: return track.title;
    case StableIdRole: return track.stableId;
    case ArtistRole: return track.artist;
    case AlbumRole: return track.album;
    case IsCurrentRole: return index.row() == m_currentTrackIndex;
    default: return {};
    }
}

QHash<int, QByteArray> PlaybackQueueModel::roleNames() const
{
    return {{StableIdRole, "stableId"}, {LabelRole, "label"}, {ArtistRole, "artist"},
            {AlbumRole, "album"}, {IsCurrentRole, "isCurrent"}};
}

QString PlaybackQueueModel::currentTrackId() const
{
    return m_currentTrackIndex >= 0 && m_currentTrackIndex < m_tracks.size()
        ? m_tracks.at(m_currentTrackIndex).stableId : QString();
}

QString PlaybackQueueModel::currentTitle() const
{
    return m_currentTrackIndex >= 0 && m_currentTrackIndex < m_tracks.size()
        ? m_tracks.at(m_currentTrackIndex).title : QString();
}

QString PlaybackQueueModel::currentArtist() const
{
    if (m_currentTrackIndex < 0 || m_currentTrackIndex >= m_tracks.size())
        return {};
    const auto &track = m_tracks.at(m_currentTrackIndex);
    return track.artist.isEmpty() ? track.albumArtist : track.artist;
}

bool PlaybackQueueModel::canPrevious() const
{
    if (m_historyPosition > 0)
        return true;
    if (m_historyPosition != 0 || m_tracks.isEmpty())
        return false;
    if (m_shuffle)
        return m_repeatMode == RepeatAll && !m_cycle.isEmpty();
    return m_currentTrackIndex > 0 || m_repeatMode == RepeatAll;
}

bool PlaybackQueueModel::canNext() const
{
    if (m_historyPosition >= 0 && m_historyPosition + 1 < m_history.size())
        return true;
    if (m_cyclePosition >= 0 && m_cyclePosition + 1 < m_offLimit)
        return true;
    return m_repeatMode == RepeatAll && !m_tracks.isEmpty();
}

void PlaybackQueueModel::setRepeatMode(int mode)
{
    if (mode < RepeatOff || mode > RepeatOne || m_repeatMode == mode)
        return;
    m_repeatMode = mode;
    if (!m_shuffle && mode == RepeatAll && m_cycle.size() < m_tracks.size()) {
        for (int i = 0; i < m_tracks.size(); ++i) {
            if (!m_cycle.contains(i))
                m_cycle.push_back(i);
        }
        m_offLimit = m_cycle.size();
    } else if (!m_shuffle && mode == RepeatOff && m_cyclePosition >= 0) {
        const int first = m_cycle.isEmpty() ? 0 : m_cycle.first();
        m_offLimit = qMax(0, m_tracks.size() - first);
    }
    emit repeatModeChanged();
    emit controlsChanged();
}

void PlaybackQueueModel::setShuffleEnabled(bool enabled)
{
    if (m_shuffle == enabled)
        return;
    m_shuffle = enabled;
    if (!m_cycle.isEmpty() && m_cyclePosition >= 0)
        rebuildUnplayedCycle(enabled);
    emit shuffleChanged();
    emit controlsChanged();
}

bool PlaybackQueueModel::startAll(const QString &stableId)
{
    return startScope(LibraryModel::AllTracks, {}, stableId, QStringLiteral("Все композиции"));
}

bool PlaybackQueueModel::startArtist(const QString &artist, const QString &stableId)
{
    return startScope(LibraryModel::ArtistTracks, artist, stableId,
                      QStringLiteral("Исполнитель · %1").arg(artist));
}

bool PlaybackQueueModel::startAlbum(const QString &albumKey, const QString &stableId)
{
    const auto start = m_library ? m_library->trackForStableId(stableId) : LibraryModel::Track{};
    const QString title = start.album.isEmpty() ? QStringLiteral("Альбом") : start.album;
    return startScope(LibraryModel::AlbumTracks, albumKey, stableId,
                      QStringLiteral("Альбом · %1").arg(title));
}

bool PlaybackQueueModel::startFolder(const QString &folder, const QString &stableId)
{
    return startScope(LibraryModel::FolderTracks, folder, stableId,
                      QStringLiteral("Папка · %1").arg(folder));
}

bool PlaybackQueueModel::jumpTo(const QString &stableId)
{
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks.at(i).stableId != stableId)
            continue;
        if (m_cycle.isEmpty())
            return false;
        m_currentCycleId += 1;
        buildInitialCycle(i);
        m_history.clear();
        m_historyPosition = -1;
        appendHistory(i, 0);
        setCurrentQueueIndex(i, false);
        setActive(true);
        requestCurrent();
        emit controlsChanged();
        return true;
    }
    return false;
}

void PlaybackQueueModel::play()
{
    if (m_currentTrackIndex < 0 || m_currentTrackIndex >= m_tracks.size())
        return;
    setErrorText({});
    setActive(true);
    requestCurrent();
}

void PlaybackQueueModel::next()
{
    advanceForward(false);
}

void PlaybackQueueModel::previous()
{
    if (!canPrevious())
        return;
    if (m_historyPosition > 0) {
        --m_historyPosition;
        const HistoryItem item = m_history.at(m_historyPosition);
        setCurrentQueueIndex(item.trackIndex, false);
        if (item.cycleId == m_currentCycleId)
            m_cyclePosition = item.cyclePosition;
    } else if (!m_shuffle && m_currentTrackIndex > 0) {
        const int previousIndex = m_currentTrackIndex - 1;
        buildInitialCycle(previousIndex);
        m_history.clear();
        m_historyPosition = -1;
        setCurrentQueueIndex(previousIndex, false);
        appendHistory(previousIndex, m_cyclePosition);
    } else if (m_repeatMode == RepeatAll && !m_cycle.isEmpty()) {
        const int originalIndex = m_currentTrackIndex;
        const int originalCyclePosition = m_cyclePosition;
        const int previousCyclePosition = m_cyclePosition > 0
            ? m_cyclePosition - 1 : m_cycle.size() - 1;
        const int previousIndex = m_cycle.at(previousCyclePosition);
        m_cyclePosition = previousCyclePosition;
        m_history.clear();
        m_historyPosition = -1;
        appendHistory(previousIndex, previousCyclePosition);
        appendHistory(originalIndex, originalCyclePosition);
        m_historyPosition = 0;
        setCurrentQueueIndex(previousIndex, false);
    }
    setErrorText({});
    setActive(true);
    requestCurrent();
    emit controlsChanged();
}

void PlaybackQueueModel::stop()
{
    if (!m_active && m_tracks.isEmpty())
        return;
    ++m_generation;
    emit generationChanged();
    setActive(false);
    emit stopRequested();
    emit controlsChanged();
}

void PlaybackQueueModel::clear()
{
    const bool hadQueue = !m_tracks.isEmpty() || m_active;
    ++m_generation;
    emit generationChanged();
    setActive(false);
    beginResetModel();
    m_tracks.clear();
    m_trackIndices.clear();
    m_cycle.clear();
    m_history.clear();
    m_cyclePosition = -1;
    m_offLimit = 0;
    m_historyPosition = -1;
    m_currentTrackIndex = -1;
    m_currentCycleId = 0;
    m_contextText.clear();
    endResetModel();
    if (hadQueue)
        emit stopRequested();
    setErrorText({});
    emit queueChanged();
    emit currentTrackChanged();
    emit controlsChanged();
}

void PlaybackQueueModel::advance(quint64 generation)
{
    if (generation != m_generation || !m_active)
        return;
    advanceForward(true);
}

void PlaybackQueueModel::reportError(quint64 generation, const QString &message)
{
    if (generation != m_generation)
        return;
    setActive(false);
    emit stopRequested();
    setErrorText(message);
}

bool PlaybackQueueModel::startScope(LibraryModel::QueueScope scope, const QString &context,
                                    const QString &stableId, const QString &contextText)
{
    if (!m_library)
        return false;
    QVector<LibraryModel::Track> tracks = m_library->tracksForQueue(scope, context);
    if (tracks.isEmpty())
        return false;
    int startIndex = -1;
    for (int i = 0; i < tracks.size(); ++i) {
        if (tracks.at(i).stableId == stableId) {
            startIndex = i;
            break;
        }
    }
    if (startIndex < 0)
        return false;

    ++m_generation;
    emit generationChanged();
    beginResetModel();
    m_tracks = std::move(tracks);
    m_trackIndices.clear();
    m_trackIndices.reserve(m_tracks.size());
    for (int i = 0; i < m_tracks.size(); ++i)
        m_trackIndices.insert(m_tracks.at(i).stableId, i);
    m_contextText = contextText;
    endResetModel();
    m_history.clear();
    m_historyPosition = -1;
    m_currentTrackIndex = startIndex;
    buildInitialCycle(startIndex);
    appendHistory(startIndex, 0);
    setErrorText({});
    setActive(true);
    emit dataChanged(index(startIndex, 0), index(startIndex, 0), {IsCurrentRole});
    requestCurrent();
    emit queueChanged();
    emit currentTrackChanged();
    emit controlsChanged();
    return true;
}

void PlaybackQueueModel::requestCurrent()
{
    if (m_currentTrackIndex < 0 || m_currentTrackIndex >= m_tracks.size())
        return;
    ++m_generation;
    emit generationChanged();
    emit trackRequested(trackMap(m_tracks.at(m_currentTrackIndex)), m_generation);
}

void PlaybackQueueModel::setCurrentQueueIndex(int index, bool request)
{
    if (index < 0 || index >= m_tracks.size())
        return;
    const int old = m_currentTrackIndex;
    m_currentTrackIndex = index;
    if (old >= 0 && old < m_tracks.size())
        emit dataChanged(this->index(old, 0), this->index(old, 0), {IsCurrentRole});
    emit dataChanged(this->index(index, 0), this->index(index, 0), {IsCurrentRole});
    emit currentTrackChanged();
    if (request)
        requestCurrent();
}

void PlaybackQueueModel::advanceForward(bool automatic)
{
    if (m_tracks.isEmpty() || m_currentTrackIndex < 0)
        return;
    if (automatic && m_repeatMode == RepeatOne) {
        setErrorText({});
        setActive(true);
        requestCurrent();
        return;
    }
    if (m_historyPosition + 1 < m_history.size()) {
        ++m_historyPosition;
        const HistoryItem item = m_history.at(m_historyPosition);
        setCurrentQueueIndex(item.trackIndex, false);
        if (item.cycleId == m_currentCycleId)
            m_cyclePosition = item.cyclePosition;
        setErrorText({});
        setActive(true);
        requestCurrent();
        emit controlsChanged();
        return;
    }
    if (m_cyclePosition + 1 < m_offLimit) {
        ++m_cyclePosition;
    } else if (m_repeatMode == RepeatAll && m_cyclePosition + 1 < m_cycle.size()) {
        ++m_cyclePosition;
    } else if (m_repeatMode == RepeatAll) {
        buildNextCycle();
    } else {
        setActive(false);
        emit stopRequested();
        emit controlsChanged();
        return;
    }
    if (m_cyclePosition < 0 || m_cyclePosition >= m_cycle.size()) {
        setActive(false);
        return;
    }
    setCurrentQueueIndex(m_cycle.at(m_cyclePosition), false);
    m_history.resize(m_historyPosition + 1);
    appendHistory(m_currentTrackIndex, m_cyclePosition);
    setErrorText({});
    setActive(true);
    requestCurrent();
    emit controlsChanged();
}

void PlaybackQueueModel::buildInitialCycle(int startIndex)
{
    m_cycle.clear();
    m_cycleStartIndex = startIndex;
    if (m_shuffle) {
        for (int i = 0; i < m_tracks.size(); ++i)
            if (i != startIndex)
                m_cycle.push_back(i);
        for (int i = m_cycle.size() - 1; i > 0; --i)
            m_cycle.swapItemsAt(i, QRandomGenerator::global()->bounded(i + 1));
        m_cycle.prepend(startIndex);
        m_offLimit = m_tracks.size();
    } else {
        for (int i = startIndex; i < m_tracks.size(); ++i)
            m_cycle.push_back(i);
        if (m_repeatMode == RepeatAll) {
            for (int i = 0; i < startIndex; ++i)
                m_cycle.push_back(i);
            m_offLimit = m_cycle.size();
        } else {
            m_offLimit = m_cycle.size();
        }
    }
    m_cyclePosition = 0;
    m_currentCycleId = 1;
}

void PlaybackQueueModel::buildNextCycle()
{
    ++m_currentCycleId;
    const int lastTrack = m_cyclePosition >= 0 && m_cyclePosition < m_cycle.size()
        ? m_cycle.at(m_cyclePosition) : -1;
    m_cycle.clear();
    for (int i = m_cycleStartIndex; i < m_tracks.size(); ++i)
        m_cycle.push_back(i);
    for (int i = 0; i < m_cycleStartIndex; ++i)
        m_cycle.push_back(i);
    if (m_shuffle) {
        for (int i = m_cycle.size() - 1; i > 0; --i)
            m_cycle.swapItemsAt(i, QRandomGenerator::global()->bounded(i + 1));
        if (m_cycle.size() > 1 && m_cycle.first() == lastTrack)
            m_cycle.swapItemsAt(0, 1 + QRandomGenerator::global()->bounded(m_cycle.size() - 1));
    }
    m_offLimit = m_cycle.size();
    m_cyclePosition = 0;
}

void PlaybackQueueModel::rebuildUnplayedCycle(bool shuffle)
{
    QVector<int> prefix;
    QSet<int> alreadyPlayed;
    for (int i = 0; i <= m_cyclePosition && i < m_cycle.size(); ++i) {
        prefix.push_back(m_cycle.at(i));
        alreadyPlayed.insert(m_cycle.at(i));
    }
    QVector<int> remaining;
    for (int i = 0; i < m_tracks.size(); ++i)
        if (!alreadyPlayed.contains(i))
            remaining.push_back(i);
    if (shuffle) {
        for (int i = remaining.size() - 1; i > 0; --i)
            remaining.swapItemsAt(i, QRandomGenerator::global()->bounded(i + 1));
    }
    m_cycle = std::move(prefix);
    m_cycle += remaining;
    m_offLimit = m_cycle.size();
}

void PlaybackQueueModel::appendHistory(int trackIndex, int cyclePosition)
{
    m_history.push_back({trackIndex, m_currentCycleId, cyclePosition});
    m_historyPosition = m_history.size() - 1;
    if (m_history.size() > MaximumHistoryEntries) {
        m_history.remove(0, HistoryTrimBatch);
        m_historyPosition -= HistoryTrimBatch;
    }
}

QVariantMap PlaybackQueueModel::trackMap(const LibraryModel::Track &track) const
{
    const QString artist = track.artist.isEmpty() ? track.albumArtist : track.artist;
    return {{QStringLiteral("stableId"), track.stableId},
            {QStringLiteral("path"), track.path},
            {QStringLiteral("title"), track.title},
            {QStringLiteral("artist"), artist},
            {QStringLiteral("albumArtist"), track.albumArtist},
            {QStringLiteral("album"), track.album},
            {QStringLiteral("size"), track.size},
            {QStringLiteral("discNumber"), track.discNumber},
            {QStringLiteral("trackNumber"), track.trackNumber}};
}

void PlaybackQueueModel::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    emit activeChanged();
}

void PlaybackQueueModel::setErrorText(const QString &message)
{
    if (m_error == message)
        return;
    m_error = message;
    emit errorChanged();
}
