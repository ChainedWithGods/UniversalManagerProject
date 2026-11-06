#include "NexusMedia.h"
#include "NexusWebSession.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTimer>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineUrlRequestInterceptor>

namespace {

constexpr qint64 kCacheTtlSeconds = 7 * 24 * 60 * 60;
constexpr int kMaxExtractionAttempts = 20;
constexpr int kExtractionRetryMs = 1200;
constexpr int kLoadTimeoutMs = 30000;

const char* kExtractionScript = R"JS(
(() => {
    if (location.host.toLowerCase() !== '__EXPECTED_HOST__'
        || location.pathname !== '__EXPECTED_PATH__') {
        return '[]';
    }
    const out = [];
    const seen = new Set();
    const clean = (value) => {
        if (!value) return '';
        try {
            const url = new URL(value, location.href);
            if (url.protocol !== 'https:') return '';
            if (url.hostname !== 'nexusmods.com' && !url.hostname.endsWith('.nexusmods.com')) return '';
            return url.href;
        } catch (error) {
            return '';
        }
    };
    const add = (full, thumb, caption) => {
        full = clean(full);
        if (!full || seen.has(full)) return;
        seen.add(full);
        out.push({
            fullUrl: full,
            thumbUrl: clean(thumb) || full,
            caption: String(caption || '').trim()
        });
    };
    document.querySelectorAll('a.mod-image[href]').forEach((item) => {
        add(item.getAttribute('href'),
            item.getAttribute('data-exthumbimage'),
            item.getAttribute('data-sub-html'));
    });
    document.querySelectorAll('li.thumb[data-src]').forEach((item) => {
        add(item.getAttribute('data-src'),
            item.getAttribute('data-exthumbimage'),
            item.getAttribute('data-sub-html'));
    });

    return JSON.stringify(out);
})()
)JS";

class NexusRequestInterceptor final : public QWebEngineUrlRequestInterceptor
{
public:
    explicit NexusRequestInterceptor(QObject* parent)
        : QWebEngineUrlRequestInterceptor(parent)
    {
    }

    void interceptRequest(QWebEngineUrlRequestInfo& info) override
    {
        const QString host = info.requestUrl().host().toLower();
        const bool allowedHost = host == "nexusmods.com"
            || host.endsWith(".nexusmods.com")
            || host == "cloudflare.com"
            || host.endsWith(".cloudflare.com");

        if (!allowedHost) {
            info.block(true);
            return;
        }

        switch (info.resourceType()) {
        case QWebEngineUrlRequestInfo::ResourceTypeStylesheet:
        case QWebEngineUrlRequestInfo::ResourceTypeScript:
        case QWebEngineUrlRequestInfo::ResourceTypeImage:
        case QWebEngineUrlRequestInfo::ResourceTypeFontResource:
        case QWebEngineUrlRequestInfo::ResourceTypeMedia:
        case QWebEngineUrlRequestInfo::ResourceTypeObject:
        case QWebEngineUrlRequestInfo::ResourceTypePing:
        case QWebEngineUrlRequestInfo::ResourceTypeFavicon:
        case QWebEngineUrlRequestInfo::ResourceTypePrefetch:
        case QWebEngineUrlRequestInfo::ResourceTypeCspReport:
            info.block(true);
            break;
        default:
            break;
        }
    }
};

class SilentWebEnginePage final : public QWebEnginePage
{
public:
    explicit SilentWebEnginePage(QWebEngineProfile* profile, QObject* parent = nullptr)
        : QWebEnginePage(profile, parent)
    {
    }

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString& message,
                                  int lineNumber,
                                  const QString& sourceID) override
    {
        Q_UNUSED(level);
        Q_UNUSED(message);
        Q_UNUSED(lineNumber);
        Q_UNUSED(sourceID);

    }
};

}

