/*
 * Linux Fan Control — Logging (header)
 * - Thread-safe logger with optional file output and size-based rotation
 * - Minimal dependencies; printf-style API for fast call-sites
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

namespace lfc {

/* Severity levels (ascending verbosity). */
enum class LogLevel {
    Error = 0,  // serious failure, user-visible
    Warn  = 1,  // recoverable anomaly
    Info  = 2,  // default operational messages
    Debug = 3,  // developer diagnostics
    Trace = 4   // very verbose, tight loops
};

/*
 * Logger — singleton, safe for concurrent writers.
 * - Files are opened lazily; directory is created if needed.
 * - Rotation is size-based (maxBytes/maxFiles). Rotation reopens a fresh file.
 * - Messages are mirrored to stdout/stderr when mirror mode is enabled.
 * - Functions are noexcept where practical; errors degrade gracefully.
 */
class Logger {
public:
    /* Acquire singleton instance. */
    static Logger& instance();

    /* Dtor flushes/close if not already shut down. */
    ~Logger();

    /*
     * Legacy-style initialization used by daemon main (kept for compatibility).
     *  - logFilePath: destination file (empty = no file).
     *  - lvl: minimum severity to emit.
     *  - mirrorToStdout: also print to stdio (stdout/stderr).
     *  - reserved: reserved for future extensions (pass nullptr).
     */
    void init(const std::string& logFilePath,
              LogLevel lvl,
              bool mirrorToStdout,
              void* reserved);

    /* Enable/disable mirroring to stdio at runtime. */
    void setMirrorToStdio(bool on);

    /* Set/replace log file path (creates parent dir when needed). */
    void setFile(const std::string& path);

    /* Change log level at runtime. */
    void setLevel(LogLevel lvl);

    /* Query current log level. */
    LogLevel level() const;

    /* Shutdown logger and close file (idempotent). */
    void shutdown();

    /*
     * Configure rotation:
     *  - maxBytes: rotate when file would exceed this size (>0 to enable).
     *  - maxFiles: how many rotated files to keep (>=1).
     * Default is 5 MiB / 5 files.
     */
    void enableRotation(size_t maxBytes, int maxFiles);

    /* Force an immediate rotation if rotation is enabled. */
    void forceRotate();

    /* Core write (printf-style). Thread-safe. */
    void write(LogLevel lvl, const char* fmt, ...) __attribute__((format(printf, 3, 4)));

    /* Va_list variant. */
    void vwrite(LogLevel lvl, const char* fmt, va_list ap);

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    /* Ensure file_ is opened and currentSize_ initialized. */
    void openFileIfNeeded();

    /* Prepend timestamp and level tag into buffer. */
    void writePrefix(LogLevel lvl, char* buf, size_t bufSize);

    /* Level tag string for prefix. */
    const char* levelTag(LogLevel lvl) const;

    /* Check rotation before writing `incomingBytes`; performs rotation if due. */
    void checkRotateBeforeWrite(size_t incomingBytes);

    /* Rotate files: shifts N-1..1 -> N..2, moves active -> .1, reopens. */
    void rotateFilesUnlocked();

private:
    std::mutex mtx_;

    /* Minimum level to emit (atomic for cheap reads). */
    std::atomic<int> level_{static_cast<int>(LogLevel::Info)};

    /* Target file path; empty means disabled. */
    std::string filePath_;

    /* Current FILE* (opened lazily). */
    FILE* file_{nullptr};

    /* Also mirror to stdio (stdout/stderr). */
    bool mirror_{false};

    /* Rotation (0 disables). */
    size_t maxBytes_{5 * 1024 * 1024};  // default 5 MiB
    int    maxFiles_{5};                // default keep 5 files

    /* Tracked size of the active file (best-effort). */
    size_t currentSize_{0};
};

/* Convenience macros with clear tags and newline. */
#define LOG_ERROR(fmt, ...) ::lfc::Logger::instance().write(::lfc::LogLevel::Error, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  ::lfc::Logger::instance().write(::lfc::LogLevel::Warn,  "[WARN] "  fmt "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  ::lfc::Logger::instance().write(::lfc::LogLevel::Info,  "[INFO] "  fmt "\n", ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) ::lfc::Logger::instance().write(::lfc::LogLevel::Debug, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define LOG_TRACE(fmt, ...) ::lfc::Logger::instance().write(::lfc::LogLevel::Trace, "[TRACE] " fmt "\n", ##__VA_ARGS__)

} // namespace lfc
