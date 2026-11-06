#include "NexusNxmRequest.h"

#include <QStringList>
#include <QUrlQuery>

bool NexusNxmRequest::isValid() const
{
    return !gameDomain.isEmpty() && modId > 0 && fileId > 0;
}

bool NexusNxmRequest::hasDownloadKey() const
{
    return !key.isEmpty();
}

QUrl NexusNxmRequest::downloadLinkUrl() const
{
    QUrl url(QStringLiteral(
        "https://api.nexusmods.com/v1/games/%1/mods/%2/files/%3/download_link.json")
        .arg(gameDomain)
        .arg(modId)
        .arg(fileId));

    QUrlQuery query;
    if (!key.isEmpty())
        query.addQueryItem(QStringLiteral("key"), key);
    if (!expires.isEmpty())
        query.addQueryItem(QStringLiteral("expires"), expires);
    url.setQuery(query);
    return url;
}

QUrl NexusNxmRequest::fileInfoUrl() const
{
    return QUrl(QStringLiteral(
        "https://api.nexusmods.com/v1/games/%1/mods/%2/files/%3.json")
        .arg(gameDomain)
        .arg(modId)
        .arg(fileId));
}

NexusNxmRequest NexusNxmRequest::parse(const QString& rawUrl)
{
    NexusNxmRequest request;

    const QUrl url(rawUrl.trimmed());
    if (!url.isValid()
        || url.scheme().compare(QStringLiteral("nxm"), Qt::CaseInsensitive) != 0) {
        return request;
    }

    request.gameDomain = url.host().toLower();

    const QStringList segments = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    const int modsIndex = segments.indexOf(QStringLiteral("mods"));
    if (modsIndex < 0 || modsIndex + 1 >= segments.size())
        return request;

    if (request.gameDomain.isEmpty() && modsIndex > 0)
        request.gameDomain = segments.first().toLower();

    request.modId = segments.at(modsIndex + 1).toInt();

    const int filesIndex = segments.indexOf(QStringLiteral("files"), modsIndex + 2);
    if (filesIndex >= 0 && filesIndex + 1 < segments.size())
        request.fileId = segments.at(filesIndex + 1).toInt();

    const QUrlQuery query(url);
    request.key = query.queryItemValue(QStringLiteral("key"));
    request.expires = query.queryItemValue(QStringLiteral("expires"));
    request.userId = query.queryItemValue(QStringLiteral("user_id"));

    return request;
}
