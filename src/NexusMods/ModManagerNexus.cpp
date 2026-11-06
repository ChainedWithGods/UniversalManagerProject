#include "ModManagerInternal.h"
#include "DownloadManager.h"
#include "NexusMedia.h"
#include "NexusOAuth.h"
#include "NexusWebSession.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDesktopServices>
#include <QHash>
#include <QRegularExpression>
#include <QTimer>
#include <QUrlQuery>
#include <QDateTime>
#include <algorithm>

using namespace mmdetail;

QNetworkAccessManager* ModManager::ensureNetworkManager() {
    if (!m_networkManager) {
        m_networkManager = new QNetworkAccessManager(this);
    }
    return m_networkManager;
}

void ModManager::applyNexusAuthHeaders(QNetworkRequest& request) const {
    request.setRawHeader("Application-Name", "UniversalModManager");
    request.setRawHeader("Application-Version", "1.0.0");

    const QString oauthToken = m_nexusOAuth ? m_nexusOAuth->accessTokenForRequest() : QString();
    if (!oauthToken.isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + oauthToken.toUtf8());
        return;
    }

    const QString apiKey = m_nexusApiKey.trimmed();
    if (!apiKey.isEmpty()) {
        request.setRawHeader("apikey", apiKey.toUtf8());
    }
}

bool ModManager::hasNexusCredentials() const {
    if (m_nexusOAuth && m_nexusOAuth->isAuthenticated()) {
        return true;
    }
    return !m_nexusApiKey.trimmed().isEmpty();
}

void ModManager::onNexusOAuthAuthenticationChanged() {
    if (!m_nexusOAuth) return;

    if (m_nexusOAuth->isAuthenticated()) {
        m_nexusValidationStatus = "valid";
        m_nexusValidationMessage = "Signed in securely with Nexus Mods.";
        emit nexusValidationStatusChanged();
        emit nexusValidationMessageChanged();

        onNexusOAuthUserInfoChanged();

        if (m_nexusEnabled) {
            fetchNexusMods(0);
        } else {
            fetchNexusMetadata();
        }
        return;
    }

    if (m_nexusOAuth->isSigningIn()) {
        return;
    }

    if (m_nexusMedia) {
        m_nexusMedia->cancel();
    }

    if (!m_nexusUsername.isEmpty() || m_nexusIsPremium || !m_nexusAvatarUrl.isEmpty()) {
        m_nexusUsername.clear();
        m_nexusIsPremium = false;
        m_nexusAvatarUrl = "";
        emit nexusUserInfoChanged();
    }

    if (!m_nexusApiKey.trimmed().isEmpty()) {
        validateNexusApiKey();
    } else {
        clearNexusCatalog();
        m_nexusValidationStatus = "idle";
        m_nexusValidationMessage.clear();
        emit nexusValidationStatusChanged();
        emit nexusValidationMessageChanged();
    }
}

void ModManager::clearNexusCatalog() {
    m_nexusMediaRequestKey.clear();

    const bool hadMods = !m_nexusModsList.isEmpty() || m_nexusTotalModsCount != 0;
    const bool hadCategories = !m_nexusCategoriesList.isEmpty();
    const bool hadTags = !m_nexusTagOptions.isEmpty();
    const bool hadLanguages = !m_nexusLanguagesList.isEmpty();

    m_nexusModsList.clear();
    m_nexusTotalModsCount = 0;
    m_nexusCurrentPage = 0;
    m_nexusCategoriesList.clear();
    m_nexusTagOptions.clear();
    m_nexusLanguagesList.clear();

    if (hadMods) {
        emit nexusModsListChanged();
        emit nexusTotalModsCountChanged();
        emit nexusCurrentPageChanged();
    }
    if (hadCategories) {
        emit nexusCategoriesListChanged();
    }
    if (hadTags) {
        emit nexusTagsListChanged();
    }
    if (hadLanguages) {
        emit nexusLanguagesListChanged();
    }

    if (!m_currentNexusModDetails.isEmpty()) {
        m_currentNexusModDetails.clear();
        emit currentNexusModDetailsChanged();
    }
    if (!m_currentNexusModFiles.isEmpty()) {
        m_currentNexusModFiles.clear();
        emit currentNexusModFilesChanged();
    }
    if (!m_nexusModPreviewImages.isEmpty()) {
        m_nexusModPreviewImages.clear();
        emit nexusModPreviewImagesChanged();
    }
}

void ModManager::onNexusOAuthUserInfoChanged() {
    if (!m_nexusOAuth || !m_nexusOAuth->isAuthenticated()) return;

    const QString name = m_nexusOAuth->userName();
    const bool premium = m_nexusOAuth->isPremium();
    const QString avatar = m_nexusOAuth->avatarUrl();

    if (m_nexusUsername != name || m_nexusIsPremium != premium || m_nexusAvatarUrl != avatar) {
        m_nexusUsername = name;
        m_nexusIsPremium = premium;
        m_nexusAvatarUrl = avatar;
        emit nexusUserInfoChanged();
    }

    const QString message = name.isEmpty()
        ? QStringLiteral("Signed in securely with Nexus Mods.")
        : QStringLiteral("Connected as %1 (%2)").arg(name, premium ? "Premium" : "Member");
    if (m_nexusValidationStatus != "valid" || m_nexusValidationMessage != message) {
        m_nexusValidationStatus = "valid";
        m_nexusValidationMessage = message;
        emit nexusValidationStatusChanged();
        emit nexusValidationMessageChanged();
    }
}

bool ModManager::nexusEnabled() const { return m_nexusEnabled; }
QString ModManager::nexusApiKey() const { return m_nexusApiKey; }
QString ModManager::nexusGameDomain() const { return m_nexusGameDomain; }
QVariantList ModManager::nexusGamesList() const { return m_nexusGamesList; }
QString ModManager::nexusUsername() const { return m_nexusUsername; }
bool ModManager::nexusIsPremium() const { return m_nexusIsPremium; }
QString ModManager::nexusAvatarUrl() const { return m_nexusAvatarUrl; }
int ModManager::nexusHourlyRemaining() const { return m_nexusHourlyRemaining; }
int ModManager::nexusHourlyLimit() const { return m_nexusHourlyLimit; }
int ModManager::nexusDailyRemaining() const { return m_nexusDailyRemaining; }
int ModManager::nexusDailyLimit() const { return m_nexusDailyLimit; }
QString ModManager::nexusValidationStatus() const { return m_nexusValidationStatus; }
QString ModManager::nexusValidationMessage() const { return m_nexusValidationMessage; }
QVariantList ModManager::nexusModsList() const { return m_nexusModsList; }
QVariantMap ModManager::nexusLatestFileVersions() const { return m_nexusLatestFileVersions; }
int ModManager::nexusTotalModsCount() const { return m_nexusTotalModsCount; }
bool ModManager::nexusIsLoadingMods() const { return m_nexusIsLoadingMods; }
int ModManager::nexusCurrentPage() const { return m_nexusCurrentPage; }
int ModManager::nexusPageSize() const { return m_nexusPageSize; }
QString ModManager::nexusSortField() const { return m_nexusSortField; }
QString ModManager::nexusSearchQuery() const { return m_nexusSearchQuery; }
bool ModManager::nexusIsDownloading() const { return m_nexusIsDownloading; }
double ModManager::nexusDownloadProgress() const { return m_nexusDownloadProgress; }
QString ModManager::nexusDownloadStatus() const { return m_nexusDownloadStatus; }

QString ModManager::nexusTimeRange() const { return m_nexusTimeRange; }
QStringList ModManager::nexusSelectedCategories() const { return m_nexusSelectedCategories; }
QString ModManager::nexusIncludeTag() const { return m_nexusIncludeTag; }
QString ModManager::nexusExcludeTag() const { return m_nexusExcludeTag; }
QString ModManager::nexusSearchTitle() const { return m_nexusSearchTitle; }
QString ModManager::nexusSearchDescription() const { return m_nexusSearchDescription; }
QString ModManager::nexusSearchAuthor() const { return m_nexusSearchAuthor; }
QString ModManager::nexusSearchUploader() const { return m_nexusSearchUploader; }
bool ModManager::nexusHideTranslations() const { return m_nexusHideTranslations; }
QString ModManager::nexusSelectedLanguage() const { return m_nexusSelectedLanguage; }
QVariantList ModManager::nexusCategoriesList() const { return m_nexusCategoriesList; }
QVariantList ModManager::nexusTagOptions() const { return m_nexusTagOptions; }
QVariantList ModManager::nexusLanguagesList() const { return m_nexusLanguagesList; }
QVariantMap ModManager::currentNexusModDetails() const { return m_currentNexusModDetails; }
QVariantList ModManager::currentNexusModFiles() const { return m_currentNexusModFiles; }
QVariantList ModManager::nexusModPreviewImages() const { return m_nexusModPreviewImages; }
QVariantMap ModManager::currentNexusModRequirements() const { return m_currentNexusModRequirements; }
bool ModManager::isLoadingNexusModDetails() const { return m_isLoadingNexusModDetails; }
bool ModManager::isLoadingNexusModFiles() const { return m_isLoadingNexusModFiles; }
bool ModManager::isLoadingNexusModPreviews() const { return m_isLoadingNexusModPreviews; }
bool ModManager::isLoadingNexusModRequirements() const { return m_isLoadingNexusModRequirements; }

int ModManager::nexusFilterActiveCount() const {
    int count = 0;
    if (m_nexusTimeRange != "all" && !m_nexusTimeRange.isEmpty()) count++;
    count += m_nexusSelectedCategories.size();
    if (!m_nexusIncludeTag.isEmpty()) count++;
    if (!m_nexusExcludeTag.isEmpty()) count++;
    if (!m_nexusSearchTitle.isEmpty()) count++;
    if (!m_nexusSearchDescription.isEmpty()) count++;
    if (!m_nexusSearchAuthor.isEmpty()) count++;
    if (!m_nexusSearchUploader.isEmpty()) count++;
    if (m_nexusHideTranslations) count++;
    if (!m_nexusSelectedLanguage.isEmpty()) count++;
    return count;
}

