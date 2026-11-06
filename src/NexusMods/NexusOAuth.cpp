#include "NexusOAuth.h"

#include "NexusToken.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QTimer>
#include <QUrlQuery>

namespace {

constexpr char kOAuthClientId[] = "nma";
constexpr char kAuthorizeEndpoint[] = "https://users.nexusmods.com/oauth/authorize";
constexpr char kTokenEndpoint[] = "https://users.nexusmods.com/oauth/token";
constexpr char kUserInfoEndpoint[] = "https://users.nexusmods.com/oauth/userinfo";
constexpr char kRedirectUri[] = "nxm://oauth/callback";
constexpr char kScopes[] = "openid profile email";

constexpr int kSignInTimeoutMs = 3 * 60 * 1000;
constexpr int kRefreshLeadTimeMs = 60 * 1000;
constexpr int kTransientRetryDelayMs = 60 * 1000;
constexpr qint64 kDefaultTokenLifetimeSeconds = 3600;

QNetworkRequest makeOAuthRequest(const char* endpoint)
{
    QNetworkRequest request(QUrl(QString::fromLatin1(endpoint)));

    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(30000);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "UniversalModManager/1.0");
    return request;
}

}

NexusOAuth::NexusOAuth(QObject* parent)
    : QObject(parent)
    , m_tokenStore(new NexusToken())
    , m_signInTimeout(new QTimer(this))
    , m_refreshTimer(new QTimer(this))
{
    m_signInTimeout->setSingleShot(true);
    connect(m_signInTimeout, &QTimer::timeout, this, &NexusOAuth::onSignInTimeout);

    m_refreshTimer->setSingleShot(true);
    connect(m_refreshTimer, &QTimer::timeout, this, &NexusOAuth::refreshAccessToken);

    QDesktopServices::setUrlHandler(QStringLiteral("nxm"), this, "handleExternalCallbackUrl");
}

NexusOAuth::~NexusOAuth()
{
    QDesktopServices::unsetUrlHandler(QStringLiteral("nxm"));

    if (m_tokenReply) {
        m_tokenReply->abort();
        m_tokenReply = nullptr;
    }

    m_tokens.accessToken.fill(QChar('\0'));
    m_tokens.refreshToken.fill(QChar('\0'));
    m_pendingVerifier.fill(QChar('\0'));
    m_pendingState.fill(QChar('\0'));

    delete m_tokenStore;
    m_tokenStore = nullptr;
}

bool NexusOAuth::isAuthenticated() const { return m_authenticated; }
bool NexusOAuth::isSigningIn() const { return m_signingIn; }
bool NexusOAuth::isBusy() const { return m_busy; }
QString NexusOAuth::userName() const { return m_userName; }
QString NexusOAuth::userId() const { return m_userId; }
QString NexusOAuth::avatarUrl() const { return m_avatarUrl; }
bool NexusOAuth::isPremium() const { return m_premium; }
bool NexusOAuth::isSupporter() const { return m_supporter; }
QString NexusOAuth::authorizationUrl() const { return m_authorizationUrl; }
QString NexusOAuth::lastError() const { return m_lastError; }
bool NexusOAuth::adultContentAllowed() const { return m_adultContentAllowed; }
bool NexusOAuth::adultContentBlurred() const { return m_adultContentBlurred; }

QString NexusOAuth::accessTokenForRequest() const
{
    if (!m_authenticated || m_tokens.accessToken.isEmpty()) {
        return {};
    }
    if (m_tokens.expiresAtUtcMs > 0
        && m_tokens.expiresAtUtcMs <= QDateTime::currentMSecsSinceEpoch()) {
        return {};
    }
    return m_tokens.accessToken;
}

QNetworkAccessManager* NexusOAuth::network()
{
    if (!m_network) {
        m_network = new QNetworkAccessManager(this);
        m_network->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    }
    return m_network;
}

