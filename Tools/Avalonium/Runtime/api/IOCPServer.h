#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "../core/Types.h"
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>

class IOCPServer {
public:
    IOCPServer();
    ~IOCPServer();

    bool Start(int port = 20030);

    void Stop();

private:
    void ServerThread();
    std::string ProcessCommand(const std::string& command);
    void ServeJsonSession(SOCKET sock, std::string buffer);
    void CleanupSocket(SOCKET sock);

    SOCKET m_ListenSocket;
    std::atomic<bool> m_Running;
    std::thread m_Thread;
    int m_Port;
};
