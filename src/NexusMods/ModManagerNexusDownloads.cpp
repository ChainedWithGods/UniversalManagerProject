#include "ModManagerInternal.h"
#include "DownloadManager.h"
#include "NexusNxmRequest.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

void ModManager::setDownloadManager(DownloadManager* manager)
{
    if (m_downloadManager == manager)
        return;

    m_downloadManager = manager;
    if (m_downloadManager) {
        connect(m_downloadManager, &DownloadManager::downloadCompleted, this,
                [this](const QString& filePath) {
                    recordCompletedNexusDownload(filePath);
                    scanDownloadedMods();
                });
    }
}

void ModManager::handleNxmUrl(const QString& rawUrl)
{
    const NexusNxmRequest request = NexusNxmRequest::parse(rawUrl);
    if (!request.isValid())
        return;

    if (!hasNexusCredentials()) {
        m_nexusDownloadStatus = QStringLiteral("Sign in to Nexus Mods to download files.");
        emit nexusDownloadStatusChanged();
        return;
    }

    if (m_downloadsPath.isEmpty()) {
        emit downloadsPathRequired();
        return;
    }

    m_nexusDownloadStatus = QStringLiteral("Requesting Nexus download link...");
    m_nexusIsDownloading = true;
    m_nexusDownloadProgress = 0.02;
    emit nexusDownloadStatusChanged();
    emit nexusDownloadStateChanged();
    emit nexusDownloadProgressChanged();

    QNetworkRequest linkRequest(request.downloadLinkUrl());
    linkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    applyNexusAuthHeaders(linkRequest);

    QNetworkReply* reply = ensureNetworkManager()->get(linkRequest);
    connect(reply, &QNetworkReply::finished, this, [this, reply, request]() {
        reply->deleteLater();
        parseNexusRateLimitHeaders(reply);

        if (reply->error() != QNetworkReply::NoError) {
            m_nexusDownloadStatus = QStringLiteral(
                "Nexus rejected the download link (%1). Click \"Mod manager download\" again.")
                .arg(reply->errorString());
            m_nexusIsDownloading = false;
            emit nexusDownloadStatusChanged();
            emit nexusDownloadStateChanged();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonArray links = doc.isArray()
            ? doc.array()
            : doc.object().value("links").toArray();

        QString downloadUri;
        if (!links.isEmpty())
            downloadUri = links.first().toObject().value("URI").toString();

        if (downloadUri.isEmpty()) {
            m_nexusDownloadStatus = QStringLiteral(
                "Nexus did not return a download link. The link may have expired; try again from the website.");
            m_nexusIsDownloading = false;
            emit nexusDownloadStatusChanged();
            emit nexusDownloadStateChanged();
            return;
        }

        QNetworkRequest infoRequest(request.fileInfoUrl());
        infoRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        applyNexusAuthHeaders(infoRequest);

        QNetworkReply* infoReply = ensureNetworkManager()->get(infoRequest);
        connect(infoReply, &QNetworkReply::finished, this,
                [this, infoReply, downloadUri, request]() {
            infoReply->deleteLater();
            parseNexusRateLimitHeaders(infoReply);

            QString fileName;
            QString fileVersion;
            QString fileTitle;
            QString fileCategory;
            if (infoReply->error() == QNetworkReply::NoError) {
                const QJsonObject file = QJsonDocument::fromJson(infoReply->readAll()).object();
                const QString displayName = file.value("name").toString();
                fileTitle = displayName;
                fileVersion = file.value("version").toString();
                fileCategory = file.value("category_name").toString();
                fileName = file.value("file_name").toString();
                if (fileName.isEmpty() && !displayName.isEmpty()) {
                    const QString extension = file.value("detected_file_extension").toString();
                    fileName = extension.isEmpty()
                        ? displayName
                        : displayName + QLatin1Char('.') + extension;
                }
            }

            if (fileName.isEmpty())
                fileName = QUrl(downloadUri).fileName();
            if (fileName.isEmpty())
                fileName = QStringLiteral("Nexus-mod-%1-%2.zip").arg(request.modId).arg(request.fileId);

            const bool isUpdate =
                isNexusUpdateDownload(request.gameDomain, request.modId, request.fileId);
            queueNexusDownloadProvenance(request.gameDomain, request.modId, request.fileId,
                                         fileName, fileTitle, fileVersion, fileCategory);
            startDownloadFromDirectUrl(QUrl(downloadUri), fileName,
                                       fileTitle.isEmpty() ? fileName : fileTitle, isUpdate);
        });
    });
}
