#include "AndroidFavoritesModel.h"
#include "TrackIdentity.h"

#include <QSettings>
#include <QSet>
#include <QVariantMap>

#include <algorithm>
#include <utility>

AndroidFavoritesModel::AndroidFavoritesModel(QObject *parent)
    : QAbstractListModel(parent),
      m_settings(std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope,
                                             QStringLiteral("MetraVolokos"),
                                             QStringLiteral("AllMusicPlayer")))
{
    m_settings->setFallbacksEnabled(false);
    load();
}

AndroidFavoritesModel::AndroidFavoritesModel(const QString &settingsFile, QObject *parent)
    : QAbstractListModel(parent),
      m_settings(std::make_unique<QSettings>(settingsFile, QSettings::IniFormat))
{
    m_settings->setFallbacksEnabled(false);
    load();
}

AndroidFavoritesModel::~AndroidFavoritesModel() = default;

int AndroidFavoritesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_favorites.size());
}

QVariant AndroidFavoritesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};

    const Favorite &favorite = m_favorites.at(static_cast<size_t>(index.row()));
    const int sourceIndex = sourceIndexForStableId(favorite.stableId);
    switch (role) {
    case Qt::DisplayRole:
    case LabelRole:
        return favorite.label;
    case StableIdRole:
        return favorite.stableId;
    case SourceIndexRole:
        return sourceIndex;
    case IsFavoriteRole:
        return true;
    case IsAudioRole:
        return true;
    case IsDirectoryRole:
        return false;
    case IsAvailableRole:
        return sourceIndex >= 0;
    case IsMissingRole:
        return m_listingReady && parentDirectoryFor(favorite.sourcePath) == m_currentDirectory
            && sourceIndex < 0;
    default:
        return {};
    }
}

QHash<int, QByteArray> AndroidFavoritesModel::roleNames() const
{
    return {
        {StableIdRole, "stableId"},
        {LabelRole, "label"},
        {SourceIndexRole, "sourceIndex"},
        {IsFavoriteRole, "isFavorite"},
        {IsAudioRole, "isAudio"},
        {IsDirectoryRole, "isDirectory"},
        {IsAvailableRole, "isAvailable"},
        {IsMissingRole, "isMissing"},
    };
}

QStringList AndroidFavoritesModel::favoriteIds() const
{
    QStringList ids;
    ids.reserve(m_favorites.size());
    for (const Favorite &favorite : m_favorites)
        ids.push_back(favorite.stableId);
    return ids;
}

int AndroidFavoritesModel::sourceIndexForStableId(const QString &stableId) const
{
    return m_currentSourceIndices.value(stableId, -1);
}

QString AndroidFavoritesModel::sourcePathForStableId(const QString &stableId) const
{
    const int row = rowForStableId(stableId);
    return row >= 0 ? m_favorites.at(static_cast<size_t>(row)).sourcePath : QString();
}

bool AndroidFavoritesModel::contains(const QString &stableId) const
{
    return rowForStableId(stableId) >= 0;
}

bool AndroidFavoritesModel::isMissing(const QString &stableId) const
{
    const int row = rowForStableId(stableId);
    return row >= 0 && m_listingReady
        && parentDirectoryFor(m_favorites.at(static_cast<size_t>(row)).sourcePath)
            == m_currentDirectory
        && sourceIndexForStableId(stableId) < 0;
}

bool AndroidFavoritesModel::toggleFavorite(const QString &stableId, const QString &label,
                                           const QString &sourcePath)
{
    if (rowForStableId(stableId) >= 0)
        return removeFavorite(stableId);
    if (!TrackIdentity::isValidStableId(stableId) || !TrackIdentity::isValidPath(sourcePath)
        || TrackIdentity::stableIdForPath(sourcePath) != stableId) {
        return false;
    }

    QVector<Favorite> next = m_favorites;
    next.push_back({stableId, label.isEmpty() ? sourcePath.section(u'/', -1) : label,
                    sourcePath});
    if (!persist(next))
        return false;

    const int row = rowCount();
    beginInsertRows(QModelIndex(), row, row);
    m_favorites = std::move(next);
    endInsertRows();
    emit countChanged();
    emit favoritesChanged();
    return true;
}

