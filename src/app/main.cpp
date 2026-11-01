#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QDebug>
#include <QVariant>
#include "AppearanceController.h"
#include "PlaybackQueueModel.h"
#ifndef Q_OS_ANDROID
#include "LocalPlayback.h"
#include "MailWebDavController.h"
#else
#include "AndroidMedia3Controller.h"
#include "MailWebDavController.h"
#endif

#include <memory>

int main(int argc, char *argv[])
{
#ifdef Q_OS_ANDROID
    // Qt's Android TLS backend resolves its custom OpenSSL SONAMEs during
    // application startup. Set this before QGuiApplication initializes Qt.
    qputenv("ANDROID_OPENSSL_SUFFIX", "_3");
#endif
#ifndef Q_OS_ANDROID
    // Fixed backend for this experiment; no network streaming in 001-B1.
    qputenv("QT_MEDIA_BACKEND", "ffmpeg");
    qputenv("QT_FFMPEG_PROTOCOL_WHITELIST", "file");
#endif
    AppearanceController::prepareFusionStyle();
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("AllMusic Player"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.0.1"));
    AppearanceController appearance;

    const auto arguments = app.arguments();
    const bool smokeTest = (arguments.size() == 3 || arguments.size() == 4)
        && arguments.at(1) == QStringLiteral("--smoke-test");
    bool capturedFrame = false;
    if (arguments.size() != 1 && !smokeTest) {
        qCritical() << "Usage: allmusic-player [--smoke-test screenshot.png [screen]]";
        return 2;
    }

#ifndef Q_OS_ANDROID
    LocalPlayback playback;
    MailWebDavController mailWebDav(nullptr, true);
    auto queue = std::make_unique<PlaybackQueueModel>(mailWebDav.libraryModel());
    QObject::connect(&mailWebDav, &MailWebDavController::playbackRequested,
                     &playback, [&mailWebDav, &playback](int index) {
        QString label;
        auto *device = mailWebDav.takePlaybackDevice(index, &label, &playback);
        playback.openMailDevice(device, label);
    });
    QObject::connect(&mailWebDav, &MailWebDavController::disconnectPlayback,
                     &playback, &LocalPlayback::stopMail);
    QObject::connect(&playback, &LocalPlayback::cloudMailActiveChanged,
                     &mailWebDav, [&playback, &mailWebDav] {
        mailWebDav.setPlaybackActive(playback.cloudMailActive());
    });
    QObject::connect(&playback, &LocalPlayback::cloudMailSessionInvalid,
                     &mailWebDav, &MailWebDavController::disconnectCloud);
    QObject::connect(queue.get(), &PlaybackQueueModel::trackRequested, &app,
                     [&mailWebDav, &playback, q = queue.get()](const QVariantMap &track,
                                                               quint64 generation) {
        const QString stableId = track.value(QStringLiteral("stableId")).toString();
        QString label;
        auto *device = mailWebDav.takePlaybackDeviceForStableId(stableId, &label, nullptr);
        if (!device) {
            q->reportError(generation, QStringLiteral("Композиция больше недоступна в Cloud Mail."));
            return;
        }
        if (!playback.openMailDevice(device, label, generation))
            q->reportError(generation, playback.error().isEmpty()
                               ? QStringLiteral("Не удалось открыть выбранную композицию.")
                               : playback.error());
    });
    QObject::connect(queue.get(), &PlaybackQueueModel::stopRequested,
                     &playback, &LocalPlayback::stop);
    QObject::connect(&playback, &LocalPlayback::trackFinished,
                     queue.get(), &PlaybackQueueModel::advance);
    QObject::connect(&playback, &LocalPlayback::errorChanged, &app,
                     [&playback, q = queue.get()] {
        if (q->active() && !playback.error().isEmpty())
            q->reportError(q->generation(), playback.error());
    });
    QObject::connect(&mailWebDav, &MailWebDavController::sessionEnded,
                     queue.get(), &PlaybackQueueModel::clear);
    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                     &mailWebDav, &MailWebDavController::disconnectCloud);
#else
    MailWebDavController mailWebDav;
    AndroidMedia3Controller media3(&mailWebDav);
    auto queue = std::make_unique<PlaybackQueueModel>(mailWebDav.libraryModel());
    QObject::connect(queue.get(), &PlaybackQueueModel::trackRequested, &app,
                     [&media3, q = queue.get()](const QVariantMap &track, quint64 generation) {
        if (!media3.playCloudStableId(track.value(QStringLiteral("stableId")).toString(),
                                      generation))
            q->reportError(generation, QStringLiteral("Не удалось запустить композицию очереди."));
    });
    QObject::connect(queue.get(), &PlaybackQueueModel::stopRequested,
                     &media3, &AndroidMedia3Controller::stop);
    QObject::connect(&media3, &AndroidMedia3Controller::trackFinished,
                     queue.get(), &PlaybackQueueModel::advance);
    QObject::connect(&media3, &AndroidMedia3Controller::cloudSessionEnded,
                     queue.get(), &PlaybackQueueModel::clear);
    QObject::connect(&media3, &AndroidMedia3Controller::changed, &app,
                     [&media3, q = queue.get()] {
        if (q->active() && !media3.errorText().isEmpty())
            q->reportError(q->generation(), media3.errorText());
    });
#endif
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
#ifdef Q_OS_ANDROID
    engine.rootContext()->setContextProperty(QStringLiteral("buildLabel"),
                                            QStringLiteral(ALLMUSIC_BUILD_LABEL));
    engine.rootContext()->setContextProperty(QStringLiteral("media3"), &media3);
    engine.rootContext()->setContextProperty(QStringLiteral("mailWebDav"), &mailWebDav);
    engine.rootContext()->setContextProperty(QStringLiteral("library"), mailWebDav.libraryModel());
    engine.rootContext()->setContextProperty(QStringLiteral("queue"), queue.get());
#else
    engine.rootContext()->setContextProperty(QStringLiteral("playback"), &playback);
    engine.rootContext()->setContextProperty(QStringLiteral("mailWebDav"), &mailWebDav);
    engine.rootContext()->setContextProperty(QStringLiteral("library"), mailWebDav.libraryModel());
    engine.rootContext()->setContextProperty(QStringLiteral("queue"), queue.get());
#endif
    engine.rootContext()->setContextProperty(QStringLiteral("smokePage"),
        smokeTest && arguments.size() == 4 ? QVariant(arguments.at(3)) : QVariant());
    engine.rootContext()->setContextProperty(QStringLiteral("appearance"), &appearance);
    engine.loadFromModule(QStringLiteral("AllMusicPlayer"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty())
        return 1;

    if (smokeTest) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        if (!window)
            return 1;
        // Require an actual rendered frame, not merely successful QML loading.
        QObject::connect(window, &QQuickWindow::frameSwapped, &app,
                         [&app, &capturedFrame, window, path = arguments.at(2)] {
            const QImage image = window->grabWindow();
            if (!window->isVisible() || !window->isExposed() || image.isNull()
                || !image.save(path)) {
                qCritical() << "Window capture failed";
                app.exit(1);
                return;
            }
            qInfo() << "Rendered window captured:" << path;
            capturedFrame = true;
            app.exit(0);
        }, static_cast<Qt::ConnectionType>(Qt::SingleShotConnection | Qt::QueuedConnection));
        QTimer::singleShot(15000, &app, [&app] {
            qCritical() << "Timed out waiting for a rendered window";
            app.exit(1);
        });
    }
    const int result = app.exec();
    return smokeTest && !capturedFrame ? 1 : result;
}