void ModManager::setNexusEnabled(bool enabled) {
    if (m_nexusEnabled != enabled) {
        m_nexusEnabled = enabled;
        emit nexusEnabledChanged();
        saveSettings();
        if (m_nexusEnabled && m_nexusModsList.isEmpty() && hasNexusCredentials()) {
            if ((m_nexusOAuth && m_nexusOAuth->isAuthenticated()) || m_nexusValidationStatus == "valid") {
                fetchNexusMods(0);
            } else {
                validateNexusApiKey();
            }
        }
    }
}

void ModManager::setNexusApiKey(const QString& key) {
    const QString trimmed = key.trimmed();
    if (m_nexusApiKey != trimmed) {
        m_nexusApiKey = trimmed;
        emit nexusApiKeyChanged();
        saveSettings();
        if (!m_nexusApiKey.isEmpty()) {
            validateNexusApiKey();
        } else {
            m_nexusValidationStatus = "idle";
            m_nexusValidationMessage = "";
            m_nexusUsername = "";
            m_nexusIsPremium = false;
            m_nexusAvatarUrl = "";
            emit nexusValidationStatusChanged();
            emit nexusValidationMessageChanged();
            emit nexusUserInfoChanged();
        }
    }
}

void ModManager::setNexusGameDomain(const QString& domain) {
    QString sanitized = domain.trimmed().toLower();
    if (sanitized.isEmpty()) {
        sanitized = "stellarblade";
    }
    if (m_nexusGameDomain != sanitized) {
        m_nexusGameDomain = sanitized;
        emit nexusGameDomainChanged();
        saveSettings();

        if (!m_nexusTagOptions.isEmpty() || !m_nexusIncludeTag.isEmpty() || !m_nexusExcludeTag.isEmpty()) {
            m_nexusTagOptions.clear();
            m_nexusIncludeTag.clear();
            m_nexusExcludeTag.clear();
            emit nexusTagsListChanged();
            emit nexusFiltersChanged();
        }

        if (hasNexusCredentials()) {
            fetchNexusMetadata();
        }
        if (m_nexusEnabled && hasNexusCredentials()) {
            fetchNexusMods(0);
        }
    }
}

void ModManager::setNexusSortField(const QString& field) {
    if (m_nexusSortField != field) {
        m_nexusSortField = field;
        emit nexusSortFieldChanged();
        fetchNexusMods(0);
    }
}

void ModManager::setNexusSearchQuery(const QString& query) {
    if (m_nexusSearchQuery != query) {
        m_nexusSearchQuery = query;
        emit nexusSearchQueryChanged();
        fetchNexusMods(0);
    }
}

void ModManager::setNexusTimeRange(const QString& range) {
    if (m_nexusTimeRange != range) {
        m_nexusTimeRange = range;
        emit nexusFiltersChanged();
        fetchNexusMods(0);
    }
}

void ModManager::toggleNexusCategory(const QString& category) {
    if (m_nexusSelectedCategories.contains(category)) {
        m_nexusSelectedCategories.removeAll(category);
    } else {
        m_nexusSelectedCategories.append(category);
    }
    emit nexusFiltersChanged();
    fetchNexusMods(0);
}

void ModManager::clearNexusCategories() {
    if (!m_nexusSelectedCategories.isEmpty()) {
        m_nexusSelectedCategories.clear();
        emit nexusFiltersChanged();
        fetchNexusMods(0);
    }
}

void ModManager::setNexusIncludeTag(const QString& tag) {
    if (m_nexusIncludeTag != tag) {
        m_nexusIncludeTag = tag;
        emit nexusFiltersChanged();
        fetchNexusMods(0);
    }
}

void ModManager::setNexusExcludeTag(const QString& tag) {
    if (m_nexusExcludeTag != tag) {
        m_nexusExcludeTag = tag;
        emit nexusFiltersChanged();
        fetchNexusMods(0);
    }
}

void ModManager::setNexusSearchTitle(const QString& title) {
    if (m_nexusSearchTitle != title) {
        m_nexusSearchTitle = title;
        emit nexusFiltersChanged();
    }
}

void ModManager::setNexusSearchDescription(const QString& desc) {
    if (m_nexusSearchDescription != desc) {
        m_nexusSearchDescription = desc;
        emit nexusFiltersChanged();
    }
}

void ModManager::setNexusSearchAuthor(const QString& author) {
    if (m_nexusSearchAuthor != author) {
        m_nexusSearchAuthor = author;
        emit nexusFiltersChanged();
    }
}

void ModManager::setNexusSearchUploader(const QString& uploader) {
    if (m_nexusSearchUploader != uploader) {
        m_nexusSearchUploader = uploader;
        emit nexusFiltersChanged();
    }
}

void ModManager::applyNexusSearchParameters(const QString& title, const QString& desc, const QString& author, const QString& uploader) {
    m_nexusSearchTitle = title.trimmed();
    m_nexusSearchDescription = desc.trimmed();
    m_nexusSearchAuthor = author.trimmed();
    m_nexusSearchUploader = uploader.trimmed();
    emit nexusFiltersChanged();
    fetchNexusMods(0);
}

void ModManager::setNexusHideTranslations(bool hide) {
    if (m_nexusHideTranslations != hide) {
        m_nexusHideTranslations = hide;
        emit nexusFiltersChanged();
        fetchNexusMods(0);
    }
}

void ModManager::setNexusSelectedLanguage(const QString& lang) {
    if (m_nexusSelectedLanguage != lang) {
        m_nexusSelectedLanguage = lang;
        emit nexusFiltersChanged();
        fetchNexusMods(0);
    }
}

void ModManager::resetAllNexusFilters() {
    m_nexusTimeRange = "all";
    m_nexusSelectedCategories.clear();
    m_nexusIncludeTag.clear();
    m_nexusExcludeTag.clear();
    m_nexusSearchTitle.clear();
    m_nexusSearchDescription.clear();
    m_nexusSearchAuthor.clear();
    m_nexusSearchUploader.clear();
    m_nexusHideTranslations = false;
    m_nexusSelectedLanguage.clear();
    emit nexusFiltersChanged();
    fetchNexusMods(0);
}

void ModManager::fetchNexusMetadata() {
    if (!hasNexusCredentials()) return;

    fetchNexusGameTags();
    fetchNexusGames();

    const QString gameDomain = m_nexusGameDomain.isEmpty() ? "stellarblade" : m_nexusGameDomain;

    const QString query = R"GRAPHQL(
        query GetCategoriesAndCounts($game: String!) {
            outfits: mods(filter: { gameDomainName: [{ value: $game }], categoryName: [{ value: "Outfits" }] }, count: 0) {
                totalCount
            }
            misc: mods(filter: { gameDomainName: [{ value: $game }], categoryName: [{ value: "Miscellaneous" }] }, count: 0) {
                totalCount
            }
            gameplay: mods(filter: { gameDomainName: [{ value: $game }], categoryName: [{ value: "Gameplay" }] }, count: 0) {
                totalCount
            }
            ui: mods(filter: { gameDomainName: [{ value: $game }], categoryName: [{ value: "User Interface" }] }, count: 0) {
                totalCount
            }
            visuals: mods(filter: { gameDomainName: [{ value: $game }], categoryName: [{ value: "Visuals" }] }, count: 0) {
                totalCount
            }
            english: mods(filter: { gameDomainName: [{ value: $game }], languageName: [{ value: "English" }] }, count: 0) {
                totalCount
            }
            korean: mods(filter: { gameDomainName: [{ value: $game }], languageName: [{ value: "Korean" }] }, count: 0) {
                totalCount
            }
            chinese: mods(filter: { gameDomainName: [{ value: $game }], languageName: [{ value: "Simplified Chinese" }] }, count: 0) {
                totalCount
            }
            japanese: mods(filter: { gameDomainName: [{ value: $game }], languageName: [{ value: "Japanese" }] }, count: 0) {
                totalCount
            }
            french: mods(filter: { gameDomainName: [{ value: $game }], languageName: [{ value: "French" }] }, count: 0) {
                totalCount
            }
            german: mods(filter: { gameDomainName: [{ value: $game }], languageName: [{ value: "German" }] }, count: 0) {
                totalCount
            }
            portuguese: mods(filter: { gameDomainName: [{ value: $game }], languageName: [{ value: "Portuguese (Portugal)" }] }, count: 0) {
                totalCount
            }
            spanish: mods(filter: { gameDomainName: [{ value: $game }], languageName: [{ value: "Spanish (Latin America)" }] }, count: 0) {
                totalCount
            }
        }
    )GRAPHQL";

    QJsonObject variables;
    variables["game"] = gameDomain;

    QJsonObject body;
    body["query"] = query;
    body["variables"] = variables;

    QNetworkRequest request(QUrl("https://api.nexusmods.com/v2/graphql"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "UniversalModManager/1.0");
    applyNexusAuthHeaders(request);

    QNetworkReply* reply = ensureNetworkManager()->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, gameDomain]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            return;
        }
        if (!hasNexusCredentials()) {
            return;
        }

        const QString currentGame = m_nexusGameDomain.isEmpty() ? "stellarblade" : m_nexusGameDomain;
        if (gameDomain != currentGame) {
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) return;

        QJsonObject root = doc.object().value("data").toObject();

        QVariantList cats;
        cats.append(QVariantMap{{"name", "Gameplay"}, {"count", root.value("gameplay").toObject().value("totalCount").toInt(0)}});
        cats.append(QVariantMap{{"name", "Miscellaneous"}, {"count", root.value("misc").toObject().value("totalCount").toInt(0)}});
        cats.append(QVariantMap{{"name", "Outfits"}, {"count", root.value("outfits").toObject().value("totalCount").toInt(0)}});
        cats.append(QVariantMap{{"name", "User Interface"}, {"count", root.value("ui").toObject().value("totalCount").toInt(0)}});
        cats.append(QVariantMap{{"name", "Visuals"}, {"count", root.value("visuals").toObject().value("totalCount").toInt(0)}});
        m_nexusCategoriesList = cats;
        emit nexusCategoriesListChanged();

        QVariantList langs;
        langs.append(QVariantMap{{"name", "English"}, {"count", root.value("english").toObject().value("totalCount").toInt(0)}});
        langs.append(QVariantMap{{"name", "Korean"}, {"count", root.value("korean").toObject().value("totalCount").toInt(0)}});
        langs.append(QVariantMap{{"name", "Simplified Chinese"}, {"count", root.value("chinese").toObject().value("totalCount").toInt(0)}});
        langs.append(QVariantMap{{"name", "Japanese"}, {"count", root.value("japanese").toObject().value("totalCount").toInt(0)}});
        langs.append(QVariantMap{{"name", "Portuguese (Portugal)"}, {"count", root.value("portuguese").toObject().value("totalCount").toInt(0)}});
        langs.append(QVariantMap{{"name", "French"}, {"count", root.value("french").toObject().value("totalCount").toInt(0)}});
        langs.append(QVariantMap{{"name", "Dutch"}, {"count", 1}});
        langs.append(QVariantMap{{"name", "German"}, {"count", root.value("german").toObject().value("totalCount").toInt(0)}});
        langs.append(QVariantMap{{"name", "Spanish (Latin America)"}, {"count", root.value("spanish").toObject().value("totalCount").toInt(0)}});
        langs.append(QVariantMap{{"name", "Ukrainian"}, {"count", 0}});
        m_nexusLanguagesList = langs;
        emit nexusLanguagesListChanged();
    });
}

