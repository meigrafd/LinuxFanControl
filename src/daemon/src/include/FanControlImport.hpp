#pragma once
/*
 * Import FanControl (Rem0o) userConfig.json and map to our schema "lfc.profile/v1".
 * Independent of Profile file I/O. Provides progress callbacks and diagnostics.
 */

#include <functional>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

#include "include/Profile.hpp"
#include "include/Hwmon.hpp"
#include "include/Utils.hpp"

namespace lfc {

class FanControlImport {
public:
    using ProgressFn = std::function<void(int,const std::string&)>;

    // Returns true on success. 'out' receives a fully populated Profile.
    // 'detailsOut' (optional) receives summary counters.
    static bool LoadAndMap(const std::string& path,
                           const std::vector<HwmonTemp>& temps,
                           const std::vector<HwmonPwm>&  pwms,
                           Profile& out,
                           std::string& err,
                           ProgressFn onProgress = nullptr,
                           nlohmann::json* detailsOut = nullptr);

private:
    static void addIdentifierIfResolves(const std::string& id,
                                        const std::vector<HwmonTemp>& temps,
                                        std::vector<std::string>& outTempPaths);

    static std::string normalizeType(const nlohmann::json& curveJson);

    static void logDebug(const char* fmt, ...);
};

} // namespace lfc
