/*
 * Linux Fan Control — Update Checker (implementation)
 * - Uses libcurl for HTTP GET and file download
 * (c) 2025 LinuxFanControl contributors
 */
#include "include/UpdateChecker.hpp"
#include "include/Log.hpp"

#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <fstream>
#include <string>

using nlohmann::json;

namespace lfc {

static size_t write_to_string(void* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* s = static_cast<std::string*>(userdata);
    s->append(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

static size_t write_to_file(void* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* f = static_cast<std::ofstream*>(userdata);
    f->write(static_cast<const char*>(ptr), (std::streamsize)(size * nmemb));
    return size * nmemb;
}

bool UpdateChecker::fetchLatest(const std::string& owner,
                                const std::string& repo,
                                ReleaseInfo& out,
                                std::string& err)
{
    err.clear();
    out = ReleaseInfo{};

    const std::string url = "https://api.github.com/repos/" + owner + "/" + repo + "/releases/latest";
    LOG_INFO("update: checking %s", url.c_str());

    CURL* curl = curl_easy_init();
    if (!curl) { err = "curl_easy_init failed"; LOG_ERROR("update: %s", err.c_str()); return false; }

    std::string buf;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "lfcd/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);

    long code = 0;
    CURLcode rc = curl_easy_perform(curl);
    if (rc == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK) {
        err = curl_easy_strerror(rc);
        LOG_ERROR("update: HTTP error: %s", err.c_str());
        return false;
    }
    if (code != 200) {
        err = "HTTP status " + std::to_string(code);
        LOG_ERROR("update: %s", err.c_str());
        return false;
    }

    try {
        json j = json::parse(buf);
        out.tag     = j.value("tag_name", "");
        out.name    = j.value("name", "");
        out.htmlUrl = j.value("html_url", "");

        if (j.contains("assets") && j["assets"].is_array()) {
            for (const auto& a : j["assets"]) {
                ReleaseAsset ra;
                ra.name = a.value("name", "");
                ra.url  = a.value("browser_download_url", "");
                out.assets.push_back(std::move(ra));
            }
        }
        LOG_INFO("update: latest tag '%s' name '%s' (assets=%zu)",
                 out.tag.c_str(), out.name.c_str(), out.assets.size());
        return true;
    } catch (const std::exception& ex) {
        err = ex.what();
        LOG_ERROR("update: parse failed: %s", ex.what());
        return false;
    }
}

bool UpdateChecker::downloadToFile(const std::string& url,
                                   const std::string& path,
                                   std::string& err)
{
    err.clear();
    LOG_INFO("update: downloading %s -> %s", url.c_str(), path.c_str());

    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs) { err = "open failed: " + path; LOG_ERROR("update: %s", err.c_str()); return false; }

    CURL* curl = curl_easy_init();
    if (!curl) { err = "curl_easy_init failed"; LOG_ERROR("update: %s", err.c_str()); return false; }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "lfcd/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_file);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ofs);

    long code = 0;
    CURLcode rc = curl_easy_perform(curl);
    if (rc == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);
    ofs.flush();
    ofs.close();

    if (rc != CURLE_OK) {
        err = curl_easy_strerror(rc);
        LOG_ERROR("update: HTTP error: %s", err.c_str());
        return false;
    }
    if (code != 200) {
        err = "HTTP status " + std::to_string(code);
        LOG_ERROR("update: %s", err.c_str());
        return false;
    }

    LOG_INFO("update: saved to %s", path.c_str());
    return true;
}

} // namespace lfc
