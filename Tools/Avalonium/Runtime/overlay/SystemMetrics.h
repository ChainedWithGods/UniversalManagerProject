#pragma once

namespace SystemMetrics {

struct Snapshot {
    float CpuUsagePercent = -1.0f;
    float GpuUsagePercent = -1.0f;
    float GpuTemperatureC = -1.0f;
};

void Initialize();

Snapshot Poll();

}
