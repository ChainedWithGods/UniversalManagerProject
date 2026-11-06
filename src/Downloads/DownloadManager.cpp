#include "DownloadManager.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

DownloadManager::DownloadManager(QObject* parent)
    : QAbstractListModel(parent)
    , m_network(new QNetworkAccessManager(this))
{
    m_clock.start();
}

DownloadManager::~DownloadManager()
{
    for (Task* task : m_tasks) {
        disconnectReply(task);

        const bool incomplete = task->state == QLatin1String("downloading")
            || task->state == QLatin1String("paused")
            || task->state == QLatin1String("failed")
            || (task->state == QLatin1String("queued") && task->received > 0);
        if (incomplete && !task->filePath.isEmpty())
            QFile::remove(task->filePath);

        delete task;
    }
}

int DownloadManager::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_tasks.size();
}

QVariant DownloadManager::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tasks.size())
        return {};

    const Task* task = m_tasks.at(index.row());
    switch (role) {
    case IdRole: return task->id;
    case NameRole: return task->displayName.isEmpty() ? task->fileName : task->displayName;
    case FileNameRole: return task->fileName;
    case StateRole: return task->state;
    case ProgressRole:
        return task->total > 0
            ? qBound(0.0, double(task->received) / double(task->total), 1.0)
            : 0.0;
    case ReceivedRole: return double(task->received);
    case TotalRole: return double(task->total);
    case SpeedRole: return task->speed;
    case EtaRole: return double(task->etaSeconds);
    case ErrorRole: return task->error;
    case FilePathRole: return task->filePath;
    case DateAddedRole: return task->addedAt.toString(QStringLiteral("yyyy-MM-dd"));
    default: return {};
    }
}

QVariantList DownloadManager::snapshot() const
{
    QVariantList items;
    for (const Task* task : m_tasks) {
        if (task->state == QLatin1String("completed"))
            continue;

        QVariantMap item;
        item["id"] = task->id;
        item["name"] = task->displayName.isEmpty() ? task->fileName : task->displayName;
        item["fileName"] = task->fileName;
        item["state"] = task->state;
        item["progress"] = task->total > 0
            ? qBound(0.0, double(task->received) / double(task->total), 1.0)
            : 0.0;
        item["received"] = double(task->received);
        item["total"] = double(task->total);
        item["speed"] = task->speed;
        item["eta"] = double(task->etaSeconds);
        item["error"] = task->error;
        item["filePath"] = task->filePath;
        item["dateAdded"] = task->addedAt.toString(QStringLiteral("yyyy-MM-dd"));
        items.append(item);
    }
    return items;
}

QHash<int, QByteArray> DownloadManager::roleNames() const
{
    return {
        {IdRole, "id"},
        {NameRole, "name"},
        {FileNameRole, "fileName"},
        {StateRole, "downloadState"},
        {ProgressRole, "progress"},
        {ReceivedRole, "received"},
        {TotalRole, "total"},
        {SpeedRole, "speed"},
        {EtaRole, "eta"},
        {ErrorRole, "error"},
        {FilePathRole, "filePath"},
        {DateAddedRole, "dateAdded"},
    };
}

void DownloadManager::enqueue(const QUrl& url, const QString& fileName,
                              const QString& displayName, const QString& directory)
{
    if (!url.isValid())
        return;

    QString targetDirectory = directory;
    if (targetDirectory.isEmpty())
        targetDirectory = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (targetDirectory.isEmpty())
        return;
    QDir().mkpath(targetDirectory);

    QString safeName = fileName;
    if (safeName.isEmpty())
        safeName = QStringLiteral("Nexus-download.zip");
    safeName.replace(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*]")), QStringLiteral("_"));

    const bool startNow = !hasActiveDownload();

    auto* task = new Task;
    task->id = m_nextId++;
    task->url = url;
    task->fileName = safeName;
    task->displayName = displayName.isEmpty() ? safeName : displayName;
    task->directory = targetDirectory;
    task->filePath = uniqueFilePath(targetDirectory, safeName);
    task->addedAt = QDateTime::currentDateTime();
    task->state = startNow ? QStringLiteral("downloading") : QStringLiteral("queued");

    beginInsertRows(QModelIndex(), 0, 0);
    m_tasks.prepend(task);
    endInsertRows();
    emit countChanged();
    ++m_revision;
    emit revisionChanged();

    emit downloadStarted();
    if (startNow)
        startTask(task);
}

void DownloadManager::pause(int id)
{
    Task* task = taskById(id);
    if (!task || task->state != QLatin1String("downloading"))
        return;

    disconnectReply(task);
    task->state = QStringLiteral("paused");
    task->speed = 0.0;
    task->etaSeconds = -1;
    emitTaskChanged(task);
    scheduleNextQueued();
}