void NexusOAuth::restoreSession()
{
    if (m_authenticated || m_signingIn
        || !m_tokens.accessToken.isEmpty() || !m_tokens.refreshToken.isEmpty()) {
        return;
    }
    if (!loadPersistedTokens()) {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool accessTokenUsable = !m_tokens.accessToken.isEmpty()
        && (m_tokens.expiresAtUtcMs <= 0 || m_tokens.expiresAtUtcMs > now);

    if (accessTokenUsable) {
        setAuthenticated(true);
        scheduleTokenRefresh();
        requestUserInfo();
        requestPreferences();
        return;
    }

    if (!m_tokens.refreshToken.isEmpty()) {
        refreshAccessToken();
        return;
    }

    m_tokens = {};
    m_tokenStore->clear();
}

void NexusOAuth::beginSignIn()
{
    if (m_authenticated || m_signingIn) {
        return;
    }

    ++m_authGeneration;
    setLastError(QString());

    m_pendingVerifier = randomUrlSafeToken(64);
    m_pendingState = randomUrlSafeToken(32);
    m_callbackConsumed = false;

    QUrl authorizeUrl(QString::fromLatin1(kAuthorizeEndpoint));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("scope"), QString::fromLatin1(kScopes));
    query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    query.addQueryItem(QStringLiteral("client_id"), QString::fromLatin1(kOAuthClientId));
    query.addQueryItem(QStringLiteral("redirect_uri"), QString::fromLatin1(kRedirectUri));
    query.addQueryItem(QStringLiteral("code_challenge"), pkceChallenge(m_pendingVerifier));
    query.addQueryItem(QStringLiteral("state"), m_pendingState);
    authorizeUrl.setQuery(query);

    m_authorizationUrl = authorizeUrl.toString(QUrl::FullyEncoded);
    emit authorizationUrlChanged();

    setBusy(false);
    setSigningIn(true);
    m_signInTimeout->start(kSignInTimeoutMs);
}

bool NexusOAuth::handleCallbackUrl(const QUrl& url)
{
    if (url.scheme().compare(QStringLiteral("nxm"), Qt::CaseInsensitive) != 0
        || url.host().compare(QStringLiteral("oauth"), Qt::CaseInsensitive) != 0) {
        return false;
    }

    const QString path = url.path();
    if (path.compare(QStringLiteral("/callback"), Qt::CaseInsensitive) != 0
        && path.compare(QStringLiteral("/callback/"), Qt::CaseInsensitive) != 0) {
        return false;
    }

    if (m_callbackConsumed) {
        return true;
    }

    if (!m_signingIn || m_pendingState.isEmpty()) {
        qWarning() << "[NexusOAuth] Ignoring an authorization callback with no sign-in in progress.";
        return false;
    }

    m_callbackConsumed = true;

    QUrlQuery query(url);
    const QString returnedState = query.queryItemValue(QStringLiteral("state"), QUrl::FullyDecoded);
    if (!constantTimeEquals(returnedState, m_pendingState)) {
        qWarning() << "[NexusOAuth] Rejected an authorization callback with a mismatched state.";
        finishSignInWithError(tr("The sign-in response could not be verified. Please try again."));
        return true;
    }

    const QString error = query.queryItemValue(QStringLiteral("error"), QUrl::FullyDecoded);
    if (!error.isEmpty()) {
        qWarning() << "[NexusOAuth] Authorization was not granted. Error code:" << error;
        if (error.compare(QStringLiteral("access_denied"), Qt::CaseInsensitive) == 0) {
            finishSignInWithError(tr("Sign-in was canceled on the Nexus Mods website."));
        } else {
            finishSignInWithError(tr("Nexus Mods could not complete the sign-in. Please try again."));
        }
        return true;
    }

    const QString code = query.queryItemValue(QStringLiteral("code"), QUrl::FullyDecoded);
    if (code.isEmpty()) {
        finishSignInWithError(tr("Nexus Mods did not return an authorization code."));
        return true;
    }

    qInfo() << "[NexusOAuth] Authorization callback accepted; exchanging the code for tokens.";

    const QString verifier = m_pendingVerifier;
    m_pendingVerifier.clear();
    m_pendingState.clear();

    startTokenExchange(code, verifier);
    return true;
}

