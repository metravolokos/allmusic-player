#include "AudioMetadataReader.h"

#include <QtEndian>

#include <utility>

namespace {
QString cleanText(QString value)
{
    value.remove(QChar(u'\0'));
    return value.trimmed();
}

quint32 synchsafe(const uchar *bytes)
{
    return (quint32(bytes[0] & 0x7f) << 21)
        | (quint32(bytes[1] & 0x7f) << 14)
        | (quint32(bytes[2] & 0x7f) << 7)
        | quint32(bytes[3] & 0x7f);
}

QByteArray deUnsynchronize(QByteArray bytes)
{
    QByteArray decoded;
    decoded.reserve(bytes.size());
    for (qsizetype i = 0; i < bytes.size(); ++i) {
        decoded.append(bytes.at(i));
        if (static_cast<uchar>(bytes.at(i)) == 0xff && i + 1 < bytes.size()
            && static_cast<uchar>(bytes.at(i + 1)) == 0x00) {
            ++i;
        }
    }
    return decoded;
}

QString decodeId3Text(const QByteArray &payload)
{
    if (payload.isEmpty())
        return {};
    const uchar encoding = static_cast<uchar>(payload.at(0));
    const QByteArray bytes = payload.mid(1);
    QString result;
    if (encoding == 0) {
        result = QString::fromLatin1(bytes);
    } else if (encoding == 3) {
        result = QString::fromUtf8(bytes);
    } else if (encoding == 1) {
        if (bytes.size() >= 2 && (bytes.startsWith("\xff\xfe") || bytes.startsWith("\xfe\xff"))) {
            const bool bigEndian = bytes.startsWith("\xfe\xff");
            for (qsizetype i = 2; i + 1 < bytes.size(); i += 2) {
                const ushort code = bigEndian
                    ? (ushort(static_cast<uchar>(bytes.at(i))) << 8)
                        | ushort(static_cast<uchar>(bytes.at(i + 1)))
                    : ushort(static_cast<uchar>(bytes.at(i)))
                        | (ushort(static_cast<uchar>(bytes.at(i + 1))) << 8);
                result.append(QChar(code));
            }
        } else {
            result = QString::fromUtf16(reinterpret_cast<const char16_t *>(bytes.constData()),
                                        bytes.size() / 2);
        }
    } else if (encoding == 2) {
        result.reserve(bytes.size() / 2);
        for (qsizetype i = 0; i + 1 < bytes.size(); i += 2) {
            const ushort code = (ushort(static_cast<uchar>(bytes.at(i))) << 8)
                | ushort(static_cast<uchar>(bytes.at(i + 1)));
            result.append(QChar(code));
        }
    }
    const qsizetype separator = result.indexOf(QChar(u'\0'));
    if (separator >= 0)
        result.truncate(separator);
    return cleanText(result);
}

int leadingNumber(const QString &value)
{
    qsizetype length = 0;
    while (length < value.size() && value.at(length).isDigit())
        ++length;
    bool ok = false;
    const int number = value.left(length).toInt(&ok);
    return ok ? number : 0;
}

void setId3Frame(AudioTags &tags, const QByteArray &id, const QByteArray &payload)
{
    const QString value = decodeId3Text(payload);
    if (value.isEmpty())
        return;
    if (id == "TIT2" || id == "TT2") tags.title = value;
    else if (id == "TPE1" || id == "TP1") tags.artist = value;
    else if (id == "TPE2" || id == "TP2") tags.albumArtist = value;
    else if (id == "TALB" || id == "TAL") tags.album = value;
    else if (id == "TRCK" || id == "TRK") tags.trackNumber = leadingNumber(value);
    else if (id == "TPOS" || id == "TPA") tags.discNumber = leadingNumber(value);
}

void setVorbisComment(AudioTags &tags, const QByteArray &comment)
{
    const qsizetype equals = comment.indexOf('=');
    if (equals <= 0)
        return;
    const QByteArray key = comment.left(equals).toUpper();
    const QString value = cleanText(QString::fromUtf8(comment.mid(equals + 1)));
    if (value.isEmpty())
        return;
    if (key == "TITLE" && tags.title.isEmpty()) tags.title = value;
    else if (key == "ARTIST" && tags.artist.isEmpty()) tags.artist = value;
    else if ((key == "ALBUMARTIST" || key == "ALBUM ARTIST") && tags.albumArtist.isEmpty())
        tags.albumArtist = value;
    else if (key == "ALBUM" && tags.album.isEmpty()) tags.album = value;
    else if (key == "TRACKNUMBER" && tags.trackNumber == 0)
        tags.trackNumber = leadingNumber(value);
    else if (key == "DISCNUMBER" && tags.discNumber == 0)
        tags.discNumber = leadingNumber(value);
}
}