void ModManager::fetchNexusGames() {
    if (!hasNexusCredentials() || m_nexusGamesLoaded) return;

    QNetworkRequest request(QUrl("https://api.nexusmods.com/v1/games.json"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "UniversalModManager/1.0");
    applyNexusAuthHeaders(request);

    QNetworkReply* reply = ensureNetworkManager()->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        if (!hasNexusCredentials()) return;

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isArray()) return;

        const QJsonArray gamesArray = doc.array();
        QVariantList games;
        games.reserve(gamesArray.size());

        for (const QJsonValue& value : gamesArray) {
            const QJsonObject game = value.toObject();
            const QString name = game.value("name").toString();
            const QString domain = game.value("domain_name").toString();
            if (name.isEmpty() || domain.isEmpty()) continue;

            const int gameId = game.value("id").toInt();
            if (gameId > 0)
                m_nexusGameIds.insert(domain, gameId);

            games.append(QVariantMap{
                {"name", name},
                {"domain", domain},
                {"label", name},
            });
        }

        std::sort(games.begin(), games.end(), [](const QVariant& left, const QVariant& right) {
            return left.toMap().value("name").toString().compare(
                       right.toMap().value("name").toString(), Qt::CaseInsensitive) < 0;
        });

        m_nexusGamesList = games;
        m_nexusGamesLoaded = true;
        emit nexusGamesListChanged();
    });
}

void ModManager::fetchNexusGameTags() {
    if (!hasNexusCredentials()) return;

    const QString gameDomain = m_nexusGameDomain.isEmpty() ? "stellarblade" : m_nexusGameDomain;

    const QString query = R"GRAPHQL(
        query GetGameTagFacets($game: String!) {
            mods(filter: { gameDomainName: [{ value: $game }] }, count: 0, facets: { tag: [] }) {
                facets {
                    facet
                    value
                    count
                }
            }
        }
    )GRAPHQL";

    QJsonObject variables;
    variables["game"] = gameDomain;

    QJsonObject body;
    body["query"] = query;
    body["variables"] = variables;

    QNetworkRequest request(QUrl("https://api.nexusmods.com/v2/graphql"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "UniversalModManager/1.0");
    applyNexusAuthHeaders(request);

    QNetworkReply* reply = ensureNetworkManager()->post(
        request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply, gameDomain]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        if (!hasNexusCredentials()) return;

        if (gameDomain != (m_nexusGameDomain.isEmpty() ? "stellarblade" : m_nexusGameDomain)) return;

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) return;

        const QJsonObject root = doc.object().value("data").toObject();
        const QJsonArray facets = root.value("mods").toObject().value("facets").toArray();

        QVariantList tags;
        for (const QJsonValue& facetValue : facets) {
            const QJsonObject facet = facetValue.toObject();
            if (facet.value("facet").toString() != "tag") continue;

            const QString name = facet.value("value").toString();
            if (name.isEmpty()) continue;

            const int count = facet.value("count").toInt(0);
            tags.append(QVariantMap{
                {"name", name},
                {"count", count},
                {"label", QString("%1 (%2)").arg(name).arg(count)},
            });
        }

        std::sort(tags.begin(), tags.end(), [](const QVariant& left, const QVariant& right) {
            return left.toMap().value("name").toString().compare(
                       right.toMap().value("name").toString(), Qt::CaseInsensitive) < 0;
        });

        if (tags != m_nexusTagOptions) {
            m_nexusTagOptions = tags;
            emit nexusTagsListChanged();
        }
    });
}

void ModManager::nextNexusPage() {
    int maxPages = (m_nexusTotalModsCount + m_nexusPageSize - 1) / m_nexusPageSize;
    if (m_nexusCurrentPage + 1 < maxPages) {
        fetchNexusMods(m_nexusCurrentPage + 1);
    }
}

void ModManager::previousNexusPage() {
    if (m_nexusCurrentPage > 0) {
        fetchNexusMods(m_nexusCurrentPage - 1);
    }
}

void ModManager::openNexusUrl(const QString& url) {
    if (!url.isEmpty()) {
        QDesktopServices::openUrl(QUrl(url));
    }
}

void ModManager::openNexusModDetails(int modId, const QVariantMap& initialData) {
    if (modId <= 0) return;

    if (m_nexusScanRefreshTimer)
        m_nexusScanRefreshTimer->stop();

    m_currentNexusModDetails = initialData;
    m_currentNexusModDetails["modId"] = modId;
    m_isLoadingNexusModDetails = true;
    m_currentNexusModFiles.clear();
    m_isLoadingNexusModFiles = true;
    m_nexusModPreviewImages.clear();
    m_isLoadingNexusModPreviews = true;

    emit currentNexusModDetailsChanged();
    emit isLoadingNexusModDetailsChanged();
    emit currentNexusModFilesChanged();
    emit isLoadingNexusModFilesChanged();
    emit nexusModPreviewImagesChanged();
    emit isLoadingNexusModPreviewsChanged();

    const QString gameDomain = m_nexusGameDomain.isEmpty() ? "stellarblade" : m_nexusGameDomain;
    m_nexusMediaRequestKey = gameDomain.trimmed().toLower() + '/' + QString::number(modId);

    if (!m_nexusMedia) {
        m_nexusMedia = new NexusMedia(m_nexusWebSession, this);

        connect(m_nexusMedia, &NexusMedia::imagesReady, this,
                [this](const QString& key, const QVariantList& images) {
            if (key != m_nexusMediaRequestKey) return;
            m_nexusModPreviewImages = images;
            emit nexusModPreviewImagesChanged();
        });

        connect(m_nexusMedia, &NexusMedia::loadingChanged, this,
                [this](const QString& key, bool loading) {
            if (key != m_nexusMediaRequestKey) return;
            if (m_isLoadingNexusModPreviews == loading) return;
            m_isLoadingNexusModPreviews = loading;
            emit isLoadingNexusModPreviewsChanged();
        });
    }

    m_nexusMedia->fetch(gameDomain, modId);

    QString detailsUrl = QString("https://api.nexusmods.com/v1/games/%1/mods/%2.json")
        .arg(gameDomain, QString::number(modId));

    QNetworkRequest detailsReq(detailsUrl);
    detailsReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    applyNexusAuthHeaders(detailsReq);

    QNetworkReply* detailsReply = ensureNetworkManager()->get(detailsReq);
    connect(detailsReply, &QNetworkReply::finished, this, [this, detailsReply, modId]() {
        detailsReply->deleteLater();
        parseNexusRateLimitHeaders(detailsReply);

        if (detailsReply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(detailsReply->readAll());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (!obj.value("name").toString().isEmpty()) {
                    m_currentNexusModDetails["name"] = obj.value("name").toString();
                }
                if (!obj.value("summary").toString().isEmpty()) {
                    m_currentNexusModDetails["summary"] = obj.value("summary").toString();
                }
                m_currentNexusModDetails["description"] = obj.value("description").toString();
                if (!obj.value("picture_url").toString().isEmpty()) {
                    m_currentNexusModDetails["pictureUrl"] = obj.value("picture_url").toString();
                }
                if (obj.contains("mod_downloads")) {
                    m_currentNexusModDetails["downloads"] = obj.value("mod_downloads").toInt();
                }
                if (obj.contains("mod_unique_downloads")) {
                    m_currentNexusModDetails["uniqueDownloads"] = obj.value("mod_unique_downloads").toInt();
                }
                if (obj.contains("endorsement_count")) {
                    m_currentNexusModDetails["endorsements"] = obj.value("endorsement_count").toInt();
                }
                if (!obj.value("version").toString().isEmpty()) {
                    m_currentNexusModDetails["version"] = obj.value("version").toString();
                }
                if (!obj.value("author").toString().isEmpty()) {
                    m_currentNexusModDetails["author"] = obj.value("author").toString();
                }
                if (!obj.value("uploaded_by").toString().isEmpty()) {
                    m_currentNexusModDetails["uploader"] = obj.value("uploaded_by").toString();
                }
                m_currentNexusModDetails["createdTime"] = obj.value("created_time").toString();
                m_currentNexusModDetails["updatedTime"] = obj.value("updated_time").toString();
                m_currentNexusModDetails["containsAdult"] = obj.value("contains_adult_content").toBool(false);
            }
        }
        m_isLoadingNexusModDetails = false;
        emit currentNexusModDetailsChanged();
        emit isLoadingNexusModDetailsChanged();
    });

    fetchNexusModFiles(modId);

    fetchNexusModRequirements(modId);
}

