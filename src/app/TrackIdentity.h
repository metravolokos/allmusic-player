#pragma once

#include <QCryptographicHash>
#include <QString>
#include <QStringList>

#include <algorithm>

namespace TrackIdentity {

inline QString stableIdForPath(const QString &path)
{
    const QByteArray digest = QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha256);
    return QStringLiteral("track-v1:") + QString::fromLatin1(digest.toHex());
}

inline bool isValidPath(const QString &path)
{
    if (!path.startsWith(QStringLiteral("/AllMusic/")) || path.endsWith(u'/')
        || path.contains(u'\\') || path.contains(QStringLiteral("//"))
        || path.contains(QChar::Null) || path.size() > 4096) {
        return false;
    }

    const QStringList segments = path.split(u'/');
    return std::none_of(segments.cbegin(), segments.cend(), [](const QString &segment) {
        return segment == QStringLiteral(".") || segment == QStringLiteral("..");
    });
}

inline bool isValidStableId(const QString &stableId)
{
    constexpr qsizetype prefixLength = sizeof("track-v1:") - 1;
    if (stableId.size() != prefixLength + 64
        || !stableId.startsWith(QStringLiteral("track-v1:"))) {
        return false;
    }
    return std::all_of(stableId.cbegin() + prefixLength, stableId.cend(), [](QChar character) {
        return (character >= u'0' && character <= u'9')
            || (character >= u'a' && character <= u'f');
    });
}

} // namespace TrackIdentity
