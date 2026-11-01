#include "AndroidMedia3Controller.h"
#include "AndroidMailRangeRegistry.h"
#include "MailWebDavController.h"
#include "TrackIdentity.h"
#include "../playback/MailWebDavRangeDevice.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#include <QStringList>

#include <algorithm>
#include <iterator>
#include <utility>

namespace {
constexpr auto kAdapterClass = "org/metravolokos/allmusicplayer/Media3PlaybackAdapter";

struct Track
{
    const char *title;
    const char *assetPath;
};

const Track kTracks[] = {
    {"Синтетический MP3 · 440 Гц", "audio/synthetic-440hz.mp3"},
    {"Синтетический FLAC · 660 Гц", "audio/synthetic-660hz.flac"},
    {"WAV · регрессионный тест · 880 Гц", "audio/synthetic-880hz.wav"},
    {"Отсутствующий файл · проверка ошибки", "audio/missing-sample.mp3"},
};

bool hasJniException(QJniEnvironment &environment)
{
    if (!environment->ExceptionCheck())
        return false;
    environment->ExceptionClear();
    return true;
}

AndroidMedia3State::Value valueFromJava(jint value)
{
    if (value < static_cast<jint>(AndroidMedia3State::Value::Preparing)
        || value > static_cast<jint>(AndroidMedia3State::Value::Error)) {
        return AndroidMedia3State::Value::Error;
    }
    return static_cast<AndroidMedia3State::Value>(value);
}
}

AndroidMedia3Controller::AndroidMedia3Controller(MailWebDavController *cloud,
                                                 QObject *parent)
    : QObject(parent), m_cloud(cloud),
      m_cloudEntriesModel(new AndroidCloudTrackListModel(this)),
      m_favoritesModel(new AndroidFavoritesModel(this))
{
    m_cloudEntriesModel->setFavoriteIds(m_favoritesModel->favoriteIds());
    connect(m_favoritesModel, &AndroidFavoritesModel::favoritesChanged, this, [this] {
        m_cloudEntriesModel->setFavoriteIds(m_favoritesModel->favoriteIds());
        if (!m_pendingFavoriteId.isEmpty()
            && !m_favoritesModel->contains(m_pendingFavoriteId)) {
            cancelFavoriteNavigation();
        }
        emit changed();
    });

    if (!m_cloud || !AndroidMailRangeRegistry::registerJniMethods()) {
        setError(tr("Не удалось подключить безопасный мост Cloud Mail."));
        return;
    }
    auto context = QNativeInterface::QAndroidApplication::context();
    if (context.isValid()) {
        QJniObject::callStaticMethod<void>(kAdapterClass, "initialize",
                                           "(Landroid/content/Context;)V",
                                           context.object<jobject>());
        QJniEnvironment environment;
        if (hasJniException(environment))
            setError(tr("Не удалось запустить Android Media3 адаптер."));
    } else {
        setError(tr("Android не передал приложению системный контекст."));
    }

    connect(&m_refreshTimer, &QTimer::timeout,
            this, &AndroidMedia3Controller::refreshState);
    m_refreshTimer.setInterval(200);
    m_refreshTimer.start();

    if (auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        connect(application, &QGuiApplication::applicationStateChanged, this,
                [this](Qt::ApplicationState state) {
            if (state != Qt::ApplicationActive)
                stop();
        });
        connect(application, &QCoreApplication::aboutToQuit,
                this, &AndroidMedia3Controller::release);
    }

    connect(m_cloud, &MailWebDavController::disconnectPlayback,
            this, &AndroidMedia3Controller::stopForDisconnect);
    connect(m_cloud, &MailWebDavController::playbackRequested,
            this, &AndroidMedia3Controller::playCloud);
    connect(m_cloud, &MailWebDavController::stateChanged, this, [this] {
        emit changed();
        if (!m_pendingFavoriteId.isEmpty() && !m_cloud->busy()) {
            QTimer::singleShot(0, this, [this] {
                if (!m_pendingFavoriteId.isEmpty())
                    continueFavoriteNavigation();
            });
        }
    });
    connect(m_cloud, &MailWebDavController::entriesChanged, this, [this] {
        QTimer::singleShot(0, this, [this] {
            const QVariantList entries = m_cloud->entries();
            m_cloudEntriesModel->setEntries(entries);
            m_favoritesModel->setCurrentEntries(entries, m_cloud->currentDirectory(),
                                                m_cloud->connected() && !m_cloud->busy());
            m_cloudEntriesModel->setFavoriteIds(m_favoritesModel->favoriteIds());
            emit cloudEntriesReset();
            emit changed();
        });
    });
    connect(m_cloud, &MailWebDavController::sessionEnded, this, [this] {
        cancelFavoriteNavigation();
        emit cloudSessionEnded();
    });

    selectTrack(0);
}

