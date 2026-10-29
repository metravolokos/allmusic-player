#include "WebDavProbe.h"
#include <QCoreApplication>
#include <QTextStream>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    bool ok = false;
    const int port = args.value(1).toInt(&ok);
    if (args.size() != 2 || !ok || port < 1 || port > 65535) {
        QTextStream(stderr) << "Usage: webdav-probe LOOPBACK_PORT\nOnly 127.0.0.1 /AllMusic; no credentials or remote URL accepted.\n";
        return 2;
    }
    WebDavProbe client(static_cast<quint16>(port));
    QObject::connect(&client, &WebDavProbe::completed, &app, [&app](const WebDavProbe::Result &r) {
        QTextStream(stdout) << "error=" << static_cast<int>(r.error) << " http=" << r.status
            << " attempts=" << r.attempts << " entries=" << r.entries.size()
            << " rejected=" << r.rejectedEntries << '\n';
        app.exit(r.error == WebDavProbe::Error::None ? 0 : 1);
    });
    client.listDirectory("/AllMusic/");
    return app.exec();
}
