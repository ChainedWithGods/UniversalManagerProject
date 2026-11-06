#include "NotificationManager.h"

NotificationManager::NotificationManager(QObject* parent)
    : QObject(parent)
{
}

void NotificationManager::notify(const QString& title, const QString& message,
                                 const QString& icon, int duration)
{
    emit notificationPosted(title, message, icon, duration);
}
