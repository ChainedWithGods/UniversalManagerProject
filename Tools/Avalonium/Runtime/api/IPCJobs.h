#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace IPCJobs {

using JobBody = std::function<std::string()>;

void Start();
void Stop();

uint64_t Submit(const std::string& label, JobBody body);

std::string Status(uint64_t id);
std::string List();

std::string Result(uint64_t id);

bool Cancel(uint64_t id, std::string& message);

}