void ModManager::fetchNexusModFiles(int modId) {
    if (modId <= 0) return;

    m_nexusModFilesModId = modId;
    m_isLoadingNexusModFiles = true;
    emit isLoadingNexusModFilesChanged();

    const QString gameDomain = m_nexusGameDomain.isEmpty() ? "stellarblade" : m_nexusGameDomain;
    const QString filesUrl = QString("https://api.nexusmods.com/v1/games/%1/mods/%2/files.json")
        .arg(gameDomain, QString::number(modId));

    QNetworkRequest request(filesUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    applyNexusAuthHeaders(request);

    QNetworkReply* reply = ensureNetworkManager()->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, modId, gameDomain]() {
        reply->deleteLater();
        parseNexusRateLimitHeaders(reply);

        if (modId != m_nexusModFilesModId)
            return;

        if (reply->error() != QNetworkReply::NoError) {
            m_isLoadingNexusModFiles = false;
            emit isLoadingNexusModFilesChanged();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            m_isLoadingNexusModFiles = false;
            emit isLoadingNexusModFilesChanged();
            return;
        }

        const QJsonArray files = doc.object().value("files").toArray();

        QVariantList parsedList;
        for (const QJsonValue& value : files) {
            const QJsonObject file = value.toObject();
            const QString category = file.value("category_name").toString("MAIN").toUpper();
            if (category == "ARCHIVED" || category == "REMOVED")
                continue;

            QVariantMap fileMap;
            fileMap["fileId"] = file.value("file_id").toInt();
            fileMap["name"] = file.value("name").toString();
            fileMap["version"] = file.value("version").toString();
            fileMap["category"] = category;
            fileMap["description"] = file.value("description").toString();
            fileMap["sizeKb"] = file.value("size_kb").toDouble();
            fileMap["uploadedTimestamp"] = file.value("uploaded_timestamp").toDouble();
            fileMap["virusScanUrl"] = file.value("external_virus_scan_url").toString();
            parsedList.append(fileMap);
        }

        const int gameId = m_nexusGameIds.value(gameDomain, 0);
        if (hasNexusCredentials()) {
            fetchNexusFileScanStatuses(modId, gameId, parsedList);
            return;
        }

        m_currentNexusModFiles = parsedList;
        m_isLoadingNexusModFiles = false;
        emit isLoadingNexusModFilesChanged();
        emit currentNexusModFilesChanged();
    });
}

void ModManager::fetchNexusFileScanStatuses(int modId, int gameId, const QVariantList& files) {
    if (modId <= 0 || !hasNexusCredentials()) {
        if (modId == m_nexusModFilesModId) {
            m_currentNexusModFiles = files;
            m_isLoadingNexusModFiles = false;
            emit isLoadingNexusModFilesChanged();
            emit currentNexusModFilesChanged();
        }
        return;
    }

    if (gameId <= 0) {
        const QString gameDomain = m_nexusGameDomain.isEmpty() ? "stellarblade" : m_nexusGameDomain;
        const QString query = R"GRAPHQL(
            query FileScanGameId($domain: String!) {
                game(domainName: $domain) {
                    id
                }
            }
        )GRAPHQL";

        QJsonObject variables;
        variables["domain"] = gameDomain;

        QJsonObject body;
        body["query"] = query;
        body["variables"] = variables;

        QNetworkRequest request(QUrl("https://api.nexusmods.com/v2/graphql"));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("User-Agent", "UniversalModManager/1.0");
        applyNexusAuthHeaders(request);

        QNetworkReply* reply = ensureNetworkManager()->post(
            request, QJsonDocument(body).toJson(QJsonDocument::Compact));
        connect(reply, &QNetworkReply::finished, this, [this, reply, modId, files, gameDomain]() {
            reply->deleteLater();
            parseNexusRateLimitHeaders(reply);

            if (modId != m_nexusModFilesModId)
                return;

            int resolvedGameId = 0;
            if (reply->error() == QNetworkReply::NoError) {
                const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                resolvedGameId = doc.object().value("data").toObject()
                    .value("game").toObject().value("id").toVariant().toInt();
            }

            if (resolvedGameId > 0) {
                m_nexusGameIds.insert(gameDomain, resolvedGameId);
                fetchNexusFileScanStatuses(modId, resolvedGameId, files);
                return;
            }

            m_currentNexusModFiles = files;
            m_isLoadingNexusModFiles = false;
            emit isLoadingNexusModFilesChanged();
            emit currentNexusModFilesChanged();
        });
        return;
    }

    const QString widgetUrl = QString(
        "https://www.nexusmods.com/Core/Libs/Common/Widgets/ModFilesTab?id=%1&game_id=%2")
        .arg(QString::number(modId), QString::number(gameId));

    QNetworkRequest request{QUrl(widgetUrl)};
    request.setRawHeader("User-Agent",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/130.0 Safari/537.36");
    request.setRawHeader("Accept", "text/html,application/xhtml+xml");
    request.setRawHeader("X-Requested-With", "XMLHttpRequest");

    QNetworkReply* reply = ensureNetworkManager()->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, modId, files]() {
        reply->deleteLater();

        if (modId != m_nexusModFilesModId)
            return;

        const bool fetched = reply->error() == QNetworkReply::NoError;
        QHash<int, QString> scanStatuses;
        int parsedRows = 0;
        if (fetched) {
            const QString html = QString::fromUtf8(reply->readAll());
            const QRegularExpression rowPattern(
                QStringLiteral("<dt[^>]*id=\"file-expander-header-(\\d+)\"[^>]*>(.*?)</dt>"),
                QRegularExpression::DotMatchesEverythingOption);
            auto matches = rowPattern.globalMatch(html);
            while (matches.hasNext()) {
                const QRegularExpressionMatch match = matches.next();
                const int fileId = match.captured(1).toInt();
                if (fileId <= 0)
                    continue;

                ++parsedRows;
                const QString row = match.captured(2);
                if (row.contains(QLatin1String("icon-question")))
                    scanStatuses.insert(fileId, QStringLiteral("QUEUED"));
                else if (row.contains(QLatin1String("icon-tickunsafe")))
                    scanStatuses.insert(fileId, QStringLiteral("QUARANTINED"));
            }
        }

        const bool parsed = fetched && (parsedRows > 0 || files.isEmpty());

        QVariantList merged;
        if (parsed) {
            merged.reserve(files.size());
            for (const QVariant& value : files) {
                QVariantMap file = value.toMap();

                file["virusScanStatus"] = scanStatuses.value(file.value("fileId").toInt());
                merged.append(file);
            }
        } else {
            merged = files;
        }

        bool statusesChanged = !parsed || m_currentNexusModFiles.size() != merged.size();
        if (!statusesChanged) {
            for (int i = 0; i < merged.size(); ++i) {
                if (merged.at(i).toMap().value("virusScanStatus").toString()
                    != m_currentNexusModFiles.at(i).toMap().value("virusScanStatus").toString()) {
                    statusesChanged = true;
                    break;
                }
            }
        }

        if (statusesChanged || m_isLoadingNexusModFiles) {
            m_currentNexusModFiles = merged;
            m_isLoadingNexusModFiles = false;
            emit isLoadingNexusModFilesChanged();
            emit currentNexusModFilesChanged();
        }

        if (parsed)
            updateNexusScanRefresh();
        else if (m_nexusScanRefreshTimer)
            m_nexusScanRefreshTimer->stop();
    });
}

void ModManager::updateNexusScanRefresh() {
    bool pending = false;
    for (const QVariant& value : m_currentNexusModFiles) {
        const QString status = value.toMap().value("virusScanStatus").toString().toUpper();
        if (status == QLatin1String("QUEUED") || status == QLatin1String("WAITING_REPORT")) {
            pending = true;
            break;
        }
    }

    const QString gameDomain = m_nexusGameDomain.isEmpty() ? "stellarblade" : m_nexusGameDomain;
    if (!pending || m_nexusModFilesModId <= 0 || !hasNexusCredentials()
        || m_nexusGameIds.value(gameDomain, 0) <= 0) {
        if (m_nexusScanRefreshTimer)
            m_nexusScanRefreshTimer->stop();
        return;
    }

    if (!m_nexusScanRefreshTimer) {
        m_nexusScanRefreshTimer = new QTimer(this);
        m_nexusScanRefreshTimer->setSingleShot(true);
        m_nexusScanRefreshTimer->setInterval(30000);
        connect(m_nexusScanRefreshTimer, &QTimer::timeout, this, [this]() {
            if (m_nexusModFilesModId <= 0 || m_currentNexusModFiles.isEmpty())
                return;
            const QString domain = m_nexusGameDomain.isEmpty() ? "stellarblade" : m_nexusGameDomain;
            fetchNexusFileScanStatuses(m_nexusModFilesModId,
                                       m_nexusGameIds.value(domain, 0),
                                       m_currentNexusModFiles);
        });
    }

    if (!m_nexusScanRefreshTimer->isActive())
        m_nexusScanRefreshTimer->start();
}

