#include "TutorialDownloader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>

TutorialDownloader::TutorialDownloader(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

QString TutorialDownloader::resolveVideo(const QString& relativePath) const
{
    if (relativePath.trimmed().isEmpty())
        return {};

    const QString installed = installedPathFor(relativePath);
    if (QFileInfo::exists(installed))
        return QDir::toNativeSeparators(installed);

    const QString downloaded = downloadPathFor(relativePath);
    if (QFileInfo::exists(downloaded))
        return QDir::toNativeSeparators(downloaded);

    return {};
}

void TutorialDownloader::download(const QString& relativePath, const QUrl& url)
{
    if (m_reply)
        return;

    if (relativePath.trimmed().isEmpty() || url.isEmpty()) {
        fail(QStringLiteral("The guide download address is missing."));
        return;
    }

    const QString existing = resolveVideo(relativePath);
    if (!existing.isEmpty()) {
        m_currentRelativePath = relativePath;
        m_localPath = existing;
        m_errorText.clear();
        emit currentRelativePathChanged();
        emit localPathChanged();
        emit errorTextChanged();
        setState(QStringLiteral("downloaded"));
        emit downloadCompleted(relativePath, existing);
        return;
    }

    m_currentRelativePath = relativePath;
    m_localPath = downloadPathFor(relativePath);
    m_errorText.clear();
    m_progress = 0.0;
    m_bytesReceived = 0;
    m_bytesTotal = -1;
    m_cancelled = false;
    emit currentRelativePathChanged();
    emit localPathChanged();
    emit errorTextChanged();
    emit progressChanged();

    const QString folder = QFileInfo(m_localPath).absolutePath();
    if (!QDir().mkpath(folder)) {
        fail(QStringLiteral("Could not create the guide folder: %1")
                 .arg(QDir::toNativeSeparators(folder)));
        return;
    }

    m_file = new QSaveFile(m_localPath, this);
    if (!m_file->open(QIODevice::WriteOnly)) {
        const QString fileError = m_file->errorString();
        delete m_file;
        m_file = nullptr;
        fail(QStringLiteral("Could not create the guide file: %1").arg(fileError));
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("UniversalManager-Tutorials/1.0"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, [this]() {
        if (m_file && m_reply)
            m_file->write(m_reply->readAll());
    });
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        m_bytesReceived = received;
        m_bytesTotal = total;
        m_progress = total > 0
            ? qBound(0.0, static_cast<qreal>(received) / static_cast<qreal>(total), 1.0)
            : 0.0;
        emit progressChanged();
    });
    connect(m_reply, &QNetworkReply::finished, this, &TutorialDownloader::handleFinished);

    setState(QStringLiteral("downloading"));
    qInfo().noquote() << "[Tutorials] downloading" << relativePath << "from" << url.toString();
}

void TutorialDownloader::cancel()
{
    if (!m_reply)
        return;
    m_cancelled = true;
    m_reply->abort();
}

void TutorialDownloader::handleFinished()
{
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    if (!reply)
        return;

    const bool cancelled = m_cancelled;
    const bool failed = reply->error() != QNetworkReply::NoError;
    if (!failed && m_file) {
        const QByteArray tail = reply->readAll();
        if (!tail.isEmpty())
            m_file->write(tail);
    }

    const QString error = reply->errorString();
    reply->deleteLater();
    m_cancelled = false;

    if (cancelled || failed) {
        cleanupTransfer();
        m_localPath.clear();
        emit localPathChanged();
        if (cancelled) {
            setState(QStringLiteral("idle"));
        } else {
            setError(error);
            setState(QStringLiteral("failed"));
        }
        return;
    }

    if (!m_file || !m_file->commit()) {
        const QString commitError = m_file
            ? m_file->errorString()
            : QStringLiteral("The guide file is not available.");
        cleanupTransfer();
        fail(QStringLiteral("Could not save the guide: %1").arg(commitError));
        return;
    }

    delete m_file;
    m_file = nullptr;
    m_progress = 1.0;
    emit progressChanged();
    setState(QStringLiteral("downloaded"));
    emit downloadCompleted(m_currentRelativePath, m_localPath);
    qInfo().noquote() << "[Tutorials] downloaded" << m_localPath;
}

void TutorialDownloader::fail(const QString& error)
{
    cleanupTransfer();
    setError(error);
    setState(QStringLiteral("failed"));
    qWarning().noquote() << "[Tutorials]" << error;
}

void TutorialDownloader::cleanupTransfer()
{
    if (m_file) {
        m_file->cancelWriting();
        delete m_file;
        m_file = nullptr;
    }
}

QString TutorialDownloader::installedPathFor(const QString& relativePath) const
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(relativePath);
}

QString TutorialDownloader::downloadPathFor(const QString& relativePath) const
{
    QString relative = relativePath;
    relative.replace(QLatin1Char('\\'), QLatin1Char('/'));

    const QString prefix = QStringLiteral("assets/Tutorials_Guides/");
    if (relative.startsWith(prefix, Qt::CaseInsensitive))
        relative = relative.mid(prefix.size());

    QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (root.isEmpty())
        root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

    return QDir(root).filePath(QStringLiteral("Tutorials_Guides/") + relative);
}

void TutorialDownloader::setState(const QString& state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
}

void TutorialDownloader::setError(const QString& error)
{
    if (m_errorText == error)
        return;
    m_errorText = error;
    emit errorTextChanged();
}