void NexusOAuth::cancelSignIn()
{
    if (!m_signingIn) {
        return;
    }

    ++m_authGeneration;
    m_signInTimeout->stop();

    if (m_tokenReply) {
        m_tokenReply->abort();
        m_tokenReply = nullptr;
    }

    clearAuthorizationRequest();
    setBusy(false);
    setSigningIn(false);
}

void NexusOAuth::signOut()
{
    ++m_authGeneration;
    m_signInTimeout->stop();
    m_refreshTimer->stop();
    m_refreshInFlight = false;

    if (m_tokenReply) {
        m_tokenReply->abort();
        m_tokenReply = nullptr;
    }

    clearAuthorizationRequest();
    m_tokens = {};
    m_tokenStore->clear();

    const bool userInfoWasPresent = clearUserInfo();
    setBusy(false);
    setSigningIn(false);
    setAuthenticated(false);

    if (userInfoWasPresent) {
        emit userInfoChanged();
    }
    emit signedOut();
    qInfo() << "[NexusOAuth] Signed out of Nexus Mods.";
}

void NexusOAuth::refreshUserInfo()
{
    requestUserInfo();
}

void NexusOAuth::handleExternalCallbackUrl(const QUrl& url)
{
    handleCallbackUrl(url);
}

void NexusOAuth::onSignInTimeout()
{
    if (!m_signingIn) {
        return;
    }
    finishSignInWithError(tr("The sign-in window timed out. Please try again."));
}

void NexusOAuth::startTokenExchange(const QString& code, const QString& verifier)
{

    m_signInTimeout->stop();
    setBusy(true);

    QNetworkRequest request = makeOAuthRequest(kTokenEndpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));

    const QString body = QStringLiteral(
        "grant_type=authorization_code&client_id=%1&redirect_uri=%2&code=%3&code_verifier=%4")
        .arg(formEncode(QString::fromLatin1(kOAuthClientId)),
             formEncode(QString::fromLatin1(kRedirectUri)),
             formEncode(code),
             formEncode(verifier));

    const quint64 generation = m_authGeneration;
    QNetworkReply* reply = network()->post(request, body.toUtf8());
    m_tokenReply = reply;

    connect(reply, &QNetworkReply::finished, this, [this, reply, generation]() {
        reply->deleteLater();
        if (m_tokenReply == reply) {
            m_tokenReply = nullptr;
        }

        if (generation != m_authGeneration || !m_signingIn) {
            return;
        }

        setBusy(false);

        const QByteArray payload = reply->readAll();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject object = QJsonDocument::fromJson(payload).object();

        if (reply->error() != QNetworkReply::NoError || statusCode < 200 || statusCode >= 300
            || object.isEmpty()) {
            qWarning() << "[NexusOAuth] Token exchange failed with HTTP status" << statusCode
                       << "and error" << reply->errorString();
            finishSignInWithError(tokenErrorMessage(reply, statusCode, object));
            return;
        }

        if (!applyTokenObject(object, false)) {
            finishSignInWithError(tr("Nexus Mods returned an invalid access token."));
            return;
        }

        m_signInTimeout->stop();
        setSigningIn(false);
        setAuthenticated(true);
        setLastError(QString());
        scheduleTokenRefresh();
        requestUserInfo();
        requestPreferences();
        qInfo() << "[NexusOAuth] Nexus Mods sign-in completed.";
    });
}