AudioTags AudioMetadataReader::read(const QString &path, const QByteArray &rangeBytes)
{
    const QString lower = path.toLower();
    if (lower.endsWith(QStringLiteral(".mp3")))
        return readMp3(rangeBytes);
    if (lower.endsWith(QStringLiteral(".flac")))
        return readFlac(rangeBytes);
    return {};
}

AudioTags AudioMetadataReader::readMp3(const QByteArray &source)
{
    AudioTags tags;
    if (source.size() < 10 || !source.startsWith("ID3"))
        return tags;
    const uchar *header = reinterpret_cast<const uchar *>(source.constData());
    const int version = header[3];
    if (version < 2 || version > 4 || header[4] == 0xff)
        return tags;
    const quint32 declaredSize = synchsafe(header + 6);
    const qsizetype available = qMin<qsizetype>(source.size() - 10, declaredSize);
    if (available <= 0)
        return tags;
    QByteArray frames = source.mid(10, available);
    if (header[5] & 0x80)
        frames = deUnsynchronize(std::move(frames));
    qsizetype offset = 0;
    if (header[5] & 0x40) {
        if (version == 3 && frames.size() >= 4)
            offset = qMin<qsizetype>(frames.size(), 4 + qFromBigEndian<quint32>(
                reinterpret_cast<const uchar *>(frames.constData())));
        else if (version == 4 && frames.size() >= 4)
            offset = qMin<qsizetype>(frames.size(), synchsafe(
                reinterpret_cast<const uchar *>(frames.constData())));
    }

    while (offset < frames.size()) {
        const qsizetype frameHeader = version == 2 ? 6 : 10;
        if (frames.size() - offset < frameHeader)
            break;
        const QByteArray id = frames.mid(offset, version == 2 ? 3 : 4);
        if (id.trimmed().isEmpty())
            break;
        const uchar *sizeBytes = reinterpret_cast<const uchar *>(frames.constData() + offset
                                                                  + (version == 2 ? 3 : 4));
        quint32 frameSize = 0;
        if (version == 2) {
            frameSize = (quint32(sizeBytes[0]) << 16) | (quint32(sizeBytes[1]) << 8)
                | quint32(sizeBytes[2]);
        } else if (version == 4) {
            frameSize = synchsafe(sizeBytes);
        } else {
            frameSize = qFromBigEndian<quint32>(sizeBytes);
        }
        offset += frameHeader;
        if (frameSize == 0 || frameSize > static_cast<quint32>(frames.size() - offset))
            break;
        if (version == 2) {
            QByteArray expandedId;
            if (id == "TT2") expandedId = "TIT2";
            else if (id == "TP1") expandedId = "TPE1";
            else if (id == "TP2") expandedId = "TPE2";
            else if (id == "TAL") expandedId = "TALB";
            else if (id == "TRK") expandedId = "TRCK";
            else if (id == "TPA") expandedId = "TPOS";
            setId3Frame(tags, expandedId.isEmpty() ? id : expandedId,
                        frames.mid(offset, frameSize));
        } else {
            setId3Frame(tags, id, frames.mid(offset, frameSize));
        }
        offset += frameSize;
    }
    return tags;
}

AudioTags AudioMetadataReader::readFlac(const QByteArray &bytes)
{
    AudioTags tags;
    if (!bytes.startsWith("fLaC"))
        return tags;
    qsizetype offset = 4;
    while (offset + 4 <= bytes.size()) {
        const uchar *header = reinterpret_cast<const uchar *>(bytes.constData() + offset);
        const bool last = (header[0] & 0x80) != 0;
        const int type = header[0] & 0x7f;
        const quint32 blockSize = (quint32(header[1]) << 16)
            | (quint32(header[2]) << 8) | quint32(header[3]);
        offset += 4;
        if (blockSize > static_cast<quint32>(bytes.size() - offset))
            break;
        if (type == 4 && blockSize >= 8) {
            const uchar *data = reinterpret_cast<const uchar *>(bytes.constData() + offset);
            qsizetype cursor = 0;
            const quint32 vendorSize = qFromLittleEndian<quint32>(data + cursor);
            cursor += 4;
            if (vendorSize > blockSize - cursor)
                break;
            cursor += vendorSize;
            if (cursor + 4 > blockSize)
                break;
            const quint32 count = qFromLittleEndian<quint32>(data + cursor);
            cursor += 4;
            for (quint32 i = 0; i < count && cursor + 4 <= blockSize; ++i) {
                const quint32 length = qFromLittleEndian<quint32>(data + cursor);
                cursor += 4;
                if (length > blockSize - cursor)
                    break;
                setVorbisComment(tags, QByteArray(bytes.constData() + offset + cursor, length));
                cursor += length;
            }
        }
        offset += blockSize;
        if (last)
            break;
    }
    return tags;
}
