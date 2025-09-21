/*
 * Linux Fan Control — Utility helpers (header)
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <system_error>

namespace lfc { namespace util {

/* ----------------------------------------------------------------------------
 * Environment helpers
 * ----------------------------------------------------------------------------*/

std::optional<std::string> getenv_c(const char* key);
std::optional<std::string> getenv_str(const char* key);

/* ----------------------------------------------------------------------------
 * String helpers
 * ----------------------------------------------------------------------------*/

std::string to_string(const std::string& s);
std::string to_string(std::string_view sv);
std::string to_string(const char* cstr);

std::string trim(std::string_view sv);
std::vector<std::string> split(std::string_view sv, char delim);
std::string join(const std::vector<std::string>& parts, std::string_view sep);

/** Lowercase copy (ASCII) */
std::string to_lower(std::string_view sv);

/* ----------------------------------------------------------------------------
 * Time helpers
 * ----------------------------------------------------------------------------*/

std::string utc_iso8601();
long long now_ms();

/* ----------------------------------------------------------------------------
 * Filesystem helpers
 * ----------------------------------------------------------------------------*/

std::string read_first_line(const std::filesystem::path& p);
std::optional<int> read_first_line_int(const std::filesystem::path& p);
std::optional<long long> read_first_line_ll(const std::filesystem::path& p);
bool read_int_file(const std::filesystem::path& p, int& out);
bool read_ll_file(const std::filesystem::path& p, long long& out);
bool write_int_file(const std::filesystem::path& p, int value);

/** Ensure parent directory of path exists (no-op if already exists). */
void ensure_parent_dirs(const std::filesystem::path& p, std::error_code* ec = nullptr);

/* ----------------------------------------------------------------------------
 * PWM helpers
 * ----------------------------------------------------------------------------*/

/** Convert raw PWM value to percent in range [0,100]. Clamps on bounds. */
int pwmPercentFromRaw(int raw, int maxRaw);

/** Backward-compatible overload assuming 8-bit max (255). */
inline int pwmPercentFromRaw(int raw) { return pwmPercentFromRaw(raw, 255); }

/** Convert percent [0,100] to raw PWM based on max. Clamps on bounds. */
int pwmRawFromPercent(int percent, int maxRaw);

/* Expand a user/home/environment path:
 *  - Leading '~' -> $HOME
 *  - ${VAR} or $VAR -> environment variable
 * Returns the expanded string (no filesystem checks). */
std::string expandUserPath(const std::string& path);

}} // namespace lfc::util
