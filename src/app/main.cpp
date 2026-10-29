#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>
#include <QDebug>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("AllMusic Player"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.0.1"));

    const auto arguments = app.arguments();
    const bool smokeTest = arguments.size() == 3
        && arguments.at(1) == QStringLiteral("--smoke-test");
    if (arguments.size() != 1 && !smokeTest) {
        qCritical() << "Usage: allmusic-player [--smoke-test screenshot.png]";
        return 2;
    }

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("AllMusicPlayer"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty())
        return 1;

    if (smokeTest) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        if (!window)
            return 1;
        // Require an actual rendered frame, not merely successful QML loading.
        QObject::connect(window, &QQuickWindow::frameSwapped, &app,
                         [&app, window, path = arguments.at(2)] {
            const QImage image = window->grabWindow();
            if (!window->isVisible() || !window->isExposed() || image.isNull()
                || !image.save(path)) {
                qCritical() << "Window capture failed";
                app.exit(1);
                return;
            }
            qInfo() << "Rendered window captured:" << path;
            app.exit(0);
        }, static_cast<Qt::ConnectionType>(Qt::SingleShotConnection | Qt::QueuedConnection));
        QTimer::singleShot(15000, &app, [&app] {
            qCritical() << "Timed out waiting for a rendered window";
            app.exit(1);
        });
    }
    return app.exec();
}
