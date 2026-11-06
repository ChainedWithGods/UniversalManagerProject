#include "IOCPServer.h"
#include "IPCEvents.h"
#include "IPCJobs.h"
#include "IPCQuery.h"
#include "UEInternalAPI.h"
#include "../core/EngineDiscovery.h"
#include "../lua/LuaManager.h"
extern "C" {
#include "../utils/SafeMemory.h"
}
#include <sstream>
#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>

extern void Log(const char* fmt, ...);
extern EngineState g_State;

#pragma comment(lib, "ws2_32.lib")

static std::vector<std::string> TokenizeCommandArgs(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;

    for (char ch : text) {
        if (ch == '"') {
            inQuotes = !inQuotes;
            continue;
        }
        if (!inQuotes && (ch == ' ' || ch == '\t')) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

static std::string TrimQuotedPath(std::string path) {
    if (path.size() >= 2 && path.front() == '"' && path.back() == '"') {
        path = path.substr(1, path.length() - 2);
    }
    return path;
}

static std::string GetDefaultRuntimeGameRoot() {
    std::string logPath = EngineDiscovery::GetLogPath();
    std::string baseDir;

    size_t pos = logPath.find_last_of("\\/");
    if (pos != std::string::npos) {
        baseDir = logPath.substr(0, pos + 1);
    } else {
        char dllPath[MAX_PATH] = {};
        HMODULE hMod = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)&GetDefaultRuntimeGameRoot, &hMod)) {
            GetModuleFileNameA(hMod, dllPath, MAX_PATH);
            FreeLibrary(hMod);
            std::string dir(dllPath);
            size_t p = dir.find_last_of("\\/");
            if (p != std::string::npos) {
                baseDir = dir.substr(0, p + 1);
            }
        }
    }

    if (baseDir.empty()) {
        char tempDir[MAX_PATH] = {};
        GetTempPathA(MAX_PATH, tempDir);
        baseDir = std::string(tempDir) + "Avalonium\\";
    }

    std::string gameTitle;
    if (g_State.GameName[0] != '\0') {
        gameTitle = g_State.GameName;
    } else {
        gameTitle = EngineDiscovery::GetMainWindowTitle();
    }
    if (gameTitle.empty()) {
        gameTitle = "UnknownGame";
    }

    for (char &p : gameTitle) {
        if (p == '/' || p == '\\' || p == ':' || p == '*' || p == '?' ||
            p == '"' || p == '<' || p == '>' || p == '|') {
            p = '_';
        }
    }

    std::string rootGameFolder = baseDir + gameTitle;
    std::error_code ec;
    std::filesystem::create_directories(rootGameFolder, ec);
    return rootGameFolder;
}

static std::string ResolveUsmapOutputPath(const std::string& argument) {
    if (!argument.empty()) return TrimQuotedPath(argument);

    const std::string usmapFolder = GetDefaultRuntimeGameRoot() + "\\USMAP";
    std::error_code ec;
    std::filesystem::create_directories(usmapFolder, ec);

    std::string gameTitle;
    if (g_State.GameName[0] != '\0') {
        gameTitle = g_State.GameName;
    } else {
        gameTitle = EngineDiscovery::GetMainWindowTitle();
    }
    if (gameTitle.empty()) {
        gameTitle = "UnknownGame";
    }
    for (char& p : gameTitle) {
        if (p == '/' || p == '\\' || p == ':' || p == '*' || p == '?' ||
            p == '"' || p == '<' || p == '>' || p == '|') {
            p = '_';
        }
    }

    return usmapFolder + "\\" + gameTitle + ".usmap";
}

#include <map>

static std::string NormalizeCommandName(std::string cmd) {

    if (cmd.size() >= 3 &&
        static_cast<unsigned char>(cmd[0]) == 0xEF &&
        static_cast<unsigned char>(cmd[1]) == 0xBB &&
        static_cast<unsigned char>(cmd[2]) == 0xBF) {
        cmd.erase(0, 3);
    }

    cmd.erase(std::remove_if(cmd.begin(), cmd.end(), [](unsigned char ch) {
        return std::iscntrl(ch) || std::isspace(ch);
    }), cmd.end());

    std::transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });

    return cmd;
}

static int ClampCount(int value, int maxValue = 1000000) {
    if (value <= 0) return 0;
    if (value > maxValue) return maxValue;
    return value;
}

static int ReadCommand(SOCKET sock, std::string& out) {
    char buffer[65536];
    out.clear();

    while (out.size() < sizeof(buffer) - 1) {
        int received = recv(sock, buffer, (int)sizeof(buffer) - 1, 0);

        if (received > 0) {
            out.append(buffer, received);
            if (out.find('\n') != std::string::npos) return (int)out.size();
            continue;
        }

        if (received == 0) return (int)out.size();
        return out.empty() ? -1 : (int)out.size();
    }

    return (int)out.size();
}

static bool SendAll(SOCKET sock, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        int n = send(sock, data.c_str() + sent, (int)(data.size() - sent), 0);
        if (n <= 0) return false;
        sent += (size_t)n;
    }
    return true;
}

struct RequestValue {
    enum Kind { Null, Bool, Number, String, Array, Object } kind = Null;
    std::string text;
    std::vector<std::string> array;
    std::map<std::string, std::string> object;
};

static void SkipRequestSpace(const std::string& s, size_t& i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) ++i;
}

static void AppendUtf8(std::string& out, unsigned codepoint) {
    if (codepoint <= 0x7F) {
        out += static_cast<char>(codepoint);
    } else if (codepoint <= 0x7FF) {
        out += static_cast<char>(0xC0 | (codepoint >> 6));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else {
        out += static_cast<char>(0xE0 | (codepoint >> 12));
        out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
}

static bool ParseRequestString(const std::string& s, size_t& i, std::string& out) {
    if (i >= s.size() || s[i] != '"') return false;
    ++i;
    out.clear();
    while (i < s.size()) {
        char c = s[i++];
        if (c == '"') return true;
        if (c != '\\') { out += c; continue; }
        if (i >= s.size()) return false;
        char esc = s[i++];
        switch (esc) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                if (i + 4 > s.size()) return false;
                unsigned cp = 0;
                for (int k = 0; k < 4; ++k) {
                    char h = s[i++];
                    cp <<= 4;
                    if (h >= '0' && h <= '9') cp |= static_cast<unsigned>(h - '0');
                    else if (h >= 'a' && h <= 'f') cp |= static_cast<unsigned>(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') cp |= static_cast<unsigned>(h - 'A' + 10);
                    else return false;
                }
                AppendUtf8(out, cp);
                break;
            }
            default: out += esc; break;
        }
    }
    return false;
}

static bool ParseRequestObject(const std::string& s, size_t& i, std::map<std::string, std::string>& out);

static bool ParseRequestValue(const std::string& s, size_t& i, RequestValue& v) {
    SkipRequestSpace(s, i);
    if (i >= s.size()) return false;

    if (s[i] == '"') {
        v.kind = RequestValue::String;
        return ParseRequestString(s, i, v.text);
    }

    if (s[i] == '[') {
        ++i;
        v.kind = RequestValue::Array;
        for (;;) {
            SkipRequestSpace(s, i);
            if (i < s.size() && s[i] == ']') { ++i; return true; }
            RequestValue item;
            if (!ParseRequestValue(s, i, item)) return false;
            if (item.kind == RequestValue::String || item.kind == RequestValue::Number ||
                item.kind == RequestValue::Bool || item.kind == RequestValue::Null) {
                v.array.push_back(item.text);
            } else {
                return false;
            }
            SkipRequestSpace(s, i);
            if (i < s.size() && s[i] == ',') { ++i; continue; }
            if (i < s.size() && s[i] == ']') { ++i; return true; }
            return false;
        }
    }

    if (s[i] == '{') {
        v.kind = RequestValue::Object;
        return ParseRequestObject(s, i, v.object);
    }

    size_t start = i;
    while (i < s.size() && s[i] != ',' && s[i] != '}' && s[i] != ']' &&
           !std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
    v.text = s.substr(start, i - start);
    if (v.text == "true" || v.text == "false") {
        v.kind = RequestValue::Bool;
        return true;
    }
    if (v.text == "null") {
        v.kind = RequestValue::Null;
        v.text.clear();
        return true;
    }
    if (v.text.empty()) return false;
    for (char ch : v.text) {
        if (!(std::isdigit(static_cast<unsigned char>(ch)) || ch == '-' || ch == '+' ||
              ch == '.' || ch == 'e' || ch == 'E')) {
            return false;
        }
    }
    v.kind = RequestValue::Number;
    return true;
}

static bool ParseRequestObject(const std::string& s, size_t& i, std::map<std::string, std::string>& out) {
    SkipRequestSpace(s, i);
    if (i >= s.size() || s[i] != '{') return false;
    ++i;
    for (;;) {
        SkipRequestSpace(s, i);
        if (i < s.size() && s[i] == '}') { ++i; return true; }
        std::string key;
        if (!ParseRequestString(s, i, key)) return false;
        SkipRequestSpace(s, i);
        if (i >= s.size() || s[i] != ':') return false;
        ++i;
        RequestValue value;
        if (!ParseRequestValue(s, i, value)) return false;
        if (value.kind == RequestValue::Object || value.kind == RequestValue::Array) return false;
        out[key] = value.text;
        SkipRequestSpace(s, i);
        if (i < s.size() && s[i] == ',') { ++i; continue; }
        if (i < s.size() && s[i] == '}') { ++i; return true; }
        return false;
    }
}

static bool ParseRequestLine(const std::string& line, std::map<std::string, RequestValue>& out) {
    size_t i = 0;
    SkipRequestSpace(line, i);
    if (i >= line.size() || line[i] != '{') return false;
    ++i;
    for (;;) {
        SkipRequestSpace(line, i);
        if (i < line.size() && line[i] == '}') {
            ++i;
            SkipRequestSpace(line, i);
            return i >= line.size();
        }
        std::string key;
        if (!ParseRequestString(line, i, key)) return false;
        SkipRequestSpace(line, i);
        if (i >= line.size() || line[i] != ':') return false;
        ++i;
        RequestValue value;
        if (!ParseRequestValue(line, i, value)) return false;
        out[key] = value;
        SkipRequestSpace(line, i);
        if (i < line.size() && line[i] == ',') { ++i; continue; }
        if (i < line.size() && line[i] == '}') {
            ++i;
            SkipRequestSpace(line, i);
            return i >= line.size();
        }
        return false;
    }
}

static std::string EscapeJsonText(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default: out += c; break;
        }
    }
    return out;
}