NexusMedia::NexusMedia(NexusWebSession* webSession, QObject* parent)
    : QObject(parent)
    , m_webSession(webSession)
{
    m_retryTimer = new QTimer(this);
    m_retryTimer->setSingleShot(true);
    connect(m_retryTimer, &QTimer::timeout, this, &NexusMedia::runExtraction);

    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &NexusMedia::finishWithoutImages);
}

NexusMedia::~NexusMedia() = default;

QString NexusMedia::makeKey(const QString& gameDomain, int modId)
{
    return gameDomain.trimmed().toLower() + '/' + QString::number(modId);
}

QString NexusMedia::cacheRoot() const
{
    return NexusWebSession::mediaCacheRoot();
}

QString NexusMedia::cacheFilePath(const QString& key) const
{
    QString safe = key;
    safe.replace('/', '_');
    safe.replace(QRegularExpression("[^a-zA-Z0-9_.-]"), "_");
    return cacheRoot() + "/" + safe + ".json";
}

QVariantList NexusMedia::sanitizeImages(const QVariantList& raw)
{
    QVariantList result;
    QSet<QString> seen;

    for (const QVariant& entry : raw) {
        const QVariantMap image = entry.toMap();
        const QString fullUrl = image.value("fullUrl").toString().trimmed();
        const QString thumbValue = image.value("thumbUrl").toString().trimmed();

        const auto isNexusUrl = [](const QString& value) {
            const QUrl url(value);
            if (!url.isValid() || url.scheme() != "https") {
                return false;
            }
            const QString host = url.host().toLower();
            return host == "nexusmods.com" || host.endsWith(".nexusmods.com");
        };

        if (!isNexusUrl(fullUrl) || seen.contains(fullUrl)) {
            continue;
        }

        seen.insert(fullUrl);

        QVariantMap clean;
        clean["fullUrl"] = fullUrl;
        clean["thumbUrl"] = isNexusUrl(thumbValue) ? thumbValue : fullUrl;
        clean["caption"] = image.value("caption").toString().trimmed();
        result.append(clean);
    }

    return result;
}

QVariantList NexusMedia::readCache(const QString& path, bool& fresh) const
{
    fresh = false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return {};
    }

    const QJsonObject root = document.object();

    if (root.value("version").toInt(0) < 3) {
        return {};
    }

    const QVariantList images = sanitizeImages(root.value("images").toArray().toVariantList());
    if (images.isEmpty()) {
        return {};
    }

    qint64 savedAt = static_cast<qint64>(root.value("savedAt").toDouble(0));
    if (savedAt <= 0) {
        savedAt = QFileInfo(path).lastModified().toSecsSinceEpoch();
    }

    const qint64 age = QDateTime::currentSecsSinceEpoch() - savedAt;
    fresh = age >= 0 && age < kCacheTtlSeconds;
    return images;
}

void NexusMedia::writeCache(const QString& path, const QVariantList& images) const
{
    if (images.isEmpty()) {
        return;
    }

    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonArray imageArray;
    for (const QVariant& entry : images) {
        const QVariantMap image = entry.toMap();
        QJsonObject object;
        object["fullUrl"] = image.value("fullUrl").toString();
        object["thumbUrl"] = image.value("thumbUrl").toString();
        object["caption"] = image.value("caption").toString();
        imageArray.append(object);
    }

    QJsonObject root;
    root["version"] = 3;
    root["savedAt"] = static_cast<double>(QDateTime::currentSecsSinceEpoch());
    root["images"] = imageArray;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.commit();
}

void NexusMedia::fetch(const QString& gameDomain, int modId)
{
    const QString domain = gameDomain.trimmed().toLower();
    if (domain.isEmpty() || modId <= 0) {
        return;
    }

    m_currentKey = makeKey(domain, modId);
    m_currentDomain = domain;
    m_currentModId = modId;
    m_attempt = 0;
    ++m_generation;

    m_retryTimer->stop();
    m_timeoutTimer->stop();

    const QString path = cacheFilePath(m_currentKey);
    bool fresh = false;
    const QVariantList cached = readCache(path, fresh);

    if (!cached.isEmpty()) {
        emit imagesReady(m_currentKey, cached);
    }

    if (!cached.isEmpty() && fresh) {
        setLoading(false);
        return;
    }

    startLoad();
}

