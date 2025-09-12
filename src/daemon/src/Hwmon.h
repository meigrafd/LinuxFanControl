#pragma once
// hwmon scanner via /sys/class/hwmon + heuristics

#include <vector>
#include <string>

struct TempSensorInfo {
    std::string name;   // "hwmonX:<driver>:<label>"
    std::string path;   // /sys/class/hwmon/hwmonX/tempY_input
    std::string type;   // heuristic "CPU/GPU/Water/Ambient/..." or "Unknown"
};

struct PwmOutputInfo {
    std::string label;       // "hwmonX:<driver>:pwmN"
    std::string pwmPath;     // /sys/class/hwmon/hwmonX/pwmN
    std::string enablePath;  // pwmN_enable (optional)
    std::string tachPath;    // fanN_input   (optional)
};

class Hwmon {
public:
    std::vector<TempSensorInfo> discoverTemps() const;
    std::vector<PwmOutputInfo>  discoverPwms()  const;

private:
    static std::string readFile(const std::string& path);
    static bool isDir(const std::string& p);
    static std::string basename(const std::string& p);

    static std::string classify(const std::string& devName, const std::string& label);
};