bool NexusOAuth::applyTokenObject(const QJsonObject& object, bool refreshGrant)
{
    const QString accessToken = object.value(QStringLiteral("access_token")).toString();
    if (accessToken.isEmpty()) {
        return false;
    }

    m_tokens.accessToken = accessToken;
    m_tokens.tokenType = object.value(QStringLiteral("token_type")).toString(QStringLiteral("Bearer"));
    m_tokens.scope = object.value(QStringLiteral("scope")).toString();

    const QString refreshToken = object.value(QStringLiteral("refresh_token")).toString();
    if (!refreshToken.isEmpty()) {
        m_tokens.refreshToken = refreshToken;
    } else if (!refreshGrant) {
        m_tokens.refreshToken.clear();
    }

    qint64 expiresInSeconds = object.value(QStringLiteral("expires_in")).toVariant().toLongLong();
    if (expiresInSeconds <= 0) {
        expiresInSeconds = kDefaultTokenLifetimeSeconds;
    }

    qint64 issuedAtMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 createdAtSeconds = object.value(QStringLiteral("created_at")).toVariant().toLongLong();
    if (createdAtSeconds > 0) {
        issuedAtMs = createdAtSeconds * 1000;
    }
    m_tokens.expiresAtUtcMs = issuedAtMs + expiresInSeconds * 1000;

    persistTokens();
    return true;
}

void NexusOAuth::requestUserInfo()
{
    if (!m_authenticated || m_userInfoRequestInFlight) {
        return;
    }

    const QString token = accessTokenForRequest();
    if (token.isEmpty()) {
        if (!m_tokens.refreshToken.isEmpty()) {
            refreshAccessToken();
        }
        return;
    }

    m_userInfoRequestInFlight = true;

    QNetworkRequest request = makeOAuthRequest(kUserInfoEndpoint);
    request.setRawHeader("Authorization", "Bearer " + token.toUtf8());

    QNetworkReply* reply = network()->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_userInfoRequestInFlight = false;

        if (!m_authenticated) {
            return;
        }

        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject object = QJsonDocument::fromJson(reply->readAll()).object();

        if (reply->error() == QNetworkReply::NoError && statusCode >= 200 && statusCode < 300) {
            m_userInfoRetryAfterRefresh = false;
            applyUserInfo(object);
            qInfo().noquote()
                << QStringLiteral("[NexusOAuth] Nexus Mods profile loaded (avatar available: %1, premium: %2).")
                       .arg(m_avatarUrl.isEmpty() ? QStringLiteral("no") : QStringLiteral("yes"),
                            m_premium ? QStringLiteral("yes") : QStringLiteral("no"));
            return;
        }

        if ((statusCode == 401 || statusCode == 403)
            && !m_userInfoRetryAfterRefresh
            && !m_tokens.refreshToken.isEmpty()) {

            m_userInfoRetryAfterRefresh = true;
            refreshAccessToken();
            return;
        }

        qWarning() << "[NexusOAuth] Could not load the Nexus Mods profile; HTTP status"
                   << statusCode << "and error" << reply->errorString();
    });
}

bool NexusOAuth::applyUserInfo(const QJsonObject& object)
{
    const QString userId = object.value(QStringLiteral("sub")).toString();
    const QString userName = object.value(QStringLiteral("name")).toString();
    const QString avatarUrl = object.value(QStringLiteral("avatar")).toString();

    bool premium = false;
    bool supporter = false;
    const QJsonArray roles = object.value(QStringLiteral("membership_roles")).toArray();
    for (const QJsonValue& roleValue : roles) {
        const QString role = roleValue.toString().toLower();
        if (role == QLatin1String("premium") || role == QLatin1String("lifetimepremium")) {
            premium = true;
        } else if (role == QLatin1String("supporter")) {
            supporter = true;
        }
    }

    const bool changed = m_userId != userId
        || m_userName != userName
        || m_avatarUrl != avatarUrl
        || m_premium != premium
        || m_supporter != supporter;

    m_userId = userId;
    m_userName = userName;
    m_avatarUrl = avatarUrl;
    m_premium = premium;
    m_supporter = supporter;

    if (changed) {
        emit userInfoChanged();
    }
    return true;
}

