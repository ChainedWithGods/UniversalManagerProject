#include "TrayHelper.h"
#include <QCryptographicHash>
#include <QDebug>
#include <memory>

TrayHelper::TrayHelper(const QString& key, QObject* parent)
    : QObject(parent)
    , m_key(key)
    , m_sharedMemory(key)
    , m_localServer(nullptr)
{
}

TrayHelper::~TrayHelper()
{
    cleanup();
}

void TrayHelper::cleanup()
{

    if (m_localServer) {
        m_localServer->close();
        delete m_localServer;
        m_localServer = nullptr;
    }

    QLocalServer::removeServer(m_key);

    if (m_sharedMemory.isAttached()) {
        m_sharedMemory.detach();
    }
}

bool TrayHelper::isAnotherInstanceRunning(const QByteArray& message)
{

    if (m_sharedMemory.attach()) {

        m_sharedMemory.detach();

        QLocalSocket socket;
        socket.connectToServer(m_key);
        if (socket.waitForConnected(1000)) {

            socket.write(message);
            socket.flush();
            socket.waitForBytesWritten(1000);
            socket.disconnectFromServer();
            return true;
        }
        return true;
    }

    if (m_sharedMemory.create(1)) {

        return false;
    }

    if (m_sharedMemory.error() == QSharedMemory::AlreadyExists) {

        if (m_sharedMemory.attach()) {
            m_sharedMemory.detach();
        }
        return true;
    }

    return false;
}

void TrayHelper::listen()
{
    if (m_localServer) {
        return;
    }

    QLocalServer::removeServer(m_key);

    m_localServer = new QLocalServer(this);
    connect(m_localServer, &QLocalServer::newConnection, this, &TrayHelper::handleNewConnection);

    if (!m_localServer->listen(m_key)) {
        qWarning() << "TrayHelper: Unable to start local server:" << m_localServer->errorString();
    }
}

void TrayHelper::handleNewConnection()
{
    QLocalSocket* socket = m_localServer->nextPendingConnection();
    if (!socket) {
        return;
    }

    auto buffer = std::make_shared<QByteArray>();
    connect(socket, &QLocalSocket::readyRead, this, [this, socket, buffer]() {
        buffer->append(socket->readAll());

        int newline = -1;
        while ((newline = buffer->indexOf('\n')) >= 0) {
            const QByteArray line = buffer->left(newline).trimmed();
            buffer->remove(0, newline + 1);

            if (line == "SHOW")
                emit anotherInstanceStarted();
            else if (line.startsWith("NXM "))
                emit nxmUrlReceived(QString::fromUtf8(line.mid(4)));
        }
    });

    connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
}
