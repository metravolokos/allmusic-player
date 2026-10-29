#include "LocalPlayback.h"
#include <QFile>
#include <QFileInfo>
#include <QMediaDevices>
#include <QAudioDevice>

LocalPlayback::LocalPlayback(QObject *parent) : QObject(parent)
{
    m_audio.setVolume(0.25);
    m_player.setAudioOutput(&m_audio);
    connect(&m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error code, const QString &) {
        if (code != QMediaPlayer::NoError)
            fail(QStringLiteral("Не удалось воспроизвести файл: он повреждён, недоступен или кодек не поддерживается. Выберите другой файл."));
    });
    auto *devices = new QMediaDevices(this);
    connect(devices, &QMediaDevices::audioOutputsChanged, this, [this] {
        m_audio.setDevice(QMediaDevices::defaultAudioOutput());
        emit deviceWarningChanged();
    });
}

QString LocalPlayback::deviceWarning() const
{
    return QMediaDevices::audioOutputs().isEmpty()
        ? QStringLiteral("Устройство вывода звука не найдено. Проверьте настройки звука ОС.") : QString();
}

void LocalPlayback::fail(const QString &message)
{
    m_error = message;
    emit errorChanged();
}

bool LocalPlayback::openLocal(const QUrl &url)
{
    // Never forward a remote URL/UNC path to the experimental backend.
    m_player.stop();
    m_player.setSource({});
    m_error.clear();
    emit errorChanged();
    const QString path = url.toLocalFile();
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    if (!url.isLocalFile() || !url.host().isEmpty() || path.startsWith("//")
        || path.startsWith("\\\\") || canonical.startsWith("//")
        || canonical.startsWith("\\\\") || !info.isFile() || !info.isReadable()) {
        fail(QStringLiteral("Выберите доступный локальный аудиофайл. Сетевые адреса и папки не поддерживаются."));
        return false;
    }
    QFile file(canonical);
    if (!file.open(QIODevice::ReadOnly)) {
        fail(QStringLiteral("Не удалось открыть файл для чтения."));
        return false;
    }
    const QByteArray header = file.read(16);
    // Content guard against playlists/text; the decoder, not the extension,
    // determines the actual audio codec. Protocol whitelist also blocks URLs
    // embedded in a container. This is not a general media security sandbox.
    const bool sync = header.size() >= 2 && uchar(header[0]) == 0xff
        && (uchar(header[1]) & 0xe0) == 0xe0;
    if (!(header.startsWith("fLaC") || header.startsWith("ID3") || sync
          || (header.size() >= 12 && header.mid(4, 4) == "ftyp"))) {
        fail(QStringLiteral("Формат файла не распознан. Для прототипа нужны MP3, FLAC, AAC (ADTS/M4A) или ALAC (M4A)."));
        return false;
    }
    m_player.setSource(QUrl::fromLocalFile(canonical));
    return true;
}
