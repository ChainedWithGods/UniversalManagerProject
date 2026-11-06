#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QJsonObject;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class NexusToken;

class NexusOAuth : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool authenticated READ isAuthenticated NOTIFY authenticationChanged)
    Q_PROPERTY(bool signingIn READ isSigningIn NOTIFY signInStateChanged)
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString userName READ userName NOTIFY userInfoChanged)
    Q_PROPERTY(QString userId READ userId NOTIFY userInfoChanged)
    Q_PROPERTY(QString avatarUrl READ avatarUrl NOTIFY userInfoChanged)
    Q_PROPERTY(bool premium READ isPremium NOTIFY userInfoChanged)
    Q_PROPERTY(bool supporter READ isSupporter NOTIFY userInfoChanged)
    Q_PROPERTY(QString authorizationUrl READ authorizationUrl NOTIFY authorizationUrlChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool adultContentAllowed READ adultContentAllowed NOTIFY preferencesChanged)
    Q_PROPERTY(bool adultContentBlurred READ adultContentBlurred NOTIFY preferencesChanged)

public:
    explicit NexusOAuth(QObject* parent = nullptr);
    ~NexusOAuth() override;

    bool isAuthenticated() const;
    bool isSigningIn() const;
    bool isBusy() const;
    QString userName() const;
    QString userId() const;
    QString avatarUrl() const;
    bool isPremium() const;
    bool isSupporter() const;
    QString authorizationUrl() const;
    QString lastError() const;
    bool adultContentAllowed() const;
    bool adultContentBlurred() const;

    QString accessTokenForRequest() const;

    void restoreSession();

public slots:
    void beginSignIn();
    bool handleCallbackUrl(const QUrl& url);
    void cancelSignIn();
    void signOut();
    void refreshUserInfo();

signals:
    void authenticationChanged();
    void signedOut();
    void signInStateChanged();
    void busyChanged();
    void userInfoChanged();
    void authorizationUrlChanged();
    void lastErrorChanged();
    void preferencesChanged();
    void authenticationFailed(const QString& message);

private slots:

    void handleExternalCallbackUrl(const QUrl& url);
    void onSignInTimeout();
    void refreshAccessToken();

private:
    struct Tokens {
        QString accessToken;
        QString refreshToken;
        QString tokenType;
        QString scope;
        qint64 expiresAtUtcMs = 0;
    };

    QNetworkAccessManager* network();
    void startTokenExchange(const QString& code, const QString& verifier);
    bool applyTokenObject(const QJsonObject& object, bool refreshGrant);
    void requestUserInfo();
    bool applyUserInfo(const QJsonObject& object);
    void requestPreferences();
    void scheduleTokenRefresh();
    void persistTokens();
    bool loadPersistedTokens();
    QString tokenErrorMessage(QNetworkReply* reply, int statusCode, const QJsonObject& object) const;

    void setAuthenticated(bool value);
    void setSigningIn(bool value);
    void setBusy(bool value);
    void setLastError(const QString& message);
    void clearAuthorizationRequest();
    void finishSignInWithError(const QString& message);
    void clearSession(const QString& reason);
    bool clearUserInfo();

    static QString randomUrlSafeToken(int byteCount);
    static QString pkceChallenge(const QString& verifier);
    static bool constantTimeEquals(const QString& left, const QString& right);
    static QString formEncode(const QString& value);

    QNetworkAccessManager* m_network = nullptr;
    QNetworkReply* m_tokenReply = nullptr;
    QTimer* m_signInTimeout = nullptr;
    QTimer* m_refreshTimer = nullptr;
    NexusToken* m_tokenStore = nullptr;

    Tokens m_tokens;
    bool m_authenticated = false;
    bool m_signingIn = false;
    bool m_busy = false;
    bool m_refreshInFlight = false;
    bool m_userInfoRequestInFlight = false;
    bool m_preferencesRequestInFlight = false;
    bool m_userInfoRetryAfterRefresh = false;
    bool m_callbackConsumed = false;
    quint64 m_authGeneration = 0;

    QString m_pendingVerifier;
    QString m_pendingState;
    QString m_authorizationUrl;
    QString m_lastError;

    QString m_userName;
    QString m_userId;
    QString m_avatarUrl;
    bool m_premium = false;
    bool m_supporter = false;
    bool m_adultContentAllowed = true;
    bool m_adultContentBlurred = false;
};
