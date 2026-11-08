#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;

class UpdateChecker : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY latestVersionChanged)
    Q_PROPERTY(QString changelog READ changelog NOTIFY changelogChanged)
    Q_PROPERTY(QString releaseUrl READ releaseUrl NOTIFY releaseUrlChanged)
    Q_PROPERTY(QString downloadUrl READ downloadUrl NOTIFY downloadUrlChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(qreal downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(qint64 bytesReceived READ bytesReceived NOTIFY downloadProgressChanged)
    Q_PROPERTY(qint64 bytesTotal READ bytesTotal NOTIFY downloadProgressChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(QString downloadedFilePath READ downloadedFilePath NOTIFY downloadedFilePathChanged)

public:
    explicit UpdateChecker(QObject* parent = nullptr);

    QString currentVersion() const { return m_currentVersion; }
    QString latestVersion() const { return m_latestVersion; }
    QString changelog() const { return m_changelog; }
    QString releaseUrl() const { return m_releaseUrl; }
    QString downloadUrl() const { return m_downloadUrl; }
    QString state() const { return m_state; }
    qreal downloadProgress() const { return m_downloadProgress; }
    qint64 bytesReceived() const { return m_bytesReceived; }
    qint64 bytesTotal() const { return m_bytesTotal; }
    QString errorText() const { return m_errorText; }
    QString downloadedFilePath() const { return m_downloadedFilePath; }

    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void startDownload();
    Q_INVOKABLE void cancelDownload();
    Q_INVOKABLE void openReleasePage() const;
    Q_INVOKABLE void openDownloadedFile() const;

signals:
    void updateAvailable();
    void latestVersionChanged();
    void changelogChanged();
    void releaseUrlChanged();
    void downloadUrlChanged();
    void stateChanged();
    void downloadProgressChanged();
    void errorTextChanged();
    void downloadedFilePathChanged();

private:
    void handleCheckFinished();
    void handleDownloadFinished();
    void writeAvailableBytes();
    void updateDownloadProgress(qint64 received, qint64 total);
    void setState(const QString& state);
    void setError(const QString& errorText);
    void cleanupDownloadFile(bool removePartialFile);

    QNetworkAccessManager* m_network = nullptr;
    QNetworkReply* m_checkReply = nullptr;
    QNetworkReply* m_downloadReply = nullptr;
    QFile* m_downloadFile = nullptr;

    QString m_currentVersion;
    QString m_latestVersion;
    QString m_changelog;
    QString m_releaseUrl;
    QString m_downloadUrl;
    QString m_state = QStringLiteral("idle");
    QString m_errorText;
    QString m_downloadedFilePath;
    qreal m_downloadProgress = 0.0;
    qint64 m_bytesReceived = 0;
    qint64 m_bytesTotal = -1;
    bool m_checked = false;
    bool m_cancelRequested = false;
};
