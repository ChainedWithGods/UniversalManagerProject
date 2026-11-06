#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;

class DownloadManager : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        FileNameRole,
        StateRole,
        ProgressRole,
        ReceivedRole,
        TotalRole,
        SpeedRole,
        EtaRole,
        ErrorRole,
        FilePathRole,
        DateAddedRole,
    };

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

    explicit DownloadManager(QObject* parent = nullptr);
    ~DownloadManager() override;

    int count() const { return m_tasks.size(); }
    int revision() const { return m_revision; }
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantList snapshot() const;

    Q_INVOKABLE void enqueue(const QUrl& url, const QString& fileName, const QString& displayName,
                             const QString& directory);

    Q_INVOKABLE void pause(int id);
    Q_INVOKABLE void resume(int id);
    Q_INVOKABLE void cancel(int id);
    Q_INVOKABLE void removeEntry(int id);
    Q_INVOKABLE void openFile(int id);
    Q_INVOKABLE void openFolder(int id);

signals:
    void countChanged();
    void revisionChanged();
    void downloadStarted();
    void downloadCompleted(const QString& filePath);

private:
    struct Task {
        int id = 0;
        QString fileName;
        QString displayName;
        QString directory;
        QString filePath;
        QDateTime addedAt;
        QUrl url;
        QFile* file = nullptr;
        QNetworkReply* reply = nullptr;
        qint64 baseBytes = 0;
        qint64 received = 0;
        qint64 total = 0;
        double speed = 0.0;
        qint64 etaSeconds = -1;
        QString state;
        QString error;
        bool rangeRequested = false;
        bool rangeChecked = false;
        qint64 sampleTimeMs = 0;
        qint64 sampleBytes = 0;
        qint64 lastNotifyMs = 0;
    };

    Task* taskById(int id) const;
    int rowById(int id) const;
    void startTask(Task* task);
    void stopNetwork(Task* task);
    void updateProgress(Task* task);
    void emitTaskChanged(Task* task);
    void finishTask(Task* task, bool success, const QString& error);
    QString uniqueFilePath(const QString& directory, const QString& fileName) const;
    bool disconnectReply(Task* task);
    bool hasActiveDownload() const;
    void startNextQueued();
    void scheduleNextQueued();

    QNetworkAccessManager* m_network;
    QList<Task*> m_tasks;
    QElapsedTimer m_clock;
    int m_nextId = 1;
    int m_revision = 0;
};
