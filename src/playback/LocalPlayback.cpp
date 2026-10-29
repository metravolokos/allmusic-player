#include "LocalPlayback.h"
#include "WebDavProbe.h"
#include "WebDavRangeDevice.h"
#include <QFile>
#include <QFileInfo>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QTimer>
#include <QRegularExpression>

LocalPlayback::LocalPlayback(QObject *parent) : QObject(parent)
{
    m_audio.setVolume(0.25);
    m_player.setAudioOutput(&m_audio);
    connect(&m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error code, const QString &) {
        if (code == QMediaPlayer::NoError)
            return;
        if (m_rangeDevice && !m_rangeDevice->errorString().isEmpty())
            fail(m_rangeDevice->errorString());
        else if (m_error.isEmpty())
            fail(QStringLiteral("Не удалось воспроизвести файл: он повреждён, недоступен или кодек не поддерживается. Выберите другой файл."));
    });
    connect(&m_player, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::LoadedMedia) {
            QTimer::singleShot(0, this, [this] {
                if (m_player.mediaStatus() == QMediaPlayer::LoadedMedia && !m_player.hasAudio())
                    fail(QStringLiteral("В выбранном файле нет доступной аудиодорожки."));
            });
        }
    });
    auto *devices = new QMediaDevices(this);
    connect(devices, &QMediaDevices::audioOutputsChanged, this, [this] {
        m_audio.setDevice(QMediaDevices::defaultAudioOutput());
        emit deviceWarningChanged();
    });
}

LocalPlayback::~LocalPlayback()
{
    // QMediaPlayer is a value member and is destroyed before QObject deletes
    // child devices. Cancel and detach the source first so its destructor
    // cannot wait for an in-flight network timeout.
    if (m_rangeDevice)
        m_rangeDevice->cancel();
    if (m_probe)
        m_probe->cancel();
    m_player.stop();
    m_player.setSource({});
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
    // Unblock an in-flight Range read before asking the backend to stop. The
    // backend may be waiting synchronously for the source device's current read.
    if (m_rangeDevice) {
        m_rangeDevice->cancel();
    }
    // Never forward a remote URL/UNC path to the experimental backend.
    m_player.stop();
    m_player.setSource({});
    if (m_rangeDevice) {
        m_rangeDevice->deleteLater();
        m_rangeDevice = nullptr;
    }
    m_error.clear();
    emit errorChanged();
    const QString path = url.toLocalFile();
    if (!url.isLocalFile() || !url.host().isEmpty() || path.startsWith("//")
        || path.startsWith("\\\\")) {
        fail(QStringLiteral("Выберите локальный файл. Сетевые адреса не поддерживаются."));
        return false;
    }
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    if (canonical.startsWith("//") || canonical.startsWith("\\\\")
        || !info.isFile() || !info.isReadable()) {
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

void LocalPlayback::scanLoopback(int port)
{
    if (port < 1 || port > 65535) {
        fail(QStringLiteral("Укажите порт локального тестового WebDAV (1–65535)."));
        return;
    }
    if (m_probe) {
        QObject::disconnect(m_probe, nullptr, this, nullptr);
        m_probe->cancel();
        m_probe->deleteLater();
    }
    m_loopbackPort = static_cast<quint16>(port);
    m_loopbackFiles.clear();
    m_references.clear();
    m_sizes.clear();
    m_etags.clear();
    emit loopbackFilesChanged();
    m_probe = new WebDavProbe(m_loopbackPort, false, this);
    m_probe->requireB2MockMarker();
    connect(m_probe, &WebDavProbe::completed, this, [this](const WebDavProbe::Result &result) {
        if (result.error != WebDavProbe::Error::None) {
            fail(QStringLiteral("Не удалось прочитать локальный WebDAV (%1, HTTP %2).")
                     .arg(static_cast<int>(result.error)).arg(result.status));
            return;
        }
        static const QRegularExpression strongTag(QStringLiteral("^\\\"[^\\\"\\\\]+\\\"$"));
        for (const auto &entry : result.entries) {
            if (entry.directory || !entry.size || *entry.size <= 0
                || !strongTag.match(QString::fromLatin1(entry.etag)).hasMatch())
                continue;
            const QString suffix = entry.path.section('.', -1).toLower();
            if (suffix != "mp3" && suffix != "flac" && suffix != "aac"
                && suffix != "m4a" && suffix != "mp4")
                continue;
            m_references.push_back(entry.reference);
            m_sizes.push_back(*entry.size);
            m_etags.push_back(entry.etag);
            const int slash = entry.path.lastIndexOf('/');
            m_loopbackFiles.push_back(slash >= 0 ? entry.path.mid(slash + 1) : entry.path);
        }
        emit loopbackFilesChanged();
        if (m_loopbackFiles.isEmpty())
            fail(QStringLiteral("В /AllMusic не найдено ни одного файла с размером и сильным ETag."));
        else {
            m_error.clear();
            emit errorChanged();
        }
    });
    m_probe->listDirectory(QStringLiteral("/AllMusic/"));
}

bool LocalPlayback::openLoopback(int index)
{
    if (index < 0 || index >= m_references.size() || m_loopbackPort == 0) {
        fail(QStringLiteral("Сначала найдите файл в локальном тестовом WebDAV."));
        return false;
    }
    if (m_rangeDevice) {
        m_rangeDevice->cancel();
    }
    m_player.stop();
    m_player.setSource({});
    if (m_rangeDevice) {
        m_rangeDevice->deleteLater();
    }
    m_networkBytes = 0;
    emit networkBytesChanged();
    auto *device = new WebDavRangeDevice(m_loopbackPort, m_references[index],
                                         m_sizes[index], m_etags[index], this);
    if (!device->open(QIODevice::ReadOnly)) {
        fail(device->errorString());
        device->deleteLater();
        return false;
    }
    m_rangeDevice = device;
    connect(device, &WebDavRangeDevice::bytesFetched, this, [this](qint64 count) {
        m_networkBytes += count;
        emit networkBytesChanged();
    });
    connect(device, &WebDavRangeDevice::requestFailed, this,
            [this](const QString &message) { fail(message); });
    m_error.clear();
    emit errorChanged();
    m_player.setSourceDevice(m_rangeDevice);
    return true;
}
