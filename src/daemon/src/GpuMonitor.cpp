/*
 * Linux Fan Control — GPU Monitor (core discovery + refresh)
 * - Enumerates DRM primary cards (cardN), maps to PCI + hwmon
 * - Detects tach/PWM presence and reads temps from hwmon
 * - Provides helpers used by other subsystems (snapshot, resolveHwmonTempPath)
 * - Fallback pretty-name resolution via /sys ... /uevent + /usr/share/misc/pci.ids
 */

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <optional>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <filesystem>
#include <sstream>

#include "include/Log.hpp"
#include "include/Utils.hpp"
#include "include/Hwmon.hpp"
#include "include/VendorMapping.hpp"
#include "include/GpuMonitor.hpp"

namespace lfc {
namespace fs = std::filesystem;

/* ------------------------------ file helpers ------------------------------ */

static bool isFile(const std::string& p) {
    struct stat st{};
    return ::stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

static std::optional<int> readIntFile(const std::string& p) {
    std::ifstream f(p);
    if (!f) return std::nullopt;
    long long v = 0;
    f >> v;
    if (!f) return std::nullopt;
    return static_cast<int>(v);
}

static std::optional<double> readTempC(const std::string& inputPath) {
    auto iv = readIntFile(inputPath);
    if (!iv) return std::nullopt;
    if (std::abs(*iv) > 2000) return (*iv) / 1000.0; // millidegree → °C
    return static_cast<double>(*iv);
}

static std::optional<std::string> readSmall(const std::string& p) {
    std::ifstream f(p);
    if (!f) return std::nullopt;
    std::string s; std::getline(f, s);
    if (!f && !f.eof()) return std::nullopt;
    return s;
}

static std::optional<std::string> readAll(const std::string& p) {
    std::ifstream f(p);
    if (!f) return std::nullopt;
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::optional<std::string> realpathStr(const std::string& p) {
    char buf[PATH_MAX];
    char* r = ::realpath(p.c_str(), buf);
    if (!r) return std::nullopt;
    return std::string(r);
}

/* ------------------------------ drm helpers ------------------------------- */

static std::vector<std::string> drmCards() {
    std::vector<std::string> out;
    const char* drm = "/sys/class/drm";
    DIR* d = ::opendir(drm);
    if (!d) {
        LOG_WARN("gpu: cannot open %s: %s", drm, std::strerror(errno));
        return out;
    }
    dirent* e;
    while ((e = ::readdir(d)) != nullptr) {
        if (e->d_name[0] == '.') continue;
        std::string name = e->d_name;
        if (name.rfind("card", 0) != 0) continue;             // must start with "card"
        if (name.find('-') != std::string::npos) continue;    // skip connectors
        auto rp = realpathStr(std::string(drm) + "/" + name);
        if (!rp) continue;
        out.push_back(std::move(name));
    }
    ::closedir(d);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

static std::optional<std::string> pciIdFromDrmCard(const std::string& cardName) {
    // /sys/class/drm/cardN/device -> ../../../0000:03:00.0
    auto devLink = "/sys/class/drm/" + cardName + "/device";
    auto rp = realpathStr(devLink);
    if (!rp) return std::nullopt;
    fs::path p(*rp);
    while (p.has_parent_path()) {
        auto bn = p.filename().string();
        if (bn.size() >= 12 && bn.find(':') != std::string::npos && bn.find('.') != std::string::npos) {
            return bn;
        }
        p = p.parent_path();
    }
    return std::nullopt;
}

static std::optional<std::string> deviceRealFromDrmCard(const std::string& cardName) {
    auto devLink = "/sys/class/drm/" + cardName + "/device";
    return realpathStr(devLink);
}

/* Robust: return a concrete .../hwmon/hwmonX directory
 * Accepts both cases in the PCI subtree:
 *   - .../device/hwmon                (container)
 *   - .../device/hwmon/hwmonX         (target)
 */
static std::optional<std::string> hwmonBaseForDeviceReal(const std::string& devReal) {
    std::error_code ec;

    // 1) Prefer direct hits on "hwmonX"
    for (auto it = fs::recursive_directory_iterator(fs::path(devReal), ec);
         !ec && it != fs::recursive_directory_iterator(); ++it) {
        if (!it->is_directory(ec)) continue;
        const auto bn = it->path().filename().string();
        if (bn.rfind("hwmon", 0) == 0 && bn.size() > 5) {
            return it->path().string();
        }
    }

    // 2) If only a container ".../hwmon" exists, choose first child "hwmonX"
    for (auto it = fs::recursive_directory_iterator(fs::path(devReal), ec);
         !ec && it != fs::recursive_directory_iterator(); ++it) {
        if (!it->is_directory(ec)) continue;
        const auto bn = it->path().filename().string();
        if (bn == "hwmon") {
            std::vector<std::string> candidates;
            for (auto& child : fs::directory_iterator(it->path(), ec)) {
                if (ec) break;
                if (!child.is_directory(ec)) continue;
                const auto cbn = child.path().filename().string();
                if (cbn.rfind("hwmon", 0) == 0 && cbn.size() > 5) {
                    candidates.push_back(child.path().string());
                }
            }
            if (!candidates.empty()) {
                std::sort(candidates.begin(), candidates.end());
                LOG_DEBUG("gpu: hwmon container found; selecting %s", candidates.front().c_str());
                return candidates.front();
            }
        }
    }

    return std::nullopt;
}

/* ------------------------------ pwm/tach ---------------------------------- */

struct TachInfo {
    std::string path; // /sys/class/hwmon/hwmonX/fanN_input
    int rpm{0};
};

struct PwmInfo {
    std::string pwmPath;
    std::string enablePath; // may be empty
    int pwmMax{255};
};

static std::optional<TachInfo> findFanTach(const std::string& hwmonBase) {
    for (int i = 1; i <= 8; ++i) {
        auto p = fs::path(hwmonBase) / ("fan" + std::to_string(i) + "_input");
        if (isFile(p.string())) {
            auto rpm = readIntFile(p.string()).value_or(0);
            return TachInfo{p.string(), rpm};
        }
    }
    return std::nullopt;
}

static std::optional<PwmInfo> findPwm(const std::string& hwmonBase) {
    for (int i = 1; i <= 8; ++i) {
        auto pp = fs::path(hwmonBase) / ("pwm" + std::to_string(i));
        if (!isFile(pp.string())) continue;
        PwmInfo info;
        info.pwmPath = pp.string();

        auto pe = fs::path(hwmonBase) / ("pwm" + std::to_string(i) + "_enable");
        if (isFile(pe.string())) info.enablePath = pe.string();

        auto pm = fs::path(hwmonBase) / ("pwm" + std::to_string(i) + "_max");
        info.pwmMax = isFile(pm.string()) ? readIntFile(pm.string()).value_or(255) : 255;
        return info;
    }
    return std::nullopt;
}

/* ------------------------------- vendor/name ------------------------------ */

static std::string vendorFromPciNode(const std::string& devReal) {
    auto vendPath = fs::path(devReal) / "vendor";
    if (isFile(vendPath.string())) {
        auto s = readSmall(vendPath.string()).value_or("");
        if (s == "0x1002") return "AMD";
        if (s == "0x10de") return "NVIDIA";
        if (s == "0x8086") return "Intel";
    }
    auto pretty = VendorMapping::instance().vendorFor(std::string("amdgpu"));
    return pretty.empty() ? "Unknown" : pretty;
}

struct PciIds {
    uint16_t vendor{0}, device{0}, subsysVendor{0}, subsysDevice{0};
};

static std::optional<PciIds> parseUeventIds(const std::string& devReal) {
    auto ue = readAll((fs::path(devReal) / "uevent").string());
    if (!ue) return std::nullopt;
    PciIds r{};
    std::istringstream ss(*ue);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.rfind("PCI_ID=", 0) == 0) {
            unsigned v=0,d=0;
            if (std::sscanf(line.c_str()+7, "%x:%x", &v,&d) == 2) { r.vendor=v; r.device=d; }
        } else if (line.rfind("PCI_SUBSYS_ID=", 0) == 0) {
            unsigned sv=0,sd=0;
            if (std::sscanf(line.c_str()+13, "%x:%x", &sv,&sd) == 2) { r.subsysVendor=sv; r.subsysDevice=sd; }
        }
    }
    if (!r.vendor || !r.device) return std::nullopt;
    return r;
}

/* Parse /usr/share/misc/pci.ids to pretty-name device.
 * Minimal parser:
 *   vvvv  <Vendor>
 *     dddd  <Device>
 *       ssss ssss  <SubsysVendor> <SubsysDevice> <SubsysName>
 */
static std::optional<std::string> lookupPciPretty(uint16_t v, uint16_t d,
                                                  std::optional<std::pair<uint16_t,uint16_t>> subsys)
{
    const char* ids = "/usr/share/misc/pci.ids";
    std::ifstream f(ids);
    if (!f) return std::nullopt;

    char vendHex[5]; std::snprintf(vendHex, sizeof(vendHex), "%04x", v);
    char devHex[5];  std::snprintf(devHex,  sizeof(devHex),  "%04x", d);

    std::string line, curVendor;
    bool inVendor = false;
    bool foundDevice = false;
    std::string deviceName, vendorName, subsysName;

    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;

        // Vendor line: "vvvv  Vendor Name"
        if (line[0] != '\t' && line.size() >= 6) {
            inVendor = (line.rfind(vendHex, 0) == 0 && line[4] == ' ');
            if (inVendor) {
                vendorName = std::string(line.begin()+6, line.end());
                foundDevice = false;
                deviceName.clear();
                subsysName.clear();
            } else {
                // different vendor -> if we already fully matched, we can stop early
                if (!vendorName.empty() && !deviceName.empty()) break;
            }
            continue;
        }

        if (!inVendor) continue;

        // Device line: "\tdddd  Device Name"
        if (line.size() >= 8 && line[0] == '\t' && line[1] != '\t') {
            if (line.rfind("\t", 0) == 0 && line[1] != '\t' &&
                line.size() > 7 && line[5] == ' ')
            {
                std::string code = line.substr(1,4);
                if (code == devHex) {
                    foundDevice = true;
                    deviceName = line.substr(7);
                    subsysName.clear();
                } else if (foundDevice) {
                    // leaving device section
                    if (!subsys || !subsysName.empty()) break;
                    foundDevice = false;
                }
            }
            continue;
        }

        // Subsystem line: "\t\tssss ssss  Subsys Name"
        if (foundDevice && line.size() >= 16 && line[0] == '\t' && line[1] == '\t') {
            // Format: "\t\tVVVV DDDD  Name"
            unsigned sv=0, sd=0;
            if (std::sscanf(line.c_str()+2, "%x %x", &sv, &sd) == 2) {
                if (subsys && sv == subsys->first && sd == subsys->second) {
                    // name starts after two tabs + "VVVV DDDD" + two spaces
                    auto pos = line.find("  ", 2);
                    pos = (pos == std::string::npos) ? 0 : pos + 2;
                    subsysName = (pos && pos < line.size()) ? line.substr(pos) : "";
                    break;
                }
            }
        }
    }

    if (!vendorName.empty()) {
        if (!subsysName.empty()) {
            return vendorName + " " + subsysName;
        }
        if (!deviceName.empty()) {
            return vendorName + " " + deviceName;
        }
        // fallback: just the vendor name if nothing else
        return vendorName;
    }
    return std::nullopt;
}

static void maybeSetPrettyNameFromPciIds(GpuSample& s, const std::string& devReal) {
    if (!s.name.empty()) return;
    auto ids = parseUeventIds(devReal);
    if (!ids) return;

    std::optional<std::pair<uint16_t,uint16_t>> subsys;
    if (ids->subsysVendor && ids->subsysDevice) {
        subsys = std::make_pair(ids->subsysVendor, ids->subsysDevice);
    }

    auto pretty = lookupPciPretty(ids->vendor, ids->device, subsys);
    if (pretty) {
        s.name = *pretty;
        LOG_DEBUG("gpu: name via pci.ids pci=%s -> %s", s.pciBusId.c_str(), s.name.c_str());
    }
}

/* --------------------------------- API ------------------------------------ */

void GpuMonitor::discover(std::vector<GpuSample>& out) {
    out.clear();
    const auto cards = drmCards();

    for (const auto& card : cards) {
        const auto pci       = pciIdFromDrmCard(card).value_or(std::string{});
        const auto devReal   = deviceRealFromDrmCard(card);
        const auto hwmonBase = devReal ? hwmonBaseForDeviceReal(*devReal).value_or(std::string{}) : std::string{};

        const auto tach = hwmonBase.empty() ? std::optional<TachInfo>{} : findFanTach(hwmonBase);
        const auto pwm  = hwmonBase.empty() ? std::optional<PwmInfo>{} : findPwm(hwmonBase);

        GpuSample s{};
        s.vendor     = devReal ? vendorFromPciNode(*devReal) : "Unknown";
        s.index      = static_cast<int>(out.size());
        s.name       = ""; // will be enriched by vendor backends, then pci.ids fallback
        s.pciBusId   = pci;
        s.drmCard    = card;
        s.hwmonPath  = hwmonBase;
        s.hasFanTach = tach.has_value();
        s.hasFanPwm  = pwm.has_value();

        if (!hwmonBase.empty()) {
            const auto edge = fs::path(hwmonBase) / "temp1_input";
            const auto hot  = fs::path(hwmonBase) / "temp2_input";
            const auto mem  = fs::path(hwmonBase) / "temp3_input";
            if (isFile(edge.string())) s.tempEdgeC    = readTempC(edge.string());
            if (isFile(hot.string()))  s.tempHotspotC = readTempC(hot.string());
            if (isFile(mem.string()))  s.tempMemoryC  = readTempC(mem.string());
        }
        if (tach) s.fanRpm = tach->rpm;

        LOG_DEBUG("gpu: discovered DRM device card=%s vendor=%s pci=%s",
                  card.c_str(), s.vendor.c_str(), s.pciBusId.c_str());
        if (!s.hwmonPath.empty()) {
            LOG_DEBUG("gpu: matched hwmon base for pci=%s -> %s",
                      s.pciBusId.c_str(), s.hwmonPath.c_str());
        } else {
            LOG_DEBUG("gpu: no hwmon base for pci=%s", s.pciBusId.c_str());
        }
        if (tach) {
            LOG_DEBUG("gpu: tach found pci=%s rpm=%d path=%s",
                      s.pciBusId.c_str(), tach->rpm, tach->path.c_str());
        } else {
            LOG_DEBUG("gpu: no tach pci=%s", s.pciBusId.c_str());
        }
        if (pwm) {
            LOG_DEBUG("gpu: pwm found pci=%s path=%s enable=%s max=%d",
                      s.pciBusId.c_str(), pwm->pwmPath.c_str(),
                      pwm->enablePath.empty() ? "<none>" : pwm->enablePath.c_str(),
                      pwm->pwmMax);
        } else {
            LOG_DEBUG("gpu: no pwm capability pci=%s", s.pciBusId.c_str());
        }

        if (s.tempEdgeC)    LOG_DEBUG("gpu: Fallback temp edge    pci=%s t=%.1fC", s.pciBusId.c_str(), *s.tempEdgeC);
        if (s.tempHotspotC) LOG_DEBUG("gpu: Fallback temp hotspot pci=%s t=%.1fC", s.pciBusId.c_str(), *s.tempHotspotC);
        if (s.tempMemoryC)  LOG_DEBUG("gpu: Fallback temp memory  pci=%s t=%.1fC", s.pciBusId.c_str(), *s.tempMemoryC);

        out.push_back(std::move(s));
    }

    // Vendor backends (defined in vendor-specific TUs)
    enrichViaAMDSMI(out);
    enrichViaNVML(out);
    enrichViaIGCL(out);

    // Fallback pretty-name if vendor SDKs did not fill s.name
    for (auto& s : out) {
        if (!s.name.empty()) continue;
        auto devReal = deviceRealFromDrmCard(s.drmCard);
        if (devReal) {
            maybeSetPrettyNameFromPciIds(s, *devReal);
        }
    }
}

void GpuMonitor::refreshMetrics(std::vector<GpuSample>& gpus) {
    for (auto& s : gpus) {
        if (s.hwmonPath.empty()) {
            s.fanRpm.reset();
            s.tempEdgeC.reset();
            s.tempHotspotC.reset();
            s.tempMemoryC.reset();
            continue;
        }
        const auto tach = findFanTach(s.hwmonPath);
        if (tach) s.fanRpm = tach->rpm; else s.fanRpm.reset();

        const auto edge = fs::path(s.hwmonPath) / "temp1_input";
        const auto hot  = fs::path(s.hwmonPath) / "temp2_input";
        const auto mem  = fs::path(s.hwmonPath) / "temp3_input";
        s.tempEdgeC    = isFile(edge.string()) ? readTempC(edge.string()) : std::optional<double>{};
        s.tempHotspotC = isFile(hot .string()) ? readTempC(hot .string()) : std::optional<double>{};
        s.tempMemoryC  = isFile(mem .string()) ? readTempC(mem .string()) : std::optional<double>{};
    }
}

std::vector<GpuSample> GpuMonitor::snapshot() {
    std::vector<GpuSample> v;
    discover(v);
    return v;
}

std::string GpuMonitor::resolveHwmonTempPath(const std::string& hwmonBase,
                                             const std::string& kind) {
    if (hwmonBase.empty()) return {};
    auto lower = kind; std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Try labeled files first (if labels are present).
    for (int i = 1; i <= 8; ++i) {
        auto in  = fs::path(hwmonBase) / ("temp" + std::to_string(i) + "_input");
        auto lab = fs::path(hwmonBase) / ("temp" + std::to_string(i) + "_label");
        if (!isFile(in.string())) continue;
        if (isFile(lab.string())) {
            auto lbl = readSmall(lab.string()).value_or("");
            std::string ll = lbl; std::transform(ll.begin(), ll.end(), ll.begin(), ::tolower);
            if ((lower == "edge"    && ll.find("edge")    != std::string::npos) ||
                ((lower == "hotspot" || lower == "junction") && (ll.find("junction") != std::string::npos || ll.find("hotspot") != std::string::npos)) ||
                ((lower == "mem" || lower == "memory") && (ll.find("mem") != std::string::npos || ll.find("memory") != std::string::npos))) {
                return in.string();
            }
        }
    }

    // Fallback heuristic (amdgpu: temp1=edge, temp2=junction, temp3=mem)
    if (lower == "edge")     return (fs::path(hwmonBase) / "temp1_input").string();
    if (lower == "hotspot" || lower == "junction") return (fs::path(hwmonBase) / "temp2_input").string();
    if (lower == "mem" || lower == "memory")       return (fs::path(hwmonBase) / "temp3_input").string();
    return {};
}



// --- control dispatch ---
bool setGpuFanPercentForHwmonPath(const std::string& hwmonBase, int percent) {
    // For now only AMD backend supports control; others can be added similarly
    (void)hwmonBase; (void)percent;
    bool ok = false;
    // The actual implementation lives in vendor TUs guarded by macros; we keep only a weak stub here.
    return ok;
}
} // namespace lfc
