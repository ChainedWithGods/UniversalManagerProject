#pragma once

#include <QString>
#include <QUrl>

struct NexusNxmRequest
{
    QString gameDomain;
    int modId = 0;
    int fileId = 0;
    QString key;
    QString expires;
    QString userId;

    bool isValid() const;
    bool hasDownloadKey() const;
    QUrl downloadLinkUrl() const;
    QUrl fileInfoUrl() const;

    static NexusNxmRequest parse(const QString& rawUrl);
};
