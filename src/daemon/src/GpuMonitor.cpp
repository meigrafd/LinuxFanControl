/*
 * Linux Fan Control — GPU monitor (NVML/AMDSMI/IGCL + hwmon + DRM)
 * (c) 2025 LinuxFanControl contributors
 */
#include "include/GpuMonitor.hpp"
#include "include/Hwmon.hpp"
#include "AutoConfig.h"
#include "include/Log.hpp"
#include "include/Utils.hpp"

#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstdio>

#ifdef HAVE_NVML
#  include <nvml.h>
#endif
#ifdef HAVE_AMDSMI
#  include <amdsmi/amdsmi.h>
#endif
#ifdef HAVE_IGCL
#  include <level_zero/ze_api.h>
#  include <level_zero/zes_api.h>
#endif

namespace fs = std::filesystem;
namespace lfc {

/* ============================== helpers =================================== */

std::string GpuMonitor::toLower(std::string v) {
    for (char& c : v) c = (char)std::tolower((unsigned char)c);
    return v;
}

/* Resolve /sys/class/drm/<cardX>/device → .../<0000:bb:dd.f>, return basename. */
std::string GpuMonitor::pciFromDrmNode(const std::string& cardNode) {
    std::error_code ec;
    fs::path p = fs::weakly_canonical(fs::path("/sys/class/drm") / cardNode / "device", ec);
    if (!ec && fs::exists(p)) {
        return p.filename().string();
    }
    return {};
}

/* Normalize PCI bus id to "0000:bb:dd.f", also accept "bb:dd.f" and "cardX". */
std::string GpuMonitor::normBusId(std::string bus) {
    for (auto& c : bus) c = (char)std::tolower((unsigned char)c);
    if (!bus.empty() && bus.rfind("card", 0) == 0) {
        std::string pci = pciFromDrmNode(bus);
        if (!pci.empty()) bus = pci;
    }
    if (bus.size() == 8 && bus[2] == ':' && bus[5] == '.') {
        bus = "0000:" + bus;
    }
    return bus;
}

/* Find matching entry in vec using strict priority:
 * 1) PCI bus id, 2) hwmon path, 3) vendor+name (weak).
 * Returns index or -1.
 */
int GpuMonitor::findMatch(const std::vector<GpuSample>& vec,
                          const std::string& pci, const std::string& hwmon,
                          const std::string& vendor, const std::string& name)
{
    const std::string pciN = normBusId(pci);
    if (!pciN.empty()) {
        for (size_t i = 0; i < vec.size(); ++i) {
            if (!vec[i].pciBusId.empty() && normBusId(vec[i].pciBusId) == pciN) return (int)i;
        }
    }
    if (!hwmon.empty()) {
        for (size_t i = 0; i < vec.size(); ++i) {
            if (!vec[i].hwmonPath.empty() && vec[i].hwmonPath == hwmon) return (int)i;
        }
    }
    if (!vendor.empty() && !name.empty()) {
        for (size_t i = 0; i < vec.size(); ++i) {
            if (!vec[i].vendor.empty() && !vec[i].name.empty() &&
                vec[i].vendor == vendor && vec[i].name == name) {
                return (int)i;
            }
        }
    }
    return -1;
}

/* Merge identifiers + freshest metrics from src into dst. */
void GpuMonitor::mergeInto(GpuSample& dst, const GpuSample& src) {
    auto setIfEmpty = [](std::string& d, const std::string& s) {
        if (d.empty() && !s.empty()) d = s;
    };
    setIfEmpty(dst.vendor,    src.vendor);
    setIfEmpty(dst.name,      src.name);
    setIfEmpty(dst.hwmonPath, src.hwmonPath);
    setIfEmpty(dst.pciBusId,  src.pciBusId);
    if (dst.index < 0 && src.index >= 0) dst.index = src.index;

    auto pickI = [](int& d, int s) { if (s >= 0) d = s; };
    auto pickD = [](double& d, double s) { if (s >= 0) d = s; };

    pickI(dst.tempC,   src.tempC);
    pickI(dst.utilPct, src.utilPct);
    pickI(dst.memPct,  src.memPct);
    pickI(dst.fanPct,  src.fanPct);
    pickI(dst.fanRpm,  src.fanRpm);
    pickD(dst.powerW,  src.powerW);
}

/* ============================= collectors ================================== */

void GpuMonitor::collectFromHwmon(std::vector<GpuSample>& vec) {
    if (!fs::exists("/sys/class/hwmon")) return;

    for (const auto& dir : fs::directory_iterator("/sys/class/hwmon")) {
        if (!dir.is_directory()) continue;
        fs::path base = dir.path();

        std::string name = util::read_first_line(base / "name");
        if (name.empty()) continue;

        std::string lname = toLower(name);
        bool likelyGpu = (lname.find("amdgpu") != std::string::npos ||
                          lname.find("nvidia") != std::string::npos ||
                          lname.find("i915")   != std::string::npos ||
                          lname.find("intel")  != std::string::npos);
        if (!likelyGpu) continue;

        GpuSample s{};
        s.vendor = (lname.find("amdgpu") != std::string::npos) ? "AMD" :
                   (lname.find("nvidia") != std::string::npos) ? "NVIDIA" :
                   (lname.find("i915")   != std::string::npos || lname.find("intel") != std::string::npos) ? "Intel" : "Unknown";
        s.name      = name;
        s.hwmonPath = base.string();

        // Try to resolve PCI bus id via device symlink
        std::error_code ec;
        fs::path dev = fs::weakly_canonical(base / "device", ec);
        if (!ec && fs::exists(dev) && dev.filename().string().find(':') != std::string::npos) {
            s.pciBusId = dev.filename().string();
        }

        // Initial basic metrics (best-effort)
        int bestTemp = -1;
        for (int i = 1; i <= 8; ++i) {
            fs::path p = base / ("temp" + std::to_string(i) + "_input");
            if (fs::exists(p)) {
                int t = 0;
                if (util::read_int_file(p.string(), t)) {
                    t /= 1000;
                    if (t > 0) { bestTemp = t; break; }
                }
            }
        }
        s.tempC = bestTemp;

        int rpm = -1;
        for (int i = 1; i <= 4; ++i) {
            fs::path p = base / ("fan" + std::to_string(i) + "_input");
            if (fs::exists(p)) {
                (void)util::read_int_file(p.string(), rpm);
                break;
            }
        }
        int raw = -1;
        for (int i = 1; i <= 4; ++i) {
            fs::path p = base / ("pwm" + std::to_string(i));
            if (fs::exists(p)) {
                (void)util::read_int_file(p.string(), raw);
                break;
            }
        }
        s.fanRpm = rpm;
        s.fanPct = util::pwmPercentFromRaw(raw);

        const int idx = findMatch(vec, s.pciBusId, s.hwmonPath, s.vendor, s.name);
        if (idx >= 0) mergeInto(vec[(size_t)idx], s);
        else          vec.push_back(std::move(s));
    }
}

void GpuMonitor::collectFromDrm(std::vector<GpuSample>& vec) {
    if (!fs::exists("/sys/class/drm")) return;

    for (const auto& e : fs::directory_iterator("/sys/class/drm")) {
        if (!e.is_directory()) continue;
        const std::string node = e.path().filename().string(); // card0, card1, ...
        if (node.rfind("card", 0) != 0) continue;

        // Resolve PCI bus id from DRM node
        const std::string pci = pciFromDrmNode(node);
        if (pci.empty()) continue;

        GpuSample s{};
        s.pciBusId = pci;

        // Resolve hwmon + name (if present) via device/hwmon
        std::error_code ec;
        fs::path devdir = fs::weakly_canonical(e.path() / "device", ec);
        if (!ec && fs::exists(devdir)) {
            for (const auto& sub : fs::directory_iterator(devdir)) {
                if (sub.path().filename() == "hwmon") {
                    for (const auto& hw : fs::directory_iterator(sub.path())) {
                        if (hw.is_directory()) {
                            s.hwmonPath = hw.path().string();
                            std::string name = util::read_first_line(hw.path() / "name");
                            if (!name.empty()) s.name = name;
                            break;
                        }
                    }
                }
            }
            if (!s.name.empty()) {
                std::string lname = toLower(s.name);
                s.vendor = (lname.find("amdgpu") != std::string::npos) ? "AMD" :
                           (lname.find("nvidia") != std::string::npos) ? "NVIDIA" :
                           (lname.find("i915")   != std::string::npos || lname.find("intel") != std::string::npos) ? "Intel" : "Unknown";
            }
        }

        // Skip if we have literally no useful identifiers
        if (s.pciBusId.empty() && s.hwmonPath.empty() && s.name.empty()) continue;

        const int idx = findMatch(vec, s.pciBusId, s.hwmonPath, s.vendor, s.name);
        if (idx >= 0) mergeInto(vec[(size_t)idx], s);
        else          vec.push_back(std::move(s));
    }
}

/* ============================ vendor enrich ================================= */

#ifdef HAVE_NVML
void GpuMonitor::enrichNvml(std::vector<GpuSample>& vec) {
    if (nvmlInit_v2() != NVML_SUCCESS) return;

    unsigned int count = 0;
    if (nvmlDeviceGetCount_v2(&count) != NVML_SUCCESS) { nvmlShutdown(); return; }

    for (unsigned int i = 0; i < count; ++i) {
        nvmlDevice_t dev{};
        if (nvmlDeviceGetHandleByIndex_v2(i, &dev) != NVML_SUCCESS) continue;

        nvmlPciInfo_t pci{};
        if (nvmlDeviceGetPciInfo_v3(dev, &pci) != NVML_SUCCESS) continue;
        std::string bus = normBusId(pci.busIdLegacy);

        GpuSample s{};
        s.vendor   = "NVIDIA";
        s.pciBusId = bus;
        s.index    = (int)i;

        char name[96] = {0};
        if (nvmlDeviceGetName(dev, name, sizeof(name)) == NVML_SUCCESS) s.name = name;

        unsigned int temp = 0;
        if (nvmlDeviceGetTemperature(dev, NVML_TEMPERATURE_GPU, &temp) == NVML_SUCCESS) s.tempC = (int)temp;

        nvmlUtilization_t util{};
        if (nvmlDeviceGetUtilizationRates(dev, &util) == NVML_SUCCESS) s.utilPct = (int)util.gpu;

        nvmlMemory_t mem{};
        if (nvmlDeviceGetMemoryInfo(dev, &mem) == NVML_SUCCESS) {
            unsigned int tot = (unsigned int)(mem.total / 1024 / 1024);
            unsigned int used = (unsigned int)(mem.used / 1024 / 1024);
            if (tot > 0) s.memPct = (int)(used * 100 / tot);
        }

        unsigned int fan = 0;
        if (nvmlDeviceGetFanSpeed(dev, &fan) == NVML_SUCCESS) s.fanPct = (int)fan;

        unsigned int mw = 0;
        if (nvmlDeviceGetPowerUsage(dev, &mw) == NVML_SUCCESS) s.powerW = (int)(mw / 1000);

        const int idx = findMatch(vec, s.pciBusId, s.hwmonPath, s.vendor, s.name);
        if (idx >= 0) mergeInto(vec[(size_t)idx], s);
        else          vec.push_back(std::move(s));
    }
    nvmlShutdown();
}
#endif

#ifdef HAVE_AMDSMI
void GpuMonitor::enrichAmdSmi(std::vector<GpuSample>& vec) {
    if (amdsmi_init(0) != AMDSMI_STATUS_SUCCESS) return;

    uint32_t devCount = 0;
    if (amdsmi_get_socket_handles(&devCount, nullptr) != AMDSMI_STATUS_SUCCESS || devCount == 0) {
        amdsmi_shut_down();
        return;
    }
    std::vector<amdsmi_socket_handle> sockets(devCount);
    if (amdsmi_get_socket_handles(&devCount, sockets.data()) != AMDSMI_STATUS_SUCCESS) {
        amdsmi_shut_down();
        return;
    }

    for (uint32_t si = 0; si < devCount; ++si) {
        amdsmi_processor_handle proc{};
        if (amdsmi_get_processor_handle_from_socket(si, &proc) != AMDSMI_STATUS_SUCCESS) continue;

        amdsmi_pcie_info_t pinfo{};
        if (amdsmi_get_gpu_pci_id(proc, &pinfo) != AMDSMI_STATUS_SUCCESS) continue;

        char busid[32];
        std::snprintf(busid, sizeof(busid), "%04x:%02x:%02x.%x",
                      pinfo.domain_num, pinfo.bus_num, pinfo.device_num, pinfo.function_num);

        GpuSample s{};
        s.vendor   = "AMD";
        s.pciBusId = normBusId(busid);

        int64_t tC = 0;
        if (amdsmi_get_temp_metric(proc, AMDSMI_TEMPERATURE_TYPE_EDGE, AMDSMI_TEMP_CURRENT, &tC) == AMDSMI_STATUS_SUCCESS) {
            s.tempC = (int)tC;
        }
        uint64_t microw = 0;
        if (amdsmi_get_power_info(proc, &microw) == AMDSMI_STATUS_SUCCESS) {
            s.powerW = (double)microw / 1'000'000.0;
        }

        const int idx = findMatch(vec, s.pciBusId, s.hwmonPath, s.vendor, s.name);
        if (idx >= 0) mergeInto(vec[(size_t)idx], s);
        else          vec.push_back(std::move(s));
    }

    amdsmi_shut_down();
}
#endif

#ifdef HAVE_IGCL
void GpuMonitor::enrichIntelIgcl(std::vector<GpuSample>& vec) {
    if (zeInit(ZE_INIT_FLAG_GPU_ONLY) != ZE_RESULT_SUCCESS) return;

    uint32_t driverCount = 0;
    zeDriverGet(&driverCount, nullptr);
    if (driverCount == 0) return;
    std::vector<ze_driver_handle_t> drivers(driverCount);
    zeDriverGet(&driverCount, drivers.data());

    for (auto drv : drivers) {
        uint32_t devCount = 0;
        zeDeviceGet(drv, &devCount, nullptr);
        if (devCount == 0) continue;
        std::vector<ze_device_handle_t> devs(devCount);
        zeDeviceGet(drv, &devCount, devs.data());

        for (auto dev : devs) {
            ze_device_properties_t props{};
            props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
            if (zeDeviceGetProperties(dev, &props) != ZE_RESULT_SUCCESS) continue;
            if (props.type != ZE_DEVICE_TYPE_GPU) continue;

            GpuSample s{};
            s.vendor = "Intel";
            if (props.name) s.name = props.name;

            const int idx = findMatch(vec, s.pciBusId, s.hwmonPath, s.vendor, s.name);
            if (idx >= 0) mergeInto(vec[(size_t)idx], s);
            else          vec.push_back(std::move(s));
        }
    }
}
#endif

/* =============================== public API ================================= */

std::vector<GpuSample> GpuMonitor::snapshot() {
    std::vector<GpuSample> v;

    // Base inventory from kernel (stable identifiers)
    collectFromDrm(v);
    collectFromHwmon(v);

    // Enrich from vendor SDKs (merge; no duplicates)
#ifdef HAVE_NVML
    enrichNvml(v);
#endif
#ifdef HAVE_AMDSMI
    enrichAmdSmi(v);
#endif
#ifdef HAVE_IGCL
    enrichIntelIgcl(v);
#endif

    LOG_INFO("gpu: snapshot complete (gpus=%zu)", v.size());
    return v;
}

void GpuMonitor::refreshMetrics(std::vector<GpuSample>& inOut) {
    // Lightweight refresh via hwmon; inventory stays constant.
    for (auto& g : inOut) {
        if (g.hwmonPath.empty()) continue;
        fs::path base = g.hwmonPath;

        int bestTemp = -1;
        for (int i = 1; i <= 8; ++i) {
            fs::path p = base / ("temp" + std::to_string(i) + "_input");
            if (fs::exists(p)) {
                int t = 0;
                if (util::read_int_file(p.string(), t)) {
                    t /= 1000;
                    if (t > 0) { bestTemp = t; break; }
                }
            }
        }
        g.tempC = bestTemp;

        int rpm = -1;
        for (int i = 1; i <= 4; ++i) {
            fs::path p = base / ("fan" + std::to_string(i) + "_input");
            if (fs::exists(p)) {
                (void)util::read_int_file(p.string(), rpm);
                break;
            }
        }
        int raw = -1;
        for (int i = 1; i <= 4; ++i) {
            fs::path p = base / ("pwm" + std::to_string(i));
            if (fs::exists(p)) {
                (void)util::read_int_file(p.string(), raw);
                break;
            }
        }
        g.fanRpm = rpm;
        g.fanPct = util::pwmPercentFromRaw(raw);

        // Optional power via hwmon (power1_input/average in microwatts)
        for (const char* cand : {"power1_input", "power1_average"}) {
            fs::path p = base / cand;
            if (fs::exists(p)) {
                long long micro = 0;
                if (util::read_ll_file(p.string(), micro) && micro > 0) {
                    g.powerW = (double)micro / 1'000'000.0;
                    break;
                }
            }
        }
    }

    // Vendor SDKs may refine util/mem/power — merge results, no new devices.
#ifdef HAVE_NVML
    enrichNvml(inOut);
#endif
#ifdef HAVE_AMDSMI
    enrichAmdSmi(inOut);
#endif
#ifdef HAVE_IGCL
    // enrichIntelIgcl(inOut); // optional for metrics
#endif

    LOG_TRACE("gpu: refreshMetrics done (gpus=%zu)", inOut.size());
}

} // namespace lfc