bool AndroidFavoritesModel::removeFavorite(const QString &stableId)
{
    const int row = rowForStableId(stableId);
    if (row < 0)
        return false;

    QVector<Favorite> next = m_favorites;
    next.removeAt(row);
    if (!persist(next))
        return false;

    beginRemoveRows(QModelIndex(), row, row);
    m_favorites = std::move(next);
    endRemoveRows();
    emit countChanged();
    emit favoritesChanged();
    return true;
}

void AndroidFavoritesModel::setCurrentEntries(const QVariantList &entries,
                                              const QString &currentDirectory,
                                              bool listingReady)
{
    QHash<QString, int> nextIndices;
    QHash<QString, QString> nextLabels;
    for (qsizetype index = 0; index < entries.size(); ++index) {
        const QVariantMap entry = entries.at(index).toMap();
        const QString stableId = entry.value(QStringLiteral("stableId")).toString();
        if (!entry.value(QStringLiteral("isAudio")).toBool()
            || !TrackIdentity::isValidStableId(stableId)) {
            continue;
        }
        nextIndices.insert(stableId, static_cast<int>(index));
        nextLabels.insert(stableId, entry.value(QStringLiteral("label")).toString());
    }

    QVector<Favorite> updated = m_favorites;
    bool labelsChanged = false;
    for (Favorite &favorite : updated) {
        const auto label = nextLabels.constFind(favorite.stableId);
        if (label != nextLabels.cend() && !label.value().isEmpty()
            && favorite.label != label.value()) {
            favorite.label = label.value();
            labelsChanged = true;
        }
    }
    if (labelsChanged && persist(updated))
        m_favorites = std::move(updated);

    m_currentSourceIndices = std::move(nextIndices);
    m_currentDirectory = normalizedDirectory(currentDirectory);
    m_listingReady = listingReady;

    if (rowCount() > 0) {
        emit dataChanged(index(0, 0), index(rowCount() - 1, 0),
                         {LabelRole, SourceIndexRole, IsAvailableRole, IsMissingRole});
    }
}

QString AndroidFavoritesModel::parentDirectoryFor(const QString &sourcePath)
{
    const qsizetype slash = sourcePath.lastIndexOf(u'/');
    return slash >= 0 ? sourcePath.left(slash + 1) : QString();
}

QString AndroidFavoritesModel::normalizedDirectory(QString directory)
{
    if (!directory.startsWith(QStringLiteral("/AllMusic/")))
        return {};
    while (directory.size() > 1 && directory.endsWith(u'/'))
        directory.chop(1);
    directory.append(u'/');
    return directory;
}

void AndroidFavoritesModel::load()
{
    m_settings->beginGroup(QStringLiteral("favorites"));
    const QVariantList stored = m_settings->value(QStringLiteral("items")).toList();
    m_settings->endGroup();

    QSet<QString> seen;
    for (const QVariant &value : stored) {
        const QVariantMap map = value.toMap();
        const QString stableId = map.value(QStringLiteral("stableId")).toString();
        const QString sourcePath = map.value(QStringLiteral("sourcePath")).toString();
        if (!TrackIdentity::isValidStableId(stableId) || !TrackIdentity::isValidPath(sourcePath)
            || TrackIdentity::stableIdForPath(sourcePath) != stableId || seen.contains(stableId)) {
            continue;
        }
        seen.insert(stableId);
        const QString label = map.value(QStringLiteral("label")).toString();
        m_favorites.push_back({stableId,
                               label.isEmpty() ? sourcePath.section(u'/', -1) : label,
                               sourcePath});
    }
}

bool AndroidFavoritesModel::persist(const QVector<Favorite> &favorites)
{
    QVariantList stored;
    stored.reserve(favorites.size());
    for (const Favorite &favorite : favorites) {
        stored.push_back(QVariantMap{
            {QStringLiteral("stableId"), favorite.stableId},
            {QStringLiteral("label"), favorite.label},
            {QStringLiteral("sourcePath"), favorite.sourcePath},
        });
    }

    m_settings->beginGroup(QStringLiteral("favorites"));
    m_settings->setValue(QStringLiteral("items"), stored);
    m_settings->endGroup();
    m_settings->sync();
    return m_settings->status() == QSettings::NoError;
}

int AndroidFavoritesModel::rowForStableId(const QString &stableId) const
{
    for (int row = 0; row < m_favorites.size(); ++row) {
        if (m_favorites.at(row).stableId == stableId)
            return row;
    }
    return -1;
}
