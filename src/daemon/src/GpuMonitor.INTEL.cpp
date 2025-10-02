/*
 * Linux Fan Control — Intel GPU enrichment via IGCL/Level Zero (ZES)
 * (c) 2025 LinuxFanControl contributors
 */

// src/daemon/src/GpuMonitor.INTEL.cpp
#include "include/GpuMonitor.hpp"
#include "include/Log.hpp"
#include "include/Utils.hpp"

#ifdef LFC_WITH_IGCL
  // We use the Level Zero System Management (ZES) API (commonly packaged with Intel oneAPI).
  #include <level_zero/ze_api.h>
  #include <level_zero/zes_api.h>
  #include <vector>
  #include <string>
  #include <optional>
  #include <cstring>
  #include <cstdio>
#endif

namespace lfc {

#ifdef LFC_WITH_IGCL

// Normalize PCI address from ZES properties (already "dddd:bb:dd.f")
static std::string normalizeBdf(const char* s) {
    if (!s || !*s) return {};
    return std::string(s);
}

static std::vector<zes_device_handle_t> enumerateDevices() {
    std::vector<zes_device_handle_t> out;

    // Init ZES (this also initializes ZE)
    if (zesInit(0) != ZE_RESULT_SUCCESS) return out;

    uint32_t dcount = 0;
    if (zesDeviceGet(nullptr, &dcount, nullptr) != ZE_RESULT_SUCCESS || dcount == 0) {
        return out;
    }
    out.resize(dcount);
    if (zesDeviceGet(nullptr, &dcount, out.data()) != ZE_RESULT_SUCCESS) {
        out.clear();
    }
    return out;
}

static std::optional<std::string> pciOf(zes_device_handle_t dev) {
    zes_device_properties_t props{};
    props.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    if (zesDeviceGetProperties(dev, &props) != ZE_RESULT_SUCCESS) return std::nullopt;
    if (props.pciAddress[0]) return normalizeBdf(props.pciAddress);
    return std::nullopt;
}

static void tryFillName(zes_device_handle_t dev, std::string& name) {
    if (!name.empty()) return;
    zes_device_properties_t props{};
    props.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    if (zesDeviceGetProperties(dev, &props) == ZE_RESULT_SUCCESS) {
        if (props.modelName[0]) name = props.modelName;
        else if (props.brandName[0]) name = props.brandName;
        else if (props.name[0]) name = props.name;
    }
}

static void readTemps(zes_device_handle_t dev,
                      std::optional<double>& edgeC,
                      std::optional<double>& hotspotC,
                      std::optional<double>& memC)
{
    uint32_t n = 0;
    if (zesDeviceEnumTemperatureSensors(dev, &n, nullptr) != ZE_RESULT_SUCCESS || n == 0) return;
    std::vector<zes_temp_handle_t> hs(n);
    if (zesDeviceEnumTemperatureSensors(dev, &n, hs.data()) != ZE_RESULT_SUCCESS) return;

    for (auto h : hs) {
        zes_temp_properties_t p{};
        p.stype = ZES_STRUCTURE_TYPE_TEMP_PROPERTIES;
        if (zesTemperatureGetProperties(h, &p) != ZE_RESULT_SUCCESS) continue;

        double v = 0.0;
        if (p.onSubdevice) {
            // Ignore subdevice split for now; pick max per sensor type
        }
        zes_temp_sensors_t type = p.type;
        double valC = 0.0;
        if (zesTemperatureGetState(h, &valC) != ZE_RESULT_SUCCESS) continue;

        switch (type) {
            case ZES_TEMP_SENSORS_GPU:       if (!edgeC || valC > *edgeC)    edgeC = valC; break;
            case ZES_TEMP_SENSORS_GLOBAL:    if (!edgeC || valC > *edgeC)    edgeC = valC; break;
#ifdef ZES_TEMP_SENSORS_GPU_MEMORY
            case ZES_TEMP_SENSORS_GPU_MEMORY:if (!memC  || valC > *memC)     memC  = valC; break;
#endif
#ifdef ZES_TEMP_SENSORS_HOT_SPOT
            case ZES_TEMP_SENSORS_HOT_SPOT:  if (!hotspotC || valC > *hotspotC) hotspotC = valC; break;
#endif
            default: break;
        }
        (void)v;
    }
}

static std::optional<int> readFanRpm(zes_device_handle_t dev) {
    uint32_t n = 0;
    if (zesDeviceEnumFans(dev, &n, nullptr) != ZE_RESULT_SUCCESS || n == 0) return std::nullopt;
    std::vector<zes_fan_handle_t> hs(n);
    if (zesDeviceEnumFans(dev, &n, hs.data()) != ZE_RESULT_SUCCESS) return std::nullopt;

    for (auto h : hs) {
        zes_fan_speed_t sp{};
        sp.mode = ZES_FAN_SPEED_MODE_RPM;
        if (zesFanGetState(h, &sp) == ZE_RESULT_SUCCESS && sp.speed) {
            return (int)sp.speed;
        }
    }
    return std::nullopt;
}

static bool hasPwmCapability(zes_device_handle_t dev) {
    uint32_t n = 0;
    if (zesDeviceEnumFans(dev, &n, nullptr) != ZE_RESULT_SUCCESS || n == 0) return false;
    std::vector<zes_fan_handle_t> hs(n);
    if (zesDeviceEnumFans(dev, &n, hs.data()) != ZE_RESULT_SUCCESS) return false;

    for (auto h : hs) {
        zes_fan_properties_t prop{};
        prop.stype = ZES_STRUCTURE_TYPE_FAN_PROPERTIES;
        if (zesFanGetProperties(h, &prop) != ZE_RESULT_SUCCESS) continue;
        if (prop.canControl) return true;
    }
    return false;
}

#endif // LFC_WITH_IGCL

void GpuMonitor::enrichViaIGCL(std::vector<GpuSample>& out)
{
#ifndef LFC_WITH_IGCL
    (void)out;
    return;
#else
    auto devs = enumerateDevices();
    if (devs.empty()) {
        LOG_DEBUG("gpu: IGCL/ZES no devices");
        return;
    }

    for (auto& g : out) {
        if (g.vendor != "Intel") continue;

        // Match by PCI BDF
        zes_device_handle_t match{};
        bool found = false;
        for (auto d : devs) {
            auto bdf = pciOf(d);
            if (bdf && *bdf == g.pci) { match = d; found = true; break; }
        }
        if (!found) {
            LOG_DEBUG("gpu: IGCL no handle match pci=%s", g.pci.c_str());
            continue;
        }

        tryFillName(match, g.name);

        // Temperatures
        readTemps(match, g.tempEdgeC, g.tempHotspotC, g.tempMemoryC);

        // Fan tachometer & capability (if exposed)
        if (!g.fanRpm) {
            if (auto rpm = readFanRpm(match)) {
                g.fanRpm = *rpm;
                g.hasFanTach = true;
            }
        }
        if (!g.hasFanPwm) {
            g.hasFanPwm = hasPwmCapability(match);
        }

        LOG_DEBUG("gpu: IGCL enriched pci=%s name='%s' edge=%.1fC hot=%.1fC mem=%.1fC rpm=%d pwmCap=%d",
                  g.pci.c_str(),
                  g.name.c_str(),
                  g.tempEdgeC.value_or(-1.0),
                  g.tempHotspotC.value_or(-1.0),
                  g.tempMemoryC.value_or(-1.0),
                  g.fanRpm.value_or(0),
                  g.hasFanPwm ? 1 : 0);
    }
#endif
}



bool gpuSetFanPercent_INTEL(const std::string& hwmonBase, int percent)
{
#ifndef LFC_WITH_IGCL
    (void)hwmonBase; (void)percent;
    return false;
#else
    if (hwmonBase.empty()) return false;
    GpuSample tmp{};
    tmp.hwmonPath = hwmonBase;
    const std::string bdf = getPciBdf(tmp);
    if (bdf.empty()) return false;

    // TODO: Wire to your actual Intel control backend (IGCL/oneAPI).
    // Placeholder symbol names:
    igcl_device_handle dev = igcl_find_device_by_bdf(bdf.c_str());
    if (!dev) return false;
    unsigned int d = (unsigned int)std::max(0, std::min(100, percent));
    return igcl_set_fan_percent(dev, d);
#endif
}
} // namespace lfc
