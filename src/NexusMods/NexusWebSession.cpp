#include "NexusWebSession.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QtWebEngineQuick/qquickwebengineprofile.h>

namespace {

constexpr char kChromeUserAgent[] =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36";

}

NexusWebSession::NexusWebSession(QObject* parent)
    : QObject(parent)
{
    const QString root = mediaCacheRoot();
    QDir().mkpath(root + QStringLiteral("/profile"));
    QDir().mkpath(root + QStringLiteral("/cache"));

    m_mediaProfile = new QWebEngineProfile(QStringLiteral("UniversalManagerNexusMedia"), this);
    m_mediaProfile->setPersistentStoragePath(root + QStringLiteral("/profile"));
    m_mediaProfile->setCachePath(root + QStringLiteral("/cache"));
    m_mediaProfile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
    m_mediaProfile->setHttpCacheMaximumSize(64 * 1024 * 1024);
    m_mediaProfile->setHttpUserAgent(QString::fromLatin1(kChromeUserAgent));
    m_mediaProfile->setHttpAcceptLanguage(QStringLiteral("en-US,en;q=0.9"));
    m_mediaProfile->setSpellCheckEnabled(false);

    QWebEngineSettings* settings = m_mediaProfile->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::AutoLoadImages, false);
    settings->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
    settings->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, false);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
}

NexusWebSession::~NexusWebSession() = default;

void NexusWebSession::attachSignInProfile(QObject* profileObject)
{
    auto* profile = qobject_cast<QQuickWebEngineProfile*>(profileObject);
    if (!profile || m_signInProfile == profile) {
        return;
    }

    if (m_signInProfile) {
        disconnect(m_signInProfile->cookieStore(), nullptr, this, nullptr);
    }

    profile->setHttpUserAgent(QString::fromLatin1(kChromeUserAgent));
    profile->setHttpAcceptLanguage(QStringLiteral("en-US,en;q=0.9"));
    profile->setSpellCheckEnabled(false);
    profile->setPersistentCookiesPolicy(QQuickWebEngineProfile::ForcePersistentCookies);
    profile->setHttpCacheType(QQuickWebEngineProfile::DiskHttpCache);

    m_signInProfile = profile;

    connect(profile->cookieStore(), &QWebEngineCookieStore::cookieAdded,
            this, &NexusWebSession::relayCookieAdded);
    connect(profile->cookieStore(), &QWebEngineCookieStore::cookieRemoved,
            this, &NexusWebSession::relayCookieRemoved);
}

QWebEngineProfile* NexusWebSession::mediaProfile() const
{
    return m_mediaProfile;
}

QString NexusWebSession::mediaCacheRoot()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::currentPath();
    }
    return base + QStringLiteral("/nexus_media");
}

bool NexusWebSession::isNexusCookie(const QNetworkCookie& cookie)
{
    const QString domain = cookie.domain().toLower();
    return domain == QLatin1String("nexusmods.com")
        || domain.endsWith(QLatin1String(".nexusmods.com"));
}

bool NexusWebSession::sameCookie(const QNetworkCookie& left, const QNetworkCookie& right)
{
    return left.name() == right.name()
        && left.domain().compare(right.domain(), Qt::CaseInsensitive) == 0
        && left.path() == right.path();
}

void NexusWebSession::relayCookieAdded(const QNetworkCookie& cookie)
{
    if (!isNexusCookie(cookie)) {
        return;
    }

    bool replaced = false;
    for (int index = 0; index < m_nexusCookies.size(); ++index) {
        if (sameCookie(m_nexusCookies.at(index), cookie)) {
            m_nexusCookies[index] = cookie;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        m_nexusCookies.append(cookie);
    }

    m_mediaProfile->cookieStore()->setCookie(cookie);

    if (cookie.name() == QByteArrayLiteral("nexusmods_session")) {

        clearGalleryCache();
        qInfo() << "[NexusWebSession] Nexus Mods website session cookie captured for gallery access.";
    }
}

void NexusWebSession::relayCookieRemoved(const QNetworkCookie& cookie)
{
    if (!isNexusCookie(cookie)) {
        return;
    }

    for (int index = 0; index < m_nexusCookies.size(); ++index) {
        if (sameCookie(m_nexusCookies.at(index), cookie)) {
            m_nexusCookies.removeAt(index);
            break;
        }
    }

    m_mediaProfile->cookieStore()->deleteCookie(cookie);
}

void NexusWebSession::clearWebsiteSession()
{
    m_nexusCookies.clear();

    if (m_signInProfile) {
        m_signInProfile->cookieStore()->deleteAllCookies();
    }
    if (m_mediaProfile) {
        m_mediaProfile->cookieStore()->deleteAllCookies();
        m_mediaProfile->clearHttpCache();
    }

    clearGalleryCache();
}

void NexusWebSession::clearGalleryCache()
{
    QDir cacheDirectory(mediaCacheRoot());
    const QStringList cachedListings = cacheDirectory.entryList(
        {QStringLiteral("*.json")}, QDir::Files);
    for (const QString& fileName : cachedListings) {
        QFile::remove(cacheDirectory.filePath(fileName));
    }
}
