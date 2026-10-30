#include "MailWebDavController.h"

MailWebDavController::MailWebDavController(QObject *parent) : QObject(parent)
{
}

MailWebDavController::~MailWebDavController()
{
    dropSession();
}

bool MailWebDavController::checkConnection(QString email, QString appPassword)
{
    if (m_busy || m_connected || email.isEmpty() || appPassword.isEmpty()) {
        email.fill(QChar(u'\0'));
        appPassword.fill(QChar(u'\0'));
        return false;
    }

    dropSession();
    m_entries.clear();
    emit entriesChanged();
    m_probe = new MailWebDavProbe(this);
    connect(m_probe, &MailWebDavProbe::completed, this,
            &MailWebDavController::handleCompleted);
    m_busy = true;
    setStatus(QStringLiteral("Проверяется только каталог /AllMusic…"));

    const bool started = m_probe->listAllMusic(email, appPassword);
    email.fill(QChar(u'\0'));
    appPassword.fill(QChar(u'\0'));
    if (!started) {
        m_busy = false;
        dropSession();
        setStatus(QStringLiteral("Не удалось начать проверку. Отключитесь и повторите попытку."));
        return false;
    }
    emit stateChanged();
    return true;
}

QVariantList MailWebDavController::entries() const
{
    QVariantList model;
    model.reserve(m_entries.size());
    for (const auto &entry : m_entries) {
        QVariantMap row;
        row.insert(QStringLiteral("label"), displayName(entry.path, entry.directory));
        row.insert(QStringLiteral("isDirectory"), entry.directory);
        row.insert(QStringLiteral("isAudio"), !entry.directory && isAudio(entry.path));
        row.insert(QStringLiteral("size"), entry.size ? QVariant::fromValue(*entry.size) : QVariant());
        model.push_back(row);
    }
    return model;
}

bool MailWebDavController::isAudio(const QString &path)
{
    const QString lower = path.toLower();
    return lower.endsWith(QStringLiteral(".mp3"))
        || lower.endsWith(QStringLiteral(".flac"))
        || lower.endsWith(QStringLiteral(".aac"))
        || lower.endsWith(QStringLiteral(".m4a"))
        || lower.endsWith(QStringLiteral(".mp4"));
}

QString MailWebDavController::displayName(const QString &path, bool directory)
{
    QString cleanPath = path;
    if (cleanPath.endsWith(u'/'))
        cleanPath.chop(1);
    const int slash = cleanPath.lastIndexOf(u'/');
    QString name = cleanPath.mid(slash + 1);
    if (directory)
        name += QStringLiteral("  · папка");
    else
        name += QStringLiteral("  · аудио");
    return name;
}

bool MailWebDavController::canCheckRange(int index) const
{
    if (!m_connected || m_busy || index < 0 || index >= m_entries.size())
        return false;
    const auto &entry = m_entries.at(index);
    return !entry.directory && isAudio(entry.path) && (!entry.size || *entry.size > 0);
}

void MailWebDavController::checkRange(int index)
{
    if (!canCheckRange(index) || !m_probe)
        return;
    m_busy = true;
    setStatus(QStringLiteral("Проверяется Range выбранного аудиофайла; предел — 64 КиБ…"));
    if (!m_probe->checkRange(m_entries.at(index))) {
        m_busy = false;
        setStatus(QStringLiteral("Этот элемент нельзя проверить через Range."));
    }
    emit stateChanged();
}

void MailWebDavController::cancel()
{
    if (m_probe && m_busy)
        m_probe->cancel();
}

void MailWebDavController::disconnectCloud()
{
    dropSession();
    m_entries.clear();
    m_busy = false;
    m_connected = false;
    emit entriesChanged();
    setStatus(QStringLiteral("Отключено. Временная сессия и учётные данные удалены."));
}

void MailWebDavController::dropSession()
{
    if (!m_probe)
        return;
    MailWebDavProbe *old = m_probe;
    m_probe = nullptr;
    QObject::disconnect(old, nullptr, this, nullptr);
    old->cancel();
    old->clearCredentials();
    old->deleteLater();
}

void MailWebDavController::setStatus(const QString &status)
{
    m_status = status;
    emit stateChanged();
}

