/*
 * Linux Fan Control — Update Checker (header)
 * - GitHub Releases check + optional download
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <string>
#include <vector>

namespace lfc {

struct ReleaseAsset {
    std::string name;
    std::string url;     // browser_download_url
    std::string type;    // content_type if known
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
    // owner: e.g. "LinuxFanControl", repo: e.g. "LinuxFanControl"
    static bool fetchLatest(const std::string& owner,
                            const std::string& repo,
                            ReleaseInfo& out,
                            std::string& err);

    // Download URL to targetPath (overwrite). Returns true on success.
    static bool downloadToFile(const std::string& url,
                               const std::string& targetPath,
                               std::string& err);

    // Simple semver-ish compare: returns -1 if a<b, 0 if equal, +1 if a>b
    static int compareVersions(const std::string& a, const std::string& b);
};

} // namespace lfc
