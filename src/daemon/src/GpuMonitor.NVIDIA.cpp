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

static std::optional<std::string> pciOf(nvmlDevice_t dev) {
    nvmlPciInfo_t pci{};
    // Prefer the newest symbol if present; fall back otherwise.
    nvmlReturn_t r = NVML_ERROR_NOT_SUPPORTED;

    // v3 (newer)
#if defined(NVML_API_VERSION) && (NVML_API_VERSION >= 12)
    r = nvmlDeviceGetPciInfo_v3(dev, &pci);
    if (r == NVML_SUCCESS) return normalizeBdf(pci.busId);
#endif
    // v2
#if defined(NVML_API_VERSION) && (NVML_API_VERSION >= 10)
    if (r != NVML_SUCCESS) {
        r = nvmlDeviceGetPciInfo_v2(dev, &pci);
        if (r == NVML_SUCCESS) return normalizeBdf(pci.busId);
    }
#endif
    // legacy
    if (r != NVML_SUCCESS) {
        r = nvmlDeviceGetPciInfo(dev, &pci);
        if (r == NVML_SUCCESS) return normalizeBdf(pci.busId);
    }
    return std::nullopt;
}

static std::optional<double> readTempGpuC(nvmlDevice_t dev) {
    unsigned int t = 0;
    if (nvmlDeviceGetTemperature(dev, NVML_TEMPERATURE_GPU, &t) == NVML_SUCCESS) {
        return static_cast<double>(t);
    }
    return std::nullopt;
}

static std::optional<double> readTempMemC(nvmlDevice_t dev) {
#ifdef NVML_TEMPERATURE_MEMORY
    unsigned int t = 0;
    if (nvmlDeviceGetTemperature(dev, NVML_TEMPERATURE_MEMORY, &t) == NVML_SUCCESS) {
        return static_cast<double>(t);
    }
#endif
    return std::nullopt;
}

static void tryFillName(nvmlDevice_t dev, std::string& name) {
    if (!name.empty()) return;
    char buf[NVML_DEVICE_NAME_BUFFER_SIZE] = {};
    if (nvmlDeviceGetName(dev, buf, sizeof(buf)) == NVML_SUCCESS && buf[0]) {
        name = buf;
    }
}

#endif // LFC_WITH_NVML

void GpuMonitor::enrichViaNVML(std::vector<GpuSample>& out)
{
#ifndef LFC_WITH_NVML
    (void)out;
    return;
#else
    nvmlReturn_t rc = nvmlInit_v2();
    if (rc != NVML_SUCCESS) {
        LOG_DEBUG("gpu: NVML init failed rc=%d", (int)rc);
        return;
    }

    unsigned int count = 0;
    rc = nvmlDeviceGetCount_v2(&count);
    if (rc != NVML_SUCCESS || count == 0) {
        LOG_DEBUG("gpu: NVML no devices rc=%d", (int)rc);
        (void)nvmlShutdown();
        return;
    }

    std::vector<nvmlDevice_t> devs;
    devs.reserve(count);
    for (unsigned int i = 0; i < count; ++i) {
        nvmlDevice_t d{};
        if (nvmlDeviceGetHandleByIndex_v2(i, &d) == NVML_SUCCESS) {
            devs.push_back(d);
        }
    }

    for (auto& g : out) {
        if (g.vendor != "NVIDIA") continue;

        // Match by PCI BDF
        nvmlDevice_t match{};
        bool found = false;
        for (auto d : devs) {
            auto bdf = pciOf(d);
            if (bdf && *bdf == g.pci) {
                match = d; found = true; break;
            }
        }
        if (!found) {
            LOG_DEBUG("gpu: NVML no handle match pci=%s", g.pci.c_str());
            continue;
        }

        tryFillName(match, g.name);

        // Temps
        if (!g.tempEdgeC) {
            if (auto t = readTempGpuC(match)) g.tempEdgeC = *t;
        }
        if (!g.tempMemoryC) {
            if (auto t = readTempMemC(match)) g.tempMemoryC = *t;
        }
        // NVML has no public RPM; do not fabricate. We keep tach via hwmon fallback when available.

        // PWM capability hint: if fan percent query works, assume control surface exists somewhere.
        if (!g.hasFanPwm) {
            unsigned int pct = 0;
            if (nvmlDeviceGetFanSpeed(match, &pct) == NVML_SUCCESS) {
                g.hasFanPwm = true;
            }
        }

        LOG_DEBUG("gpu: NVML enriched pci=%s name='%s' edge=%.1fC hot=%.1fC mem=%.1fC rpm=%d pwmCap=%d",
                  g.pci.c_str(),
                  g.name.c_str(),
                  g.tempEdgeC.value_or(-1.0),
                  g.tempHotspotC.value_or(-1.0),
                  g.tempMemoryC.value_or(-1.0),
                  g.fanRpm.value_or(0),
                  g.hasFanPwm ? 1 : 0);
    }

    (void)nvmlShutdown();
#endif
}

} // namespace lfc