QString MailWebDavController::errorText(MailWebDavProbe::Error error, int status)
{
    using Error = MailWebDavProbe::Error;
    switch (error) {
    case Error::InvalidCredentials:
        return QStringLiteral("Введите адрес почты и пароль внешнего приложения Mail. Основной пароль почты не подходит.");
    case Error::UnsafePath:
        return QStringLiteral("Ответ WebDAV содержит небезопасный путь. Запрос остановлен.");
    case Error::Auth:
        return QStringLiteral("Ошибка авторизации (HTTP 401). Проверьте адрес и пароль внешнего приложения.");
    case Error::Forbidden:
        return QStringLiteral("Доступ запрещён (HTTP 403). Проверьте права доступа в Облаке Mail.");
    case Error::NotFound:
        return QStringLiteral("Папка /AllMusic не найдена (HTTP 404). Проверка остановлена; папки не создаются и поиск не выполняется.");
    case Error::Redirect:
        return QStringLiteral("Сервер прислал перенаправление. Оно не выполнялось; проверьте адрес WebDAV.");
    case Error::RangeUnsupported:
        return status == 200
            ? QStringLiteral("HTTP 200 вместо Range 206. Сервер не подтвердил диапазоны; полный файл не скачивался.")
            : QStringLiteral("Сервер не принял диапазон (HTTP %1). Полный файл не скачивался.").arg(status);
    case Error::VersionChanged:
        return status == 200
            ? QStringLiteral("HTTP 200 при Range/If-Range: сервер не подтвердил диапазон или версия файла изменилась. Полный файл не скачивался.")
            : QStringLiteral("Размер или версия файла не совпали с ответом PROPFIND. Полный файл не скачивался.");
    case Error::RateLimited:
        return QStringLiteral("Сервер ограничил запросы (HTTP 429). Автоматических повторов нет.");
    case Error::Server:
        return QStringLiteral("Ошибка сервера Mail (HTTP %1). Попробуйте позже.").arg(status);
    case Error::Network:
        return QStringLiteral("Сетевая ошибка при проверке Cloud Mail.");
    case Error::Timeout:
        return QStringLiteral("Время ожидания ответа истекло. Проверку можно повторить вручную.");
    case Error::Cancelled:
        return QStringLiteral("Проверка отменена.");
    case Error::TooLarge:
        return QStringLiteral("Ответ сервера превысил установленный лимит; он остановлен.");
    case Error::Protocol:
        return QStringLiteral("Ответ WebDAV не прошёл проверку формата или диапазона.");
    case Error::Tls:
        return QStringLiteral("Ошибка проверки TLS-сертификата. Сертификаты не игнорировались; пароль не отправлен.");
    case Error::Http:
        return QStringLiteral("Неожиданный HTTP-ответ (%1).").arg(status);
    case Error::Busy:
        return QStringLiteral("Другая проверка уже выполняется.");
    case Error::None:
        break;
    }
    return QStringLiteral("Проверка завершилась без результата.");
}