void NexusOAuth::requestPreferences()
{
    if (!m_authenticated || m_preferencesRequestInFlight) {
        return;
    }

    const QString token = accessTokenForRequest();
    if (token.isEmpty()) {
        return;
    }

    m_preferencesRequestInFlight = true;

    QNetworkRequest request(QUrl(QStringLiteral("https://api.nexusmods.com/v2/graphql")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(15000);
    request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    request.setRawHeader("User-Agent", "UniversalModManager/1.0");
    request.setRawHeader("Application-Name", "UniversalModManager");
    request.setRawHeader("Application-Version", "1.0.0");

    QJsonObject body;
    body[QStringLiteral("query")] = QStringLiteral(
        "query GetPreferences { preferences { adult adultBlurImages } }");

    QNetworkReply* reply = network()->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_preferencesRequestInFlight = false;

        if (!m_authenticated) {
            return;
        }

        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();

        if (reply->error() != QNetworkReply::NoError || statusCode < 200 || statusCode >= 300) {
            qWarning() << "[NexusOAuth] Could not load content preferences; HTTP status"
                       << statusCode << "and error" << reply->errorString();
            return;
        }

        const QJsonObject preferences = root.value(QStringLiteral("data"))
                                            .toObject()
                                            .value(QStringLiteral("preferences"))
                                            .toObject();
        if (preferences.isEmpty()) {
            qWarning() << "[NexusOAuth] Content preferences were missing from the API response.";
            return;
        }

        const bool allowed = preferences.value(QStringLiteral("adult")).toBool(m_adultContentAllowed);
        const bool blurred = preferences.value(QStringLiteral("adultBlurImages")).toBool(m_adultContentBlurred);
        const bool changed = m_adultContentAllowed != allowed || m_adultContentBlurred != blurred;

        m_adultContentAllowed = allowed;
        m_adultContentBlurred = blurred;

        if (changed) {
            emit preferencesChanged();
        }

        qInfo().noquote()
            << QStringLiteral("[NexusOAuth] Content preferences loaded (adult content: %1, blur: %2).")
                   .arg(allowed ? QStringLiteral("allowed") : QStringLiteral("hidden"),
                        blurred ? QStringLiteral("on") : QStringLiteral("off"));
    });
}

void NexusOAuth::scheduleTokenRefresh()
{
    m_refreshTimer->stop();

    if (!m_authenticated || m_tokens.refreshToken.isEmpty() || m_tokens.expiresAtUtcMs <= 0) {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 delayMs = m_tokens.expiresAtUtcMs - kRefreshLeadTimeMs - now;
    const qint64 boundedDelay = qBound<qint64>(5000, delayMs, 24LL * 60 * 60 * 1000);
    m_refreshTimer->start(static_cast<int>(boundedDelay));
}

void NexusOAuth::refreshAccessToken()
{
    if (m_refreshInFlight) {
        return;
    }

    if (m_tokens.refreshToken.isEmpty()) {
        if (m_authenticated) {
            clearSession(tr("Your Nexus Mods session has expired. Please sign in again."));
        }
        return;
    }

    m_refreshInFlight = true;
    m_refreshTimer->stop();
    setBusy(true);

    QNetworkRequest request = makeOAuthRequest(kTokenEndpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));

    const QString body = QStringLiteral(
        "grant_type=refresh_token&client_id=%1&refresh_token=%2")
        .arg(formEncode(QString::fromLatin1(kOAuthClientId)),
             formEncode(m_tokens.refreshToken));

    const quint64 generation = m_authGeneration;
    QNetworkReply* reply = network()->post(request, body.toUtf8());
    m_tokenReply = reply;

    connect(reply, &QNetworkReply::finished, this, [this, reply, generation]() {
        reply->deleteLater();
        if (m_tokenReply == reply) {
            m_tokenReply = nullptr;
        }

        if (generation != m_authGeneration) {
            return;
        }

        m_refreshInFlight = false;
        setBusy(false);

        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject object = QJsonDocument::fromJson(reply->readAll()).object();

        if (reply->error() != QNetworkReply::NoError || statusCode < 200 || statusCode >= 300
            || object.isEmpty()) {
            const QString error = object.value(QStringLiteral("error")).toString();
            const bool permanentlyRefused = statusCode == 400 || statusCode == 401
                || error == QLatin1String("invalid_grant")
                || error == QLatin1String("invalid_client");

            if (permanentlyRefused) {
                clearSession(tr("Your Nexus Mods session has expired. Please sign in again."));
            } else {

                qWarning() << "[NexusOAuth] Token refresh failed with HTTP status" << statusCode
                           << "and error" << reply->errorString();
                m_refreshTimer->start(kTransientRetryDelayMs);
            }
            return;
        }

        if (!applyTokenObject(object, true)) {
            clearSession(tr("Nexus Mods returned an invalid session. Please sign in again."));
            return;
        }

        if (!m_authenticated) {
            setAuthenticated(true);
        }

        if (m_userName.isEmpty() || m_userInfoRetryAfterRefresh) {
            requestUserInfo();
        }
        requestPreferences();

        scheduleTokenRefresh();
    });
}

