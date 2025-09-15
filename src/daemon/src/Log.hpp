#pragma once
/*
 * Linux Fan Control - Simple logger
 * (c) 2025 LinuxFanControl contributors
 *
 * Thread-safe logger with file rotation. Designed to work on Linux.
 * No external dependencies.
 */

#include <cstdint>
#include <string>

namespace Log {

    enum class Level : int { Debug = 0, Info = 1, Warn = 2, Error = 3 };

    /** Initialize logging. If file is empty, logs go only to stderr. */
    void Init(const std::string& filePath,
              std::uint64_t maxBytes = 5ull * 1024ull * 1024ull,
              int rotateCount = 3,
              bool alsoStderr = true);

    /** Change minimum level (messages below are dropped). */
    void SetLevel(Level lvl);

    /** Convenience: enable/disable debug (sets level to Debug / Info). */
    void SetDebug(bool enable);

    /** Current minimum level. */
    Level GetLevel();

    /** Log formatted message (printf-style). */
    void Write(Level lvl, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

    /** Flush file (and stderr if mirroring). */
    void Flush();

    /** Close file handles. Safe to call multiple times. */
    void Shutdown();

    /* ---- Convenience macros ---- */
} // namespace Log

#define LOG_DEBUG(fmt, ...) ::Log::Write(::Log::Level::Debug, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  ::Log::Write(::Log::Level::Info,  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  ::Log::Write(::Log::Level::Warn,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) ::Log::Write(::Log::Level::Error, fmt, ##__VA_ARGS__)