QString MailWebDavController::rangeDiagnosticText(const MailWebDavProbe::Result &result)
{
    using Issue = MailWebDavProbe::RangeIssue;
    QString issue;
    switch (result.rangeIssue) {
    case Issue::None:
        issue = result.error == MailWebDavProbe::Error::None
            ? QStringLiteral("диапазон подтверждён") : errorText(result.error, result.status);
        break;
    case Issue::RangeIgnored:
        issue = QStringLiteral("сервер вернул 200 вместо 206; Range не подтверждён");
        break;
    case Issue::RangeUnsatisfiable:
        issue = QStringLiteral("сервер отверг диапазон (416)");
        break;
    case Issue::MissingContentRange:
        issue = QStringLiteral("в ответе 206 отсутствует Content-Range");
        break;
    case Issue::InvalidContentRange:
        issue = QStringLiteral("Content-Range имеет неподдерживаемый формат");
        break;
    case Issue::InvalidRangeBounds:
        issue = QStringLiteral("границы диапазона противоречат друг другу");
        break;
    case Issue::RangeStartMismatch:
        issue = QStringLiteral("начало Content-Range не совпадает с запрошенным байтом 0");
        break;
    case Issue::RangeEndMismatch:
        issue = QStringLiteral("конец Content-Range не совпадает с запрошенной длиной");
        break;
    case Issue::TotalSizeMismatch:
        issue = QStringLiteral("полный размер из Content-Range отличается от PROPFIND");
        break;
    case Issue::InvalidContentLength:
        issue = QStringLiteral("Content-Length присутствует, но не является корректным числом");
        break;
    case Issue::ContentLengthMismatch:
        issue = QStringLiteral("Content-Length не совпадает с длиной Content-Range");
        break;
    case Issue::BodyLengthMismatch:
        issue = QStringLiteral("число полученных байтов не совпадает с Content-Range");
        break;
    case Issue::UnsupportedContentEncoding:
        issue = QStringLiteral("Content-Encoding меняет представление запрошенных байтов");
        break;
    case Issue::UnsupportedTransferEncoding:
        issue = QStringLiteral("Transfer-Encoding не поддержан диагностикой");
        break;
    case Issue::ConflictingMessageFraming:
        issue = QStringLiteral("одновременно заданы Transfer-Encoding и Content-Length");
        break;
    case Issue::EtagMismatch:
        issue = QStringLiteral("ответный ETag не совпал с отправленным If-Range");
        break;
    case Issue::IncompleteTransfer:
        issue = QStringLiteral("Qt Network сообщил о неполном или прерванном теле ответа");
        break;
    case Issue::TooLarge:
        issue = QStringLiteral("ответ превысил предел запрошенного диапазона");
        break;
    }

    const QString contentRangeText = !result.contentRangePresent
        ? QStringLiteral("не указан")
        : result.contentRangeUnsatisfied && result.totalSize
            ? QStringLiteral("bytes */%1").arg(*result.totalSize)
        : result.rangeFirst && result.rangeLast
            ? QStringLiteral("bytes %1-%2/%3")
                .arg(*result.rangeFirst)
                .arg(*result.rangeLast)
                .arg(result.totalSize ? QString::number(*result.totalSize) : QStringLiteral("*"))
            : QStringLiteral("присутствует, формат не распознан");

    const QString contentLengthText = !result.contentLengthPresent
        ? QStringLiteral("не указан")
        : result.declaredContentLength
            ? QStringLiteral("%1 байт").arg(*result.declaredContentLength)
            : QStringLiteral("есть, числовой формат не распознан");
    const QString transferEncodingText = !result.transferEncodingPresent
        ? QStringLiteral("не указан; границу сообщения проверил Qt Network")
        : result.transferEncodingChunked ? QStringLiteral("chunked")
            : QStringLiteral("есть, значение не поддержано");
    const QString contentEncodingText = !result.contentEncodingPresent
        ? QStringLiteral("не указан, трактуется как identity")
        : result.contentEncodingIdentity ? QStringLiteral("identity")
            : QStringLiteral("есть, не identity");
    QString etagText = !result.etagPresent
        ? QStringLiteral("не указан")
        : result.strongEtag ? QStringLiteral("сильный")
            : QStringLiteral("слабый или невалидный");
    QString ifRangeText = result.ifRangeSent
        ? QStringLiteral("отправлен") : QStringLiteral("не отправлялся");
    if (result.ifRangeSent && result.etagPresent)
        ifRangeText += result.etagMatchesIfRange
            ? QStringLiteral("; ответный ETag совпал")
            : QStringLiteral("; ответный ETag не совпал");

    const qint64 expectedBytes = result.rangeExpectedBytes.value_or(result.requestedRangeBytes);
    const QString statusText = result.status > 0
        ? QStringLiteral("HTTP %1").arg(result.status)
        : QStringLiteral("HTTP статус отсутствует");
    const QString receivedText = result.bytesReceivedIsLowerBound
        ? QStringLiteral("не менее %1 байт").arg(result.bytesReceived)
        : QStringLiteral("%1 байт").arg(result.bytesReceived);
    return QStringLiteral("%1; причина: %2. Content-Range: %3; Content-Length: %4; "
                          "Transfer-Encoding: %5; получено: %6 из %7 байт; "
                          "Content-Encoding: %8; ETag: %9; If-Range: %10.")
        .arg(statusText)
        .arg(issue)
        .arg(contentRangeText)
        .arg(contentLengthText)
        .arg(transferEncodingText)
        .arg(receivedText)
        .arg(expectedBytes)
        .arg(contentEncodingText)
        .arg(etagText)
        .arg(ifRangeText);
}

void MailWebDavController::handleCompleted(const MailWebDavProbe::Result &result)
{
    m_busy = false;
    if (result.listing) {
        if (result.error == MailWebDavProbe::Error::None) {
            m_entries.clear();
            for (const auto &entry : result.entries) {
                if (entry.directory || isAudio(entry.path))
                    m_entries.push_back(entry);
            }
            m_connected = true;
            QString text = QStringLiteral("PROPFIND завершён: %1 непосредственных элементов /AllMusic. Список хранится только локально.")
                .arg(m_entries.size());
            if (result.rejectedEntries > 0)
                text += QStringLiteral(" Отброшено небезопасных ответов: %1.").arg(result.rejectedEntries);
            setStatus(text);
            emit entriesChanged();
            emit stateChanged();
            return;
        }
        const QString message = errorText(result.error, result.status);
        dropSession();
        m_connected = false;
        m_entries.clear();
        emit entriesChanged();
        setStatus(message);
        return;
    }

    const QString message = result.rangeCheck
        ? rangeDiagnosticText(result) : errorText(result.error, result.status);
    if (result.error == MailWebDavProbe::Error::Auth || result.error == MailWebDavProbe::Error::Tls) {
        dropSession();
        m_connected = false;
        m_entries.clear();
        emit entriesChanged();
    }
    setStatus(message);
    emit stateChanged();
}