QString NexusOAuth::tokenErrorMessage(QNetworkReply* reply, int statusCode,
                                                const QJsonObject& object) const
{
    const QString error = object.value(QStringLiteral("error")).toString();

    if (error == QLatin1String("invalid_grant")) {
        return tr("The sign-in code expired or was already used. Please try again.");
    }
    if (error == QLatin1String("invalid_client")
        || error == QLatin1String("unauthorized_client")) {
        return tr("Nexus Mods rejected this application's sign-in request.");
    }
    if (error == QLatin1String("invalid_scope")) {
        return tr("Nexus Mods did not grant the permissions required to sign in.");
    }
    if (statusCode == 429) {
        return tr("Nexus Mods is rate limiting sign-in requests. Please try again in a moment.");
    }
    if (statusCode >= 500) {
        return tr("Nexus Mods is temporarily unavailable. Please try again shortly.");
    }
    if (reply && reply->error() != QNetworkReply::NoError) {
        return tr("Could not reach Nexus Mods: %1").arg(reply->errorString());
    }
    return tr("Nexus Mods rejected the sign-in request (HTTP %1).").arg(statusCode);
}

void NexusOAuth::setAuthenticated(bool value)
{
    if (m_authenticated == value) {
        return;
    }
    m_authenticated = value;
    emit authenticationChanged();
}

void NexusOAuth::setSigningIn(bool value)
{
    if (m_signingIn == value) {
        return;
    }
    m_signingIn = value;
    emit signInStateChanged();
}

void NexusOAuth::setBusy(bool value)
{
    if (m_busy == value) {
        return;
    }
    m_busy = value;
    emit busyChanged();
}

void NexusOAuth::setLastError(const QString& message)
{
    if (m_lastError == message) {
        return;
    }
    m_lastError = message;
    emit lastErrorChanged();
}

void NexusOAuth::clearAuthorizationRequest()
{
    m_pendingVerifier.fill(QChar('\0'));
    m_pendingVerifier.clear();
    m_pendingState.fill(QChar('\0'));
    m_pendingState.clear();
    m_callbackConsumed = false;

    if (!m_authorizationUrl.isEmpty()) {
        m_authorizationUrl.clear();
        emit authorizationUrlChanged();
    }
}

void NexusOAuth::finishSignInWithError(const QString& message)
{
    m_signInTimeout->stop();
    ++m_authGeneration;

    if (m_tokenReply) {
        m_tokenReply->abort();
        m_tokenReply = nullptr;
    }

    clearAuthorizationRequest();
    setBusy(false);
    setSigningIn(false);
    setLastError(message);
    emit authenticationFailed(message);
}

