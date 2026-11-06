#ifndef TRAYHELPER_H
#define TRAYHELPER_H

#include <QObject>
#include <QByteArray>
#include <QSharedMemory>
#include <QLocalServer>
#include <QLocalSocket>
#include <QString>

class TrayHelper : public QObject
{
    Q_OBJECT

public:
    explicit TrayHelper(const QString& key, QObject* parent = nullptr);
    ~TrayHelper();

    bool isAnotherInstanceRunning(const QByteArray& message = QByteArrayLiteral("SHOW\n"));
    void listen();
    Q_INVOKABLE void cleanup();

signals:
    void anotherInstanceStarted();
    void nxmUrlReceived(const QString& url);

private slots:
    void handleNewConnection();

private:
    QString m_key;
    QSharedMemory m_sharedMemory;
    QLocalServer* m_localServer;
};

#endif