void ModManager::closeNexusModDetails() {
    if (m_nexusScanRefreshTimer)
        m_nexusScanRefreshTimer->stop();
    m_nexusModFilesModId = 0;
}

void ModManager::fetchNexusModRequirements(int modId) {
    if (modId <= 0) return;

    m_nexusRequirementsModId = modId;
    m_nexusRequirementsGameDomain = m_nexusGameDomain.isEmpty() ? "stellarblade" : m_nexusGameDomain;
    m_nexusModsRequiringLoadingMore = false;

    m_currentNexusModRequirements = QVariantMap{
        {"nexus", QVariantList()},
        {"offsite", QVariantList()},
        {"dlc", QVariantList()},
        {"using", QVariantList()},
        {"usingTotal", 0},
        {"error", false},
    };
    m_isLoadingNexusModRequirements = true;
    emit currentNexusModRequirementsChanged();
    emit isLoadingNexusModRequirementsChanged();

    if (m_nexusGameIds.contains(m_nexusRequirementsGameDomain)) {
        requestNexusModRequirementsPage(0);
        return;
    }

    if (!m_nexusGameIdLookupsInFlight.contains(m_nexusRequirementsGameDomain)) {
        m_nexusGameIdLookupsInFlight.insert(m_nexusRequirementsGameDomain);
        resolveNexusGameId(m_nexusRequirementsGameDomain);
    }
}

void ModManager::resolveNexusGameId(const QString& gameDomain) {
    const QString query = R"GRAPHQL(
        query GameId($domain: String!) {
            game(domainName: $domain) {
                id
            }
        }
    )GRAPHQL";

    QJsonObject variables;
    variables["domain"] = gameDomain;

    QJsonObject body;
    body["query"] = query;
    body["variables"] = variables;

    QNetworkRequest request(QUrl("https://api.nexusmods.com/v2/graphql"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "UniversalModManager/1.0");
    applyNexusAuthHeaders(request);

    QNetworkReply* reply = ensureNetworkManager()->post(
        request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply, gameDomain]() {
        reply->deleteLater();
        m_nexusGameIdLookupsInFlight.remove(gameDomain);

        bool resolved = false;
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            const int gameId = doc.object().value("data").toObject()
                .value("game").toObject().value("id").toVariant().toInt();
            if (gameId > 0) {
                m_nexusGameIds.insert(gameDomain, gameId);
                resolved = true;
            }
        }

        if (gameDomain != m_nexusRequirementsGameDomain)
            return;

        if (resolved && m_isLoadingNexusModRequirements) {
            requestNexusModRequirementsPage(0);
            return;
        }

        QVariantMap next = m_currentNexusModRequirements;
        next["error"] = true;
        m_currentNexusModRequirements = next;
        emit currentNexusModRequirementsChanged();

        if (m_isLoadingNexusModRequirements) {
            m_isLoadingNexusModRequirements = false;
            emit isLoadingNexusModRequirementsChanged();
        }
    });
}

void ModManager::loadMoreNexusModsRequiring() {
    if (m_nexusRequirementsModId <= 0 || m_isLoadingNexusModRequirements
        || m_nexusModsRequiringLoadingMore) {
        return;
    }

    const int loaded = m_currentNexusModRequirements.value("using").toList().size();
    const int total = m_currentNexusModRequirements.value("usingTotal").toInt();
    if (loaded >= total)
        return;

    requestNexusModRequirementsPage(loaded);
}

void ModManager::requestNexusModRequirementsPage(int offset) {
    const int gameId = m_nexusGameIds.value(m_nexusRequirementsGameDomain, 0);
    if (m_nexusRequirementsModId <= 0 || gameId <= 0 || !hasNexusCredentials()) {
        if (m_isLoadingNexusModRequirements) {
            m_isLoadingNexusModRequirements = false;
            emit isLoadingNexusModRequirementsChanged();
        }
        return;
    }

    const int modId = m_nexusRequirementsModId;
    const QString gameDomain = m_nexusRequirementsGameDomain;
    const bool append = offset > 0;
    constexpr int pageSize = 100;

    const QString query = R"GRAPHQL(
        query ModRequirements($modId: ID!, $gameId: ID!, $count: Int!, $offset: Int!) {
            mod(modId: $modId, gameId: $gameId) {
                modRequirements {
                    nexusRequirements(count: 100) {
                        nodes {
                            externalRequirement
                            gameId
                            modId
                            modName
                            notes
                            url
                        }
                    }
                    dlcRequirements {
                        notes
                        gameExpansion {
                            name
                        }
                    }
                    modsRequiringThisMod(count: $count, offset: $offset) {
                        totalCount
                        nodes {
                            externalRequirement
                            gameId
                            modId
                            modName
                            notes
                            url
                        }
                    }
                }
            }
        }
    )GRAPHQL";

    QJsonObject variables;
    variables["modId"] = QString::number(modId);
    variables["gameId"] = QString::number(gameId);
    variables["count"] = pageSize;
    variables["offset"] = offset;

    QJsonObject body;
    body["query"] = query;
    body["variables"] = variables;

    QNetworkRequest request(QUrl("https://api.nexusmods.com/v2/graphql"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "UniversalModManager/1.0");
    applyNexusAuthHeaders(request);

    if (append)
        m_nexusModsRequiringLoadingMore = true;

    QNetworkReply* reply = ensureNetworkManager()->post(
        request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, modId, gameDomain, append]() {
        reply->deleteLater();
        parseNexusRateLimitHeaders(reply);

        if (modId != m_nexusRequirementsModId || gameDomain != m_nexusRequirementsGameDomain)
            return;

        const auto finish = [this, append]() {
            if (append) {
                m_nexusModsRequiringLoadingMore = false;
            } else if (m_isLoadingNexusModRequirements) {
                m_isLoadingNexusModRequirements = false;
                emit isLoadingNexusModRequirementsChanged();
            }
        };

        const auto fail = [this, append, &finish]() {
            if (!append) {
                QVariantMap next = m_currentNexusModRequirements;
                next["error"] = true;
                m_currentNexusModRequirements = next;
                emit currentNexusModRequirementsChanged();
            }
            finish();
        };

        if (reply->error() != QNetworkReply::NoError) {
            fail();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            fail();
            return;
        }

        const QJsonObject root = doc.object().value("data").toObject();
        const QJsonObject mod = root.value("mod").toObject();
        if (mod.isEmpty()) {
            fail();
            return;
        }

        const QJsonObject requirements = mod.value("modRequirements").toObject();

        const auto requirementMap = [&gameDomain](const QJsonObject& node) {
            const int requiredModId = node.value("modId").toVariant().toInt();
            const bool external = node.value("externalRequirement").toBool();
            QString url = node.value("url").toString();
            if (url.isEmpty() && !external && requiredModId > 0) {
                url = QStringLiteral("https://www.nexusmods.com/%1/mods/%2")
                    .arg(gameDomain).arg(requiredModId);
            }
            return QVariantMap{
                {"modId", requiredModId},
                {"modName", node.value("modName").toString()},
                {"notes", node.value("notes").toString()},
                {"external", external},
                {"url", url},
            };
        };

        QVariantList nexusRows;
        QVariantList offSiteRows;
        const QJsonArray requirementNodes = requirements.value("nexusRequirements")
            .toObject().value("nodes").toArray();
        for (const QJsonValue& value : requirementNodes) {
            const QVariantMap row = requirementMap(value.toObject());
            if (row.value("external").toBool())
                offSiteRows.append(row);
            else
                nexusRows.append(row);
        }

        QVariantList dlcRows;
        const QJsonArray dlcNodes = requirements.value("dlcRequirements").toArray();
        for (const QJsonValue& value : dlcNodes) {
            const QJsonObject dlc = value.toObject();
            dlcRows.append(QVariantMap{
                {"name", dlc.value("gameExpansion").toObject().value("name").toString()},
                {"notes", dlc.value("notes").toString()},
            });
        }

        const QJsonObject requiring = requirements.value("modsRequiringThisMod").toObject();
        QVariantList requiringRows;
        const QJsonArray requiringNodes = requiring.value("nodes").toArray();
        for (const QJsonValue& value : requiringNodes)
            requiringRows.append(requirementMap(value.toObject()));

        QVariantMap next = m_currentNexusModRequirements;
        if (append) {
            QVariantList merged = next.value("using").toList();
            for (const QVariant& row : requiringRows)
                merged.append(row);
            next["using"] = merged;
        } else {
            next["nexus"] = nexusRows;
            next["offsite"] = offSiteRows;
            next["dlc"] = dlcRows;
            next["using"] = requiringRows;
            next["error"] = false;
        }
        next["usingTotal"] = requiring.value("totalCount").toInt();
        m_currentNexusModRequirements = next;
        emit currentNexusModRequirementsChanged();

        finish();
    });
}

void ModManager::parseNexusRateLimitHeaders(QNetworkReply* reply) {
    if (!reply) return;

    bool changed = false;

    if (reply->hasRawHeader("X-RL-Hourly-Remaining")) {
        int hr = reply->rawHeader("X-RL-Hourly-Remaining").toInt();
        if (m_nexusHourlyRemaining != hr) {
            m_nexusHourlyRemaining = hr;
            changed = true;
        }
    }
    if (reply->hasRawHeader("X-RL-Hourly-Limit")) {
        int hl = reply->rawHeader("X-RL-Hourly-Limit").toInt();
        if (m_nexusHourlyLimit != hl) {
            m_nexusHourlyLimit = hl;
            changed = true;
        }
    }
    if (reply->hasRawHeader("X-RL-Daily-Remaining")) {
        int dr = reply->rawHeader("X-RL-Daily-Remaining").toInt();
        if (m_nexusDailyRemaining != dr) {
            m_nexusDailyRemaining = dr;
            changed = true;
        }
    }
    if (reply->hasRawHeader("X-RL-Daily-Limit")) {
        int dl = reply->rawHeader("X-RL-Daily-Limit").toInt();
        if (m_nexusDailyLimit != dl) {
            m_nexusDailyLimit = dl;
            changed = true;
        }
    }

    if (changed) {
        emit nexusRateLimitsChanged();
    }
}