AndroidMedia3Controller::~AndroidMedia3Controller()
{
    release();
}

QStringList AndroidMedia3Controller::trackNames() const
{
    QStringList result;
    for (const auto &track : kTracks)
        result.append(QString::fromUtf8(track.title));
    return result;
}

int AndroidMedia3Controller::trackIndex() const
{
    return m_trackIndex;
}

QString AndroidMedia3Controller::activeTrackName() const
{
    if (m_cloudTrackActive)
        return m_cloudTrackName;
    if (m_trackIndex < 0 || m_trackIndex >= static_cast<int>(std::size(kTracks)))
        return tr("Трек не выбран");
    return QString::fromUtf8(kTracks[m_trackIndex].title);
}

QString AndroidMedia3Controller::stateText() const
{
    switch (m_state) {
    case AndroidMedia3State::Value::Preparing: return tr("Подготовка Media3");
    case AndroidMedia3State::Value::Ready: return tr("Готово к запуску");
    case AndroidMedia3State::Value::Playing: return tr("Воспроизведение");
    case AndroidMedia3State::Value::Paused: return tr("Пауза");
    case AndroidMedia3State::Value::Stopped: return tr("Остановлено");
    case AndroidMedia3State::Value::Finished: return tr("Трек завершён");
    case AndroidMedia3State::Value::Error: return tr("Ошибка");
    }
    return tr("Неизвестное состояние");
}

QString AndroidMedia3Controller::outputStatusText() const
{
    return tr("Media3 / ExoPlayer · системный Android AudioTrack");
}

QString AndroidMedia3Controller::errorText() const
{
    return m_error;
}

qint64 AndroidMedia3Controller::positionMs() const
{
    return m_positionMs;
}

qint64 AndroidMedia3Controller::durationMs() const
{
    return m_durationMs;
}

bool AndroidMedia3Controller::playEnabled() const
{
    return AndroidMedia3State::actions(m_state).play;
}

bool AndroidMedia3Controller::pauseEnabled() const
{
    return AndroidMedia3State::actions(m_state).pause;
}

bool AndroidMedia3Controller::stopEnabled() const
{
    return AndroidMedia3State::actions(m_state).stop;
}

bool AndroidMedia3Controller::seekEnabled() const
{
    return AndroidMedia3State::seekEnabled(m_state, m_durationMs > 0);
}

bool AndroidMedia3Controller::cloudConnected() const
{
    return m_cloud && m_cloud->connected();
}

bool AndroidMedia3Controller::cloudBusy() const
{
    return m_cloud && m_cloud->busy();
}

QString AndroidMedia3Controller::cloudStatus() const
{
    if (!m_cloud)
        return tr("Cloud Mail недоступен.");
    if (m_cloud->busy())
        return tr("Выполняется защищённый запрос Cloud Mail…");
    if (m_cloud->connected())
        return tr("Подключено. Каталог Cloud Mail готов.");
    return m_cloud->status();
}

