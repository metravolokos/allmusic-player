#pragma once

#include <QAbstractListModel>
#include <QCollator>
#include <QHash>
#include <QLocale>
#include <QSortFilterProxyModel>
#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class AndroidCloudTrackSourceModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        LabelRole = Qt::UserRole + 1,
        IsDirectoryRole,
        IsAudioRole,
        SizeRole,
        SourceIndexRole,
        NormalizedLabelRole,
        StableIdRole,
        IsFavoriteRole,
        IsAvailableRole,
        IsMissingRole
    };

    explicit AndroidCloudTrackSourceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setEntries(const QVariantList &entries);
    void setFavoriteIds(const QSet<QString> &favoriteIds);
    int sourceIndexForStableId(const QString &stableId) const;

private:
    QVector<QVariantMap> m_rows;
    QSet<QString> m_favoriteIds;
    QHash<QString, int> m_sourceIndicesByStableId;
};

class AndroidCloudTrackListModel final : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(int sortMode READ sortMode WRITE setSortMode NOTIFY sortModeChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY totalCountChanged)
    Q_PROPERTY(int resultCount READ resultCount NOTIFY resultCountChanged)

public:
    enum SortMode {
        NameAscending = 0,
        NameDescending = 1,
        SourceOrder = 2
    };
    Q_ENUM(SortMode)

    explicit AndroidCloudTrackListModel(QObject *parent = nullptr);

    QString searchText() const { return m_searchText; }
    int sortMode() const { return m_sortMode; }
    int totalCount() const;
    int resultCount() const { return rowCount(); }

    void setEntries(const QVariantList &entries);
    void setFavoriteIds(const QStringList &favoriteIds);
    void setSearchText(const QString &text);
    void setSortMode(int mode);

    Q_INVOKABLE bool containsSourceIndex(int sourceIndex) const;
    Q_INVOKABLE int sourceIndexForStableId(const QString &stableId) const;

signals:
    void searchTextChanged();
    void sortModeChanged();
    void totalCountChanged();
    void resultCountChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    AndroidCloudTrackSourceModel *sourceEntriesModel() const;

    QString m_searchText;
    int m_sortMode = NameAscending;
    QCollator m_collator{QLocale(QLocale::Russian, QLocale::Russia)};
};
