/*
 * Linux Fan Control — Profile model & loader
 * - FanControl.Releases compatible subset
 * - JSON loader using nlohmann::json
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <string>
#include <vector>

namespace lfc {

struct Profile {
    struct StringRef { std::string Name; };
    struct TempSource { std::string Identifier; };

    struct Curve {
        std::string Name;
        int CommandMode{0};                         // 0=Curve, 1=Mix, 2=Trigger

        // Curve mode (0)
        TempSource SelectedTempSource;
        int MaximumTemperature{100};                // °C
        int MinimumTemperature{0};                  // °C
        int MaximumCommand{100};                    // %
        std::vector<std::string> Points;            // "X,Y" with X=°C, Y=%

        // Mix mode (1)
        int SelectedMixFunction{0};                 // 0=max, 1=min, 2=avg
        std::vector<StringRef> SelectedFanCurves;   // referenced curve names

        // Trigger mode (2)
        TempSource TriggerTempSource;
        int LoadFanSpeed{100};                      // %
        int LoadTemperature{80};                    // °C
        int IdleFanSpeed{20};                       // %
        int IdleTemperature{40};                    // °C
    };

    struct Control {
        bool Enable{true};
        int MinimumPercent{0};
        std::string Identifier;                     // PWM identifier (substring match)
        StringRef SelectedFanCurve;                 // curve/mix/trigger to use
    };

    std::vector<Control> Controls;
    std::vector<Curve>   FanCurves;

    bool loadFromFile(const std::string& path, std::string* err = nullptr);
    bool loadFromJson(const std::string& jsonText, std::string* err = nullptr);
};

} // namespace lfc