void ModManager::validateNexusApiKey() {

    if (m_nexusOAuth && m_nexusOAuth->isAuthenticated()) {
        m_nexusValidationStatus = "valid";
        m_nexusValidationMessage = m_nexusUsername.isEmpty()
            ? QStringLiteral("Signed in securely with Nexus Mods.")
            : QStringLiteral("Connected as %1 (%2)")
                  .arg(m_nexusUsername, m_nexusIsPremium ? "Premium" : "Member");
        emit nexusValidationStatusChanged();
        emit nexusValidationMessageChanged();
        return;
    }

    if (m_nexusApiKey.trimmed().isEmpty()) {
        m_nexusValidationStatus = "idle";
        m_nexusValidationMessage = "Please enter your Nexus Mods API Key.";
        emit nexusValidationStatusChanged();
        emit nexusValidationMessageChanged();
        return;
    }

    m_nexusValidationStatus = "checking";
    m_nexusValidationMessage = "Validating API key with Nexus Mods...";
    emit nexusValidationStatusChanged();
    emit nexusValidationMessageChanged();

    QNetworkRequest request(QUrl("https://api.nexusmods.com/v1/users/validate.json"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("apikey", m_nexusApiKey.trimmed().toUtf8());
    request.setRawHeader("Application-Name", "UniversalModManager");
    request.setRawHeader("Application-Version", "1.0.0");

    QNetworkReply* reply = ensureNetworkManager()->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        parseNexusRateLimitHeaders(reply);

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                m_nexusUsername = obj.value("name").toString();
                m_nexusIsPremium = obj.value("is_premium").toBool(false);
                m_nexusAvatarUrl = obj.value("profile_url").toString();
                m_nexusValidationStatus = "valid";
                m_nexusValidationMessage = QString("Connected as %1 (%2)")
                    .arg(m_nexusUsername, m_nexusIsPremium ? "Premium" : "Member");

                emit nexusUserInfoChanged();
                emit nexusValidationStatusChanged();
                emit nexusValidationMessageChanged();

                fetchNexusMetadata();
                if (m_nexusEnabled) {
                    fetchNexusMods(0);
                }
                return;
            }
        }

        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        m_nexusValidationStatus = "invalid";
        if (statusCode == 401 || statusCode == 403) {
            m_nexusValidationMessage = "Invalid API Key. Please check the key and try again.";
        } else if (statusCode == 429) {
            m_nexusValidationMessage = "Nexus Mods API rate limit reached. Please wait for reset.";
        } else {
            m_nexusValidationMessage = QString("Connection failed: %1").arg(reply->errorString());
        }
        emit nexusValidationStatusChanged();
        emit nexusValidationMessageChanged();
    });
}

void ModManager::fetchNexusMods(int page) {
    if (!hasNexusCredentials()) {
        m_nexusValidationStatus = "idle";
        m_nexusValidationMessage = "Sign in with Nexus Mods to browse mods.";
        emit nexusValidationStatusChanged();
        emit nexusValidationMessageChanged();
        return;
    }

    if (m_nexusCategoriesList.isEmpty()) {
        fetchNexusMetadata();
    }

    m_nexusCurrentPage = page;
    m_nexusIsLoadingMods = true;
    emit nexusCurrentPageChanged();
    emit nexusIsLoadingModsChanged();

    const int offset = page * m_nexusPageSize;
    const QString gameDomain = m_nexusGameDomain.isEmpty() ? "stellarblade" : m_nexusGameDomain;

    QJsonObject variables;
    QJsonObject filterObj;

    QJsonArray gameDomainArray;
    QJsonObject gameDomainVal;
    gameDomainVal["value"] = gameDomain;
    gameDomainArray.append(gameDomainVal);
    filterObj["gameDomainName"] = gameDomainArray;

    if (m_nexusTimeRange != "all" && !m_nexusTimeRange.isEmpty()) {
        qint64 nowSecs = QDateTime::currentSecsSinceEpoch();
        qint64 thresholdSecs = nowSecs;
        if (m_nexusTimeRange == "24h") {
            thresholdSecs -= 86400;
        } else if (m_nexusTimeRange == "7d") {
            thresholdSecs -= 7 * 86400;
        } else if (m_nexusTimeRange == "14d") {
            thresholdSecs -= 14 * 86400;
        } else if (m_nexusTimeRange == "28d") {
            thresholdSecs -= 28 * 86400;
        } else if (m_nexusTimeRange == "1y") {
            thresholdSecs -= 365 * 86400;
        }

        QJsonArray dateArray;
        QJsonObject dateVal;
        dateVal["op"] = "GTE";
        dateVal["value"] = QString::number(thresholdSecs);
        dateArray.append(dateVal);
        filterObj["updatedAt"] = dateArray;
    }

    if (m_nexusSelectedCategories.size() == 1) {
        QJsonArray catArray;
        QJsonObject catVal;
        catVal["value"] = m_nexusSelectedCategories.first();
        catArray.append(catVal);
        filterObj["categoryName"] = catArray;
    } else if (m_nexusSelectedCategories.size() > 1) {
        QJsonArray orFilters;
        for (const QString& cat : m_nexusSelectedCategories) {
            QJsonObject singleCatFilter;
            QJsonArray catArray;
            QJsonObject catVal;
            catVal["value"] = cat;
            catArray.append(catVal);
            singleCatFilter["categoryName"] = catArray;
            orFilters.append(singleCatFilter);
        }
        QJsonObject nestedOrObj;
        nestedOrObj["op"] = "OR";
        nestedOrObj["filter"] = orFilters;

        QJsonArray filterArray;
        filterArray.append(nestedOrObj);
        filterObj["filter"] = filterArray;
    }

    QString effectiveTitle = !m_nexusSearchTitle.trimmed().isEmpty()
        ? m_nexusSearchTitle.trimmed()
        : m_nexusSearchQuery.trimmed();
    if (!effectiveTitle.isEmpty()) {
        QJsonArray nameArray;
        QJsonObject nameVal;
        nameVal["op"] = "WILDCARD";
        nameVal["value"] = effectiveTitle;
        nameArray.append(nameVal);
        filterObj["name"] = nameArray;
    }

    if (!m_nexusSearchDescription.trimmed().isEmpty()) {
        QJsonArray descArray;
        QJsonObject descVal;
        descVal["op"] = "MATCHES";
        descVal["value"] = m_nexusSearchDescription.trimmed();
        descArray.append(descVal);
        filterObj["description"] = descArray;
    }

    if (!m_nexusSearchAuthor.trimmed().isEmpty()) {
        QJsonArray authorArray;
        QJsonObject authorVal;
        authorVal["op"] = "WILDCARD";
        authorVal["value"] = m_nexusSearchAuthor.trimmed();
        authorArray.append(authorVal);
        filterObj["author"] = authorArray;
    }

    if (!m_nexusSearchUploader.trimmed().isEmpty()) {
        QJsonArray uploaderArray;
        QJsonObject uploaderVal;
        uploaderVal["op"] = "WILDCARD";
        uploaderVal["value"] = m_nexusSearchUploader.trimmed();
        uploaderArray.append(uploaderVal);
        filterObj["uploader"] = uploaderArray;
    }

    if (!m_nexusIncludeTag.trimmed().isEmpty()) {
        QJsonArray tagArray;
        QJsonObject tagVal;
        tagVal["op"] = "EQUALS";
        tagVal["value"] = m_nexusIncludeTag.trimmed();
        tagArray.append(tagVal);
        filterObj["tag"] = tagArray;
    }

    if (!m_nexusSelectedLanguage.trimmed().isEmpty()) {
        QJsonArray langArray;
        QJsonObject langVal;
        langVal["value"] = m_nexusSelectedLanguage.trimmed();
        langArray.append(langVal);
        filterObj["languageName"] = langArray;
    }

    if (m_nexusOAuth && m_nexusOAuth->isAuthenticated()
            && !m_nexusOAuth->adultContentAllowed()) {
        QJsonArray adultArray;
        QJsonObject adultValue;
        adultValue["value"] = false;
        adultArray.append(adultValue);
        filterObj["adultContent"] = adultArray;
    }

    variables["filter"] = filterObj;
    variables["offset"] = offset;
    variables["count"] = m_nexusPageSize;

    QString sortKey = "downloads";
    if (m_nexusSortField == "ENDORSEMENTS") {
        sortKey = "endorsements";
    } else if (m_nexusSortField == "UPDATED_TIME") {
        sortKey = "updatedAt";
    } else if (m_nexusSortField == "CREATED_TIME") {
        sortKey = "createdAt";
    }

    QJsonArray sortArray;
    QJsonObject sortKeyObj;
    QJsonObject dirObj;
    dirObj["direction"] = "DESC";
    sortKeyObj[sortKey] = dirObj;
    sortArray.append(sortKeyObj);
    variables["sort"] = sortArray;

    const QString queryString = R"GRAPHQL(
        query GetMods($filter: ModsFilter, $count: Int, $offset: Int, $sort: [ModsSort!]) {
            mods(filter: $filter, count: $count, offset: $offset, sort: $sort) {
                totalCount
                nodesCount
                nodes {
                    modId
                    name
                    summary
                    pictureUrl
                    thumbnailUrl
                    version
                    downloads
                    endorsements
                    createdAt
                    updatedAt
                    category
                    adultContent
                    uploader {
                        name
                    }
                }
            }
        }
    )GRAPHQL";

    QJsonObject body;
    body["query"] = queryString;
    body["variables"] = variables;

    QNetworkRequest request(QUrl("https://api.nexusmods.com/v2/graphql"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "UniversalModManager/1.0");
    applyNexusAuthHeaders(request);

    QNetworkReply* reply = ensureNetworkManager()->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_nexusIsLoadingMods = false;
        emit nexusIsLoadingModsChanged();

        parseNexusRateLimitHeaders(reply);

        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "[NexusMods] GraphQL HTTP request failed:" << reply->errorString();
            return;
        }

        if (!hasNexusCredentials()) {
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) return;

        QJsonObject root = doc.object();
        if (root.contains("errors")) {
            qWarning() << "[NexusMods] GraphQL returned errors:" << root.value("errors");
        }

        QJsonObject dataObj = root.value("data").toObject();
        QJsonObject modsObj = dataObj.value("mods").toObject();

        int totalCount = modsObj.value("totalCount").toInt(0);
        QJsonArray nodes = modsObj.value("nodes").toArray();

        QVariantList list;
        list.reserve(nodes.size());

        for (const QJsonValue& val : nodes) {
            QJsonObject node = val.toObject();
            QVariantMap item;
            item["modId"] = node.value("modId").toInt(0);
            item["name"] = node.value("name").toString("Unnamed Mod");
            item["summary"] = node.value("summary").toString("");

            QString pic = node.value("pictureUrl").toString();
            if (pic.isEmpty()) {
                pic = node.value("thumbnailUrl").toString();
            }
            item["pictureUrl"] = pic;

            item["version"] = node.value("version").toString("1.0");
            item["author"] = node.value("uploader").toObject().value("name").toString("Unknown");
            item["downloads"] = node.value("downloads").toInt(0);
            item["endorsements"] = node.value("endorsements").toInt(0);
            item["updatedAt"] = node.value("updatedAt").toString("");
            item["createdAt"] = node.value("createdAt").toString("");
            item["category"] = node.value("category").toString("");
            item["adultContent"] = node.value("adultContent").toBool(false);
            item["gameDomain"] = m_nexusGameDomain;
            list.append(item);
        }

        m_nexusTotalModsCount = totalCount;
        m_nexusModsList = list;
        emit nexusTotalModsCountChanged();
        emit nexusModsListChanged();

        fetchLatestNexusFileVersions(list);
    });
}

