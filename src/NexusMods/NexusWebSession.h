#pragma once

#include <QList>
#include <QNetworkCookie>
#include <QObject>
#include <QPointer>
#include <QString>

class QWebEngineCookieStore;
class QWebEngineProfile;
class QQuickWebEngineProfile;

class NexusWebSession : public QObject
{
    Q_OBJECT

public:
    explicit NexusWebSession(QObject* parent = nullptr);
    ~NexusWebSession() override;

    Q_INVOKABLE void attachSignInProfile(QObject* profileObject);

    QWebEngineProfile* mediaProfile() const;

    void clearWebsiteSession();
    void clearGalleryCache();

    static QString mediaCacheRoot();

private:
    void relayCookieAdded(const QNetworkCookie& cookie);
    void relayCookieRemoved(const QNetworkCookie& cookie);

    static bool isNexusCookie(const QNetworkCookie& cookie);
    static bool sameCookie(const QNetworkCookie& left, const QNetworkCookie& right);

    QPointer<QQuickWebEngineProfile> m_signInProfile;
    QWebEngineProfile* m_mediaProfile = nullptr;
    QList<QNetworkCookie> m_nexusCookies;
};
