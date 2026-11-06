#pragma once

#include <QObject>
#include <QTimer>
#include <atomic>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

class ModManager;

class AggressiveRam : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(qint64 ramThresholdMB READ ramThresholdMB WRITE setRamThresholdMB NOTIFY ramThresholdMBChanged)
    Q_PROPERTY(int checkIntervalMs READ checkIntervalMs WRITE setCheckIntervalMs NOTIFY checkIntervalMsChanged)
    Q_PROPERTY(qint64 currentRamUsageMB READ currentRamUsageMB NOTIFY currentRamUsageMBChanged)
    Q_PROPERTY(int cleanupCount READ cleanupCount NOTIFY cleanupCountChanged)
    Q_PROPERTY(QString lastCleanupTime READ lastCleanupTime NOTIFY lastCleanupTimeChanged)

public:
    explicit AggressiveRam(QObject *parent = nullptr, ModManager *modManager = nullptr);
    ~AggressiveRam();

    bool enabled() const;
    qint64 ramThresholdMB() const;
    int checkIntervalMs() const;
    qint64 currentRamUsageMB() const;
    int cleanupCount() const;
    QString lastCleanupTime() const;

    void setEnabled(bool enabled);
    void setRamThresholdMB(qint64 thresholdMB);
    void setCheckIntervalMs(int intervalMs);

    Q_INVOKABLE void performCleanup();

    Q_INVOKABLE qint64 getCurrentProcessMemoryMB();

    void saveSettings();
    void loadSettings();

signals:
    void enabledChanged();
    void ramThresholdMBChanged();
    void checkIntervalMsChanged();
    void currentRamUsageMBChanged();
    void cleanupCountChanged();
    void lastCleanupTimeChanged();
    void cleanupPerformed(qint64 beforeMB, qint64 afterMB);

private slots:
    void checkMemoryUsage();

private:
    void performMemoryCleanup();
    qint64 getProcessWorkingSetSize();
    void emptyWorkingSet();
    void releaseUnusedMemory();

    QTimer* m_monitorTimer;
    bool m_enabled;
    qint64 m_ramThresholdMB;
    int m_checkIntervalMs;
    std::atomic<qint64> m_currentRamUsageMB;
    std::atomic<int> m_cleanupCount;
    QString m_lastCleanupTime;
    ModManager* m_modManager;

#ifdef Q_OS_WIN
    HANDLE m_processHandle;
#endif
};
