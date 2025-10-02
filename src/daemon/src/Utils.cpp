/*
 * Linux Fan Control — Utility helpers (implementation; Linux-only)
 * (c) 2025 LinuxFanControl contributors
 */

#include <nlohmann/json.hpp>
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

using json = nlohmann::json;

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

bool icontains(const std::string& hay, const std::string& needle) {
    const auto H = to_lower(hay);
    const auto N = to_lower(needle);
    return H.find(N) != std::string::npos;
}

std::string baseName(const std::string& p) {
    const auto pos = p.find_last_of('/');
    return (pos == std::string::npos) ? p : p.substr(pos + 1);
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

bool readFile(const std::filesystem::path& path, std::string& out) {
    std::error_code ec;
    std::filesystem::path p = path;
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs) return false;
    ifs.seekg(0, std::ios::end);
    std::streamoff sz = ifs.tellg();
    if (sz < 0) sz = 0;
    out.resize(static_cast<size_t>(sz));
    ifs.seekg(0, std::ios::beg);
    if (!out.empty()) ifs.read(&out[0], static_cast<std::streamsize>(out.size()));
    return static_cast<bool>(ifs);
}

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

/*
std::optional<long long> read_json_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;
    return json::parse(f, nullptr, true, true); // allow comments
}
*/

/* ----------------------------------------------------------------------------
 * JSON helpers
 * ----------------------------------------------------------------------------*/

static inline void stripBomAndNulls_(std::string& s) {
    // Strip UTF-8 BOM
    if (s.size() >= 3 &&
        static_cast<unsigned char>(s[0]) == 0xEF &&
        static_cast<unsigned char>(s[1]) == 0xBB &&
        static_cast<unsigned char>(s[2]) == 0xBF) {
        s.erase(0, 3);
    }
    // Drop NUL bytes
    std::string tmp;
    tmp.reserve(s.size());
    for (char ch : s) if (ch != '\0') tmp.push_back(ch);
    s.swap(tmp);
}

static inline std::string stripCommentsAndTrailingCommas_(const std::string& in) {
    // This is a heuristic best-effort sanitizer for common FanControl exports.
    // It removes // line comments, /* block comments */, and trailing commas
    // before '}' or ']'. It is NOT a full JSON5 parser and intentionally simple.
    std::string s = in;

    // Remove /* ... */ (non-greedy)
    s = std::regex_replace(s, std::regex(R"(/\*.*?\*/)", std::regex::extended | std::regex::icase), "");

    // Remove // ... (till end of line)
    s = std::regex_replace(s, std::regex(R"(//[^\n\r]*)"), "");

    // Remove trailing commas: ,   }
    s = std::regex_replace(s, std::regex(R"(,\s*([}\]]))"), "$1");

    return s;
}

json read_json_file(const std::string& path) {
    std::string text;
    if (!readFile(path, text)) {
        std::fprintf(stderr, "[WARN] ReadJsonFile: cannot open '%s' (errno=%d)\n",
                     path.c_str(), errno);
        return json(); // discarded
    }

    stripBomAndNulls_(text);

    // First attempt: strict parse (no exceptions)
    json j = json::parse(text, /*cb=*/nullptr, /*allow_exceptions=*/false);
    if (!j.is_discarded()) return j;

    // Second attempt: heuristic sanitizer for comments/trailing commas
    const std::string sanitized = stripCommentsAndTrailingCommas_(text);
    if (sanitized != text) {
        j = json::parse(sanitized, nullptr, /*allow_exceptions=*/false);
        if (!j.is_discarded()) return j;
    }

    std::fprintf(stderr, "[WARN] ReadJsonFile: parse failed for '%s'\n", path.c_str());
    return json(); // discarded
}

static inline bool parseIntFromString_(std::string_view sv, long long& out) {
    std::string s = trim(sv);
    if (s.empty()) return false;
    if (!s.empty() && s.back() == '%') s.pop_back();
    // Allow underscores as visual separators
    s.erase(std::remove(s.begin(), s.end(), '_'), s.end());
    // Reject non [+-0-9]
    for (char c : s) {
        if (!(c=='+' || c=='-' || (c>='0'&&c<='9'))) return false;
    }
    try {
        size_t idx = 0;
        long long v = std::stoll(s, &idx, 10);
        if (idx != s.size()) return false;
        out = v;
        return true;
    } catch (...) { return false; }
}

static inline bool parseDoubleFromString_(std::string_view sv, double& out) {
    std::string s = trim(sv);
    if (s.empty()) return false;
    if (!s.empty() && s.back() == '%') s.pop_back();
    // Replace comma decimal with dot
    for (char& c : s) if (c == ',') c = '.';
    // Allow underscores as visual separators
    s.erase(std::remove(s.begin(), s.end(), '_'), s.end());
    try {
        size_t idx = 0;
        double v = std::stod(s, &idx);
        if (idx != s.size()) return false;
        out = v;
        return true;
    } catch (...) { return false; }
}

long long parseIntLoose(const json& v, long long defValue) {
    if (v.is_number_integer()) {
        return v.get<long long>();
    }
    if (v.is_number_unsigned()) {
        return static_cast<long long>(v.get<unsigned long long>());
    }
    if (v.is_number_float()) {
        return static_cast<long long>(v.get<double>());
    }
    if (v.is_string()) {
        long long out;
        if (parseIntFromString_(v.get_ref<const std::string&>(), out)) return out;
    }
    return defValue;
}

double parseDoubleLoose(const json& v, double defValue) {
    if (v.is_number_float()) {
        return v.get<double>();
    }
    if (v.is_number_integer()) {
        return static_cast<double>(v.get<long long>());
    }
    if (v.is_string()) {
        double out;
        if (parseDoubleFromString_(v.get_ref<const std::string&>(), out)) return out;
    }
    return defValue;
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