QString AndroidMedia3Controller::cloudDirectoryLabel() const
{
    if (!m_cloud)
        return {};
    QString path = m_cloud->currentDirectory();
    while (path.endsWith(u'/'))
        path.chop(1);
    const int slash = path.lastIndexOf(u'/');
    return slash >= 0 ? path.mid(slash + 1) : path;
}

AndroidCloudTrackListModel *AndroidMedia3Controller::cloudEntriesModel() const
{
    return m_cloudEntriesModel;
}

AndroidFavoritesModel *AndroidMedia3Controller::favoritesModel() const
{
    return m_favoritesModel;
}

void AndroidMedia3Controller::connectCloud(QString email, QString appPassword)
{
    if (m_released || !m_cloud)
        return;
    m_cloud->checkConnection(std::move(email), std::move(appPassword));
    emit changed();
}

void AndroidMedia3Controller::disconnectCloud()
{
    cancelFavoriteNavigation();
    if (m_cloud)
        m_cloud->disconnectCloud();
}

void AndroidMedia3Controller::openCloudDirectory(int index)
{
    cancelFavoriteNavigation();
    if (m_cloud && m_cloudEntriesModel->containsSourceIndex(index))
        m_cloud->openDirectory(index);
}

bool AndroidMedia3Controller::canGoToCloudParent() const
{
    return m_cloud && m_cloud->canGoToParent();
}

void AndroidMedia3Controller::goToCloudParent()
{
    cancelFavoriteNavigation();
    if (m_cloud)
        m_cloud->goToParent();
}

bool AndroidMedia3Controller::canPlayCloud(int index) const
{
    return m_cloud && m_cloudEntriesModel->containsSourceIndex(index)
        && m_cloud->canPlay(index);
}

bool AndroidMedia3Controller::toggleFavorite(const QString &stableId, const QString &label)
{
    if (m_released || !m_cloud || !m_favoritesModel || !m_cloud->connected()
        || m_cloud->busy()) {
        return false;
    }

    const QVariantList entries = m_cloud->entries();
    for (const QVariant &value : entries) {
        const QVariantMap entry = value.toMap();
        if (entry.value(QStringLiteral("stableId")).toString() == stableId
            && entry.value(QStringLiteral("isAudio")).toBool()
            && !entry.value(QStringLiteral("isDirectory")).toBool()) {
            const QString sourcePath = m_cloud->sourcePathForStableId(stableId);
            return m_favoritesModel->toggleFavorite(stableId, label, sourcePath);
        }
    }
    return false;
}

bool AndroidMedia3Controller::removeFavorite(const QString &stableId)
{
    return m_favoritesModel && m_favoritesModel->removeFavorite(stableId);
}

void AndroidMedia3Controller::selectTrack(int index)
{
    if (m_released || index < 0 || index >= static_cast<int>(std::size(kTracks)))
        return;
    cancelFavoriteNavigation();
    releaseCloudSource();
    m_trackIndex = index;
    m_error.clear();
    m_positionMs = 0;
    m_durationMs = 0;
    m_state = AndroidMedia3State::Value::Preparing;
    emit changed();
    invokeTrack(QString::fromLatin1(kTracks[index].assetPath),
                QString::fromUtf8(kTracks[index].title));
}

void AndroidMedia3Controller::play()
{
    if (m_released || !playEnabled())
        return;
    m_error.clear();
    invokeCommand("play");
    m_state = AndroidMedia3State::Value::Preparing;
    emit changed();
}

void AndroidMedia3Controller::pause()
{
    if (m_released || !pauseEnabled())
        return;
    invokeCommand("pause");
    m_state = AndroidMedia3State::Value::Paused;
    emit changed();
}

void AndroidMedia3Controller::stop()
{
    cancelFavoriteNavigation();
    if (m_released || !stopEnabled())
        return;
    invokeCommand("stop");
    if (m_cloudTrackActive && m_cloud)
        m_cloud->setPlaybackActive(false);
    m_state = AndroidMedia3State::Value::Stopped;
    m_positionMs = 0;
    emit changed();
}

