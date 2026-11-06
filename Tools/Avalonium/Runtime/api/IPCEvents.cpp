#include "IPCEvents.h"

#include <deque>
#include <mutex>

namespace {

std::mutex g_Mutex;
std::deque<std::string> g_Queue;
constexpr size_t kMaxQueuedEvents = 4096;

}

namespace IPCEvents {

void Push(const std::string& json) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    if (g_Queue.size() >= kMaxQueuedEvents) {
        g_Queue.pop_front();
    }
    g_Queue.push_back(json);
}

void Drain(std::vector<std::string>& out) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    out.insert(out.end(), g_Queue.begin(), g_Queue.end());
    g_Queue.clear();
}

void Clear() {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_Queue.clear();
}

}
