#include "AndroidCloudTrackListModel.h"

#include <QModelIndex>

#include <utility>

AndroidCloudTrackSourceModel::AndroidCloudTrackSourceModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AndroidCloudTrackSourceModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant AndroidCloudTrackSourceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};

    const QVariantMap &row = m_rows.at(static_cast<size_t>(index.row()));
    switch (role) {
    case Qt::DisplayRole:
    case LabelRole:
        return row.value(QStringLiteral("label"));
    case IsDirectoryRole:
        return row.value(QStringLiteral("isDirectory"));
    case IsAudioRole:
        return row.value(QStringLiteral("isAudio"));
    case SizeRole:
        return row.value(QStringLiteral("size"));
    case SourceIndexRole:
        return row.value(QStringLiteral("sourceIndex"));
    case NormalizedLabelRole:
        return row.value(QStringLiteral("normalizedLabel"));
    default:
        return {};
    }
}

QHash<int, QByteArray> AndroidCloudTrackSourceModel::roleNames() const
{
    return {
        {LabelRole, "label"},
        {IsDirectoryRole, "isDirectory"},
        {IsAudioRole, "isAudio"},
        {SizeRole, "size"},
        {SourceIndexRole, "sourceIndex"},
    };
}

void AndroidCloudTrackSourceModel::setEntries(const QVariantList &entries)
{
    QVector<QVariantMap> nextRows;
    nextRows.reserve(entries.size());
    for (qsizetype index = 0; index < entries.size(); ++index) {
        const QVariantMap source = entries.at(index).toMap();
        QVariantMap row;
        const QString label = source.value(QStringLiteral("label")).toString();
        row.insert(QStringLiteral("label"), label);
        row.insert(QStringLiteral("normalizedLabel"),
                   label.normalized(QString::NormalizationForm_C));
        row.insert(QStringLiteral("isDirectory"), source.value(QStringLiteral("isDirectory")));
        row.insert(QStringLiteral("isAudio"), source.value(QStringLiteral("isAudio")));
        row.insert(QStringLiteral("size"), source.value(QStringLiteral("size")));
        row.insert(QStringLiteral("sourceIndex"), static_cast<int>(index));
        nextRows.push_back(std::move(row));
    }

    beginResetModel();
    m_rows = std::move(nextRows);
    endResetModel();
}

AndroidCloudTrackListModel::AndroidCloudTrackListModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    m_collator.setCaseSensitivity(Qt::CaseInsensitive);
    m_collator.setNumericMode(true);
    setDynamicSortFilter(true);
    setSourceModel(new AndroidCloudTrackSourceModel(this));
    sort(0, Qt::AscendingOrder);
}

int AndroidCloudTrackListModel::totalCount() const
{
    return sourceEntriesModel() ? sourceEntriesModel()->rowCount() : 0;
}

void AndroidCloudTrackListModel::setEntries(const QVariantList &entries)
{
    if (auto *source = sourceEntriesModel()) {
        source->setEntries(entries);
        emit totalCountChanged();
        emit resultCountChanged();
    }
}

void AndroidCloudTrackListModel::setSearchText(const QString &text)
{
    const QString normalizedText = text.normalized(QString::NormalizationForm_C);
    if (m_searchText == normalizedText)
        return;

    m_searchText = normalizedText;
    invalidateFilter();
    emit searchTextChanged();
    emit resultCountChanged();
}

void AndroidCloudTrackListModel::setSortMode(int mode)
{
    if (mode < NameAscending || mode > SourceOrder || m_sortMode == mode)
        return;

    m_sortMode = mode;
    if (m_sortMode == SourceOrder) {
        sort(-1);
    } else {
        sort(0, m_sortMode == NameDescending ? Qt::DescendingOrder : Qt::AscendingOrder);
    }
    emit sortModeChanged();
}

bool AndroidCloudTrackListModel::containsSourceIndex(int sourceIndex) const
{
    const auto *source = sourceEntriesModel();
    if (!source || sourceIndex < 0 || sourceIndex >= source->rowCount())
        return false;
    return mapFromSource(source->index(sourceIndex, 0)).isValid();
}

bool AndroidCloudTrackListModel::filterAcceptsRow(
    int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_searchText.isEmpty())
        return true;

    const QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    const QString normalizedLabel = sourceIndex
        .data(AndroidCloudTrackSourceModel::NormalizedLabelRole).toString();
    return normalizedLabel.contains(m_searchText, Qt::CaseInsensitive);
}

bool AndroidCloudTrackListModel::lessThan(const QModelIndex &left,
                                          const QModelIndex &right) const
{
    const QString leftLabel = left.data(AndroidCloudTrackSourceModel::LabelRole).toString();
    const QString rightLabel = right.data(AndroidCloudTrackSourceModel::LabelRole).toString();
    const int comparison = m_collator.compare(leftLabel, rightLabel);
    if (comparison != 0)
        return comparison < 0;

    return left.data(AndroidCloudTrackSourceModel::SourceIndexRole).toInt()
        < right.data(AndroidCloudTrackSourceModel::SourceIndexRole).toInt();
}

AndroidCloudTrackSourceModel *AndroidCloudTrackListModel::sourceEntriesModel() const
{
    return qobject_cast<AndroidCloudTrackSourceModel *>(sourceModel());
}
