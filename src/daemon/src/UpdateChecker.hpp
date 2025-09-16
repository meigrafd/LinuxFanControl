/*
 * Linux Fan Control — Update Checker (header)
 * - GitHub releases check and asset download
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <string>
#include <vector>

namespace lfc {

struct ReleaseAsset {
    std::string name;
    std::string url;
    std::string type;
    std::size_t size{0};
};

struct ReleaseInfo {
    std::string tag;
    std::string name;
    std::string htmlUrl;
    std::vector<ReleaseAsset> assets;
};

class UpdateChecker {
public:
    static bool fetchLatest(const std::string& owner,
                            const std::string& repo,
                            ReleaseInfo& out,
                            std::string& err);

    static int compareVersions(const std::string& a, const std::string& b);

    static bool downloadToFile(const std::string& url,
                               const std::string& target,
                               std::string& err);
};

} // namespace lfc
