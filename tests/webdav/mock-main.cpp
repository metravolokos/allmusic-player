#include "MockWebDav.h"

#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <QUrl>

namespace {
QByteArray tagFor(const QByteArray &bytes)
{
    return "\"" + QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex() + "\"";
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("AllMusic loopback WebDAV mock");
    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption rootOption({"r", "root"}, "Directory containing generated test tones.", "directory");
    QCommandLineOption portOption({"p", "port"}, "Loopback port (default 8765).", "port", "8765");
    QCommandLineOption ignoreRangeOption("ignore-range", "Return HTTP 200 to GET requests with Range.");
    parser.addOption(rootOption);
    parser.addOption(portOption);
    parser.addOption(ignoreRangeOption);
    parser.process(app);

    bool portOk = false;
    const int port = parser.value(portOption).toInt(&portOk);
    if (!portOk || port < 1 || port > 65535) return 2;
    QString root;
    QStringList names;
    if (parser.isSet(rootOption)) {
        const QFileInfo rootInfo(parser.value(rootOption));
        root = rootInfo.canonicalFilePath();
        if (root.isEmpty() || !rootInfo.isDir()) {
            QTextStream(stderr) << "The test tone directory does not exist." << Qt::endl;
            return 2;
        }
        const QDir dir(root);
        for (const auto &name : dir.entryList(QDir::Files, QDir::Name)) {
            if (!QRegularExpression(QStringLiteral("^tone-[A-Za-z0-9-]+\\.(mp3|flac|aac|m4a)$"),
                                    QRegularExpression::CaseInsensitiveOption).match(name).hasMatch())
                continue;
            const QFileInfo file(dir.filePath(name));
            if (file.canonicalPath() == root && file.isReadable()) names.push_back(name);
        }
        if (names.isEmpty()) {
            QTextStream(stderr) << "No generated tone files found. Expected tone-*.mp3/.flac/.aac/.m4a." << Qt::endl;
            return 2;
        }
    }

    MockWebDav server;
    if (!server.listen(QHostAddress::LocalHost, static_cast<quint16>(port))) return 3;
    server.onRequest = [](const MockWebDav::Request &request) {
        const QByteArray range = request.headers.value("range");
        qInfo().noquote() << QString::fromLatin1(request.method) << QString::fromLatin1(request.target)
                          << (range.isEmpty() ? QString() : QStringLiteral("Range: ") + QString::fromLatin1(range));
    };
    if (parser.isSet(rootOption)) {
        const bool ignoreRange = parser.isSet(ignoreRangeOption);
        server.handler = [root, names, ignoreRange](const MockWebDav::Request &request) {
            MockWebDav::Response response;
            if (request.method == "PROPFIND") {
                QByteArray xml = "<?xml version=\"1.0\"?><d:multistatus xmlns:d=\"DAV:\">"
                    "<d:response><d:href>/AllMusic/</d:href><d:propstat><d:prop>"
                    "<d:resourcetype><d:collection/></d:resourcetype></d:prop>"
                    "<d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>";
                for (const auto &name : names) {
                    QFile file(QDir(root).filePath(name));
                    if (!file.open(QIODevice::ReadOnly)) continue;
                    const auto data = file.readAll();
                    xml += "<d:response><d:href>/AllMusic/" + name.toUtf8() + "</d:href><d:propstat><d:prop>"
                        "<d:getcontentlength>" + QByteArray::number(data.size()) + "</d:getcontentlength>"
                        "<d:getetag>&quot;" + tagFor(data).mid(1, tagFor(data).size() - 2) + "&quot;</d:getetag>"
                        "</d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>";
                }
                xml += "</d:multistatus>";
                response.status = 207;
                response.body = xml;
                return response;
            }

            const QString target = QUrl::fromEncoded(request.target).path(QUrl::FullyDecoded);
            const int slash = target.lastIndexOf('/');
            const QString name = target.mid(slash + 1);
            if (slash != QStringLiteral("/AllMusic").size() || !names.contains(name)) {
                response.status = 404;
                return response;
            }
            QFile file(QDir(root).filePath(name));
            if (!file.open(QIODevice::ReadOnly)) {
                response.status = 404;
                return response;
            }
            const QByteArray all = file.readAll();
            const QByteArray etag = tagFor(all);
            response.headers["ETag"] = etag;
            response.headers["Accept-Ranges"] = "bytes";
            if (ignoreRange || !request.headers.contains("range")
                || (request.headers.contains("if-range") && request.headers.value("if-range") != etag)) {
                response.status = 200;
                response.body = all;
                return response;
            }
            const QByteArray range = request.headers.value("range");
            const auto parts = range.startsWith("bytes=") ? range.mid(6).split('-') : QList<QByteArray>{};
            bool firstOk = false, lastOk = false;
            const qint64 first = parts.value(0).toLongLong(&firstOk);
            const qint64 last = parts.value(1).toLongLong(&lastOk);
            if (!firstOk || !lastOk || first < 0 || last < first || first >= all.size()) {
                response.status = 416;
                response.headers["Content-Range"] = "bytes */" + QByteArray::number(all.size());
                return response;
            }
            const qint64 boundedLast = qMin(last, all.size() - 1);
            response.status = 206;
            response.body = all.mid(first, boundedLast - first + 1);
            response.headers["Content-Range"] = "bytes " + QByteArray::number(first) + "-"
                + QByteArray::number(boundedLast) + "/" + QByteArray::number(all.size());
            return response;
        };
    }

    QTextStream(stdout) << "Loopback-only WebDAV: http://127.0.0.1:" << port
        << "/AllMusic/ (read-only, Ctrl+C to stop)" << Qt::endl;
    if (parser.isSet(rootOption))
        QTextStream(stdout) << names.size() << " generated test tones; Range mode: "
            << (parser.isSet(ignoreRangeOption) ? "disabled (HTTP 200 fallback test)" : "206") << Qt::endl;
    return app.exec();
}