void ModManager::fetchLatestNexusFileVersions(const QVariantList& mods) {
    if (mods.isEmpty() || !hasNexusCredentials())
        return;

    const int gameId = m_nexusGameIds.value(m_nexusGameDomain, 0);
    if (gameId <= 0)
        return;

    QStringList variableDeclarations{"$gameId: ID!"};
    QStringList selections;
    QJsonObject variables;
    variables["gameId"] = QString::number(gameId);

    for (int i = 0; i < mods.size(); ++i) {
        const int modId = mods.at(i).toMap().value("modId").toInt();
        if (modId <= 0)
            continue;
        const QString suffix = QString::number(i);
        variableDeclarations.append(QStringLiteral("$m") + suffix + QStringLiteral(": ID!"));
        selections.append(QStringLiteral("f%1: modFiles(modId: $m%1, gameId: $gameId) { version date primary category }")
                              .arg(suffix));
        variables[QStringLiteral("m") + suffix] = QString::number(modId);
    }

    if (selections.isEmpty())
        return;

    const QString query = QStringLiteral("query LatestFiles(") + variableDeclarations.join(QStringLiteral(", "))
        + QStringLiteral(") { ") + selections.join(QStringLiteral(" ")) + QStringLiteral(" }");

    QJsonObject body;
    body["query"] = query;
    body["variables"] = variables;

    QNetworkRequest request(QUrl("https://api.nexusmods.com/v2/graphql"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "UniversalModManager/1.0");
    applyNexusAuthHeaders(request);

    QNetworkReply* reply = ensureNetworkManager()->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, mods]() {
        reply->deleteLater();
        parseNexusRateLimitHeaders(reply);

        if (reply->error() != QNetworkReply::NoError)
            return;
        if (!hasNexusCredentials())
            return;

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonObject data = doc.object().value("data").toObject();
        if (data.isEmpty())
            return;

        QHash<int, QString> latestVersions;
        for (int i = 0; i < mods.size(); ++i) {
            const int modId = mods.at(i).toMap().value("modId").toInt();
            if (modId <= 0)
                continue;

            QString bestPrimaryVersion;
            QString bestAnyVersion;
            qint64 bestPrimaryDate = -1;
            qint64 bestAnyDate = -1;

            const QJsonArray files = data.value(QString("f%1").arg(i)).toArray();
            for (const QJsonValue& fileValue : files) {
                const QJsonObject file = fileValue.toObject();
                const QString fileVersion = file.value("version").toString().trimmed();
                if (fileVersion.isEmpty())
                    continue;

                const QString category = file.value("category").toString().toUpper();
                if (category == QLatin1String("ARCHIVED") || category == QLatin1String("REMOVED"))
                    continue;

                const QJsonValue dateValue = file.value("date");
                qint64 uploadedMs = 0;
                if (dateValue.isDouble()) {
                    uploadedMs = qint64(dateValue.toDouble() * 1000.0);
                } else if (dateValue.isString()) {
                    const QString dateText = dateValue.toString();
                    QDateTime uploadedAt = QDateTime::fromString(dateText, Qt::ISODateWithMs);
                    if (!uploadedAt.isValid())
                        uploadedAt = QDateTime::fromString(dateText, Qt::ISODate);
                    uploadedMs = uploadedAt.isValid()
                        ? uploadedAt.toMSecsSinceEpoch()
                        : qint64(dateText.toDouble() * 1000.0);
                }

                const QJsonValue primaryValue = file.value("primary");
                const bool primary = primaryValue.isBool() ? primaryValue.toBool()
                                                           : primaryValue.toInt(0) != 0;

                if (uploadedMs >= bestAnyDate) {
                    bestAnyDate = uploadedMs;
                    bestAnyVersion = fileVersion;
                }
                if (primary && uploadedMs >= bestPrimaryDate) {
                    bestPrimaryDate = uploadedMs;
                    bestPrimaryVersion = fileVersion;
                }
            }

            const QString chosen = !bestPrimaryVersion.isEmpty() ? bestPrimaryVersion : bestAnyVersion;
            if (!chosen.isEmpty())
                latestVersions.insert(modId, chosen);
        }

        if (latestVersions.isEmpty())
            return;

        bool changed = false;
        for (auto it = latestVersions.constBegin(); it != latestVersions.constEnd(); ++it) {
            const QString key = QString::number(it.key());
            if (m_nexusLatestFileVersions.value(key).toString() == it.value())
                continue;
            m_nexusLatestFileVersions.insert(key, it.value());
            changed = true;
        }

        if (changed)
            emit nexusLatestFileVersionsChanged();
    });
}

void ModManager::cancelNexusDownload() {
    if (m_activeNexusDownloadReply) {
        m_activeNexusDownloadReply->abort();
        m_activeNexusDownloadReply = nullptr;
    }
    if (m_activeNexusDownloadFile) {
        m_activeNexusDownloadFile->close();
        delete m_activeNexusDownloadFile;
        m_activeNexusDownloadFile = nullptr;
    }
    m_nexusIsDownloading = false;
    m_nexusDownloadProgress = 0.0;
    m_nexusDownloadStatus = "Download cancelled.";
    emit nexusDownloadStateChanged();
    emit nexusDownloadProgressChanged();
    emit nexusDownloadStatusChanged();
}

void ModManager::requestDownloadsPathSetup() {
    emit downloadsPathRequired();
}

QString ModManager::nexusDownloadDestination(bool isUpdate)
{
    const QString folder = mmdetail::downloadLibraryFolderName(isUpdate);
    const QDir downloadsRoot(m_downloadsPath);
    const QString directory = downloadsRoot.filePath(folder);
    if (!QDir().mkpath(directory)) {
        qWarning() << "Could not create downloads subfolder:" << directory;
        return m_downloadsPath;
    }

    if (m_downloadsWatcher && !m_downloadsWatcher->directories().contains(directory))
        m_downloadsWatcher->addPath(directory);

    return directory;
}