static std::string QuoteLegacyArg(const std::string& value) {
    bool needsQuotes = value.empty();
    for (char c : value) {
        if (std::isspace(static_cast<unsigned char>(c)) || c == '"') { needsQuotes = true; break; }
    }
    if (!needsQuotes) return value;
    std::string out = "\"";
    for (char c : value) {
        if (c == '"') out += '\\';
        out += c;
    }
    out += '"';
    return out;
}

static bool BuildLegacyRequest(const std::map<std::string, RequestValue>& request,
                               std::string& legacy, std::string& idJson, bool& quit, std::string& error) {
    idJson = "null";
    auto id = request.find("id");
    if (id != request.end()) {
        if (id->second.kind == RequestValue::String) idJson = "\"" + EscapeJsonText(id->second.text) + "\"";
        else if (id->second.kind == RequestValue::Number || id->second.kind == RequestValue::Bool) idJson = id->second.text;
    }

    auto cmdIt = request.find("cmd");
    if (cmdIt == request.end() || cmdIt->second.kind != RequestValue::String || cmdIt->second.text.empty()) {
        error = "missing cmd";
        return false;
    }

    const std::string cmd = NormalizeCommandName(cmdIt->second.text);
    if (cmd == "QUIT" || cmd == "EXIT") {
        quit = true;
        legacy = cmd;
        return true;
    }

    std::string args;
    auto argsIt = request.find("args");
    if (argsIt != request.end()) {
        if (argsIt->second.kind == RequestValue::Object) {
            for (const auto& kv : argsIt->second.object) {
                if (!args.empty()) args += ' ';
                args += kv.first;
                args += '=';
                args += QuoteLegacyArg(kv.second);
            }
        } else if (argsIt->second.kind == RequestValue::Array) {
            for (const auto& item : argsIt->second.array) {
                if (!args.empty()) args += ' ';
                args += QuoteLegacyArg(item);
            }
        } else {
            args = argsIt->second.text;
        }
    }

    legacy = cmd;
    if (!args.empty()) {
        legacy += ' ';
        legacy += args;
    }
    return true;
}

static bool LooksLikeJsonNumber(const std::string& s) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[i] == '-') ++i;

    const size_t digitsStart = i;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
    if (i == digitsStart) return false;

    if (i < s.size() && s[i] == '.') {
        ++i;
        const size_t fractionStart = i;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
        if (i == fractionStart) return false;
    }

    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
        const size_t exponentStart = i;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
        if (i == exponentStart) return false;
    }

    return i == s.size();
}

static std::string JsonValueToken(const std::string& body) {
    const size_t first = body.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "null";

    const std::string candidate = body.substr(first);
    const bool structured = (candidate.front() == '{' || candidate.front() == '[') &&
                            (candidate.back() == '}' || candidate.back() == ']');
    const bool scalar = candidate == "null" || candidate == "true" || candidate == "false" ||
                        LooksLikeJsonNumber(candidate);
    const bool quoted = candidate.size() >= 2 && candidate.front() == '"' && candidate.back() == '"';
    return (structured || scalar || quoted) ? candidate : "\"" + EscapeJsonText(body) + "\"";
}

static std::string FormatPointer(uint64_t value) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << value;
    return oss.str();
}

static std::string BuildSessionResponse(const std::string& idJson, const std::string& response) {
    std::string body = response;
    while (!body.empty() && (body.back() == '\n' || body.back() == '\r')) body.pop_back();

    const size_t first = body.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "{\"id\":" + idJson + ",\"ok\":true,\"data\":null}\n";
    }

    if (body.compare(first, 6, "ERROR:") == 0) {
        std::string err = body.substr(first + 6);
        const size_t start = err.find_first_not_of(" \t");
        err = start == std::string::npos ? std::string() : err.substr(start);
        return "{\"id\":" + idJson + ",\"ok\":false,\"error\":\"" + EscapeJsonText(err) + "\"}\n";
    }

    return "{\"id\":" + idJson + ",\"ok\":true,\"data\":" + JsonValueToken(body) + "}\n";
}

IOCPServer::IOCPServer()
    : m_ListenSocket(INVALID_SOCKET)
    , m_Running(false)
    , m_Port(20030) {
}

IOCPServer::~IOCPServer() {
    Stop();
}

