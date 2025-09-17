/*
 * Linux Fan Control — FanControl.Releases importer
 * Maps upstream JSON schema to our Profile (incl. Mix/Trigger)
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <string>
#include <vector>
#include <utility>
#include <nlohmann/json_fwd.hpp>

namespace lfc {

struct HwmonTemp;     // from Hwmon.hpp
struct HwmonPwm;      // from Hwmon.hpp
struct Profile;       // from Profile.hpp

class FanControlImport {
public:
    // Load FanControl.Releases JSON from path, map to Profile using discovered temps/pwms.
    // On success returns true and fills 'out'. On error returns false and sets 'err'.
    static bool LoadAndMap(const std::string& path,
                           const std::vector<HwmonTemp>& temps,
                           const std::vector<HwmonPwm>& pwms,
                           Profile& out,
                           std::string& err);

    template <typename HW>
    static bool LoadAndMap(const std::string& path, const HW& hw, Profile& out, std::string& err) {
        return LoadAndMap(path, hw.temps, hw.pwms, out, err);
    }

private:
    static std::string mapPwmFromIdentifier(const std::vector<HwmonPwm>& pwms,
                                            const std::string& identifier);

    // Return best single temp path for a FanControl identifier (no duplicates)
    static std::string mapBestTempFromIdentifier(const std::vector<HwmonTemp>& temps,
                                                 const std::string& identifier);

    static std::string lower(std::string s);

    static bool buildSourceFromCurveJson(const nlohmann::json& curveJson,
                                         const std::vector<HwmonTemp>& temps,
                                         std::vector<std::string>& outTempPaths,
                                         std::vector<std::pair<double,int>>& outPoints,
                                         int& outMinPercent, int& outMaxPercent, double& outHystC,
                                         int& outSpinupPercent, int& outSpinupMs,
                                         bool& outStopBelowMin,
                                         bool   isTrigger,
                                         std::string& err);

    static void parsePointsArray(const nlohmann::json& arr, std::vector<std::pair<double,int>>& dst);
    static void buildTriggerPoints(double idleTemp, int idlePct, double loadTemp, int loadPct,
                                   std::vector<std::pair<double,int>>& dst);
};

} // namespace lfc