void ModManager::startDownloadFromDirectUrl(const QUrl& url, const QString& targetFileName,
                                            const QString& modName, bool isUpdate) {
    if (m_downloadsPath.isEmpty()) {
        emit downloadsPathRequired();
        return;
    }

    if (m_downloadManager) {
        m_downloadManager->enqueue(url, targetFileName, modName,
                                   nexusDownloadDestination(isUpdate));
        return;
    }

    if (m_downloadsPath.isEmpty() || !QDir(m_downloadsPath).exists()) {
        m_nexusDownloadStatus = "Downloads directory is not configured in Settings.";
        m_nexusIsDownloading = false;
        emit nexusDownloadStatusChanged();
        emit nexusDownloadStateChanged();
        return;
    }

    cancelNexusDownload();

    QString safeFileName = targetFileName;
    if (safeFileName.isEmpty()) {
        safeFileName = url.fileName();
    }
    if (safeFileName.isEmpty()) {
        safeFileName = QString("%1.zip").arg(modName);
    }
    safeFileName.replace(QRegularExpression("[<>:\"/\\\\|?*]"), "_");

    const QString destinationDirectory = nexusDownloadDestination(isUpdate);
    QString destinationPath = destinationDirectory + "/" + safeFileName;
    m_activeNexusDownloadFile = new QFile(destinationPath, this);
    if (!m_activeNexusDownloadFile->open(QIODevice::WriteOnly)) {
        m_nexusDownloadStatus = QString("Failed to open destination file: %1").arg(safeFileName);
        delete m_activeNexusDownloadFile;
        m_activeNexusDownloadFile = nullptr;
        m_nexusIsDownloading = false;
        emit nexusDownloadStatusChanged();
        emit nexusDownloadStateChanged();
        return;
    }

    m_nexusIsDownloading = true;
    m_nexusDownloadProgress = 0.0;
    m_nexusDownloadStatus = QString("Downloading %1...").arg(modName);
    emit nexusDownloadStateChanged();
    emit nexusDownloadProgressChanged();
    emit nexusDownloadStatusChanged();

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    m_activeNexusDownloadReply = ensureNetworkManager()->get(request);

    connect(m_activeNexusDownloadReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_activeNexusDownloadReply && m_activeNexusDownloadFile) {
            m_activeNexusDownloadFile->write(m_activeNexusDownloadReply->readAll());
        }
    });

    connect(m_activeNexusDownloadReply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0) {
            m_nexusDownloadProgress = static_cast<double>(received) / static_cast<double>(total);
            emit nexusDownloadProgressChanged();
        }
    });

    connect(m_activeNexusDownloadReply, &QNetworkReply::finished, this, [this, modName, destinationPath]() {
        if (m_activeNexusDownloadFile) {
            m_activeNexusDownloadFile->flush();
            m_activeNexusDownloadFile->close();
            delete m_activeNexusDownloadFile;
            m_activeNexusDownloadFile = nullptr;
        }

        if (m_activeNexusDownloadReply) {
            if (m_activeNexusDownloadReply->error() == QNetworkReply::NoError) {
                m_nexusDownloadProgress = 1.0;
                m_nexusDownloadStatus = QString("Downloaded %1 successfully!").arg(modName);
                emit nexusDownloadProgressChanged();
                emit nexusDownloadStatusChanged();

                scanDownloadedMods();
            } else {
                m_nexusDownloadStatus = QString("Download failed: %1").arg(m_activeNexusDownloadReply->errorString());
                emit nexusDownloadStatusChanged();
                QFile::remove(destinationPath);
            }
            m_activeNexusDownloadReply->deleteLater();
            m_activeNexusDownloadReply = nullptr;
        }

        m_nexusIsDownloading = false;
        emit nexusDownloadStateChanged();
    });
}

void ModManager::downloadNexusMod(int modId, int fileId, const QString& modName) {
    if (modId <= 0) return;

    if (m_downloadsPath.isEmpty()) {
        emit downloadsPathRequired();
        return;
    }

    if (!m_nexusIsPremium) {
        QString filesUrl = QString("https://www.nexusmods.com/%1/mods/%2?tab=files")
            .arg(m_nexusGameDomain, QString::number(modId));
        if (fileId > 0)
            filesUrl += QString("&file_id=%1").arg(fileId);
        filesUrl += QStringLiteral("&nmm=1");
        m_nexusDownloadStatus = fileId > 0
            ? QStringLiteral("Retrieving the Nexus download link...")
            : QStringLiteral("Free tier: choose a file to get the Nexus download link.");
        emit nexusDownloadStatusChanged();
        emit nexusDownloadPageRequested(QUrl(filesUrl).toString(), modName);
        return;
    }

    m_nexusDownloadStatus = QString("Requesting download link for %1...").arg(modName);
    m_nexusIsDownloading = true;
    m_nexusDownloadProgress = 0.05;
    emit nexusDownloadStatusChanged();
    emit nexusDownloadStateChanged();
    emit nexusDownloadProgressChanged();

    if (fileId <= 0) {
        QString filesApiUrl = QString("https://api.nexusmods.com/v1/games/%1/mods/%2/files.json")
            .arg(m_nexusGameDomain, QString::number(modId));

        QNetworkRequest req;
        req.setUrl(QUrl(filesApiUrl));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        applyNexusAuthHeaders(req);

        QNetworkReply* reply = ensureNetworkManager()->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, modId, modName]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                m_nexusDownloadStatus = QString("Could not retrieve mod files list: %1").arg(reply->errorString());
                m_nexusIsDownloading = false;
                emit nexusDownloadStatusChanged();
                emit nexusDownloadStateChanged();
                return;
            }

            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject root = doc.object();
            QJsonArray files = root.value("files").toArray();

            int selectedFileId = -1;
            QString targetFileName;

            for (const QJsonValue& val : files) {
                QJsonObject f = val.toObject();
                QString cat = f.value("category_name").toString();
                if (cat.toUpper() == "MAIN" || f.value("is_primary").toBool(false)) {
                    selectedFileId = f.value("file_id").toInt();
                    targetFileName = f.value("file_name").toString();
                    break;
                }
            }

            if (selectedFileId <= 0 && !files.isEmpty()) {
                QJsonObject f = files.first().toObject();
                selectedFileId = f.value("file_id").toInt();
                targetFileName = f.value("file_name").toString();
            }

            if (selectedFileId > 0) {

                downloadNexusMod(modId, selectedFileId, modName);
            } else {
                m_nexusDownloadStatus = "No downloadable files found for this mod.";
                m_nexusIsDownloading = false;
                emit nexusDownloadStatusChanged();
                emit nexusDownloadStateChanged();
            }
        });
        return;
    }

    QString linkApiUrl = QString("https://api.nexusmods.com/v1/games/%1/mods/%2/files/%3/download_link.json")
        .arg(m_nexusGameDomain, QString::number(modId), QString::number(fileId));

    QNetworkRequest linkReq;
    linkReq.setUrl(QUrl(linkApiUrl));
    linkReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    applyNexusAuthHeaders(linkReq);

    QNetworkReply* linkReply = ensureNetworkManager()->get(linkReq);
    connect(linkReply, &QNetworkReply::finished, this, [this, linkReply, modName, modId, fileId]() {
        linkReply->deleteLater();
        parseNexusRateLimitHeaders(linkReply);

        if (linkReply->error() != QNetworkReply::NoError) {
            m_nexusDownloadStatus = QString("Failed to generate download link: %1").arg(linkReply->errorString());
            m_nexusIsDownloading = false;
            emit nexusDownloadStatusChanged();
            emit nexusDownloadStateChanged();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(linkReply->readAll());
        QJsonArray links = doc.isArray() ? doc.array() : doc.object().value("links").toArray();

        QString downloadUri;
        if (!links.isEmpty()) {
            downloadUri = links.first().toObject().value("URI").toString();
        }

        if (downloadUri.isEmpty()) {
            m_nexusDownloadStatus = "Download link was empty or unavailable.";
            m_nexusIsDownloading = false;
            emit nexusDownloadStatusChanged();
            emit nexusDownloadStateChanged();
            return;
        }

        const QString infoUrl = QString("https://api.nexusmods.com/v1/games/%1/mods/%2/files/%3.json")
            .arg(m_nexusGameDomain, QString::number(modId), QString::number(fileId));
        QNetworkRequest infoReq(QUrl{infoUrl});
        infoReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        applyNexusAuthHeaders(infoReq);

        QNetworkReply* infoReply = ensureNetworkManager()->get(infoReq);
        connect(infoReply, &QNetworkReply::finished, this,
                [this, infoReply, downloadUri, modName, modId, fileId]() {
            infoReply->deleteLater();
            parseNexusRateLimitHeaders(infoReply);

            QString fileName;
            QString version;
            QString title;
            QString category;
            if (infoReply->error() == QNetworkReply::NoError) {
                const QJsonObject file = QJsonDocument::fromJson(infoReply->readAll()).object();
                title = file.value("name").toString();
                version = file.value("version").toString();
                category = file.value("category_name").toString();
                fileName = file.value("file_name").toString();
                if (fileName.isEmpty() && !title.isEmpty()) {
                    const QString extension = file.value("detected_file_extension").toString();
                    fileName = extension.isEmpty()
                        ? title
                        : title + QLatin1Char('.') + extension;
                }
            }
            if (fileName.isEmpty())
                fileName = QUrl(downloadUri).fileName();
            if (fileName.isEmpty())
                fileName = QStringLiteral("Nexus-mod-%1-%2.zip").arg(modId).arg(fileId);

            const bool isUpdate = isNexusUpdateDownload(m_nexusGameDomain, modId, fileId);
            queueNexusDownloadProvenance(m_nexusGameDomain, modId, fileId, fileName,
                                         title.isEmpty() ? modName : title, version, category);
            startDownloadFromDirectUrl(QUrl(downloadUri), fileName,
                                       title.isEmpty() ? modName : title, isUpdate);
        });
    });
}

bool ModManager::registerNxmProtocol() {
#ifdef Q_OS_WIN
    QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());

    QSettings reg("HKEY_CURRENT_USER\\Software\\Classes\\nxm", QSettings::NativeFormat);
    reg.setValue(".", "URL:Nexus Mod Manager Protocol");
    reg.setValue("URL Protocol", "");

    QSettings shellReg("HKEY_CURRENT_USER\\Software\\Classes\\nxm\\shell\\open\\command", QSettings::NativeFormat);
    shellReg.setValue(".", QString("\"%1\" \"%2\"").arg(appPath, "%1"));

    return true;
#else
    return false;
#endif
}

bool ModManager::isNxmProtocolRegistered() const {
#ifdef Q_OS_WIN
    const QSettings commandReg(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\nxm\\shell\\open\\command"),
        QSettings::NativeFormat);
    const QString command = commandReg.value(QStringLiteral(".")).toString();
    if (command.isEmpty())
        return false;

    const QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    return command.contains(appPath, Qt::CaseInsensitive);
#else
    return false;
#endif
}

bool ModManager::unregisterNxmProtocol() {
#ifdef Q_OS_WIN
    if (!isNxmProtocolRegistered())
        return true;

    QSettings classesReg(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes"),
                         QSettings::NativeFormat);
    classesReg.remove(QStringLiteral("nxm"));
    classesReg.sync();
    return !isNxmProtocolRegistered();
#else
    return false;
#endif
}
