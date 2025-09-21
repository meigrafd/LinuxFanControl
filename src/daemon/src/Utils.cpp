/*
 * Linux Fan Control — Utility helpers (implementation; Linux-only)
 * (c) 2025 LinuxFanControl contributors
 */
#include "include/Utils.hpp"

#include <cerrno>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace lfc { namespace util {

/* ----------------------------------------------------------------------------
 * Environment helpers
 * ----------------------------------------------------------------------------*/

std::optional<std::string> getenv_c(const char* key) {
    if (!key || !*key) return std::nullopt;
    const char* v = std::getenv(key);
    if (!v) return std::nullopt;
    return std::string(v);
}

std::optional<std::string> getenv_str(const char* key) {
    return getenv_c(key);
}

/* ----------------------------------------------------------------------------
 * String helpers
 * ----------------------------------------------------------------------------*/

std::string to_string(const std::string& s) { return s; }
std::string to_string(std::string_view sv)  { return std::string(sv); }
std::string to_string(const char* cstr)     { return cstr ? std::string(cstr) : std::string(); }

std::string trim(std::string_view sv) {
    size_t i = 0, j = sv.size();
    while (i < j && std::isspace(static_cast<unsigned char>(sv[i]))) ++i;
    while (j > i && std::isspace(static_cast<unsigned char>(sv[j-1]))) --j;
    return std::string(sv.substr(i, j - i));
}

std::vector<std::string> split(std::string_view sv, char delim) {
    std::vector<std::string> out;
    size_t start = 0;
    for (size_t i = 0; i <= sv.size(); ++i) {
        if (i == sv.size() || sv[i] == delim) {
            out.emplace_back(sv.substr(start, i - start));
            start = i + 1;
        }
    }
    return out;
}

std::string join(const std::vector<std::string>& parts, std::string_view sep) {
    std::ostringstream oss;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) oss << sep;
        oss << parts[i];
    }
    return oss.str();
}

std::string to_lower(std::string_view sv) {
    std::string s(sv);
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

/* ----------------------------------------------------------------------------
 * Time helpers
 * ----------------------------------------------------------------------------*/

std::string utc_iso8601()
{
    char buf[32] = {0};
    std::time_t now = std::time(nullptr);
    std::tm tm_utc{};
    gmtime_r(&now, &tm_utc);
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_utc) == 0) {
        return std::string();
    }
    return std::string(buf);
}

long long now_ms()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

/* ----------------------------------------------------------------------------
 * Filesystem helpers
 * ----------------------------------------------------------------------------*/

std::string read_first_line(const fs::path& p) {
    std::ifstream f(p);
    if (!f) return {};
    std::string s;
    std::getline(f, s);
    return s;
}

std::optional<int> read_first_line_int(const fs::path& p) {
    std::ifstream f(p);
    if (!f) return std::nullopt;
    long long v = 0;
    f >> v;
    if (!f.good() && !f.eof()) return std::nullopt;
    return static_cast<int>(v);
}

std::optional<long long> read_first_line_ll(const fs::path& p) {
    std::ifstream f(p);
    if (!f) return std::nullopt;
    long long v = 0;
    f >> v;
    if (!f.good() && !f.eof()) return std::nullopt;
    return v;
}

bool read_int_file(const fs::path& p, int& out) {
    auto v = read_first_line_ll(p);
    if (!v) return false;
    out = static_cast<int>(*v);
    return true;
}

bool read_ll_file(const fs::path& p, long long& out) {
    auto v = read_first_line_ll(p);
    if (!v) return false;
    out = *v;
    return true;
}

bool write_int_file(const fs::path& p, int value) {
    std::ofstream f(p);
    if (!f) return false;
    f << value;
    return f.good();
}

void ensure_parent_dirs(const fs::path& p, std::error_code* ec) {
    fs::path dir = p.parent_path();
    if (dir.empty()) return;
    std::error_code tmp;
    fs::create_directories(dir, tmp);
    if (ec) *ec = tmp;
}

/* ----------------------------------------------------------------------------
 * PWM helpers
 * ----------------------------------------------------------------------------*/

int pwmPercentFromRaw(int raw, int maxRaw) {
    if (maxRaw <= 0) maxRaw = 255;
    if (raw < 0) raw = 0;
    if (raw > maxRaw) raw = maxRaw;
    return (raw * 100 + (maxRaw / 2)) / maxRaw;
}

int pwmRawFromPercent(int percent, int maxRaw) {
    if (maxRaw <= 0) maxRaw = 255;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return (percent * maxRaw + 50) / 100;
}

/* ----------------------------------------------------------------------------
 * Path expansion
 * ----------------------------------------------------------------------------*/

static inline bool isIdentChar_(char c) {
    return (c == '_') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9');
}

std::string expandUserPath(const std::string& in) {
    if (in.empty()) return in;

    std::string out = in;

    // "~" -> $HOME
    if (!out.empty() && out.front() == '~') {
        auto homeOpt = getenv_str("HOME");
        if (homeOpt && !homeOpt->empty()) {
            if (out.size() == 1) return *homeOpt;
            if (out.size() >= 2 && out[1] == '/') {
                out = *homeOpt + out.substr(1);
            }
        }
    }

    // Expand $VAR and ${VAR}
    std::string result;
    result.reserve(out.size());
    for (size_t i = 0; i < out.size(); ++i) {
        char c = out[i];
        if (c == '$') {
            if (i + 1 < out.size() && out[i + 1] == '{') {
                size_t j = i + 2;
                while (j < out.size() && out[j] != '}') ++j;
                if (j < out.size()) {
                    std::string key = out.substr(i + 2, j - (i + 2));
                    auto v = getenv_str(key.c_str());
                    if (v) result += *v;
                    i = j; // skip '}'
                    continue;
                }
            } else {
                size_t j = i + 1;
                while (j < out.size() && isIdentChar_(out[j])) ++j;
                if (j > i + 1) {
                    std::string key = out.substr(i + 1, j - (i + 1));
                    auto v = getenv_str(key.c_str());
                    if (v) result += *v;
                    i = j - 1;
                    continue;
                }
            }
        }
        result.push_back(c);
    }
    return result;
}

}} // namespace lfc::util
