// src/daemon/src/GpuMonitor.AMD.cpp
//
// AMD GPU enrichment using AMD SMI (AMDSMI) SDK.
// This augments GpuMonitor’s sysfs discovery with:
//   - board/marketing name,
//   - temperatures (edge/junction),
//   - fan tachometer RPM,
//   - PWM capability hint.
//
// Sysfs/HWMON discovery remains the baseline and is never removed.
// Matching is done by PCI BDF derived from the sample's hwmonPath,
// which is stable across kernels and does not depend on extra fields.
//
// Build guard: LFC_WITH_AMDSMI is defined by CMake when AMDSMI was found.

#include "include/GpuMonitor.hpp"
#include "include/Log.hpp"
#include "include/Utils.hpp"

#include <vector>
#include <string>
#include <optional>
#include <cstring>
#include <algorithm>
#include <cctype>

#ifdef LFC_WITH_AMDSMI
  #include <amd_smi/amdsmi.h>
  #include <cinttypes>
  #include <cstdio>
#endif

namespace lfc {

#ifdef LFC_WITH_AMDSMI

// --- helpers ----------------------------------------------------------------

// Scan a path and return the *last* substring that matches PCI BDF
// format "dddd:bb:dd.f" (hex digits), which avoids picking "pci0000:00".
static std::string pciFromResolvedPath(const std::string& path) {
    auto ishex = [](char c){ return std::isxdigit(static_cast<unsigned char>(c)) != 0; };

    // Minimal BDF length is 12: "0000:03:00.0"
    const size_t need = 12;
    if (path.size() < need) return {};

    std::string last;
    for (size_t i = 0; i + need <= path.size(); ++i) {
        // dddd:bb:dd.f
        if (!(ishex(path[i]) && ishex(path[i+1]) && ishex(path[i+2]) && ishex(path[i+3]))) continue;
        if (path[i+4] != ':') continue;
        if (!(ishex(path[i+5]) && ishex(path[i+6]))) continue;
        if (path[i+7] != ':') continue;
        if (!(ishex(path[i+8]) && ishex(path[i+9]))) continue;
        if (path[i+10] != '.') continue;
        if (!ishex(path[i+11])) continue;

        // Found a candidate; extend if there are more hex chars after function
        size_t j = i + need;
        while (j < path.size() && ishex(path[j])) ++j; // unusually long funcs, still safe
        last.assign(path.begin() + i, path.begin() + j);
        // Keep scanning to prefer the last (deepest) BDF in the path
    }
    return last;
}

// Return PCI BDF string from a GpuSample using robust fallback.
// We intentionally do not rely on optional struct members like "drm" or "pci".
static std::string getPciBdf(const GpuSample& g) {
    if (!g.hwmonPath.empty()) {
        if (auto b = pciFromResolvedPath(g.hwmonPath); !b.empty()) return b;
    }
    return {};
}

// Format BDF from AMDSMI structure to "dddd:bb:dd.f".
static std::string bdfToString(const amdsmi_bdf_t& b) {
    char out[32];
    std::snprintf(out, sizeof(out),
                  "%04x:%02x:%02x.%1x",
                  (unsigned)b.domain_number,
                  (unsigned)b.bus_number,
                  (unsigned)b.device_number,
                  (unsigned)b.function_number);
    return std::string(out);
}

static void logAmdSmiVersionsOnce() {
#if defined(AMDSMI_VERSION_MAJOR)
    static bool did = false;
    if (did) return;
    did = true;
    amdsmi_version_t ver{};
    if (amdsmi_get_version(&ver) == AMDSMI_STATUS_SUCCESS) {
        LOG_DEBUG("gpu: AMDSMI lib v=%u.%u.%u drv=%u.%u.%u",
                  (unsigned)ver.major, (unsigned)ver.minor, (unsigned)ver.patch,
                  (unsigned)ver.drv_major, (unsigned)ver.drv_minor, (unsigned)ver.drv_patch);
    } else {
        LOG_DEBUG("gpu: AMDSMI version query not available");
    }
#endif
}

// --- enumeration ------------------------------------------------------------

static std::vector<amdsmi_socket_handle> enumerateSockets() {
    uint32_t count = 0;
    amdsmi_status_t st = amdsmi_get_socket_handles(&count, nullptr);
    LOG_DEBUG("gpu: AMDSMI get_socket_handles status=%d count=%u", (int)st, count);

    std::vector<amdsmi_socket_handle> sockets;
    if (st == AMDSMI_STATUS_SUCCESS && count > 0) {
        sockets.resize(count);
        st = amdsmi_get_socket_handles(&count, sockets.data());
        if (st == AMDSMI_STATUS_SUCCESS) {
            sockets.resize(count);
            LOG_DEBUG("gpu: AMDSMI sockets filled=%u", count);
        } else {
            sockets.clear();
            LOG_DEBUG("gpu: AMDSMI get_socket_handles(data) failed status=%d", (int)st);
        }
    }
    return sockets;
}

static void appendProcessorsForSocket(std::vector<amdsmi_processor_handle>& out,
                                      amdsmi_socket_handle s)
{
    uint32_t cnt = 0;
    amdsmi_status_t st = amdsmi_get_processor_handles(s, &cnt, nullptr);
    LOG_DEBUG("gpu: AMDSMI get_processor_handles(sock) status=%d count=%u", (int)st, cnt);
    if (st != AMDSMI_STATUS_SUCCESS || cnt == 0) return;

    size_t base = out.size();
    out.resize(base + cnt, nullptr);
    st = amdsmi_get_processor_handles(s, &cnt, out.data() + base);
    if (st == AMDSMI_STATUS_SUCCESS) {
        out.resize(base + cnt);
        LOG_DEBUG("gpu: AMDSMI processors(sock) filled=%u", cnt);
    } else {
        out.resize(base);
        LOG_DEBUG("gpu: AMDSMI get_processor_handles(sock,data) failed status=%d", (int)st);
    }
}

static void appendProcessorsGlobal(std::vector<amdsmi_processor_handle>& out) {
    uint32_t cnt = 0;
    amdsmi_status_t st = amdsmi_get_processor_handles(nullptr, &cnt, nullptr);
    LOG_DEBUG("gpu: AMDSMI get_processor_handles(global) status=%d count=%u", (int)st, cnt);
    if (st != AMDSMI_STATUS_SUCCESS || cnt == 0) return;

    out.resize(cnt, nullptr);
    st = amdsmi_get_processor_handles(nullptr, &cnt, out.data());
    if (st == AMDSMI_STATUS_SUCCESS) {
        out.resize(cnt);
        LOG_DEBUG("gpu: AMDSMI processors(global) filled=%u", cnt);
    } else {
        out.clear();
        LOG_DEBUG("gpu: AMDSMI get_processor_handles(global,data) failed status=%d", (int)st);
    }
}

static std::vector<amdsmi_processor_handle> enumerateProcessors() {
    std::vector<amdsmi_processor_handle> procs;
    auto sockets = enumerateSockets();
    for (auto s : sockets) appendProcessorsForSocket(procs, s);
    if (procs.empty()) appendProcessorsGlobal(procs);
    LOG_DEBUG("gpu: AMDSMI processors total=%zu", procs.size());
    return procs;
}

// Match AMDSMI handle by PCI BDF.
static amdsmi_processor_handle findHandleByBdf(const std::vector<amdsmi_processor_handle>& handles,
                                               const std::string& wantPci)
{
    if (wantPci.empty()) return nullptr;
    for (auto h : handles) {
        amdsmi_bdf_t bdf{};
        if (amdsmi_get_gpu_device_bdf(h, &bdf) == AMDSMI_STATUS_SUCCESS) {
            const auto s = bdfToString(bdf);
            LOG_DEBUG("gpu: AMDSMI candidate bdf=%s", s.c_str());
            if (s == wantPci) return h;
        }
    }
    return nullptr;
}

// --- data readers -----------------------------------------------------------

static std::optional<double> readTempC(amdsmi_processor_handle h,
                                       amdsmi_temperature_type_t type)
{
    int64_t milli_c = 0;
    amdsmi_status_t st = amdsmi_get_temp_metric(h, type, AMDSMI_TEMP_CURRENT, &milli_c);
    if (st != AMDSMI_STATUS_SUCCESS) return std::nullopt;
    return static_cast<double>(milli_c) / 1000.0;
}

// Probe a few fan sensor indices and return the first valid RPM.
static std::optional<int> readFanRpm(amdsmi_processor_handle h) {
    for (uint32_t idx = 0; idx < 8; ++idx) {
        int64_t rpm = 0;
        amdsmi_status_t st = amdsmi_get_gpu_fan_rpms(h, idx, &rpm);
        if (st == AMDSMI_STATUS_SUCCESS && rpm >= 0) {
            return static_cast<int>(rpm);
        }
    }
    return std::nullopt;
}

static bool hasPwmCapability(amdsmi_processor_handle h) {
    for (uint32_t idx = 0; idx < 8; ++idx) {
        int64_t pct = 0;
        if (amdsmi_get_gpu_fan_speed(h, idx, &pct) == AMDSMI_STATUS_SUCCESS) {
            return true;
        }
    }
    return false;
}

static std::string queryMarketingName(amdsmi_processor_handle h) {
    amdsmi_asic_info_t asic{};
    if (amdsmi_get_gpu_asic_info(h, &asic) == AMDSMI_STATUS_SUCCESS) {
        if (asic.market_name[0]) return std::string(asic.market_name);
    }
    amdsmi_board_info_t board{};
    if (amdsmi_get_gpu_board_info(h, &board) == AMDSMI_STATUS_SUCCESS) {
        if (board.product_name[0]) return std::string(board.product_name);
    }
    return {};
}

#endif // LFC_WITH_AMDSMI


void GpuMonitor::enrichViaAMDSMI(std::vector<GpuSample>& out)
{
#ifndef LFC_WITH_AMDSMI
    (void)out;
    return;
#else
    amdsmi_status_t st = amdsmi_init(AMDSMI_INIT_AMD_GPUS);
    if (st != AMDSMI_STATUS_SUCCESS) {
        LOG_DEBUG("gpu: AMDSMI init failed (status=%d)", (int)st);
        return;
    }
    LOG_DEBUG("gpu: AMDSMI init OK");
    logAmdSmiVersionsOnce();

    auto handles = enumerateProcessors();
    if (handles.empty()) {
        LOG_DEBUG("gpu: AMDSMI no processors after robust enumeration");
        (void)amdsmi_shut_down();
        return;
    }

    for (auto& g : out) {
        if (g.vendor != "AMD") continue;

        const std::string wantPci = getPciBdf(g);
        if (wantPci.empty()) {
            LOG_DEBUG("gpu: AMDSMI skip (no pci) index=%d vendor=%s hwmon='%s'",
                      g.index, g.vendor.c_str(), g.hwmonPath.c_str());
            continue;
        }

        auto h = findHandleByBdf(handles, wantPci);
        if (!h) {
            LOG_DEBUG("gpu: AMDSMI no handle match for pci=%s", wantPci.c_str());
            continue;
        }

        if (g.name.empty()) {
            if (auto nm = queryMarketingName(h); !nm.empty()) {
                g.name = std::move(nm);
                LOG_DEBUG("gpu: AMDSMI name pci=%s -> '%s'", wantPci.c_str(), g.name.c_str());
            }
        }

        if (!g.tempEdgeC)    if (auto v = readTempC(h, AMDSMI_TEMPERATURE_TYPE_EDGE))     g.tempEdgeC    = *v;
        if (!g.tempHotspotC) if (auto v = readTempC(h, AMDSMI_TEMPERATURE_TYPE_JUNCTION)) g.tempHotspotC = *v;

        if (!g.fanRpm) {
            if (auto rpm = readFanRpm(h)) {
                g.fanRpm = *rpm;
                g.hasFanTach = true;
            }
        }
        if (!g.hasFanPwm) {
            g.hasFanPwm = hasPwmCapability(h);
        }

        LOG_DEBUG("gpu: AMDSMI enriched pci=%s edge=%.1fC hot=%.1fC mem=%.1fC rpm=%d pwmCap=%d name='%s'",
                  wantPci.c_str(),
                  g.tempEdgeC.value_or(-1.0),
                  g.tempHotspotC.value_or(-1.0),
                  g.tempMemoryC.value_or(-1.0),
                  g.fanRpm.value_or(0),
                  g.hasFanPwm ? 1 : 0,
                  g.name.c_str());
    }

    (void)amdsmi_shut_down();
#endif
}

} // namespace lfc
