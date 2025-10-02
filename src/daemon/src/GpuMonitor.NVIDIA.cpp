/*
 * Linux Fan Control — NVIDIA GPU enrichment via NVML
 * (c) 2025 LinuxFanControl contributors
 */

// src/daemon/src/GpuMonitor.NVIDIA.cpp
#include "include/GpuMonitor.hpp"
#include "include/Log.hpp"
#include "include/Utils.hpp"

#ifdef LFC_WITH_NVML
  #include <nvml.h>
  #include <vector>
  #include <string>
  #include <cstring>
  #include <optional>
  #include <cstdio>
#endif

namespace lfc {

#ifdef LFC_WITH_NVML

// Normalize PCI BDF string to "0000:bb:dd.f"
static std::string normalizeBdf(const char* busId) {
    if (!busId || !*busId) return {};
    // NVML already returns canonical form "0000:bb:dd.f"
    return std::string(busId);
}

static std::vector<nvmlDevice_t> enumerateDevices() {
    std::vector<nvmlDevice_t> out;
    if (nvmlInit_v2() != NVML_SUCCESS) return out;

    unsigned int count = 0;
    if (nvmlDeviceGetCount_v2(&count) != NVML_SUCCESS || count == 0) {
        (void)nvmlShutdown();
        return out;
    }
    out.resize(count);
    for (unsigned int i = 0; i < count; ++i) {
        if (nvmlDeviceGetHandleByIndex_v2(i, &out[i]) != NVML_SUCCESS) {
            out[i] = nullptr;
        }
    }
    // remove nulls
    out.erase(std::remove(out.begin(), out.end(), nullptr), out.end());
    return out;
}

static std::optional<std::string> pciOf(nvmlDevice_t dev) {
    nvmlPciInfo_t pci{};
    if (nvmlDeviceGetPciInfo_v3(dev, &pci) != NVML_SUCCESS) return std::nullopt;
    return normalizeBdf(pci.busId);
}

static void tryFillName(nvmlDevice_t dev, std::string& name) {
    if (!name.empty()) return;
    char buf[128] = {};
    if (nvmlDeviceGetName(dev, buf, sizeof(buf)) == NVML_SUCCESS && buf[0]) {
        name = buf;
    }
}

static void readTemps(nvmlDevice_t dev,
                      std::optional<double>& edgeC,
                      std::optional<double>& hotspotC,
                      std::optional<double>& memC)
{
    // Edge / GPU core
    unsigned int t = 0;
    if (!edgeC && nvmlDeviceGetTemperature(dev, NVML_TEMPERATURE_GPU, &t) == NVML_SUCCESS) {
        edgeC = (double)t;
    }

#ifdef NVML_TEMPERATURE_SENSOR_GPU_MEMORY
    // Memory temperature if available (recent NVML)
    if (!memC && nvmlDeviceGetTemperature(dev, NVML_TEMPERATURE_MEMORY, &t) == NVML_SUCCESS) {
        memC = (double)t;
    }
#endif

#ifdef NVML_TEMPERATURE_GPU_HOTSPOT
    if (!hotspotC && nvmlDeviceGetTemperature(dev, NVML_TEMPERATURE_GPU_HOTSPOT, &t) == NVML_SUCCESS) {
        hotspotC = (double)t;
    }
#endif
}

static std::optional<int> readFanRpm(nvmlDevice_t dev) {
    // NVML exposes fan speed in percent; tach RPM is not always available.
    // Try RPM first (if supported), otherwise map percent to RPM if max known (not here).
#ifdef NVML_FAN_SPEED_RPM
    unsigned int rpm = 0;
    if (nvmlDeviceGetFanSpeed_v2(dev, &rpm) == NVML_SUCCESS && rpm > 0) {
        return (int)rpm;
    }
#endif
    return std::nullopt;
}

static std::optional<int> readFanPercent(nvmlDevice_t dev) {
    unsigned int pct = 0;
    if (nvmlDeviceGetFanSpeed(dev, &pct) == NVML_SUCCESS) {
        return (int)pct;
    }
    return std::nullopt;
}

static bool hasPwmCapability(nvmlDevice_t dev) {
    // If NVML exposes a settable fan control policy or supports manual control, we could probe.
    // Conservative: treat as capability present if we can read fan speed (percent).
    (void)dev;
    return true;
}

#endif // LFC_WITH_NVML

void GpuMonitor::enrichViaNVML(std::vector<GpuSample>& out)
{
#ifndef LFC_WITH_NVML
    (void)out;
    return;
#else
    auto devs = enumerateDevices();
    if (devs.empty()) {
        LOG_DEBUG("gpu: NVML no devices");
        return;
    }

    for (auto& g : out) {
        if (g.vendor != "NVIDIA") continue;

        // Match by PCI BDF
        nvmlDevice_t match{};
        bool found = false;
        for (auto d : devs) {
            auto bdf = pciOf(d);
            if (bdf && *bdf == g.pci) { match = d; found = true; break; }
        }
        if (!found) {
            LOG_DEBUG("gpu: NVML no handle match pci=%s", g.pci.c_str());
            continue;
        }

        tryFillName(match, g.name);

        // Temperatures
        readTemps(match, g.tempEdgeC, g.tempHotspotC, g.tempMemoryC);

        // Fan tach / percent & capability
        if (!g.fanRpm) {
            if (auto rpm = readFanRpm(match)) {
                g.fanRpm = *rpm;
                g.hasFanTach = true;
            }
        }
        if (!g.fanPercent) {
            if (auto p = readFanPercent(match)) g.fanPercent = *p;
        }
        if (!g.hasFanPwm) {
            g.hasFanPwm = hasPwmCapability(match);
        }

        LOG_DEBUG("gpu: NVML enriched pci=%s name='%s' edge=%.1fC hot=%.1fC mem=%.1fC rpm=%d %%=%d pwmCap=%d",
                  g.pci.c_str(),
                  g.name.c_str(),
                  g.tempEdgeC.value_or(-1.0),
                  g.tempHotspotC.value_or(-1.0),
                  g.tempMemoryC.value_or(-1.0),
                  g.fanRpm.value_or(0),
                  g.fanPercent.value_or(-1),
                  g.hasFanPwm ? 1 : 0);
    }

    (void)nvmlShutdown();
#endif
}



bool gpuSetFanPercent_NVIDIA(const std::string& hwmonBase, int percent)
{
#ifndef LFC_WITH_NVML
    (void)hwmonBase; (void)percent;
    return false;
#else
    if (hwmonBase.empty()) return false;
    GpuSample tmp{};
    tmp.hwmonPath = hwmonBase;
    const std::string bdf = getPciBdf(tmp);
    if (bdf.empty()) return false;

    if (nvmlInit_v2() != NVML_SUCCESS) return false;
    nvmlDevice_t dev = nullptr;
    if (nvmlDeviceGetHandleByPciBusId_v2(bdf.c_str(), &dev) != NVML_SUCCESS) {
        nvmlShutdown();
        return false;
    }
    unsigned int d = (unsigned int)std::max(0, std::min(100, percent));
    nvmlReturn_t r = nvmlDeviceSetFanSpeed_v2(dev, 0 /* fan index */, d);
    if (r != NVML_SUCCESS) {
        nvmlShutdown();
        return false;
    }
    nvmlShutdown();
    return true;
#endif
}
} // namespace lfc
