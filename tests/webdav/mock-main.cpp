#include "MockWebDav.h"
#include <QCoreApplication>
#include <QTextStream>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    MockWebDav server;
    if (app.arguments().size() != 1 || !server.start()) return 2;
    QTextStream(stdout) << "Mock WebDAV: 127.0.0.1:" << server.serverPort() << "/AllMusic/ (Ctrl+C to stop)" << Qt::endl;
    return app.exec();
}
