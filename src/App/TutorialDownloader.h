#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QSaveFile;

class TutorialDownloader : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(qint64 bytesReceived READ bytesReceived NOTIFY progressChanged)
    Q_PROPERTY(qint64 bytesTotal READ bytesTotal NOTIFY progressChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(QString localPath READ localPath NOTIFY localPathChanged)
    Q_PROPERTY(QString currentRelativePath READ currentRelativePath NOTIFY currentRelativePathChanged)

public:
    explicit TutorialDownloader(QObject* parent = nullptr);

    QString state() const { return m_state; }
    qreal progress() const { return m_progress; }
    qint64 bytesReceived() const { return m_bytesReceived; }
    qint64 bytesTotal() const { return m_bytesTotal; }
    QString errorText() const { return m_errorText; }
    QString localPath() const { return m_localPath; }
    QString currentRelativePath() const { return m_currentRelativePath; }

    Q_INVOKABLE QString resolveVideo(const QString& relativePath) const;
    Q_INVOKABLE void download(const QString& relativePath, const QUrl& url);
    Q_INVOKABLE void cancel();

signals:
    void stateChanged();
    void progressChanged();
    void errorTextChanged();
    void localPathChanged();
    void currentRelativePathChanged();
    void downloadCompleted(const QString& relativePath, const QString& localPath);

private:
    void handleFinished();
    void fail(const QString& error);
    void cleanupTransfer();
    QString installedPathFor(const QString& relativePath) const;
    QString downloadPathFor(const QString& relativePath) const;
    void setState(const QString& state);
    void setError(const QString& error);

    QNetworkAccessManager* m_network = nullptr;
    QNetworkReply* m_reply = nullptr;
    QSaveFile* m_file = nullptr;
    QString m_state = QStringLiteral("idle");
    QString m_errorText;
    QString m_localPath;
    QString m_currentRelativePath;
    qreal m_progress = 0.0;
    qint64 m_bytesReceived = 0;
    qint64 m_bytesTotal = -1;
    bool m_cancelled = false;
};
