/*
 * Linux Fan Control — Update Checker (implementation)
 * - Uses libcurl for HTTP GET and file download
 * (c) 2025 LinuxFanControl contributors
 */
#include "UpdateChecker.hpp"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <fstream>

using nlohmann::json;

namespace lfc {

static size_t write_to_string(void* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* s = static_cast<std::string*>(userdata);
    s->append(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

static size_t write_to_file(void* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* f = static_cast<std::ofstream*>(userdata);
    f->write(static_cast<char*>(ptr), static_cast<std::streamsize>(size * nmemb));
    return size * nmemb;
}

bool UpdateChecker::fetchLatest(const std::string& owner,
                                const std::string& repo,
                                ReleaseInfo& out,
                                std::string& err) {
    err.clear();
    std::string url = "https://api.github.com/repos/" + owner + "/" + repo + "/releases/latest";
    std::string buf;

    CURL* curl = curl_easy_init();
    if (!curl) {
        err = "curl init failed";
        return false;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "LinuxFanControl/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    CURLcode rc = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK || code < 200 || code >= 300) {
        err = "http error";
        return false;
    }

    try {
        json j = json::parse(buf);
        out.tag = j.value("tag_name", "");
        out.name = j.value("name", "");
        out.htmlUrl = j.value("html_url", "");
        out.assets.clear();
        if (j.contains("assets") && j["assets"].is_array()) {
            for (auto& a : j["assets"]) {
                ReleaseAsset as;
                as.name = a.value("name", "");
                as.url  = a.value("browser_download_url", "");
                as.type = a.value("content_type", "");
                as.size = static_cast<std::size_t>(a.value("size", 0));
                out.assets.push_back(as);
            }
        }
        return true;
    } catch (...) {
        err = "json parse failed";
        return false;
    }
}

static int cmp_num(int x, int y) {
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

int UpdateChecker::compareVersions(const std::string& a, const std::string& b) {
    // Strip leading 'v'
    auto sa = (a.size() && (a[0] == 'v' || a[0] == 'V')) ? a.substr(1) : a;
    auto sb = (b.size() && (b[0] == 'v' || b[0] == 'V')) ? b.substr(1) : b;

    int ai = 0, bi = 0;
    while (ai < (int)sa.size() || bi < (int)sb.size()) {
        int av = 0, bv = 0;
        while (ai < (int)sa.size() && isdigit(sa[ai])) { av = av * 10 + (sa[ai++] - '0'); }
        while (bi < (int)sb.size() && isdigit(sb[bi])) { bv = bv * 10 + (sb[bi++] - '0'); }
        int c = cmp_num(av, bv);
        if (c != 0) return c;
        while (ai < (int)sa.size() && (sa[ai] == '.' || sa[ai] == '-')) ++ai;
        while (bi < (int)sb.size() && (sb[bi] == '.' || sb[bi] == '-')) ++bi;
    }
    return 0;
}

bool UpdateChecker::downloadToFile(const std::string& url,
                                   const std::string& target,
                                   std::string& err) {
    err.clear();
    std::ofstream f(target, std::ios::binary | std::ios::trunc);
    if (!f) {
        err = "open target failed";
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        err = "curl init failed";
        return false;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "LinuxFanControl/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_file);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &f);
    CURLcode rc = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);
    f.close();

    if (rc != CURLE_OK || code < 200 || code >= 300) {
        err = "http error";
        return false;
    }
    return true;
}

} // namespace lfc
