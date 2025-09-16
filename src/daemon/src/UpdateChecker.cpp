/*
 * Linux Fan Control — Update Checker (implementation)
 * - GitHub Releases check + optional download
 * (c) 2025 LinuxFanControl contributors
 */
#include "UpdateChecker.hpp"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

using nlohmann::json;

namespace lfc {

static size_t writeString(void* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* s = static_cast<std::string*>(userdata);
    s->append(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

bool UpdateChecker::fetchLatest(const std::string& owner,
                                const std::string& repo,
                                ReleaseInfo& out,
                                std::string& err) {
    std::ostringstream url;
    url << "https://api.github.com/repos/" << owner << "/" << repo << "/releases/latest";

    CURL* curl = curl_easy_init();
    if (!curl) { err = "curl init failed"; return false; }

    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "LinuxFanControl-Updater/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

    CURLcode rc = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK || code != 200) {
        err = "http error: " + std::to_string(code) + " (" + curl_easy_strerror(rc) + ")";
        return false;
    }

    json j;
    try { j = json::parse(body); }
    catch (const std::exception& e) { err = e.what(); return false; }

    out = ReleaseInfo{};
    out.tag = j.value("tag_name", "");
    out.name = j.value("name", "");
    out.htmlUrl = j.value("html_url", "");

    if (j.contains("assets") && j["assets"].is_array()) {
        for (auto& a : j["assets"]) {
            ReleaseAsset ra;
            ra.name = a.value("name", "");
            ra.url = a.value("browser_download_url", "");
            ra.type = a.value("content_type", "");
            ra.size = a.value("size", 0);
            out.assets.push_back(std::move(ra));
        }
    }

    return !out.tag.empty();
}

bool UpdateChecker::downloadToFile(const std::string& url,
                                   const std::string& targetPath,
                                   std::string& err) {
    CURL* curl = curl_easy_init();
    if (!curl) { err = "curl init failed"; return false; }

    std::ofstream of(targetPath, std::ios::binary | std::ios::trunc);
    if (!of) { err = "open target failed"; curl_easy_cleanup(curl); return false; }

    auto writeFile = [](void* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
        std::ofstream* ofp = static_cast<std::ofstream*>(userdata);
        ofp->write(static_cast<const char*>(ptr), size * nmemb);
        return size * nmemb;
    };

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "LinuxFanControl-Updater/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &of);

    CURLcode rc = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);
    of.close();

    if (rc != CURLE_OK || code != 200) {
        err = "http error: " + std::to_string(code) + " (" + curl_easy_strerror(rc) + ")";
        return false;
    }
    return true;
}

static void split(const std::string& s, std::vector<int>& out) {
    out.clear();
    int acc = 0;
    bool have = false;
    for (char c : s) {
        if (c >= '0' && c <= '9') { acc = acc * 10 + (c - '0'); have = true; }
        else {
            if (have) { out.push_back(acc); acc = 0; have = false; }
        }
    }
    if (have) out.push_back(acc);
}

int UpdateChecker::compareVersions(const std::string& a, const std::string& b) {
    std::vector<int> va, vb;
    split(a, va);
    split(b, vb);
    size_t n = std::max(va.size(), vb.size());
    for (size_t i = 0; i < n; ++i) {
        int ai = i < va.size() ? va[i] : 0;
        int bi = i < vb.size() ? vb[i] : 0;
        if (ai < bi) return -1;
        if (ai > bi) return 1;
    }
    return 0;
}

} // namespace lfc