void AndroidMedia3Controller::playCloud(int index)
{
    cancelFavoriteNavigation();
    if (m_released || !m_cloud || !m_cloudEntriesModel->containsSourceIndex(index)
        || !m_cloud->canPlay(index))
        return;

    playCloudSourceIndex(index);
}

bool AndroidMedia3Controller::canPlayFavorite(const QString &stableId) const
{
    if (m_released || !m_cloud || !m_cloudEntriesModel || !m_cloud->connected()
        || m_cloud->busy() || !m_favoritesModel || !m_favoritesModel->contains(stableId)
        || m_favoritesModel->isMissing(stableId)) {
        return false;
    }
    const int sourceIndex = m_cloudEntriesModel->sourceIndexForStableId(stableId);
    if (sourceIndex >= 0)
        return m_cloud->canPlay(sourceIndex);

    const QString sourcePath = m_favoritesModel->sourcePathForStableId(stableId);
    if (!TrackIdentity::isValidPath(sourcePath))
        return false;
    const QString lowerPath = sourcePath.toLower();
    return lowerPath.endsWith(QStringLiteral(".mp3"))
        || lowerPath.endsWith(QStringLiteral(".flac"));
}

void AndroidMedia3Controller::playFavorite(const QString &stableId)
{
    if (!canPlayFavorite(stableId))
        return;
    const int sourceIndex = m_cloudEntriesModel->sourceIndexForStableId(stableId);
    if (sourceIndex >= 0) {
        playCloudSourceIndex(sourceIndex);
        return;
    }

    cancelFavoriteNavigation();
    m_pendingFavoriteId = stableId;
    continueFavoriteNavigation();
}

void AndroidMedia3Controller::continueFavoriteNavigation()
{
    if (m_pendingFavoriteId.isEmpty() || m_waitingForFavoriteListing || !m_cloud
        || m_cloud->busy()) {
        return;
    }
    if (!m_cloud->connected()) {
        cancelFavoriteNavigation();
        return;
    }

    if (!m_expectedFavoriteDirectory.isEmpty()) {
        if (m_cloud->currentDirectory() != m_expectedFavoriteDirectory) {
            cancelFavoriteNavigation();
            setError(tr("Не удалось открыть папку избранного."));
            return;
        }
        m_expectedFavoriteDirectory.clear();
    }

    const QString stableId = m_pendingFavoriteId;
    const QString sourcePath = m_favoritesModel->sourcePathForStableId(stableId);
    const qsizetype separator = sourcePath.lastIndexOf(u'/');
    if (!TrackIdentity::isValidPath(sourcePath) || separator < 0) {
        cancelFavoriteNavigation();
        return;
    }
    const QString targetDirectory = sourcePath.left(separator + 1);
    const QString currentDirectory = m_cloud->currentDirectory();

    if (currentDirectory == targetDirectory) {
        const int sourceIndex = m_cloudEntriesModel->sourceIndexForStableId(stableId);
        if (sourceIndex >= 0 && m_cloud->canPlay(sourceIndex)) {
            cancelFavoriteNavigation();
            playCloudSourceIndex(sourceIndex);
        } else {
            cancelFavoriteNavigation();
            if (m_favoritesModel->isMissing(stableId))
                setError(tr("Избранный трек больше не найден в медиатеке."));
        }
        return;
    }

    if (!currentDirectory.startsWith(QStringLiteral("/AllMusic/"))) {
        cancelFavoriteNavigation();
        setError(tr("Не удалось открыть папку избранного."));
        return;
    }

    if (!targetDirectory.startsWith(currentDirectory)) {
        if (!m_cloud->canGoToParent()) {
            cancelFavoriteNavigation();
            return;
        }
        QString parentDirectory = currentDirectory;
        if (parentDirectory.endsWith(u'/'))
            parentDirectory.chop(1);
        const qsizetype parentSeparator = parentDirectory.lastIndexOf(u'/');
        if (parentSeparator < 0) {
            cancelFavoriteNavigation();
            return;
        }
        m_expectedFavoriteDirectory = parentDirectory.left(parentSeparator + 1);
        m_waitingForFavoriteListing = true;
        m_cloud->goToParent();
        m_waitingForFavoriteListing = false;
        return;
    }

    const QString remainder = targetDirectory.mid(currentDirectory.size());
    const qsizetype nextSeparator = remainder.indexOf(u'/');
    if (nextSeparator < 0) {
        cancelFavoriteNavigation();
        setError(tr("Не удалось определить папку избранного."));
        return;
    }
    const QString nextDirectory = currentDirectory + remainder.left(nextSeparator + 1);
    openFavoriteNavigationDirectory(nextDirectory);
}

