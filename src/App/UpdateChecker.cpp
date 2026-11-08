#include "UpdateChecker.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QVersionNumber>

#ifdef Q_OS_WIN
#include <windows.h>
#include <winver.h>
#endif

namespace {

constexpr const char* kReleasesApi =
    "https://api.github.com/repos/ChainedWithGods/UniversalManagerProject/releases?per_page=100";
constexpr const char* kManagerTagPrefix = "UniversalManager";

QString detectCurrentVersion()
{
#ifdef Q_OS_WIN
    const QString executablePath = QCoreApplication::applicationFilePath();
    const auto* widePath = reinterpret_cast<LPCWSTR>(executablePath.utf16());
    DWORD ignoredHandle = 0;
    const DWORD size = GetFileVersionInfoSizeW(widePath, &ignoredHandle);
    if (size == 0)
        return {};

    QByteArray buffer(static_cast<int>(size), '\0');
    if (!GetFileVersionInfoW(widePath, 0, size, buffer.data()))
        return {};

    VS_FIXEDFILEINFO* info = nullptr;
    UINT infoSize = 0;
    if (!VerQueryValueW(buffer.data(), L"\\", reinterpret_cast<LPVOID*>(&info), &infoSize)
        || !info || infoSize < sizeof(VS_FIXEDFILEINFO)) {
        return {};
    }

    return QStringLiteral("%1.%2.%3.%4")
        .arg(HIWORD(info->dwFileVersionMS))
        .arg(LOWORD(info->dwFileVersionMS))
        .arg(HIWORD(info->dwFileVersionLS))
        .arg(LOWORD(info->dwFileVersionLS));
#else
    return QCoreApplication::applicationVersion();
#endif
}

QVersionNumber parseVersion(const QString& tag)
{
    static const QRegularExpression versionPattern(QStringLiteral("(\\d+(?:\\.\\d+){0,3})"));
    const QRegularExpressionMatch match = versionPattern.match(tag);
    if (!match.hasMatch())
        return {};
    return QVersionNumber::fromString(match.captured(1));
}

QString chooseAssetUrl(const QJsonArray& assets)
{
    QString firstUrl;
    QString archiveUrl;

    for (const QJsonValue& value : assets) {
        const QJsonObject asset = value.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString().toLower();
        const QString url = asset.value(QStringLiteral("browser_download_url")).toString();
        if (url.isEmpty())
            continue;

        if (firstUrl.isEmpty())
            firstUrl = url;
        if (name.contains(QStringLiteral("setup")))
            return url;
        if (archiveUrl.isEmpty()
            && (name.endsWith(QStringLiteral(".7z")) || name.endsWith(QStringLiteral(".zip"))
                || name.endsWith(QStringLiteral(".exe")))) {
            archiveUrl = url;
        }
    }

    return archiveUrl.isEmpty() ? firstUrl : archiveUrl;
}

}

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_currentVersion(detectCurrentVersion())
{
}

void UpdateChecker::checkForUpdates()
{
    if (m_checked)
        return;
    m_checked = true;

    if (m_currentVersion.isEmpty()) {
        qWarning().noquote() << "[Update] skipped: the executable has no file version";
        return;
    }

    setState(QStringLiteral("checking"));

    QNetworkRequest request{QUrl(QString::fromLatin1(kReleasesApi))};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("UniversalManager/%1").arg(m_currentVersion));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setTransferTimeout(20000);

    m_checkReply = m_network->get(request);
    connect(m_checkReply, &QNetworkReply::finished, this, &UpdateChecker::handleCheckFinished);

    qInfo().noquote() << "[Update] checking GitHub releases; current version" << m_currentVersion;
}

