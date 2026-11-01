#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QCollator>
#include <QHash>
#include <QTimer>
#include <QVector>

class LibraryModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY viewChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY viewChanged)
    Q_PROPERTY(int sortMode READ sortMode WRITE setSortMode NOTIFY viewChanged)
    Q_PROPERTY(QString filterArtist READ filterArtist WRITE setFilterArtist NOTIFY viewChanged)
    Q_PROPERTY(QString filterAlbum READ filterAlbum WRITE setFilterAlbum NOTIFY viewChanged)
    Q_PROPERTY(QString filterFolder READ filterFolder WRITE setFilterFolder NOTIFY viewChanged)
    Q_PROPERTY(int trackCount READ trackCount NOTIFY trackCountChanged)
    Q_PROPERTY(bool scanRunning READ scanRunning NOTIFY scanProgressChanged)
    Q_PROPERTY(int scanCompleted READ scanCompleted NOTIFY scanProgressChanged)
    Q_PROPERTY(int scanTotal READ scanTotal NOTIFY scanProgressChanged)

public:
    enum Mode { Tracks = 0, Artists = 1, Albums = 2 };
    Q_ENUM(Mode)
    enum SortMode { NameAscending = 0, NameDescending = 1, SourceOrder = 2 };
    Q_ENUM(SortMode)
    enum QueueScope { AllTracks = 0, ArtistTracks = 1, AlbumTracks = 2, FolderTracks = 3 };
    Q_ENUM(QueueScope)

    enum Role {
        StableIdRole = Qt::UserRole + 1,
        LabelRole,
        ArtistRole,
        AlbumArtistRole,
        AlbumRole,
        AlbumKeyRole,
        FilePathRole,
        ParentPathRole,
        SizeRole,
        IsDirectoryRole,
        IsAudioRole,
        IsPlayableRole,
        IsAvailableRole,
        TrackCountRole,
        DiscNumberRole,
        TrackNumberRole,
        KindRole
    };

    struct Track {
        QString stableId;
        QString path;
        QString title;
        QString artist;
        QString albumArtist;
        QString album;
        QByteArray etag;
        qint64 size = 0;
        int discNumber = 0;
        int trackNumber = 0;
        bool metadataLoaded = false;
        bool available = false;
        bool playable = false;
    };

    explicit LibraryModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int mode() const { return m_mode; }
    QString searchText() const { return m_searchText; }
    int sortMode() const { return m_sortMode; }
    QString filterArtist() const { return m_filterArtist; }
    QString filterAlbum() const { return m_filterAlbum; }
    QString filterFolder() const { return m_filterFolder; }
    int trackCount() const { return m_tracks.size(); }
    bool scanRunning() const { return m_scanRunning; }
    int scanCompleted() const { return m_scanCompleted; }
    int scanTotal() const { return m_scanTotal; }

    void setMode(int mode);
    void setSearchText(const QString &text);
    void setSortMode(int mode);
    void setFilterArtist(const QString &artist);
    void setFilterAlbum(const QString &albumKey);
    void setFilterFolder(const QString &folder);
    void setRemoteTracks(const QVector<Track> &tracks);
    void applyTags(const QString &stableId, const QString &title, const QString &artist,
                   const QString &albumArtist, const QString &album,
                   int discNumber, int trackNumber);
    bool needsTags(const QString &stableId) const;
    void updateScanProgress(bool running, int completed, int total);
    void finishScan();
    void markUnavailable();
    void markTrackUnavailable(const QString &stableId);
    Track trackForStableId(const QString &stableId) const;
    QVector<Track> tracksForQueue(QueueScope scope, const QString &context) const;
    Q_INVOKABLE QString firstPlayableStableId(int scope, const QString &context) const;
    static QString fallbackTitle(const QString &path);
    static QString albumKeyFor(const Track &track);
    static QString parentDirectory(const QString &path);

signals:
    void viewChanged();
    void trackCountChanged();
    void scanProgressChanged();
    void trackMetadataChanged(const QString &stableId);

private:
    struct Row {
        int trackIndex = -1;
        QString stableId;
        QString label;
        QString artist;
        QString albumArtist;
        QString album;
        QString albumKey;
        QString filePath;
        QString parentPath;
        int trackCount = 0;
        int discNumber = 0;
        int trackNumber = 0;
        bool playable = false;
        bool available = false;
        int kind = 0;
    };

    void rebuildRows();
    void updateTrackRow(const QString &stableId);
    void loadCache();
    void saveCache() const;
    static QString albumArtistFor(const Track &track);
    static bool contains(const QString &value, const QString &query);

    QVector<Track> m_tracks;
    QVector<Row> m_rows;
    QHash<QString, int> m_trackIndices;
    int m_mode = Tracks;
    int m_sortMode = NameAscending;
    QString m_searchText;
    QString m_filterArtist;
    QString m_filterAlbum;
    QString m_filterFolder;
    bool m_scanRunning = false;
    int m_scanCompleted = 0;
    int m_scanTotal = 0;
    QCollator m_collator;
    QTimer m_groupRefreshTimer;
};
