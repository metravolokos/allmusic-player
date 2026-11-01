#pragma once

#include "LibraryModel.h"

#include <QAbstractListModel>
#include <QHash>
#include <QVariantMap>

class PlaybackQueueModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY queueChanged)
    Q_PROPERTY(QString currentTrackId READ currentTrackId NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentTitle READ currentTitle NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentArtist READ currentArtist NOTIFY currentTrackChanged)
    Q_PROPERTY(QString contextText READ contextText NOTIFY queueChanged)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool canPrevious READ canPrevious NOTIFY controlsChanged)
    Q_PROPERTY(bool canNext READ canNext NOTIFY controlsChanged)
    Q_PROPERTY(int repeatMode READ repeatMode WRITE setRepeatMode NOTIFY repeatModeChanged)
    Q_PROPERTY(bool shuffleEnabled READ shuffleEnabled WRITE setShuffleEnabled NOTIFY shuffleChanged)
    Q_PROPERTY(quint64 generation READ generation NOTIFY generationChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    enum RepeatMode { RepeatOff = 0, RepeatAll = 1, RepeatOne = 2 };
    Q_ENUM(RepeatMode)
    enum Role { StableIdRole = Qt::UserRole + 1, LabelRole, ArtistRole, AlbumRole, IsCurrentRole };

    explicit PlaybackQueueModel(LibraryModel *library, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const { return m_tracks.size(); }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    QString currentTrackId() const;
    QString currentTitle() const;
    QString currentArtist() const;
    QString contextText() const { return m_contextText; }
    bool active() const { return m_active; }
    bool canPrevious() const;
    bool canNext() const;
    int repeatMode() const { return m_repeatMode; }
    bool shuffleEnabled() const { return m_shuffle; }
    quint64 generation() const { return m_generation; }
    QString error() const { return m_error; }

    void setRepeatMode(int mode);
    void setShuffleEnabled(bool enabled);

    Q_INVOKABLE bool startAll(const QString &stableId);
    Q_INVOKABLE bool startArtist(const QString &artist, const QString &stableId);
    Q_INVOKABLE bool startAlbum(const QString &albumKey, const QString &stableId);
    Q_INVOKABLE bool startFolder(const QString &folder, const QString &stableId);
    Q_INVOKABLE bool jumpTo(const QString &stableId);
    Q_INVOKABLE void play();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void advance(quint64 generation);
    Q_INVOKABLE void reportError(quint64 generation, const QString &message);

signals:
    void queueChanged();
    void currentTrackChanged();
    void activeChanged();
    void controlsChanged();
    void repeatModeChanged();
    void shuffleChanged();
    void generationChanged();
    void errorChanged();
    void trackRequested(const QVariantMap &track, quint64 generation);
    void stopRequested();

private:
    struct HistoryItem { int trackIndex = -1; int cycleId = 0; int cyclePosition = -1; };

    bool startScope(LibraryModel::QueueScope scope, const QString &context,
                    const QString &stableId, const QString &contextText);
    void requestCurrent();
    void setCurrentQueueIndex(int index, bool request);
    void advanceForward(bool automatic);
    void buildInitialCycle(int startIndex);
    void buildNextCycle();
    void rebuildUnplayedCycle(bool shuffle);
    void appendHistory(int trackIndex, int cyclePosition);
    QVariantMap trackMap(const LibraryModel::Track &track) const;
    void setActive(bool active);
    void setErrorText(const QString &message);

    LibraryModel *m_library = nullptr;
    QVector<LibraryModel::Track> m_tracks;
    QHash<QString, int> m_trackIndices;
    QVector<int> m_cycle;
    QVector<HistoryItem> m_history;
    int m_cyclePosition = -1;
    int m_offLimit = 0;
    int m_historyPosition = -1;
    int m_currentTrackIndex = -1;
    int m_cycleStartIndex = 0;
    int m_currentCycleId = 0;
    int m_repeatMode = RepeatOff;
    quint64 m_generation = 0;
    bool m_shuffle = false;
    bool m_active = false;
    QString m_contextText;
    QString m_error;
};