void UpdateChecker::handleCheckFinished()
{
    QNetworkReply* reply = m_checkReply;
    m_checkReply = nullptr;
    if (!reply)
        return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qInfo().noquote() << "[Update] check failed:" << reply->errorString();
        setState(QStringLiteral("idle"));
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
    if (!document.isArray()) {
        qWarning().noquote() << "[Update] unexpected releases response";
        setState(QStringLiteral("idle"));
        return;
    }

    const QVersionNumber current = parseVersion(m_currentVersion);
    if (current.isNull()) {
        qWarning().noquote() << "[Update] unreadable app version:" << m_currentVersion;
        setState(QStringLiteral("idle"));
        return;
    }

    QVersionNumber bestVersion;
    QJsonObject bestRelease;
    for (const QJsonValue& value : document.array()) {
        const QJsonObject release = value.toObject();
        if (release.value(QStringLiteral("draft")).toBool()
            || release.value(QStringLiteral("prerelease")).toBool()) {
            continue;
        }

        const QString tag = release.value(QStringLiteral("tag_name")).toString().trimmed();
        if (!tag.startsWith(QLatin1String(kManagerTagPrefix), Qt::CaseInsensitive))
            continue;

        const QVersionNumber version = parseVersion(tag);
        if (version.isNull())
            continue;

        if (bestRelease.isEmpty() || version > bestVersion) {
            bestVersion = version;
            bestRelease = release;
        }
    }

    if (bestRelease.isEmpty()) {
        qInfo().noquote() << "[Update] no manager release found";
        setState(QStringLiteral("idle"));
        return;
    }

    if (bestVersion <= current) {
        qInfo().noquote() << "[Update] up to date; latest manager release" << bestVersion.toString();
        setState(QStringLiteral("idle"));
        return;
    }

    m_latestVersion = bestVersion.toString();
    m_changelog = bestRelease.value(QStringLiteral("body")).toString().trimmed();
    if (m_changelog.isEmpty())
        m_changelog = QStringLiteral("No changelog was provided for this release.");
    m_releaseUrl = bestRelease.value(QStringLiteral("html_url")).toString();
    m_downloadUrl = chooseAssetUrl(bestRelease.value(QStringLiteral("assets")).toArray());
    m_errorText.clear();

    qInfo().noquote() << "[Update] update available:" << m_latestVersion
                      << "asset:" << (m_downloadUrl.isEmpty() ? QStringLiteral("none") : m_downloadUrl);

    emit latestVersionChanged();
    emit changelogChanged();
    emit releaseUrlChanged();
    emit downloadUrlChanged();
    emit errorTextChanged();
    setState(QStringLiteral("available"));
    emit updateAvailable();
}

void UpdateChecker::startDownload()
{
    if (m_downloadReply)
        return;
    if (m_state != QStringLiteral("available") && m_state != QStringLiteral("failed"))
        return;
    if (m_downloadUrl.isEmpty()) {
        openReleasePage();
        return;
    }

    QString directory = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (directory.isEmpty())
        directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    directory += QStringLiteral("/Universal Manager Updates");
    if (!QDir().mkpath(directory)) {
        setError(QStringLiteral("Could not create the download folder."));
        setState(QStringLiteral("failed"));
        return;
    }

    QString fileName = QUrl(m_downloadUrl).fileName();
    if (fileName.isEmpty())
        fileName = QStringLiteral("UniversalManagerUpdate");

    QString filePath = directory + QLatin1Char('/') + fileName;
    if (QFile::exists(filePath)) {
        const QFileInfo existing(filePath);
        const QString baseName = existing.completeBaseName();
        const QString suffix = existing.suffix();
        for (int copy = 2; QFile::exists(filePath); ++copy) {
            const QString copyName = suffix.isEmpty()
                ? QStringLiteral("%1 (%2)").arg(baseName).arg(copy)
                : QStringLiteral("%1 (%2).%3").arg(baseName).arg(copy).arg(suffix);
            filePath = directory + QLatin1Char('/') + copyName;
        }
    }

    m_downloadFile = new QFile(filePath, this);
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        delete m_downloadFile;
        m_downloadFile = nullptr;
        setError(QStringLiteral("Could not write to the download folder."));
        setState(QStringLiteral("failed"));
        return;
    }

    m_cancelRequested = false;
    m_downloadedFilePath = filePath;
    m_bytesReceived = 0;
    m_bytesTotal = -1;
    m_downloadProgress = 0.0;
    setError(QString());
    emit downloadedFilePathChanged();
    emit downloadProgressChanged();

    QNetworkRequest request{QUrl(m_downloadUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("UniversalManager/%1").arg(m_currentVersion));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    m_downloadReply = m_network->get(request);
    connect(m_downloadReply, &QNetworkReply::readyRead, this, &UpdateChecker::writeAvailableBytes);
    connect(m_downloadReply, &QNetworkReply::downloadProgress,
            this, &UpdateChecker::updateDownloadProgress);
    connect(m_downloadReply, &QNetworkReply::finished, this, &UpdateChecker::handleDownloadFinished);

    setState(QStringLiteral("downloading"));
    qInfo().noquote() << "[Update] downloading" << m_downloadUrl << "to" << filePath;
}

