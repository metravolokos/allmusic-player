#include "LocalPlayback.h"
#include "MailWebDavRangeDevice.h"
#include "WebDavProbe.h"
#include "WebDavRangeDevice.h"
#include <QFile>
#include <QFileInfo>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QTimer>
#include <QRegularExpression>
#include <QPointer>

LocalPlayback::LocalPlayback(QObject *parent) : QObject(parent)
{
    m_audio.setVolume(0.25);
    m_player.setAudioOutput(&m_audio);
    connect(&m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error code, const QString &) {
        if (code == QMediaPlayer::NoError)
            return;
        if (m_mailRangeDevice && !m_mailRangeDevice->errorString().isEmpty())
            fail(m_mailRangeDevice->errorString());
        else if (m_rangeDevice && !m_rangeDevice->errorString().isEmpty())
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
    if (m_mailRangeDevice)
        m_mailRangeDevice->cancel();
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

void LocalPlayback::setCurrentLoopbackIndex(int index)
{
    if (m_currentLoopbackIndex == index)
        return;
    m_currentLoopbackIndex = index;
    emit currentLoopbackIndexChanged();
}

void LocalPlayback::detachNetworkSource()
{
    auto *device = m_rangeDevice;
    if (!device)
        return;

    // A decoder read can be waiting on a Range response. Cancel first so
    // stop/detach cannot leave the worker blocked on the network timeout.
    device->cancel();
    m_player.stop();
    m_player.setSource({});
    if (m_rangeDevice == device)
        m_rangeDevice = nullptr;
    device->deleteLater();
}

void LocalPlayback::detachMailSource()
{
    auto *device = m_mailRangeDevice;
    if (!device)
        return;
    device->cancel();
    m_player.stop();
    m_player.setSource({});
    if (m_mailRangeDevice == device)
        m_mailRangeDevice = nullptr;
    device->deleteLater();
    if (m_cloudMailActive) {
        m_cloudMailActive = false;
        emit cloudMailActiveChanged();
    }
    if (m_buffering) {
        m_buffering = false;
        emit bufferingChanged();
    }
}

void LocalPlayback::resetPlayerSource()
{
    if (m_mailRangeDevice) {
        detachMailSource();
        return;
    }
    if (m_rangeDevice) {
        detachNetworkSource();
        return;
    }
    m_player.stop();
    m_player.setSource({});
}

void LocalPlayback::stop()
{
    if (m_mailRangeDevice) {
        detachMailSource();
    } else if (m_rangeDevice) {
        detachNetworkSource();
    } else {
        // Local playback keeps its source attached; QMediaPlayer resets its
        // position on stop and the next play starts the same file again.
        m_player.stop();
    }
    if (!m_error.isEmpty()) {
        m_error.clear();
        emit errorChanged();
    }
}

void LocalPlayback::stopMail()
{
    if (m_mailRangeDevice)
        detachMailSource();
    if (!m_activeSourceLabel.isEmpty()) {
        m_activeSourceLabel.clear();
        emit activeSourceLabelChanged();
    }
}

void LocalPlayback::togglePlayback()
{
    switch (m_player.playbackState()) {
    case QMediaPlayer::PlayingState:
        m_player.pause();
        return;
    case QMediaPlayer::PausedState:
        m_player.play();
        return;
    case QMediaPlayer::StoppedState:
        break;
    }

    // A stopped network source was cancelled and detached. Reopen its current
    // WebDAV item before starting; pause/resume above keeps the existing source.
    if (m_currentLoopbackIndex >= 0 && !m_rangeDevice) {
        const int index = m_currentLoopbackIndex;
        if (openLoopback(index))
            m_player.play();
        return;
    }
    m_player.play();
}

bool LocalPlayback::openLocal(const QUrl &url)
{
    resetPlayerSource();
    if (!m_activeSourceLabel.isEmpty()) {
        m_activeSourceLabel.clear();
        emit activeSourceLabelChanged();
    }
    setCurrentLoopbackIndex(-1);
    // Never forward a remote URL/UNC path to the experimental backend.
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
    if (m_mailRangeDevice)
        stopMail();
    if (m_rangeDevice)
        stop();
    setCurrentLoopbackIndex(-1);
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
    resetPlayerSource();
    if (!m_activeSourceLabel.isEmpty()) {
        m_activeSourceLabel.clear();
        emit activeSourceLabelChanged();
    }
    setCurrentLoopbackIndex(-1);
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
    const QPointer<WebDavRangeDevice> guardedDevice(device);
    connect(device, &WebDavRangeDevice::bytesFetched, this, [this, guardedDevice](qint64 count) {
        if (!guardedDevice || m_rangeDevice != guardedDevice.data())
            return;
        m_networkBytes += count;
        emit networkBytesChanged();
    });
    connect(device, &WebDavRangeDevice::requestFailed, this,
            [this, guardedDevice](const QString &message) {
        if (guardedDevice && m_rangeDevice == guardedDevice.data())
            fail(message);
    });
    m_error.clear();
    emit errorChanged();
    setCurrentLoopbackIndex(index);
    m_player.setSourceDevice(m_rangeDevice);
    return true;
}

bool LocalPlayback::openMailDevice(MailWebDavRangeDevice *device, const QString &label)
{
    if (!device) {
        fail(QStringLiteral("Не удалось открыть выбранный файл Cloud Mail."));
        return false;
    }
    resetPlayerSource();
    setCurrentLoopbackIndex(-1);
    m_networkBytes = 0;
    emit networkBytesChanged();
    if (!device->open(QIODevice::ReadOnly)) {
        fail(device->errorString());
        device->cancel();
        device->deleteLater();
        return false;
    }
    device->setParent(this);
    m_mailRangeDevice = device;
    const QPointer<MailWebDavRangeDevice> guardedDevice(device);
    connect(device, &MailWebDavRangeDevice::bytesFetched, this,
            [this, guardedDevice](qint64 count) {
        if (!guardedDevice || m_mailRangeDevice != guardedDevice.data())
            return;
        m_networkBytes += count;
        emit networkBytesChanged();
    });
    connect(device, &MailWebDavRangeDevice::bufferingChanged, this,
            [this, guardedDevice] {
        if (!guardedDevice || m_mailRangeDevice != guardedDevice.data())
            return;
        const bool value = guardedDevice->buffering();
        if (m_buffering != value) {
            m_buffering = value;
            emit bufferingChanged();
        }
    });
    connect(device, &MailWebDavRangeDevice::requestFailed, this,
            [this, guardedDevice](const QString &message) {
        if (guardedDevice && m_mailRangeDevice == guardedDevice.data())
            fail(message);
    });
    connect(device, &MailWebDavRangeDevice::sessionInvalid, this,
            [this, guardedDevice] {
        if (guardedDevice && m_mailRangeDevice == guardedDevice.data())
            emit cloudMailSessionInvalid();
    });
    m_error.clear();
    emit errorChanged();
    if (m_activeSourceLabel != label) {
        m_activeSourceLabel = label;
        emit activeSourceLabelChanged();
    }
    m_cloudMailActive = true;
    emit cloudMailActiveChanged();
    m_player.setSourceDevice(device);
    m_player.play();
    return true;
}
