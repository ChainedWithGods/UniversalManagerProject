#pragma once

#include <QObject>
#include <QVariantList>
#include <QUrl>

class QTimer;
class QWebEnginePage;
class QWebEngineProfile;
class NexusWebSession;

class NexusMedia : public QObject
{
    Q_OBJECT

public:
    explicit NexusMedia(NexusWebSession* webSession, QObject* parent = nullptr);
    ~NexusMedia() override;

    void fetch(const QString& gameDomain, int modId);
    void cancel();

signals:
    void imagesReady(const QString& key, const QVariantList& images);
    void loadingChanged(const QString& key, bool loading);

private:
    void ensureEngine();
    void onLoadFinished(bool ok);
    void startLoad();
    void scheduleExtraction(int delayMs);
    void runExtraction();
    void finishWithImages(const QVariantList& images);
    void finishWithoutImages();
    void setLoading(bool loading);

    QString cacheRoot() const;
    QString cacheFilePath(const QString& key) const;
    QVariantList readCache(const QString& path, bool& fresh) const;
    void writeCache(const QString& path, const QVariantList& images) const;

    static QString makeKey(const QString& gameDomain, int modId);
    static QVariantList sanitizeImages(const QVariantList& raw);

    QWebEngineProfile* m_profile = nullptr;
    QWebEnginePage* m_page = nullptr;
    NexusWebSession* m_webSession = nullptr;
    QTimer* m_retryTimer = nullptr;
    QTimer* m_timeoutTimer = nullptr;
    QString m_currentKey;
    QString m_currentDomain;
    int m_currentModId = 0;
    QUrl m_pendingUrl;
    bool m_loading = false;
    int m_attempt = 0;
    quint64 m_generation = 0;
};