void UpdateChecker::cancelDownload()
{
    if (!m_downloadReply)
        return;
    m_cancelRequested = true;
    m_downloadReply->abort();
}

void UpdateChecker::openReleasePage() const
{
    if (!m_releaseUrl.isEmpty())
        QDesktopServices::openUrl(QUrl(m_releaseUrl));
}

void UpdateChecker::openDownloadedFile() const
{
    if (!m_downloadedFilePath.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_downloadedFilePath));
}

void UpdateChecker::handleDownloadFinished()
{
    QNetworkReply* reply = m_downloadReply;
    m_downloadReply = nullptr;
    if (!reply)
        return;

    const bool cancelled = m_cancelRequested;
    const bool failed = reply->error() != QNetworkReply::NoError;
    if (!failed && m_downloadFile) {
        const QByteArray tail = reply->readAll();
        if (!tail.isEmpty())
            m_downloadFile->write(tail);
    }

    cleanupDownloadFile(cancelled || failed);

    const QString error = reply->errorString();
    reply->deleteLater();
    m_cancelRequested = false;

    if (cancelled) {
        qInfo().noquote() << "[Update] download cancelled";
        setState(QStringLiteral("available"));
        return;
    }

    if (failed) {
        qInfo().noquote() << "[Update] download failed:" << error;
        setError(error);
        setState(QStringLiteral("failed"));
        return;
    }

    m_downloadProgress = 1.0;
    emit downloadProgressChanged();
    qInfo().noquote() << "[Update] download complete:" << m_downloadedFilePath;
    setState(QStringLiteral("downloaded"));
}

void UpdateChecker::writeAvailableBytes()
{
    if (!m_downloadFile || !m_downloadReply)
        return;
    const QByteArray chunk = m_downloadReply->readAll();
    if (!chunk.isEmpty())
        m_downloadFile->write(chunk);
}

void UpdateChecker::updateDownloadProgress(qint64 received, qint64 total)
{
    m_bytesReceived = received;
    m_bytesTotal = total;
    m_downloadProgress = total > 0
        ? qBound(0.0, static_cast<qreal>(received) / static_cast<qreal>(total), 1.0)
        : 0.0;
    emit downloadProgressChanged();
}

void UpdateChecker::setState(const QString& state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
}

void UpdateChecker::setError(const QString& errorText)
{
    if (m_errorText == errorText)
        return;
    m_errorText = errorText;
    emit errorTextChanged();
}

void UpdateChecker::cleanupDownloadFile(bool removePartialFile)
{
    if (m_downloadFile) {
        m_downloadFile->close();
        delete m_downloadFile;
        m_downloadFile = nullptr;
    }

    if (removePartialFile && !m_downloadedFilePath.isEmpty()) {
        QFile::remove(m_downloadedFilePath);
        m_downloadedFilePath.clear();
        emit downloadedFilePathChanged();
    }
}
