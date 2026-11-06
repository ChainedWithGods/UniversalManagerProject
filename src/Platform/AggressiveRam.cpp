#include "AggressiveRam.h"
#include "modmanager.h"
#include <QDateTime>
#include <QDebug>
#include <QThread>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "kernel32.lib")
#endif

AggressiveRam::AggressiveRam(QObject *parent, ModManager *modManager)
    : QObject(parent)
    , m_monitorTimer(new QTimer(this))
    , m_enabled(true)
    , m_ramThresholdMB(700)
    , m_checkIntervalMs(1500)
    , m_currentRamUsageMB(0)
    , m_cleanupCount(0)
    , m_modManager(modManager)
{
#ifdef Q_OS_WIN
    m_processHandle = GetCurrentProcess();
#endif

    connect(m_monitorTimer, &QTimer::timeout, this, &AggressiveRam::checkMemoryUsage);

    loadSettings();

    if (m_enabled) {
        m_monitorTimer->start(m_checkIntervalMs);
    }
}

AggressiveRam::~AggressiveRam()
{
    if (m_monitorTimer) {
        m_monitorTimer->stop();
    }

    saveSettings();
}

bool AggressiveRam::enabled() const
{
    return m_enabled;
}

qint64 AggressiveRam::ramThresholdMB() const
{
    return m_ramThresholdMB;
}

int AggressiveRam::checkIntervalMs() const
{
    return m_checkIntervalMs;
}

qint64 AggressiveRam::currentRamUsageMB() const
{
    return m_currentRamUsageMB.load();
}

int AggressiveRam::cleanupCount() const
{
    return m_cleanupCount.load();
}

QString AggressiveRam::lastCleanupTime() const
{
    return m_lastCleanupTime;
}

void AggressiveRam::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;

    if (m_enabled) {
        m_monitorTimer->start(m_checkIntervalMs);
        qDebug() << "Aggressive RAM cleaner ENABLED - Threshold:" << m_ramThresholdMB << "MB";
    } else {
        m_monitorTimer->stop();
        qDebug() << "Aggressive RAM cleaner DISABLED";
    }

    saveSettings();
    emit enabledChanged();
}

void AggressiveRam::setRamThresholdMB(qint64 thresholdMB)
{
    if (m_ramThresholdMB == thresholdMB)
        return;

    m_ramThresholdMB = thresholdMB;
    qDebug() << "RAM threshold changed to:" << m_ramThresholdMB << "MB";
    saveSettings();
    emit ramThresholdMBChanged();
}

void AggressiveRam::setCheckIntervalMs(int intervalMs)
{
    if (m_checkIntervalMs == intervalMs)
        return;

    m_checkIntervalMs = intervalMs;

    if (m_enabled && m_monitorTimer->isActive()) {
        m_monitorTimer->stop();
        m_monitorTimer->start(m_checkIntervalMs);
    }

    saveSettings();
    emit checkIntervalMsChanged();
}

void AggressiveRam::checkMemoryUsage()
{
    if (!m_enabled)
        return;

    qint64 currentUsageMB = getProcessWorkingSetSize();
    m_currentRamUsageMB.store(currentUsageMB);
    emit currentRamUsageMBChanged();

    if (currentUsageMB >= m_ramThresholdMB) {
        qDebug() << "RAM threshold exceeded:" << currentUsageMB << "MB >=" << m_ramThresholdMB << "MB - Performing cleanup";
        performMemoryCleanup();
    }
}

void AggressiveRam::performCleanup()
{
    qDebug() << "Manual RAM cleanup triggered";
    performMemoryCleanup();
}

qint64 AggressiveRam::getCurrentProcessMemoryMB()
{
    return getProcessWorkingSetSize();
}

qint64 AggressiveRam::getProcessWorkingSetSize()
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(m_processHandle, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {

        return static_cast<qint64>(pmc.WorkingSetSize / (1024 * 1024));
    }
#endif
    return 0;
}

void AggressiveRam::performMemoryCleanup()
{
    qint64 beforeMB = getProcessWorkingSetSize();

    if (m_modManager) {
        m_modManager->trimMemory();
    }

    emptyWorkingSet();

    releaseUnusedMemory();

#ifdef Q_OS_WIN
    SetProcessWorkingSetSizeEx(m_processHandle,
                               static_cast<SIZE_T>(-1),
                               static_cast<SIZE_T>(-1),
                               0);
#endif

    QThread::msleep(250);

    qint64 afterMB = getProcessWorkingSetSize();
    qint64 freedMB = beforeMB - afterMB;

    m_cleanupCount++;
    m_lastCleanupTime = QDateTime::currentDateTime().toString("hh:mm:ss");

    emit cleanupCountChanged();
    emit lastCleanupTimeChanged();
    emit cleanupPerformed(beforeMB, afterMB);

    qDebug() << "RAM Cleanup completed:"
             << "Before:" << beforeMB << "MB"
             << "After:" << afterMB << "MB"
             << "Freed:" << freedMB << "MB"
             << "Total cleanups:" << m_cleanupCount.load();
}

void AggressiveRam::emptyWorkingSet()
{
#ifdef Q_OS_WIN

    if (m_processHandle != INVALID_HANDLE_VALUE) {
        EmptyWorkingSet(m_processHandle);
    }
#endif
}

void AggressiveRam::releaseUnusedMemory()
{
#ifdef Q_OS_WIN

    if (m_processHandle != INVALID_HANDLE_VALUE) {

        SIZE_T minSize = 1024 * 1024;
        SIZE_T maxSize = static_cast<SIZE_T>(-1);
        SetProcessWorkingSetSize(m_processHandle, minSize, maxSize);

        SetProcessWorkingSetSize(m_processHandle,
                                static_cast<SIZE_T>(-1),
                                static_cast<SIZE_T>(-1));
    }
#endif
}

void AggressiveRam::saveSettings()
{
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataDir.isEmpty()) {
        appDataDir = QDir::currentPath();
    }

    QDir dir(appDataDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QString settingsPath = appDataDir + "/aggressive_ram_settings.json";

    QJsonObject obj;
    obj["enabled"] = m_enabled;
    obj["ramThresholdMB"] = m_ramThresholdMB;
    obj["checkIntervalMs"] = m_checkIntervalMs;

    QJsonDocument doc(obj);
    QFile file(settingsPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }
}

void AggressiveRam::loadSettings()
{
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataDir.isEmpty()) {
        appDataDir = QDir::currentPath();
    }

    QString settingsPath = appDataDir + "/aggressive_ram_settings.json";
    QFile file(settingsPath);

    if (!file.exists()) {

        return;
    }

    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            m_enabled = obj.value("enabled").toBool(true);
            m_ramThresholdMB = obj.value("ramThresholdMB").toInt(700);
            m_checkIntervalMs = obj.value("checkIntervalMs").toInt(1500);
        }
    }
}
