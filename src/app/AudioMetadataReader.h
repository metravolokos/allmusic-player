#pragma once

#include <QByteArray>
#include <QString>

struct AudioTags
{
    QString title;
    QString artist;
    QString albumArtist;
    QString album;
    int discNumber = 0;
    int trackNumber = 0;
};

class AudioMetadataReader final
{
public:
    static AudioTags read(const QString &path, const QByteArray &rangeBytes);

private:
    static AudioTags readMp3(const QByteArray &bytes);
    static AudioTags readFlac(const QByteArray &bytes);
};