void AndroidMedia3Controller::openFavoriteNavigationDirectory(const QString &directory)
{
    const QString stableId = TrackIdentity::stableIdForPath(directory);
    const int sourceIndex = m_cloud->sourceIndexForStableId(stableId);
    if (m_cloud->canOpenDirectory(sourceIndex)) {
        m_expectedFavoriteDirectory = directory;
        m_waitingForFavoriteListing = true;
        m_cloud->openDirectory(sourceIndex);
        m_waitingForFavoriteListing = false;
        return;
    }
    cancelFavoriteNavigation();
    setError(tr("Папка избранного больше не найдена в медиатеке."));
}

void AndroidMedia3Controller::cancelFavoriteNavigation()
{
    m_pendingFavoriteId.clear();
    m_expectedFavoriteDirectory.clear();
    m_waitingForFavoriteListing = false;
}

void AndroidMedia3Controller::playCloudSourceIndex(int index)
{
    if (m_released || !m_cloud || !m_cloud->canPlay(index))
        return;

    releaseCloudSource();
    QString label;
    auto *device = m_cloud->takePlaybackDevice(index, &label, nullptr);
    if (!device || !device->open(QIODevice::ReadOnly)) {
        if (device)
            delete device;
        setError(tr("Не удалось открыть поток Cloud Mail."));
        return;
    }

    const qint64 handle = AndroidMailRangeRegistry::add(device);
    if (handle <= 0) {
        device->retire();
        setError(tr("Не удалось создать поток Media3."));
        return;
    }

    connect(device, &MailWebDavRangeDevice::requestFailed, this,
            [this](const QString &message) {
        if (!m_released) {
            m_error = message;
            m_state = AndroidMedia3State::Value::Error;
            const QJniObject error = QJniObject::fromString(message);
            QJniObject::callStaticMethod<void>(kAdapterClass, "reportNativeError",
                "(Ljava/lang/String;)V", error.object<jstring>());
            QJniEnvironment environment;
            hasJniException(environment);
            if (m_cloud)
                m_cloud->setPlaybackActive(false);
            emit changed();
        }
    });
    connect(device, &MailWebDavRangeDevice::sessionInvalid,
            m_cloud, &MailWebDavController::disconnectCloud);

    m_cloudHandle = handle;
    m_cloudTrackIndex = index;
    m_cloudTrackActive = true;
    m_cloudTrackName = label;
    m_trackIndex = -1;
    m_error.clear();
    m_positionMs = 0;
    m_durationMs = 0;
    m_state = AndroidMedia3State::Value::Preparing;
    m_cloud->setPlaybackActive(true);
    QJniObject::callStaticMethod<void>(kAdapterClass, "selectCloudTrack",
        "(J)V", static_cast<jlong>(handle));
    QJniEnvironment environment;
    if (hasJniException(environment)) {
        releaseCloudSource();
        setError(tr("Media3 не смог открыть поток Cloud Mail."));
        return;
    }
    emit changed();
}

