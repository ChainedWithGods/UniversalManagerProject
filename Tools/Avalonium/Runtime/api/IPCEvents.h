#pragma once

#include <string>
#include <vector>

namespace IPCEvents {

void Push(const std::string& json);
void Drain(std::vector<std::string>& out);
void Clear();

}