void DownloadManager::resume(int id)
{
    Task* task = taskById(id);
    if (!task || task->state != QLatin1String("paused"))
        return;

    task->error.clear();
    task->speed = 0.0;
    task->etaSeconds = -1;
    task->baseBytes = QFileInfo(task->filePath).size();
    task->received = task->baseBytes;

    if (hasActiveDownload()) {
        task->state = QStringLiteral("queued");
        emitTaskChanged(task);
        return;
    }

    startTask(task);
}

void DownloadManager::cancel(int id)
{
    const int row = rowById(id);
    if (row < 0)
        return;

    Task* task = m_tasks.at(row);
    if (task->state == QLatin1String("completed"))
        return;

    beginRemoveRows(QModelIndex(), row, row);
    disconnectReply(task);
    QFile::remove(task->filePath);
    delete task;
    m_tasks.removeAt(row);
    endRemoveRows();
    emit countChanged();
    ++m_revision;
    emit revisionChanged();
    scheduleNextQueued();
}

void DownloadManager::removeEntry(int id)
{
    const int row = rowById(id);
    if (row < 0)
        return;

    Task* task = m_tasks.at(row);
    if (task->state == QLatin1String("downloading")
        || task->state == QLatin1String("paused")
        || task->state == QLatin1String("queued"))
        return;

    beginRemoveRows(QModelIndex(), row, row);
    disconnectReply(task);
    if (task->state == QLatin1String("failed"))
        QFile::remove(task->filePath);
    delete task;
    m_tasks.removeAt(row);
    endRemoveRows();
    emit countChanged();
    ++m_revision;
    emit revisionChanged();
    scheduleNextQueued();
}

void DownloadManager::openFile(int id)
{
    const Task* task = taskById(id);
    if (task)
        QDesktopServices::openUrl(QUrl::fromLocalFile(task->filePath));
}

void DownloadManager::openFolder(int id)
{
    const Task* task = taskById(id);
    if (task)
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(task->filePath).absolutePath()));
}

DownloadManager::Task* DownloadManager::taskById(int id) const
{
    for (Task* task : m_tasks) {
        if (task->id == id)
            return task;
    }
    return nullptr;
}

int DownloadManager::rowById(int id) const
{
    for (int row = 0; row < m_tasks.size(); ++row) {
        if (m_tasks.at(row)->id == id)
            return row;
    }
    return -1;
}

bool DownloadManager::hasActiveDownload() const
{
    for (const Task* task : m_tasks) {
        if (task->state == QLatin1String("downloading"))
            return true;
    }
    return false;
}

void DownloadManager::startNextQueued()
{
    if (hasActiveDownload())
        return;

    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        Task* task = m_tasks.at(i);
        if (task->state != QLatin1String("queued"))
            continue;

        if (task->received <= 0)
            task->filePath = uniqueFilePath(task->directory, task->fileName);

        task->state = QStringLiteral("downloading");
        task->error.clear();
        task->speed = 0.0;
        task->etaSeconds = -1;
        emitTaskChanged(task);
        startTask(task);
        return;
    }
}

void DownloadManager::scheduleNextQueued()
{
    QTimer::singleShot(0, this, [this]() { startNextQueued(); });
}