void NexusMedia::cancel()
{
    m_retryTimer->stop();
    m_timeoutTimer->stop();
    ++m_generation;
    if (m_page) {
        m_page->triggerAction(QWebEnginePage::Stop);
    }
    setLoading(false);
}

void NexusMedia::ensureEngine()
{
    if (m_page) {
        return;
    }
    if (!m_webSession || !m_webSession->mediaProfile()) {
        return;
    }

    m_profile = m_webSession->mediaProfile();
    m_profile->setUrlRequestInterceptor(new NexusRequestInterceptor(m_profile));

    m_page = new SilentWebEnginePage(m_profile, this);
    connect(m_page, &QWebEnginePage::loadFinished, this, &NexusMedia::onLoadFinished);
}

void NexusMedia::startLoad()
{
    ensureEngine();
    if (!m_page) {
        setLoading(false);
        return;
    }

    setLoading(true);
    m_pendingUrl = QUrl(QStringLiteral("https://www.nexusmods.com/%1/mods/%2?tab=images")
                            .arg(m_currentDomain, QString::number(m_currentModId)));
    m_attempt = 0;
    m_timeoutTimer->start(kLoadTimeoutMs);
    m_page->load(m_pendingUrl);

    scheduleExtraction(kExtractionRetryMs);
}

void NexusMedia::onLoadFinished(bool ok)
{
    if (m_currentKey.isEmpty() || !m_loading || !m_page) {
        return;
    }

    const QUrl current = m_page->url();
    if (current.host().compare(m_pendingUrl.host(), Qt::CaseInsensitive) != 0
        || current.path() != m_pendingUrl.path()) {
        return;
    }

    Q_UNUSED(ok);
    m_attempt = 0;
    scheduleExtraction(150);
}

void NexusMedia::scheduleExtraction(int delayMs)
{
    if (m_currentKey.isEmpty() || !m_loading) {
        return;
    }
    m_retryTimer->stop();
    m_retryTimer->start(delayMs);
}

void NexusMedia::runExtraction()
{
    if (m_currentKey.isEmpty() || !m_loading || !m_page) {
        return;
    }

    const quint64 generation = m_generation;
    QString script = QString::fromLatin1(kExtractionScript);
    script.replace(QStringLiteral("__EXPECTED_HOST__"), m_pendingUrl.host());
    script.replace(QStringLiteral("__EXPECTED_PATH__"), m_pendingUrl.path());
    m_page->runJavaScript(script,
                          [this, generation](const QVariant& value) {
        if (generation != m_generation || !m_loading) {
            return;
        }

        const QJsonDocument document = QJsonDocument::fromJson(value.toString().toUtf8());
        const QVariantList images = document.isArray()
            ? sanitizeImages(document.toVariant().toList())
            : QVariantList();

        if (!images.isEmpty()) {
            finishWithImages(images);
            return;
        }

        ++m_attempt;
        if (m_attempt < kMaxExtractionAttempts) {
            scheduleExtraction(kExtractionRetryMs);
        } else {
            finishWithoutImages();
        }
    });
}

void NexusMedia::finishWithImages(const QVariantList& images)
{
    m_retryTimer->stop();
    m_timeoutTimer->stop();
    ++m_generation;

    writeCache(cacheFilePath(m_currentKey), images);
    emit imagesReady(m_currentKey, images);
    setLoading(false);
}

void NexusMedia::finishWithoutImages()
{
    m_retryTimer->stop();
    m_timeoutTimer->stop();
    ++m_generation;
    setLoading(false);
}

void NexusMedia::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    emit loadingChanged(m_currentKey, loading);
}
