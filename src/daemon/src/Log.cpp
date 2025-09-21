// PATH: src/daemon/src/Log.cpp
/*
 * Linux Fan Control — Logging (implementation)
 * - Thread-safe logger with optional file output and size-based rotation
 * (c) 2025 LinuxFanControl contributors
 */

#include "include/Log.hpp"
#include "include/Utils.hpp"

#include <array>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>

namespace lfc {

// -----------------------------------------------------------------------------
// helpers
// -----------------------------------------------------------------------------

static inline size_t fileSizeOrZero(const std::string& path) {
    std::error_code ec;
    auto sz = std::filesystem::file_size(path, ec);
    return ec ? 0u : static_cast<size_t>(sz);
}

static inline std::string makeTimestamp() {
    // Format: "YYYY-MM-DD HH:MM:SS"
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    std::array<char, 32> buf{};
    // std::strftime guarantees null-termination if it returns > 0
    if (std::strftime(buf.data(), buf.size(), "%Y-%m-%d %H:%M:%S", &tmv) == 0) {
        return "1970-01-01 00:00:00";
    }
    return std::string(buf.data());
}

// -----------------------------------------------------------------------------
// Logger
// -----------------------------------------------------------------------------

Logger& Logger::instance() {
    static Logger g;
    return g;
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (file_) {
        std::fflush(file_);
        std::fclose(file_);
        file_ = nullptr;
    }
}

void Logger::init(const std::string& logFilePath,
                  LogLevel lvl,
                  bool mirrorToStdout,
                  void* /*reserved*/) {
    std::lock_guard<std::mutex> lock(mtx_);

    level_.store(static_cast<int>(lvl), std::memory_order_relaxed);
    mirror_ = mirrorToStdout;

    filePath_.clear();
    if (file_) {
        std::fflush(file_);
        std::fclose(file_);
        file_ = nullptr;
    }

    if (!logFilePath.empty()) {
        filePath_ = logFilePath;
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(filePath_).parent_path(), ec);
        openFileIfNeeded();
        currentSize_ = fileSizeOrZero(filePath_);
    }
}

void Logger::setMirrorToStdio(bool on) {
    mirror_ = on;
}

void Logger::setFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (file_) {
        std::fflush(file_);
        std::fclose(file_);
        file_ = nullptr;
    }
    filePath_ = path;
    currentSize_ = 0;

    if (!filePath_.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(filePath_).parent_path(), ec);
        openFileIfNeeded();
        currentSize_ = fileSizeOrZero(filePath_);
        if (maxBytes_ > 0 && maxFiles_ > 0 && currentSize_ >= maxBytes_) {
            rotateFilesUnlocked();
            openFileIfNeeded();
            currentSize_ = fileSizeOrZero(filePath_);
        }
    }
}

void Logger::setLevel(LogLevel lvl) {
    level_.store(static_cast<int>(lvl), std::memory_order_relaxed);
}

LogLevel Logger::level() const {
    return static_cast<LogLevel>(level_.load(std::memory_order_relaxed));
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (file_) {
        std::fflush(file_);
        std::fclose(file_);
        file_ = nullptr;
    }
}

void Logger::enableRotation(size_t maxBytes, int maxFiles) {
    std::lock_guard<std::mutex> lock(mtx_);
    maxBytes_ = maxBytes;
    maxFiles_ = maxFiles;
    if (!filePath_.empty()) {
        currentSize_ = fileSizeOrZero(filePath_);
        if (maxBytes_ > 0 && maxFiles_ > 0 && currentSize_ >= maxBytes_) {
            rotateFilesUnlocked();
            openFileIfNeeded();
            currentSize_ = fileSizeOrZero(filePath_);
        }
    }
}

void Logger::forceRotate() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (maxBytes_ == 0 || maxFiles_ <= 0 || filePath_.empty()) return;
    rotateFilesUnlocked();
    openFileIfNeeded();
    currentSize_ = 0;
}

void Logger::openFileIfNeeded() {
    if (file_ || filePath_.empty()) return;
    file_ = ::fopen(filePath_.c_str(), "a");
    if (!file_) {
        mirror_ = true; // degrade gracefully
    } else {
        std::fseek(file_, 0, SEEK_END);
        long pos = std::ftell(file_);
        currentSize_ = pos > 0 ? static_cast<size_t>(pos) : 0;
    }
}

const char* Logger::levelTag(LogLevel lvl) const {
    switch (lvl) {
        case LogLevel::Error: return "E";
        case LogLevel::Warn:  return "W";
        case LogLevel::Info:  return "I";
        case LogLevel::Debug: return "D";
        case LogLevel::Trace: return "T";
    }
    return "?";
}

void Logger::writePrefix(LogLevel lvl, char* buf, size_t bufSize) {
    // Prefix example: "2025-09-20 14:22:11 [I] "
    const std::string ts = makeTimestamp();
    std::snprintf(buf, bufSize, "%s [%s] ", ts.c_str(), levelTag(lvl));
}

void Logger::checkRotateBeforeWrite(size_t incomingBytes) {
    if (maxBytes_ == 0 || maxFiles_ <= 0 || filePath_.empty()) return;
    if (currentSize_ + incomingBytes > maxBytes_) {
        rotateFilesUnlocked();
        openFileIfNeeded();
        currentSize_ = 0;
    }
}

void Logger::rotateFilesUnlocked() {
    if (file_) {
        std::fflush(file_);
        std::fclose(file_);
        file_ = nullptr;
    }

    // Shift N-1..1 -> N..2
    for (int i = maxFiles_ - 1; i >= 1; --i) {
        std::filesystem::path src = filePath_ + "." + std::to_string(i);
        std::filesystem::path dst = filePath_ + "." + std::to_string(i + 1);
        std::error_code ec;
        if (std::filesystem::exists(src, ec)) {
            std::filesystem::remove(dst, ec);
            std::filesystem::rename(src, dst, ec);
        }
    }

    // Move active -> .1
    {
        std::error_code ec;
        std::filesystem::path src = filePath_;
        std::filesystem::path dst = filePath_ + std::string(".1");
        if (std::filesystem::exists(src, ec)) {
            std::filesystem::remove(dst, ec);
            std::filesystem::rename(src, dst, ec);
        }
    }
}

void Logger::write(LogLevel lvl, const char* fmt, ...) {
    if (static_cast<int>(lvl) > level_.load(std::memory_order_relaxed)) return;

    va_list ap;
    va_start(ap, fmt);
    vwrite(lvl, fmt, ap);
    va_end(ap);
}

void Logger::vwrite(LogLevel lvl, const char* fmt, va_list ap) {
    if (static_cast<int>(lvl) > level_.load(std::memory_order_relaxed)) return;

    char prefix[64];
    writePrefix(lvl, prefix, sizeof(prefix));

    char msgBuf[2048];
    vsnprintf(msgBuf, sizeof(msgBuf), fmt, ap);

    std::string line = std::string(prefix) + msgBuf;
    if (!line.empty() && line.back() != '\n') line.push_back('\n');

    std::lock_guard<std::mutex> lock(mtx_);

    if (!file_ && !filePath_.empty()) {
        openFileIfNeeded();
    }

    checkRotateBeforeWrite(line.size());

    if (file_) {
        std::fwrite(line.data(), 1, line.size(), file_);
        std::fflush(file_);
        currentSize_ += line.size();
    }

    if (mirror_) {
        FILE* out = (lvl == LogLevel::Error || lvl == LogLevel::Warn) ? stderr : stdout;
        std::fwrite(line.data(), 1, line.size(), out);
        std::fflush(out);
    }
}

} // namespace lfc