void NexusOAuth::clearSession(const QString& reason)
{
    ++m_authGeneration;
    m_signInTimeout->stop();
    m_refreshTimer->stop();
    m_refreshInFlight = false;

    if (m_tokenReply) {
        m_tokenReply->abort();
        m_tokenReply = nullptr;
    }

    clearAuthorizationRequest();
    m_tokens = {};
    m_tokenStore->clear();

    const bool userInfoWasPresent = clearUserInfo();
    const bool wasAuthenticated = m_authenticated;
    setAuthenticated(false);
    setSigningIn(false);
    setBusy(false);

    if (userInfoWasPresent) {
        emit userInfoChanged();
    }
    if (!reason.isEmpty()) {
        setLastError(reason);
        if (wasAuthenticated) {
            emit authenticationFailed(reason);
        }
    }
}

bool NexusOAuth::clearUserInfo()
{
    const bool hadUserInfo = !m_userName.isEmpty() || !m_userId.isEmpty()
        || !m_avatarUrl.isEmpty() || m_premium || m_supporter;

    m_userName.clear();
    m_userId.clear();
    m_avatarUrl.clear();
    m_premium = false;
    m_supporter = false;
    m_userInfoRetryAfterRefresh = false;

    const bool hadCustomPreferences = !m_adultContentAllowed || m_adultContentBlurred;
    m_adultContentAllowed = true;
    m_adultContentBlurred = false;
    m_preferencesRequestInFlight = false;
    if (hadCustomPreferences) {
        emit preferencesChanged();
    }

    return hadUserInfo;
}

void NexusOAuth::persistTokens()
{
    if (m_tokens.refreshToken.isEmpty()) {
        m_tokenStore->clear();
        return;
    }

    if (!m_tokenStore->saveRefreshToken(m_tokens.refreshToken)) {
        qWarning() << "[NexusOAuth] Failed to store the Nexus Mods refresh token in Windows Credential Manager.";
    }
}

bool NexusOAuth::loadPersistedTokens()
{
    const QString refreshToken = m_tokenStore->loadRefreshToken();
    if (refreshToken.isEmpty()) {
        return false;
    }

    m_tokens.refreshToken = refreshToken;
    return true;
}

QString NexusOAuth::randomUrlSafeToken(int byteCount)
{
    QByteArray bytes(byteCount, Qt::Uninitialized);
    QRandomGenerator* generator = QRandomGenerator::system();

    for (int index = 0; index < byteCount; index += 4) {
        const quint32 value = generator->generate();
        const int remaining = qMin(4, byteCount - index);
        for (int offset = 0; offset < remaining; ++offset) {
            bytes[index + offset] = static_cast<char>((value >> (offset * 8)) & 0xFF);
        }
    }

    const QByteArray encoded = bytes.toBase64(QByteArray::Base64UrlEncoding
                                              | QByteArray::OmitTrailingEquals);
    bytes.fill('\0');
    return QString::fromLatin1(encoded);
}

QString NexusOAuth::pkceChallenge(const QString& verifier)
{
    const QByteArray digest = QCryptographicHash::hash(verifier.toLatin1(),
                                                       QCryptographicHash::Sha256);
    return QString::fromLatin1(digest.toBase64(QByteArray::Base64UrlEncoding
                                               | QByteArray::OmitTrailingEquals));
}

bool NexusOAuth::constantTimeEquals(const QString& left, const QString& right)
{
    const QByteArray leftBytes = left.toUtf8();
    const QByteArray rightBytes = right.toUtf8();

    if (leftBytes.isEmpty() || leftBytes.size() != rightBytes.size()) {
        return false;
    }

    unsigned char difference = 0;
    for (int index = 0; index < leftBytes.size(); ++index) {
        difference |= static_cast<unsigned char>(leftBytes[index] ^ rightBytes[index]);
    }
    return difference == 0;
}

QString NexusOAuth::formEncode(const QString& value)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(value));
}