void DownloadManager::startTask(Task* task)
{
    task->rangeRequested = task->baseBytes > 0;
    task->rangeChecked = false;
    task->speed = 0.0;
    task->etaSeconds = -1;
    task->error.clear();
    task->state = QStringLiteral("downloading");

    if (!task->file) {
        task->file = new QFile(task->filePath);
        const QIODevice::OpenMode mode = task->rangeRequested
            ? (QIODevice::ReadWrite | QIODevice::Append)
            : (QIODevice::WriteOnly | QIODevice::Truncate);
        if (!task->file->open(mode)) {
            delete task->file;
            task->file = nullptr;
            finishTask(task, false, QStringLiteral("Could not open the destination file."));
            return;
        }
    }

    QNetworkRequest request(task->url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    if (task->rangeRequested) {
        request.setRawHeader("Range",
                             QByteArray("bytes=") + QByteArray::number(task->baseBytes) + "-");
    }

    task->reply = m_network->get(request);
    task->sampleTimeMs = m_clock.elapsed();
    task->sampleBytes = task->received;
    task->lastNotifyMs = 0;
    emitTaskChanged(task);

    connect(task->reply, &QNetworkReply::metaDataChanged, this, [this, task]() {
        if (!task->reply)
            return;

        const int status = task->reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (task->rangeRequested && !task->rangeChecked) {
            if (status == 200 && task->file) {

                task->file->close();
                if (!task->file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    const QString message = QStringLiteral("Could not reset the destination file.");
                    disconnectReply(task);
                    finishTask(task, false, message);
                    return;
                }
                task->baseBytes = 0;
                task->received = 0;
                task->total = 0;
                task->sampleBytes = 0;
                task->rangeChecked = true;
            } else if (status == 206) {
                task->rangeChecked = true;
            }
        }

        bool ok = false;
        const qint64 length = task->reply->header(QNetworkRequest::ContentLengthHeader).toLongLong(&ok);
        if (ok && length > 0) {
            task->total = task->baseBytes + length;
            emitTaskChanged(task);
        }
    });

    connect(task->reply, &QNetworkReply::readyRead, this, [this, task]() {
        if (!task->reply || !task->file)
            return;
        const QByteArray chunk = task->reply->readAll();
        if (chunk.isEmpty())
            return;
        task->file->write(chunk);
        task->received += chunk.size();
        updateProgress(task);
    });

    connect(task->reply, &QNetworkReply::finished, this, [this, task]() {
        if (!task->reply)
            return;

        const QNetworkReply::NetworkError error = task->reply->error();
        const QString errorText = task->reply->errorString();
        task->reply->deleteLater();
        task->reply = nullptr;

        if (task->file) {
            task->file->flush();
            task->file->close();
            delete task->file;
            task->file = nullptr;
        }

        if (error != QNetworkReply::NoError) {
            const bool resumable = task->received > 0
                && (error == QNetworkReply::RemoteHostClosedError
                    || error == QNetworkReply::TimeoutError
                    || error == QNetworkReply::TemporaryNetworkFailureError
                    || error == QNetworkReply::NetworkSessionFailedError);
            if (resumable) {
                task->state = QStringLiteral("paused");
                task->speed = 0.0;
                task->etaSeconds = -1;
                task->error = errorText + QStringLiteral(" Press Resume to continue.");
                emitTaskChanged(task);
                scheduleNextQueued();
                return;
            }
            finishTask(task, false, errorText);
            return;
        }

        const qint64 finalSize = QFileInfo(task->filePath).size();
        if (task->total > 0 && finalSize < task->total) {
            task->state = QStringLiteral("paused");
            task->speed = 0.0;
            task->etaSeconds = -1;
            task->error = QStringLiteral("Connection closed early. Press Resume to finish the download.");
            emitTaskChanged(task);
            scheduleNextQueued();
            return;
        }
        if (task->total > 0 && finalSize > task->total) {
            finishTask(task, false, QStringLiteral("Downloaded size does not match the expected size. Remove and retry."));
            return;
        }

        task->state = QStringLiteral("completed");
        task->speed = 0.0;
        task->etaSeconds = -1;
        emitTaskChanged(task);
        emit downloadCompleted(task->filePath);
        scheduleNextQueued();
    });
}

bool DownloadManager::disconnectReply(Task* task)
{
    if (!task || !task->reply)
        return false;

    QObject::disconnect(task->reply, nullptr, this, nullptr);
    task->reply->abort();
    task->reply->deleteLater();
    task->reply = nullptr;

    if (task->file) {
        task->file->flush();
        task->file->close();
        delete task->file;
        task->file = nullptr;
    }
    return true;
}

void DownloadManager::updateProgress(Task* task)
{
    const qint64 now = m_clock.elapsed();
    const qint64 elapsed = now - task->sampleTimeMs;
    if (elapsed >= 300) {
        const qint64 delta = task->received - task->sampleBytes;
        task->speed = elapsed > 0 ? double(delta) * 1000.0 / double(elapsed) : 0.0;
        task->etaSeconds = (task->total > task->received && task->speed > 1.0)
            ? qint64((task->total - task->received) / task->speed)
            : -1;
        task->sampleTimeMs = now;
        task->sampleBytes = task->received;
    }

    if (now - task->lastNotifyMs >= 120) {
        task->lastNotifyMs = now;
        emitTaskChanged(task);
    }
}

void DownloadManager::emitTaskChanged(Task* task)
{
    const int row = rowById(task->id);
    if (row < 0)
        return;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx,
                     {NameRole, StateRole, ProgressRole, ReceivedRole, TotalRole,
                      SpeedRole, EtaRole, ErrorRole});
    ++m_revision;
    emit revisionChanged();
}

void DownloadManager::finishTask(Task* task, bool success, const QString& error)
{
    task->speed = 0.0;
    task->etaSeconds = -1;
    task->state = success ? QStringLiteral("completed") : QStringLiteral("failed");
    task->error = error;
    emitTaskChanged(task);
    scheduleNextQueued();
}

QString DownloadManager::uniqueFilePath(const QString& directory, const QString& fileName) const
{
    const QFileInfo info(fileName);
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix();

    QString candidate = QDir(directory).filePath(fileName);
    int counter = 1;
    while (QFileInfo::exists(candidate)) {
        const QString numbered = suffix.isEmpty()
            ? QStringLiteral("%1 (%2)").arg(base).arg(counter)
            : QStringLiteral("%1 (%2).%3").arg(base).arg(counter).arg(suffix);
        candidate = QDir(directory).filePath(numbered);
        ++counter;
    }
    return candidate;
}
