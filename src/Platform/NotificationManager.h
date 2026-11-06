#pragma once

#include <QObject>
#include <QString>

class NotificationManager : public QObject
{
    Q_OBJECT

public:
    explicit NotificationManager(QObject* parent = nullptr);

    Q_INVOKABLE void notify(const QString& title, const QString& message,
                            const QString& icon = QString(), int duration = 3200);

signals:
    void notificationPosted(const QString& title, const QString& message,
                            const QString& icon, int duration);
};
