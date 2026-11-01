#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QStringList>
#include <QVariantList>
#include <QVector>

#include <memory>

class QSettings;

class AndroidFavoritesModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        StableIdRole = Qt::UserRole + 1,
        LabelRole,
        SourceIndexRole,
        IsFavoriteRole,
        IsAudioRole,
        IsDirectoryRole,
        IsAvailableRole,
        IsMissingRole
    };

    explicit AndroidFavoritesModel(QObject *parent = nullptr);
    explicit AndroidFavoritesModel(const QString &settingsFile, QObject *parent = nullptr);
    ~AndroidFavoritesModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const { return rowCount(); }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QStringList favoriteIds() const;
    int sourceIndexForStableId(const QString &stableId) const;
    QString sourcePathForStableId(const QString &stableId) const;

    Q_INVOKABLE bool contains(const QString &stableId) const;
    Q_INVOKABLE bool isMissing(const QString &stableId) const;
    Q_INVOKABLE bool toggleFavorite(const QString &stableId, const QString &label,
                                    const QString &sourcePath);
    Q_INVOKABLE bool removeFavorite(const QString &stableId);

    void setCurrentEntries(const QVariantList &entries, const QString &currentDirectory,
                           bool listingReady);

signals:
    void countChanged();
    void favoritesChanged();

private:
    struct Favorite {
        QString stableId;
        QString label;
        QString sourcePath;
    };

    static QString parentDirectoryFor(const QString &sourcePath);
    static QString normalizedDirectory(QString directory);

    void load();
    bool persist(const QVector<Favorite> &favorites);
    int rowForStableId(const QString &stableId) const;

    std::unique_ptr<QSettings> m_settings;
    QVector<Favorite> m_favorites;
    QHash<QString, int> m_currentSourceIndices;
    QString m_currentDirectory;
    bool m_listingReady = false;
};
