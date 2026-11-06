#include "IPCJobs.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <condition_variable>
#include <deque>
#include <exception>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <utility>

namespace {

struct Job {
    uint64_t id = 0;
    std::string label;
    std::string state = "queued";
    std::string result;
    bool cancelRequested = false;
    uint64_t createdMs = 0;
    uint64_t startedMs = 0;
    uint64_t finishedMs = 0;
    IPCJobs::JobBody body;
};

std::mutex g_Mutex;
std::condition_variable g_Cv;
std::deque<uint64_t> g_Queue;
std::map<uint64_t, Job> g_Jobs;
std::thread g_Thread;
bool g_Running = false;
uint64_t g_NextId = 1;

uint64_t NowMs() {
    return GetTickCount64();
}

std::string EscapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

void Worker() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    for (;;) {
        uint64_t id = 0;
        IPCJobs::JobBody body;
        {
            std::unique_lock<std::mutex> lock(g_Mutex);
            g_Cv.wait(lock, [] { return !g_Running || !g_Queue.empty(); });
            if (!g_Running && g_Queue.empty()) return;

            id = g_Queue.front();
            g_Queue.pop_front();

            auto it = g_Jobs.find(id);
            if (it == g_Jobs.end() || it->second.state != "queued") continue;

            it->second.state = "running";
            it->second.startedMs = NowMs();
            body = it->second.body;
            it->second.body = nullptr;
        }

        std::string response;
        try {
            response = body ? body() : std::string("ERROR: empty job\n");
        } catch (const std::exception& ex) {
            response = std::string("ERROR: Internal exception: ") + ex.what() + "\n";
        } catch (...) {
            response = "ERROR: Internal exception (unknown type)\n";
        }

        {
            std::lock_guard<std::mutex> lock(g_Mutex);
            auto it = g_Jobs.find(id);
            if (it == g_Jobs.end()) continue;

            Job& job = it->second;
            job.finishedMs = NowMs();
            if (job.cancelRequested) {
                job.state = "cancelled";
                job.result.clear();
            } else if (response.rfind("ERROR:", 0) == 0) {
                job.state = "failed";
                job.result = response;
            } else {
                job.state = "done";
                job.result = response;
            }
        }
    }
}

std::string StateJson(const Job& job) {
    uint64_t elapsed = 0;
    if (job.startedMs) {
        elapsed = (job.finishedMs ? job.finishedMs : NowMs()) - job.startedMs;
    }
    std::ostringstream oss;
    oss << "\"id\":" << job.id
        << ",\"label\":\"" << EscapeJson(job.label) << "\""
        << ",\"state\":\"" << job.state << "\""
        << ",\"createdMs\":" << job.createdMs
        << ",\"startedMs\":" << job.startedMs
        << ",\"finishedMs\":" << job.finishedMs
        << ",\"elapsedMs\":" << elapsed;
    return oss.str();
}

}

namespace IPCJobs {

void Start() {
    std::lock_guard<std::mutex> lock(g_Mutex);
    if (g_Running) return;
    g_Running = true;
    g_Thread = std::thread(Worker);
}

void Stop() {
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        if (!g_Running) return;
        g_Running = false;
    }
    g_Cv.notify_all();
    if (g_Thread.joinable()) g_Thread.join();
}

uint64_t Submit(const std::string& label, JobBody body) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    const uint64_t id = g_NextId++;
    Job job;
    job.id = id;
    job.label = label;
    job.createdMs = NowMs();
    job.body = std::move(body);
    g_Jobs[id] = std::move(job);
    g_Queue.push_back(id);
    g_Cv.notify_one();
    return id;
}

std::string Status(uint64_t id) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    auto it = g_Jobs.find(id);
    if (it == g_Jobs.end()) {
        return "{\"ok\":false,\"error\":\"unknown job\"}";
    }
    return "{\"ok\":true," + StateJson(it->second) + "}";
}

std::string List() {
    std::lock_guard<std::mutex> lock(g_Mutex);
    std::ostringstream oss;
    oss << "{\"ok\":true,\"count\":" << g_Jobs.size() << ",\"jobs\":[";
    bool first = true;
    for (const auto& kv : g_Jobs) {
        if (!first) oss << ",";
        first = false;
        oss << "{" << StateJson(kv.second) << "}";
    }
    oss << "]}";
    return oss.str();
}

std::string Result(uint64_t id) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    auto it = g_Jobs.find(id);
    if (it == g_Jobs.end()) {
        return "ERROR: unknown job\n";
    }
    const Job& job = it->second;
    if (job.state == "done" || job.state == "failed") {
        return job.result;
    }
    if (job.state == "cancelled") {
        return "ERROR: job " + std::to_string(id) + " cancelled\n";
    }
    return "ERROR: job " + std::to_string(id) + " is " + job.state + "\n";
}

bool Cancel(uint64_t id, std::string& message) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    auto it = g_Jobs.find(id);
    if (it == g_Jobs.end()) {
        message = "ERROR: unknown job\n";
        return false;
    }

    Job& job = it->second;
    if (job.state == "queued") {
        job.state = "cancelled";
        job.finishedMs = NowMs();
        message = "OK: job " + std::to_string(id) + " cancelled\n";
        return true;
    }
    if (job.state == "running") {
        job.cancelRequested = true;
        job.state = "cancelling";
        message = "OK: job " + std::to_string(id) + " cancel requested\n";
        return true;
    }

    message = "ERROR: job " + std::to_string(id) + " is " + job.state + "\n";
    return false;
}

}