bool IOCPServer::Start(int port) {
    if (m_Running) return true;

    m_Port = port;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        OutputDebugStringA("[IOCPServer] WSAStartup failed\n");
        return false;
    }

    m_ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_ListenSocket == INVALID_SOCKET) {
        OutputDebugStringA("[IOCPServer] Failed to create socket\n");
        WSACleanup();
        return false;
    }

    int opt = 1;
    setsockopt(m_ListenSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    setsockopt(m_ListenSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&opt, sizeof(opt));

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serverAddr.sin_port = htons(static_cast<u_short>(m_Port));

    if (bind(m_ListenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        char msg[256];
        snprintf(msg, sizeof(msg), "[IOCPServer] Bind failed: %d\n", WSAGetLastError());
        OutputDebugStringA(msg);
        closesocket(m_ListenSocket);
        WSACleanup();
        return false;
    }

    if (listen(m_ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
        OutputDebugStringA("[IOCPServer] Listen failed\n");
        closesocket(m_ListenSocket);
        WSACleanup();
        return false;
    }

    m_Running = true;
    IPCJobs::Start();

    m_Thread = std::thread(&IOCPServer::ServerThread, this);

    char msg[256];
    snprintf(msg, sizeof(msg), "[IOCPServer] Started on port %d\n", m_Port);
    OutputDebugStringA(msg);

    return true;
}

void IOCPServer::Stop() {
    if (!m_Running) return;

    m_Running = false;

    if (m_ListenSocket != INVALID_SOCKET) {
        closesocket(m_ListenSocket);
        m_ListenSocket = INVALID_SOCKET;
    }

    if (m_Thread.joinable()) m_Thread.join();

    IPCJobs::Stop();
    WSACleanup();
    OutputDebugStringA("[IOCPServer] Stopped\n");
}

void IOCPServer::CleanupSocket(SOCKET sock) {
    if (sock == INVALID_SOCKET) return;

    shutdown(sock, SD_SEND);

    char scratch[4096];
    DWORD start = GetTickCount();
    for (;;) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 20000;
        if (select(0, &readfds, nullptr, nullptr, &tv) <= 0) break;
        int n = recv(sock, scratch, sizeof(scratch), 0);
        if (n <= 0) break;
        if (GetTickCount() - start > 1000) break;
    }

    closesocket(sock);
}

void IOCPServer::ServeJsonSession(SOCKET sock, std::string buffer) {
    const size_t maxBuffered = 4 * 1024 * 1024;
    bool eof = false;

    struct SessionWatch {
        int id = 0;
        uint64_t ptr = 0;
        std::string path;
        int intervalMs = 500;
        uint64_t nextDue = 0;
        std::string lastJson;
    };
    std::vector<SessionWatch> watches;
    int nextWatchId = 1;

    struct SessionSubscription {
        int id = 0;
        uint64_t token = 0;
        std::string kind;
    };
    std::vector<SessionSubscription> subscriptions;
    struct SubscriptionGuard {
        std::vector<SessionSubscription>& subs;
        ~SubscriptionGuard() {
            for (const auto& sub : subs) LuaManager::Get().UnsubscribeIpcEvent(sub.id);
        }
    } subscriptionGuard{subscriptions};
    static std::atomic<uint64_t> nextEventToken{1};

    IPCEvents::Clear();

    while (m_Running) {
        std::vector<std::string> pendingEvents;
        IPCEvents::Drain(pendingEvents);
        for (const auto& event : pendingEvents) {
            if (!SendAll(sock, event + "\n")) return;
        }

        size_t newline;
        while ((newline = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, newline);
            buffer.erase(0, newline + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            std::map<std::string, RequestValue> request;
            if (!ParseRequestLine(line, request)) {
                if (!SendAll(sock, "{\"id\":null,\"ok\":false,\"error\":\"invalid json request\"}\n")) return;
                continue;
            }

            std::string legacy, idJson, error;
            bool quit = false;
            if (!BuildLegacyRequest(request, legacy, idJson, quit, error)) {
                if (!SendAll(sock, "{\"id\":" + idJson + ",\"ok\":false,\"error\":\"" + EscapeJsonText(error) + "\"}\n")) return;
                continue;
            }

            const std::vector<std::string> requestTokens = TokenizeCommandArgs(legacy);
            const std::string requestVerb = requestTokens.empty() ? legacy : requestTokens[0];

            if (requestVerb == "WATCH") {
                if (requestTokens.size() < 3) {
                    if (!SendAll(sock, BuildSessionResponse(idJson, "ERROR: Usage: WATCH <ptr> <property.path> [interval_ms]\n"))) return;
                    continue;
                }

                const uint64_t ptr = strtoull(requestTokens[1].c_str(), nullptr, 16);
                const std::string path = requestTokens[2];
                int interval = requestTokens.size() > 3 ? atoi(requestTokens[3].c_str()) : 500;
                if (interval < 50) interval = 50;
                if (interval > 60000) interval = 60000;

                if (!ptr || !SafeMem_IsReadable(reinterpret_cast<void*>(static_cast<uintptr_t>(ptr)), 0x10)) {
                    if (!SendAll(sock, BuildSessionResponse(idJson, "ERROR: unreadable object pointer\n"))) return;
                    continue;
                }
                if (watches.size() >= 64) {
                    if (!SendAll(sock, BuildSessionResponse(idJson, "ERROR: too many watches in this session\n"))) return;
                    continue;
                }

                std::string json;
                if (!LuaManager::Get().GetPropertyJson(reinterpret_cast<void*>(static_cast<uintptr_t>(ptr)), path, json)) {
                    if (!SendAll(sock, BuildSessionResponse(idJson, "ERROR: " + json + "\n"))) return;
                    continue;
                }

                SessionWatch watch;
                watch.id = nextWatchId++;
                watch.ptr = ptr;
                watch.path = path;
                watch.intervalMs = interval;
                watch.nextDue = GetTickCount64() + static_cast<uint64_t>(interval);
                watch.lastJson = json;
                watches.push_back(std::move(watch));

                const std::string response = "{\"id\":" + idJson + ",\"ok\":true,\"data\":{\"watchId\":" +
                    std::to_string(watches.back().id) + ",\"value\":" + JsonValueToken(json) + "}}\n";
                if (!SendAll(sock, response)) return;
                continue;
            }

            if (requestVerb == "UNWATCH") {
                if (requestTokens.size() < 2) {
                    if (!SendAll(sock, BuildSessionResponse(idJson, "ERROR: Usage: UNWATCH <watchId>\n"))) return;
                    continue;
                }

                const int watchId = atoi(requestTokens[1].c_str());
                bool removed = false;
                for (size_t i = 0; i < watches.size(); ++i) {
                    if (watches[i].id == watchId) {
                        watches.erase(watches.begin() + static_cast<std::ptrdiff_t>(i));
                        removed = true;
                        break;
                    }
                }

                const std::string response = removed
                    ? "{\"id\":" + idJson + ",\"ok\":true,\"data\":\"unwatched\"}\n"
                    : BuildSessionResponse(idJson, "ERROR: unknown watch\n");
                if (!SendAll(sock, response)) return;
                continue;
            }

            if (requestVerb == "SUBSCRIBE") {
                if (requestTokens.size() < 2) {
                    if (!SendAll(sock, BuildSessionResponse(idJson, "ERROR: Usage: SUBSCRIBE ProcessEvent <Function>|ObjectCreated|ObjectDestroyed|LevelTransition\n"))) return;
                    continue;
                }

                const std::string eventName = NormalizeCommandName(requestTokens[1]);
                std::string kind;
                std::string filter;
                if (eventName == "PROCESSEVENT") {
                    if (requestTokens.size() < 3) {
                        if (!SendAll(sock, BuildSessionResponse(idJson, "ERROR: SUBSCRIBE ProcessEvent requires a function name\n"))) return;
                        continue;
                    }
                    kind = "ProcessEvent";
                    filter = requestTokens[2];
                } else if (eventName == "OBJECTCREATED" || eventName == "BEGINPLAY") {
                    kind = "BeginPlay";
                } else if (eventName == "OBJECTDESTROYED" || eventName == "ENDPLAY") {
                    kind = "EndPlay";
                } else if (eventName == "LEVELTRANSITION" || eventName == "LOADMAP") {
                    kind = "LoadMap";
                } else {
                    if (!SendAll(sock, BuildSessionResponse(idJson, "ERROR: Unknown event: " + eventName + "\n"))) return;
                    continue;
                }

                const uint64_t token = nextEventToken++;
                const int subscriptionId = LuaManager::Get().SubscribeIpcEvent(kind, filter, token);
                if (subscriptionId == 0) {
                    if (!SendAll(sock, BuildSessionResponse(idJson, "ERROR: Could not register event hook (Lua disabled, invalid function, or hook failure)\n"))) return;
                    continue;
                }

                subscriptions.push_back({subscriptionId, token, kind});
                const std::string response = "{\"id\":" + idJson + ",\"ok\":true,\"data\":{\"subscription\":" +
                    std::to_string(subscriptionId) + ",\"token\":" + std::to_string(token) +
                    ",\"kind\":\"" + kind + "\",\"filter\":\"" + EscapeJsonText(filter) + "\"}}\n";
                if (!SendAll(sock, response)) return;
                continue;
            }

            if (requestVerb == "UNSUBSCRIBE") {
                if (requestTokens.size() < 2) {
                    if (!SendAll(sock, BuildSessionResponse(idJson, "ERROR: Usage: UNSUBSCRIBE <subscriptionId>\n"))) return;
                    continue;
                }

                const int subscriptionId = atoi(requestTokens[1].c_str());
                bool removed = false;
                for (size_t i = 0; i < subscriptions.size(); ++i) {
                    if (subscriptions[i].id == subscriptionId) {
                        LuaManager::Get().UnsubscribeIpcEvent(subscriptionId);
                        subscriptions.erase(subscriptions.begin() + static_cast<std::ptrdiff_t>(i));
                        removed = true;
                        break;
                    }
                }

                const std::string response = removed
                    ? "{\"id\":" + idJson + ",\"ok\":true,\"data\":\"unsubscribed\"}\n"
                    : BuildSessionResponse(idJson, "ERROR: unknown subscription\n");
                if (!SendAll(sock, response)) return;
                continue;
            }

            std::string response;
            try {
                response = quit ? std::string("OK\n") : ProcessCommand(legacy);
            } catch (const std::exception& ex) {
                response = std::string("ERROR: Internal exception: ") + ex.what() + "\n";
            } catch (...) {
                response = "ERROR: Internal exception (unknown type)\n";
            }

            if (!SendAll(sock, BuildSessionResponse(idJson, response))) return;
            if (quit) return;
        }

        if (eof) return;
        if (!m_Running) return;
        if (buffer.size() > maxBuffered) {
            SendAll(sock, "{\"id\":null,\"ok\":false,\"error\":\"request too large\"}\n");
            return;
        }

        if (!watches.empty()) {
            const uint64_t now = GetTickCount64();
            for (size_t i = 0; i < watches.size(); ) {
                SessionWatch& watch = watches[i];
                if (!SafeMem_IsReadable(reinterpret_cast<void*>(static_cast<uintptr_t>(watch.ptr)), 0x10)) {
                    watches.erase(watches.begin() + static_cast<std::ptrdiff_t>(i));
                    continue;
                }
                if (watch.nextDue > now) {
                    ++i;
                    continue;
                }

                watch.nextDue = now + static_cast<uint64_t>(watch.intervalMs);

                std::string json;
                if (!LuaManager::Get().GetPropertyJson(
                        reinterpret_cast<void*>(static_cast<uintptr_t>(watch.ptr)), watch.path, json)) {
                    ++i;
                    continue;
                }
                if (json != watch.lastJson) {
                    watch.lastJson = json;
                    const std::string event = "{\"event\":\"watch\",\"watchId\":" + std::to_string(watch.id) +
                        ",\"ptr\":\"" + FormatPointer(watch.ptr) + "\",\"path\":\"" +
                        EscapeJsonText(watch.path) + "\",\"value\":" + JsonValueToken(json) + "}\n";
                    if (!SendAll(sock, event)) return;
                }
                ++i;
            }
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        timeval timeout;
        if (watches.empty() && subscriptions.empty()) {
            timeout.tv_sec = 120;
            timeout.tv_usec = 0;
        } else {
            const uint64_t now = GetTickCount64();
            uint64_t waitMs = 100;
            for (const SessionWatch& watch : watches) {
                const uint64_t due = watch.nextDue > now ? watch.nextDue - now : 0;
                if (due < waitMs) waitMs = due;
            }
            if (waitMs < 10) waitMs = 10;
            timeout.tv_sec = static_cast<long>(waitMs / 1000);
            timeout.tv_usec = static_cast<long>((waitMs % 1000) * 1000);
        }
        const int ready = select(0, &readfds, nullptr, nullptr, &timeout);
        if (ready == 0) continue;
        if (ready < 0) return;

        char chunk[65536];
        const int received = recv(sock, chunk, sizeof(chunk), 0);
        if (received <= 0) {
            eof = true;
            if (!buffer.empty()) buffer += '\n';
            continue;
        }
        buffer.append(chunk, received);
    }
}

void IOCPServer::ServerThread() {
    EngineDiscovery::SetLogThreadName("rpc");
    OutputDebugStringA("[IOCPServer] Server thread started\n");

    fd_set readfds;
    timeval timeout;

    while (m_Running) {

        FD_ZERO(&readfds);
        FD_SET(m_ListenSocket, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        int selectResult = select(0, &readfds, nullptr, nullptr, &timeout);

        if (selectResult <= 0) {
            continue;
        }

        if (FD_ISSET(m_ListenSocket, &readfds)) {
            SOCKET clientSocket = accept(m_ListenSocket, nullptr, nullptr);

            if (clientSocket == INVALID_SOCKET) {
                continue;
            }

            OutputDebugStringA("[IOCPServer] Client connected\n");

            int flag = 1;
            setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

            DWORD socketTimeoutMs = 10000;
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&socketTimeoutMs, sizeof(socketTimeoutMs));
            setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&socketTimeoutMs, sizeof(socketTimeoutMs));

            int bufSize = 1048576;
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVBUF, (char*)&bufSize, sizeof(bufSize));
            setsockopt(clientSocket, SOL_SOCKET, SO_SNDBUF, (char*)&bufSize, sizeof(bufSize));

            std::string command;
            if (ReadCommand(clientSocket, command) > 0 && !command.empty()) {
                const size_t probe = command.find_first_not_of(" \t\r\n");
                if (probe != std::string::npos && command[probe] == '{') {
                    ServeJsonSession(clientSocket, command);
                } else {
                    std::string response;

                    try {
                        response = ProcessCommand(command);
                    } catch (const std::exception& ex) {
                        response = std::string("ERROR: Internal exception: ") + ex.what() + "\n";
                    } catch (...) {
                        response = "ERROR: Internal exception (unknown type)\n";
                    }

                    SendAll(clientSocket, response);
                }
            }

            CleanupSocket(clientSocket);
            OutputDebugStringA("[IOCPServer] Client disconnected\n");
        }
    }

    OutputDebugStringA("[IOCPServer] Server thread stopped\n");
}

std::string IOCPServer::ProcessCommand(const std::string& command) {
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;
    cmd = NormalizeCommandName(cmd);

    if (cmd == "STATUS") {
        return GetStatus();
    }

    if (cmd == "INIT") {
        bool ok = Initialize();
        return ok ? "OK\n" : (std::string("ERROR: ") + GetLastErrorMsg() + "\n");
    }

    if (cmd == "GLOBALS") {
        extern EngineState g_State;
        void* liveWorld = nullptr;
        if (g_State.GWorldPtr) SafeMem_Read(g_State.GWorldPtr, &liveWorld, sizeof(void*));
        std::ostringstream oss;
        oss << "GUObjectArray=" << GetGUObjectArray() << "\n";
        oss << "FNamePool=" << GetFNamePool() << "\n";
        oss << "GWorld=" << liveWorld << "\n";
        oss << "GWorldPtr=" << (void*)g_State.GWorldPtr << "\n";
        oss << "GMalloc=" << GetGMalloc() << "\n";
        oss << "GNatives=" << GetGNatives() << "\n";
        oss << "GUObjectHashTables=" << GetGUObjectHashTables() << "\n";
        oss << "ObjectCount=" << GetObjectCount() << "\n";
        return oss.str();
    }

    if (cmd == "VERSION") {
        return std::string(GetDetectedVersion()) + "\n";
    }

    if (cmd == "CLASSES") {
        int maxCount = 10000;
        std::string maxStr;
        if (iss >> maxStr) maxCount = ClampCount(atoi(maxStr.c_str()));

        std::vector<void*> classes(maxCount);
        int count = GetAllUClasses(classes.data(), maxCount);

        std::ostringstream oss;
        oss << "Count=" << count << "\n";
        for (int i = 0; i < count; i++) {
            char name[256] = {};
            GetObjectNameSafe(classes[i], name, sizeof(name));
            oss << classes[i] << " " << name << "\n";
        }
        return oss.str();
    }

    if (cmd == "ACTORS") {
        int maxCount = 10000;
        std::vector<void*> actors(maxCount);
        int count = GetAllActors(actors.data(), maxCount);

        std::ostringstream oss;
        oss << "Count=" << count << "\n";
        for (int i = 0; i < count; i++) {
            char name[256] = {};
            GetObjectNameSafe(actors[i], name, sizeof(name));
            oss << actors[i] << " " << name << "\n";
        }
        return oss.str();
    }

    if (cmd == "WORLDACTORS") {
        std::string limitStr;
        iss >> limitStr;
        int maxCount = limitStr.empty() ? 10000 : std::atoi(limitStr.c_str());
        if (maxCount <= 0) maxCount = 10000;
        maxCount = ClampCount(maxCount);

        std::vector<void*> actors(maxCount);
        int count = GetWorldActors(actors.data(), maxCount);

        std::ostringstream oss;
        oss << "Count=" << count << "\n";
        for (int i = 0; i < count; i++) {
            char name[256] = {};
            GetObjectNameSafe(actors[i], name, sizeof(name));
            oss << actors[i] << " " << name << "\n";
        }
        return oss.str();
    }

    if (cmd == "FINDACTOR") {
        std::string searchName;
        iss >> searchName;
        if (searchName.empty()) return "Count=0\n";

        std::vector<void*> actors(50000);
        int count = GetWorldActors(actors.data(), 50000);

        std::ostringstream oss;
        int found = 0;
        for (int i = 0; i < count; i++) {
            char name[256] = {};
            GetObjectNameSafe(actors[i], name, sizeof(name));
            if (std::string(name).find(searchName) != std::string::npos) {
                oss << actors[i] << " " << name << "\n";
                found++;
                if (found >= 100) break;
            }
        }
        return "Count=" + std::to_string(found) + "\n" + oss.str();
    }

    if (cmd == "ENUMS") {
        int maxCount = 10000;
        std::vector<void*> enums(maxCount);
        int count = GetAllEnums(enums.data(), maxCount);

        std::ostringstream oss;
        oss << "Count=" << count << "\n";
        for (int i = 0; i < count; i++) {
            char name[256] = {};
            GetObjectNameSafe(enums[i], name, sizeof(name));
            oss << enums[i] << " " << name << "\n";
        }
        return oss.str();
    }

    if (cmd == "NAME" || cmd == "FULLNAME" || cmd == "CHAIN" || cmd == "SUPER") {
        std::string ptrStr;
        iss >> ptrStr;
        void* ptr = (void*)strtoull(ptrStr.c_str(), nullptr, 16);

        if (cmd == "NAME") return std::string(GetObjectName(ptr)) + "\n";
        if (cmd == "FULLNAME") return std::string(GetObjectFullName(ptr)) + "\n";
        if (cmd == "CHAIN") return std::string(GetInheritanceChain(ptr)) + "\n";
        if (cmd == "SUPER") {
            void* super = GetSuperClass(ptr);
            std::ostringstream oss;
            oss << super;
            if (super) oss << " " << GetObjectName(super);
            return oss.str() + "\n";
        }
    }

    if (cmd == "INHERITS") {
        std::string ptrStr, baseName;
        iss >> ptrStr >> baseName;
        void* ptr = (void*)strtoull(ptrStr.c_str(), nullptr, 16);

        return InheritsFrom(ptr, baseName.c_str()) ? "true\n" : "false\n";
    }

    if (cmd == "FINDOBJECT") {
        std::string searchName;
        iss >> searchName;
        if (searchName.empty()) return "Count=0\n";

        extern EngineState g_State;

        if (g_State.bIndexBuilt) {
            auto results = EngineDiscovery::FindObjectsByPrefix(g_State, searchName);
            if (!results.empty()) {
                std::ostringstream oss;
                oss << "Count=" << results.size() << "\n";
                for (UObject* obj : results) {
                    std::string name = EngineDiscovery::GetObjectNameStr(g_State, obj);
                    oss << obj << " " << name << "\n";
                }
                return oss.str();
            }
        }

        int count = EngineDiscovery::GetObjectCount(g_State);
        int found = 0;
        std::ostringstream oss;
        for (int i = 0; i < count; i++) {
            void* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
            if (!obj) continue;

            FName* namePtr = (FName*)((uint8_t*)obj + g_State.UObject_NameOff);
            if (!SafeMem_IsReadable(namePtr, sizeof(FName))) continue;

            std::string name = EngineDiscovery::GetCachedFName(g_State, namePtr->ComparisonIndex);
            if (name.find(searchName) != std::string::npos) {
                oss << obj << " " << name << "\n";
                found++;
                if (found >= 100) break;
            }
        }
        return "Count=" + std::to_string(found) + "\n" + oss.str();
    }

    if (cmd == "PROPS") {
        std::string ptrStr;
        iss >> ptrStr;
        void* ptr = (void*)strtoull(ptrStr.c_str(), nullptr, 16);

        if (!ptr && !ptrStr.empty() && ptrStr.find("0x") == std::string::npos && ptrStr.length() > 2) {
            extern EngineState g_State;
            int count = EngineDiscovery::GetObjectCount(g_State);
            for (int i = 0; i < count; i++) {
                void* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
                if (!obj) continue;
                std::string name = EngineDiscovery::GetObjectNameStr(g_State, (UObject*)obj);
                if (name == ptrStr) {
                    ptr = obj;
                    break;
                }
            }
            if (!ptr) return "ERROR: Object not found by name\n";
        }

        PropertyInfoEx props[256] = {};
        int count = GetClassProperties(ptr, props, 256);

        std::ostringstream oss;
        oss << "Count=" << count << "\n";
        for (int i = 0; i < count; i++) {
            char offStr[32];
            snprintf(offStr, sizeof(offStr), "[0x%X]", props[i].Offset);
            oss << offStr << " " << props[i].Name << " : " << props[i].TypeName;
            if (props[i].bIsArray) oss << " [Array]";
            if (props[i].bIsMap) oss << " [Map]";
            if (props[i].bIsStruct) oss << " [Struct]";
            oss << "\n";
        }
        return oss.str();
    }

    if (cmd == "FUNCS") {
        std::string ptrStr;
        iss >> ptrStr;
        void* ptr = (void*)strtoull(ptrStr.c_str(), nullptr, 16);
        if (!ptr) return "ERROR: Invalid pointer\n";

        extern EngineState g_State;

        std::ostringstream oss;
        void* child = EngineDiscovery::GetChildrenHead(g_State, (UStruct*)ptr);
        int count = 0;
        while (child && count < 500) {
            char name[256] = {};
            GetObjectNameSafe(child, name, sizeof(name));

            std::string flagStr;
            if (g_State.UFunction_FlagsOff >= 0) {
                uint32_t flags = 0;
                SafeMem_Read((uint8_t*)child + g_State.UFunction_FlagsOff, &flags, 4);

                char hexBuf[32];
                snprintf(hexBuf, sizeof(hexBuf), " [0x%X]", flags);
                flagStr = hexBuf;
                if (flags & 0x400) flagStr += " Native";
                if (flags & 0x1) flagStr += " Final";
                if (flags & 0x200000) flagStr += " Public";
                if (flags & 0x04000000) flagStr += " BPCallable";
                if (flags & 0x100) flagStr += " BlueprintEvent";
                if (flags & 0x4000) flagStr += " Net";
            }

            oss << child << " " << name << flagStr << "\n";
            child = EngineDiscovery::GetFieldNext(g_State, child);
            count++;
        }
        oss << "Count=" << count << "\n";
        return oss.str();
    }

    if (cmd == "CALLFUNC") {
        extern EngineState g_State;

        std::string objStr, funcName;
        iss >> objStr >> funcName;

        if (objStr.empty() || funcName.empty()) return "ERROR: Usage: CALLFUNC <objectPtr> <functionName>\n";

        void* objPtr = (void*)strtoull(objStr.c_str(), nullptr, 16);
        if (!objPtr) return "ERROR: Invalid object pointer\n";

        if (g_State.ProcessEvent_VTableIdx < 0) return "ERROR: ProcessEvent not discovered\n";

        UClass* cls = EngineDiscovery::GetObjectClass(g_State, (UObject*)objPtr);
        UStruct* searchStruct = nullptr;

        if (cls && cls == g_State.UClassClass) {
            searchStruct = (UStruct*)objPtr;
        } else if (cls) {
            searchStruct = (UStruct*)cls;
        } else {
            return "ERROR: Failed to get object class\n";
        }

        void* ufunc = EngineDiscovery::FindFunctionByName(g_State, searchStruct, funcName.c_str());
        if (!ufunc) return "ERROR: Function '" + funcName + "' not found on class\n";

        int32_t paramsSize = 0;
        int32_t propsSizeOff = g_State.UStruct_ChildPropsOff >= 0 ? g_State.UStruct_ChildPropsOff + 8 : 0x58;
        SafeMem_Read((uint8_t*)ufunc + propsSizeOff, &paramsSize, 4);

        uint8_t* paramsBuf = nullptr;
        if (paramsSize > 0 && paramsSize < 0x10000) {
            paramsBuf = new uint8_t[paramsSize];
            memset(paramsBuf, 0, paramsSize);
        }

        bool ok = EngineDiscovery::CallProcessEvent(g_State, objPtr, ufunc, paramsBuf);

        std::ostringstream oss;
        oss << "Called " << funcName << " on " << objPtr << " -> " << (ok ? "OK" : "FAIL") << "\n";
        oss << "UFunction=" << ufunc << " ParamsSize=" << paramsSize << "\n";

        if (paramsBuf) delete[] paramsBuf;
        return oss.str();
    }

    if (cmd == "GET") {
        std::string ptrStr, path;
        iss >> ptrStr;
        std::getline(iss >> std::ws, path);
        path = TrimQuotedPath(path);
        if (ptrStr.empty() || path.empty()) return "ERROR: Usage: GET <ptr> <property.path>\n";

        void* object = reinterpret_cast<void*>(strtoull(ptrStr.c_str(), nullptr, 16));
        if (!object || !SafeMem_IsReadable(object, 0x10)) return "ERROR: unreadable object pointer\n";

        std::string json;
        if (!LuaManager::Get().GetPropertyJson(object, path, json)) {
            return "ERROR: " + json + "\n";
        }
        return json;
    }

    if (cmd == "SET") {
        std::string ptrStr, path, value;
        iss >> ptrStr;
        iss >> path;
        std::getline(iss >> std::ws, value);
        path = TrimQuotedPath(path);
        if (ptrStr.empty() || path.empty() || value.empty()) {
            return "ERROR: Usage: SET <ptr> <property.path> <json_value>\n";
        }

        void* object = reinterpret_cast<void*>(strtoull(ptrStr.c_str(), nullptr, 16));
        if (!object || !SafeMem_IsReadable(object, 0x10)) return "ERROR: unreadable object pointer\n";

        std::string result;
        if (!LuaManager::Get().SetPropertyJson(object, path, value, result)) {
            return "ERROR: " + result + "\n";
        }
        return "OK\n";
    }

    if (cmd == "WATCH" || cmd == "UNWATCH") {
        return "ERROR: WATCH requires a JSON session\n";
    }

    if (cmd == "READMEM") {
        std::string addrStr, sizeStr;
        iss >> addrStr >> sizeStr;
        void* addr = (void*)strtoull(addrStr.c_str(), nullptr, 16);
        int size = sizeStr.empty() ? 64 : std::atoi(sizeStr.c_str());
        if (size <= 0 || size > 4096) size = 64;

        std::vector<uint8_t> data(size);
        if (!SafeMem_Read(addr, data.data(), size)) return "ERROR: Read failed\n";

        std::ostringstream oss;
        for (int i = 0; i < size; i += 16) {
            char line[128];
            snprintf(line, sizeof(line), "%04X: ", i);
            oss << line;
            for (int j = 0; j < 16 && i + j < size; j++) {
                snprintf(line, sizeof(line), "%02X ", data[i + j]);
                oss << line;
            }
            oss << " ";
            for (int j = 0; j < 16 && i + j < size; j++) {
                char c = data[i + j];
                oss << (c >= 32 && c < 127 ? c : '.');
            }
            oss << "\n";
        }
        return oss.str();
    }

    if (cmd == "READPTR") {
        std::string baseStr, offStr;
        iss >> baseStr >> offStr;
        uint8_t* base = (uint8_t*)strtoull(baseStr.c_str(), nullptr, 16);
        int offset = (int)strtol(offStr.c_str(), nullptr, 16);

        void* ptr = nullptr;
        if (!SafeMem_Read(base + offset, &ptr, sizeof(void*))) return "ERROR: Read failed\n";

        std::ostringstream oss;
        oss << ptr << "\n";
        if (ptr) {
            char name[256] = {};
            GetObjectNameSafe(ptr, name, sizeof(name));
            if (name[0]) oss << "Name=" << name << "\n";
        }
        return oss.str();
    }

    if (cmd == "WRITEINT") {
        std::string addrStr, valStr;
        iss >> addrStr >> valStr;
        void* addr = (void*)strtoull(addrStr.c_str(), nullptr, 16);
        int32_t val = (int32_t)strtol(valStr.c_str(), nullptr, 0);

        DWORD oldProtect = 0;
        VirtualProtect(addr, 4, PAGE_READWRITE, &oldProtect);
        memcpy(addr, &val, 4);
        if (oldProtect) VirtualProtect(addr, 4, oldProtect, &oldProtect);

        std::ostringstream oss;
        oss << "Written " << val << " (0x" << std::hex << val << ") to " << addr << "\n";
        return oss.str();
    }

    if (cmd == "SCANINT") {
        std::string addrStr, sizeStr, valStr;
        iss >> addrStr >> sizeStr >> valStr;
        uint8_t* base = (uint8_t*)strtoull(addrStr.c_str(), nullptr, 16);
        int size = std::atoi(sizeStr.c_str());
        int32_t target = (int32_t)strtol(valStr.c_str(), nullptr, 0);

        if (size <= 0 || size > 0x100000) size = 0x10000;

        std::vector<uint8_t> data(size);
        if (SafeMem_ReadSpanExact(base, data.data(), static_cast<size_t>(size)) !=
            static_cast<size_t>(size))
            return "ERROR: Read failed\n";

        std::ostringstream oss;
        int found = 0;
        for (int i = 0; i <= size - 4; i += 4) {
            int32_t val = *(int32_t*)(data.data() + i);
            if (val == target) {
                char line[128];
                snprintf(line, sizeof(line), "FOUND at %p (base+0x%X) = %d (0x%X)\n",
                    base + i, i, val, val);
                oss << line;
                found++;
                if (found >= 50) { oss << "... (capped at 50)\n"; break; }
            }
        }
        oss << "Total=" << found << "\n";
        return oss.str();
    }

    if (cmd == "DEEPSCAN") {
        std::string addrStr, valStr;
        iss >> addrStr >> valStr;
        uint8_t* base = (uint8_t*)strtoull(addrStr.c_str(), nullptr, 16);
        int32_t target = (int32_t)strtol(valStr.c_str(), nullptr, 0);

        extern EngineState g_State;
        auto mainMod = PatternScan::GetMainModule();

        std::ostringstream oss;
        int totalFound = 0;

        uint8_t objData[0x1000];
        if (SafeMem_Read(base, objData, sizeof(objData))) {
            for (int i = 0; i <= (int)sizeof(objData) - 4; i += 4) {
                int32_t val = *(int32_t*)(objData + i);
                if (val == target) {
                    char line[128];
                    snprintf(line, sizeof(line), "OBJ+0x%X = %d at %p\n", i, val, base + i);
                    oss << line;
                    totalFound++;
                }
            }

            for (int i = 0; i <= (int)sizeof(objData) - 8; i += 8) {
                uintptr_t ptr = *(uintptr_t*)(objData + i);
                if (ptr < 0x10000 || ptr > 0x00007FFFFFFFFFFF) continue;
                if (ptr >= mainMod.base && ptr < mainMod.base + mainMod.size) continue;

                uint8_t subData[0x1000];
                if (!SafeMem_Read((void*)ptr, subData, sizeof(subData))) continue;

                for (int j = 0; j <= (int)sizeof(subData) - 4; j += 4) {
                    int32_t val = *(int32_t*)(subData + j);
                    if (val == target) {
                        char line[256];
                        char name[128] = {};
                        GetObjectNameSafe((void*)ptr, name, sizeof(name));
                        snprintf(line, sizeof(line), "PTR[OBJ+0x%X]->0x%X = %d at %p (%s)\n",
                            i, j, val, (void*)(ptr + j), name[0] ? name : "?");
                        oss << line;
                        totalFound++;
                        if (totalFound >= 50) break;
                    }
                }
                if (totalFound >= 50) break;
            }
        }

        oss << "Total=" << totalFound << "\n";
        return oss.str();
    }

    if (cmd == "DUMP") {
        int totalSize = 0;
        const char* shmName = DumpAllClassesSharedMem(&totalSize);
        std::ostringstream oss;
        oss << "SharedMemory=" << shmName << "\n";
        oss << "Size=" << totalSize << "\n";
        return oss.str();
    }

    if (cmd == "DISCOVER") {

        std::string type, filter;
        iss >> type >> filter;
        if (type.empty()) type = "items";

        extern EngineState g_State;
        std::ostringstream oss;
        int found = 0;

        std::vector<UObject*> candidates;
        if (!filter.empty()) {
            candidates = EngineDiscovery::FindObjectsByPrefix(g_State, filter);
        } else {
            int count = EngineDiscovery::GetObjectCount(g_State);
            for (int i = 0; i < count && i < 5000; i++) {
                UObject* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
                if (obj) candidates.push_back(obj);
            }
        }

        for (UObject* obj : candidates) {
            if (found >= 100) break;

            std::string name = EngineDiscovery::GetObjectNameStr(g_State, obj);
            if (name.empty() || name[0] == '<') continue;

            if (name.find("_C") == name.length() - 2 && name.length() > 2) continue;
            if (name.find("_d") == name.length() - 2) continue;
            if (name.find("_m") == name.length() - 2) continue;
            if (name.find("_orm") != std::string::npos) continue;
            if (name.find("Default__") == 0) continue;

            UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
            if (!cls) continue;

            struct PropInfo { std::string name; int offset; };
            std::vector<PropInfo> mapProps, arrayProps, numericProps;

            void* prop = EngineDiscovery::GetChildPropsHead(g_State, (UStruct*)cls);
            int pc = 0;
            while (prop && pc < 100) {
                std::string propName = EngineDiscovery::GetFFieldName(g_State, prop);
                std::string propType = EngineDiscovery::GetFFieldTypeName(g_State, prop);
                int offset = 0;
                int offsetOff = g_State.FProperty_OffsetOff;
                if (offsetOff < 0) offsetOff = 0x4C;
                SafeMem_Read((uint8_t*)prop + offsetOff, &offset, 4);

                if (propType.find("Map") != std::string::npos) mapProps.push_back({propName, offset});
                else if (propType.find("Array") != std::string::npos) arrayProps.push_back({propName, offset});
                else if (propType.find("Int") != std::string::npos || propType.find("Float") != std::string::npos || propType.find("Byte") != std::string::npos)
                    numericProps.push_back({propName, offset});

                prop = EngineDiscovery::GetFFieldNext(g_State, prop);
                pc++;
            }

            bool isInventoryLike = !mapProps.empty() || !arrayProps.empty();

            if (type == "items" && isInventoryLike) {
                oss << obj << " " << name;
                for (auto& mp : mapProps) oss << " [Map:" << mp.name << "]";
                for (auto& ap : arrayProps) oss << " [Array:" << ap.name << "]";
                for (auto& np : numericProps) {
                    int32_t val = 0;
                    SafeMem_Read((uint8_t*)obj + np.offset, &val, 4);
                    if (val != 0) oss << " [" << np.name << "=" << val << "]";
                }
                oss << "\n";
                found++;
            } else if (type == "actors") {
                if (cls && InheritsFrom(cls, "Actor")) {
                    oss << obj << " " << name << "\n";
                    found++;
                }
            } else if (type == "components") {
                if (cls && InheritsFrom(cls, "ActorComponent")) {
                    oss << obj << " " << name << "\n";
                    found++;
                }
            } else if (type == "all") {
                oss << obj << " " << name;
                for (auto& mp : mapProps) oss << " [Map:" << mp.name << "]";
                for (auto& ap : arrayProps) oss << " [Array:" << ap.name << "]";
                for (auto& np : numericProps) {
                    int32_t val = 0;
                    SafeMem_Read((uint8_t*)obj + np.offset, &val, 4);
                    if (val != 0) oss << " [" << np.name << "=" << val << "]";
                }
                oss << "\n";
                found++;
            }
        }

        return "Count=" + std::to_string(found) + "\n" + oss.str();
    }

    if (cmd == "QUERY") {
        std::string rest;
        std::getline(iss >> std::ws, rest);
        return IPCQuery::HandleQuery(rest) + "\n";
    }

    if (cmd == "MODULES") {
        return IPCQuery::Modules() + "\n";
    }

    if (cmd == "STATS") {
        return IPCQuery::Stats() + "\n";
    }

    if (cmd == "CLASSINFO") {
        std::string target;
        std::getline(iss >> std::ws, target);
        return IPCQuery::ClassInfo(target) + "\n";
    }

    if (cmd == "VTABLE") {
        std::string ptrStr, limitStr;
        iss >> ptrStr >> limitStr;
        uint64_t ptr = 0;
        if (!ptrStr.empty()) ptr = strtoull(ptrStr.c_str(), nullptr, 16);
        int limit = limitStr.empty() ? 64 : atoi(limitStr.c_str());
        return IPCQuery::VTable((uintptr_t)ptr, limit) + "\n";
    }

    if (cmd == "AOB" || cmd == "PATTERN") {

        std::string rest;
        std::getline(iss >> std::ws, rest);

        static const char* kScopes[] = {"image", "module", "all", "heap", "exec", "main"};
        std::string pattern = TrimQuotedPath(rest), scope = "image", limitStr, baseStr, sizeStr;

        for (const char* s : kScopes) {
            size_t p = rest.rfind(std::string(" ") + s);
            if (p == std::string::npos) continue;
            pattern = rest.substr(0, p);
            scope = s;
            std::string tail = rest.substr(p + 1 + strlen(s));
            if (!tail.empty()) {
                std::istringstream ts(tail);
                ts >> limitStr;
                if (!(ts >> baseStr)) baseStr.clear();
                if (!(ts >> sizeStr)) sizeStr.clear();
            }
            break;
        }

        uint64_t base = 0, size = 0;
        if (!baseStr.empty()) base = strtoull(baseStr.c_str(), nullptr, 16);
        if (!sizeStr.empty()) size = strtoull(sizeStr.c_str(), nullptr, 0);
        int limit = limitStr.empty() ? 64 : atoi(limitStr.c_str());

        return IPCQuery::SearchPattern((uintptr_t)base, (size_t)size, pattern, scope, limit) + "\n";
    }

    if (cmd == "STRINGS" || cmd == "FINDSTR") {

        std::string rest;
        std::getline(iss >> std::ws, rest);

        static const char* kScopes[] = {"image", "module", "all", "heap", "exec", "main"};
        std::string needle = TrimQuotedPath(rest), scope = "all", limitStr, utf16Str;

        for (const char* s : kScopes) {
            size_t p = rest.rfind(std::string(" ") + s);
            if (p == std::string::npos) continue;
            needle = TrimQuotedPath(rest.substr(0, p));
            scope = s;
            std::string tail = rest.substr(p + 1 + strlen(s));
            if (!tail.empty()) {
                std::istringstream ts(tail);
                ts >> limitStr;
                if (!(ts >> utf16Str)) utf16Str.clear();
            }
            break;
        }

        int limit = limitStr.empty() ? 64 : atoi(limitStr.c_str());
        bool utf16 = utf16Str.empty() ? true : (utf16Str != "0" && utf16Str != "false");
        return IPCQuery::SearchString(0, 0, needle, scope, limit, utf16) + "\n";
    }

    if (cmd == "XREF" || cmd == "POINTERS") {
        std::string ptrStr, scope = "all", limitStr;
        iss >> ptrStr >> scope >> limitStr;
        uint64_t target = 0;
        if (!ptrStr.empty()) target = strtoull(ptrStr.c_str(), nullptr, 16);
        int limit = limitStr.empty() ? 64 : atoi(limitStr.c_str());
        return IPCQuery::SearchPointers(0, 0, (uintptr_t)target, scope, limit) + "\n";
    }

    if (cmd == "REGIONS") {

        std::string scope, limitStr;
        iss >> scope >> limitStr;
        std::string s = scope.empty() ? "all" : scope;
        std::map<std::string, std::string> kv;
        kv["action"] = "regions";
        kv["limit"] = limitStr.empty() ? "2000" : limitStr;
        if (s == "image") { kv["images"] = "true"; kv["mapped"] = "false"; kv["private"] = "false"; }
        else if (s == "heap") { kv["images"] = "false"; kv["mapped"] = "false"; kv["private"] = "true"; }
        std::ostringstream args;
        for (auto& p : kv) args << p.first << "=" << p.second << " ";
        return IPCQuery::HandleQuery(args.str()) + "\n";
    }

    if (cmd == "QUIT" || cmd == "EXIT") {
        return "BYE\n";
    }

    if (cmd == "USMAP") {
        std::string outputPath;
        std::getline(iss >> std::ws, outputPath);
        outputPath = ResolveUsmapOutputPath(outputPath);

        bool success = GenerateUSMapFile(outputPath.c_str());
        if (success) {
            return "Ok: USMap generated at " + outputPath + "\n";
        }
        return "ERROR: Failed to generate USMap\n";
    }

    if (cmd == "GENSDK") {
        std::string outputPath;
        std::getline(iss >> std::ws, outputPath);

        if (outputPath.empty()) {
            std::string sdkFolder = GetDefaultRuntimeGameRoot() + "\\SDK";
            std::error_code ec;
            std::filesystem::create_directories(sdkFolder, ec);
            outputPath = sdkFolder;
        } else {
            outputPath = TrimQuotedPath(outputPath);
        }

        bool success = GenerateSDKFile(outputPath.c_str());
        if (success) {
            return "Ok: C++ SDK generated at " + outputPath + "\n";
        }
        return "ERROR: Failed to generate C++ SDK\n";
    }

    if (cmd == "GENDIFF") {
        std::string remaining;
        std::getline(iss >> std::ws, remaining);
        const std::vector<std::string> args = TokenizeCommandArgs(remaining);
        if (args.size() < 3) {
            return "ERROR: Usage: GENDIFF \"<outputDir>\" \"<previousSnapshot.tsv>\" \"<currentSnapshot.tsv>\"\n";
        }

        const bool success = GenerateReflectionDiffFile(args[0].c_str(), args[1].c_str(), args[2].c_str());
        if (success) {
            return "Ok: Reflection diff generated at " + args[0] + "\\ReflectionDiff.json\n";
        }
        return "ERROR: Failed to generate reflection diff\n";
    }

    if (cmd == "JOB") {
        std::string sub;
        iss >> sub;
        sub = NormalizeCommandName(sub);

        if (sub == "START") {
            std::string rest;
            std::getline(iss >> std::ws, rest);
            const std::vector<std::string> args = TokenizeCommandArgs(rest);
            if (args.empty()) {
                return "ERROR: Usage: JOB START GENSDK|USMAP|GENDIFF ...\n";
            }

            const std::string generator = NormalizeCommandName(args[0]);
            std::string label;
            IPCJobs::JobBody body;

            if (generator == "GENSDK") {
                std::string outputPath = args.size() > 1 ? TrimQuotedPath(args[1])
                                                         : GetDefaultRuntimeGameRoot() + "\\SDK";
                std::error_code ec;
                std::filesystem::create_directories(outputPath, ec);
                label = "GENSDK " + outputPath;
                body = [outputPath]() -> std::string {
                    const bool ok = GenerateSDKFile(outputPath.c_str());
                    return ok ? "Ok: C++ SDK generated at " + outputPath + "\n"
                              : std::string("ERROR: Failed to generate C++ SDK\n");
                };
            } else if (generator == "USMAP") {
                const std::string outputPath = ResolveUsmapOutputPath(args.size() > 1 ? args[1] : "");
                label = "USMAP " + outputPath;
                body = [outputPath]() -> std::string {
                    const bool ok = GenerateUSMapFile(outputPath.c_str());
                    return ok ? "Ok: USMap generated at " + outputPath + "\n"
                              : std::string("ERROR: Failed to generate USMap\n");
                };
            } else if (generator == "GENDIFF") {
                if (args.size() < 4) {
                    return "ERROR: Usage: JOB START GENDIFF \"<outDir>\" \"<previous>\" \"<current>\"\n";
                }
                const std::string outDir = args[1];
                const std::string previous = args[2];
                const std::string current = args[3];
                label = "GENDIFF " + outDir;
                body = [outDir, previous, current]() -> std::string {
                    const bool ok = GenerateReflectionDiffFile(outDir.c_str(), previous.c_str(), current.c_str());
                    return ok ? "Ok: Reflection diff generated at " + outDir + "\\ReflectionDiff.json\n"
                              : std::string("ERROR: Failed to generate reflection diff\n");
                };
            } else {
                return "ERROR: JOB START supports GENSDK, USMAP, GENDIFF\n";
            }

            const uint64_t id = IPCJobs::Submit(label, std::move(body));
            std::ostringstream oss;
            oss << "OK: job " << id << " started: " << label << "\n";
            return oss.str();
        }

        if (sub == "STATUS" || sub == "RESULT" || sub == "CANCEL") {
            std::string idStr;
            iss >> idStr;
            const uint64_t id = idStr.empty() ? 0 : strtoull(idStr.c_str(), nullptr, 10);
            if (id == 0) return "ERROR: Usage: JOB " + sub + " <id>\n";
            if (sub == "STATUS") return IPCJobs::Status(id) + "\n";
            if (sub == "RESULT") return IPCJobs::Result(id);

            std::string message;
            IPCJobs::Cancel(id, message);
            return message;
        }

        if (sub == "LIST") return IPCJobs::List() + "\n";

        return "ERROR: Unknown JOB subcommand. Valid: START, STATUS, RESULT, LIST, CANCEL\n";
    }

    if (cmd == "LUA") {
        std::string subCmd;
        iss >> subCmd;
        std::transform(subCmd.begin(), subCmd.end(), subCmd.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });

        if (subCmd == "STATUS") {
            if (!LuaManager::Get().IsInitialized()) {
                return "Lua: Not Initialized\n";
            }
            std::ostringstream oss;
            oss << "Lua: Initialized (Lua 5.4.6)\n";
            oss << "ModsDir: " << LuaManager::Get().GetModsDirectory() << "\n";
            oss << "LoadedModsCount=" << LuaManager::Get().GetLoadedMods().size() << "\n";
            for (const auto& mod : LuaManager::Get().GetLoadedMods()) {
                oss << "Mod=" << mod << "\n";
            }
            return oss.str();
        }

        if (subCmd == "JSON") {
            std::string script;
            std::getline(iss >> std::ws, script);
            if (script.empty()) {
                return "ERROR: Usage: LUA JSON <lua_code>\n";
            }
            std::string json;
            const bool ok = LuaManager::Get().ExecuteStringJson(script, json);
            return ok ? json : (std::string("ERROR: ") + json + "\n");
        }

        if (subCmd == "EVAL" || subCmd == "RUN") {
            std::string script;
            std::getline(iss >> std::ws, script);
            if (script.empty()) {
                return "ERROR: Usage: LUA EVAL <lua_code>\n";
            }
            std::string result;
            bool ok = LuaManager::Get().ExecuteString(script, result);
            return (ok ? "OK: " : "ERROR: ") + result + "\n";
        }

        if (subCmd == "LOAD" || subCmd == "DOFILE") {
            std::string filePath;
            std::getline(iss >> std::ws, filePath);
            filePath = TrimQuotedPath(filePath);
            if (filePath.empty()) {
                return "ERROR: Usage: LUA LOAD <file_path>\n";
            }
            std::string result;
            bool ok = LuaManager::Get().ExecuteFile(filePath, result);
            return (ok ? "OK: Loaded " + filePath + "\n" : "ERROR: " + result + "\n");
        }

        if (subCmd == "RELOAD" || subCmd == "RESET") {
            if (!LuaManager::Get().IsInitialized()) {
                return "ERROR: Lua mods runtime is disabled in configuration\n";
            }
            LuaManager::Get().ReloadMods();
            return "OK: Lua mods reloaded\n";
        }

        return "ERROR: Unknown LUA subcommand. Valid: STATUS, EVAL, LOAD, RELOAD\n";
    }

    return "ERROR: Unknown command: " + cmd + "\n";
}