void AndroidMedia3Controller::seekTo(qint64 positionMs)
{
    if (!seekEnabled())
        return;
    const qint64 clamped = std::clamp(positionMs, qint64(0), m_durationMs);
    QJniObject::callStaticMethod<void>(kAdapterClass, "seekTo", "(J)V",
                                       static_cast<jlong>(clamped));
    QJniEnvironment environment;
    if (hasJniException(environment))
        setError(tr("Media3 не смог переместиться к выбранной позиции."));
    else
        m_positionMs = clamped;
    emit changed();
}

void AndroidMedia3Controller::release()
{
    if (m_released)
        return;
    m_released = true;
    cancelFavoriteNavigation();
    m_refreshTimer.stop();
    releaseCloudSource();
    AndroidMailRangeRegistry::releaseAll();
    invokeCommand("release");
}

void AndroidMedia3Controller::releaseCloudSource()
{
    if (m_cloudHandle <= 0)
        return;
    const qint64 handle = m_cloudHandle;
    m_cloudHandle = 0;
    AndroidMailRangeRegistry::release(handle);
    QJniObject::callStaticMethod<void>(kAdapterClass, "clearCloudTrack", "(J)V",
                                       static_cast<jlong>(handle));
    QJniEnvironment environment;
    hasJniException(environment);
    m_cloudTrackActive = false;
    m_cloudTrackIndex = -1;
    m_cloudTrackName.clear();
    if (m_cloud)
        m_cloud->setPlaybackActive(false);
}

void AndroidMedia3Controller::stopForDisconnect()
{
    if (m_released)
        return;
    if (m_cloudTrackActive)
        invokeCommand("stop");
    releaseCloudSource();
}

void AndroidMedia3Controller::refreshState()
{
    if (m_released)
        return;

    QJniEnvironment environment;
    const jint state = QJniObject::callStaticMethod<jint>(kAdapterClass, "stateCode", "()I");
    const jlong position = QJniObject::callStaticMethod<jlong>(kAdapterClass, "positionMs", "()J");
    const jlong duration = QJniObject::callStaticMethod<jlong>(kAdapterClass, "durationMs", "()J");
    const QString error = QJniObject::callStaticObjectMethod(kAdapterClass, "errorText",
                                                              "()Ljava/lang/String;").toString();
    if (hasJniException(environment)) {
        setError(tr("Не удалось прочитать состояние Android Media3."));
        return;
    }

    const auto nextState = valueFromJava(state);
    const qint64 nextPosition = std::max<qint64>(0, position);
    const qint64 nextDuration = std::max<qint64>(0, duration);
    if (nextState == m_state && nextPosition == m_positionMs
        && nextDuration == m_durationMs && error == m_error) {
        return;
    }
    m_state = nextState;
    m_positionMs = nextPosition;
    m_durationMs = nextDuration;
    m_error = error;
    emit changed();
}

void AndroidMedia3Controller::setError(const QString &message)
{
    m_error = message.trimmed().isEmpty() ? tr("Неизвестная ошибка Media3.") : message.trimmed();
    m_state = AndroidMedia3State::Value::Error;
    emit changed();
}

void AndroidMedia3Controller::invokeTrack(const QString &assetPath, const QString &title)
{
    const QJniObject path = QJniObject::fromString(assetPath);
    const QJniObject label = QJniObject::fromString(title);
    QJniObject::callStaticMethod<void>(kAdapterClass, "selectTrack",
                                       "(Ljava/lang/String;Ljava/lang/String;)V",
                                       path.object<jstring>(), label.object<jstring>());
    QJniEnvironment environment;
    if (hasJniException(environment))
        setError(tr("Media3 не смог выбрать встроенный тестовый трек."));
}

void AndroidMedia3Controller::invokeCommand(const char *method)
{
    QJniObject::callStaticMethod<void>(kAdapterClass, method, "()V");
    QJniEnvironment environment;
    if (hasJniException(environment))
        setError(tr("Не удалось выполнить команду Android Media3."));
}
