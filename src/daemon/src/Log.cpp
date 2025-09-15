/*
 * Linux Fan Control - Simple logger
 * (c) 2025 LinuxFanControl contributors
 */

#include "Log.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#include <sys/types.h>
#endif

namespace {

    struct Logger {
        std::mutex      mtx;
        std::string     path;
        std::FILE*      fp            = nullptr;
        std::uint64_t   maxBytes      = 5ull * 1024ull * 1024ull;
        int             rotateCount   = 3;
        bool            mirrorStderr  = true;
        std::atomic<int> level        { static_cast<int>(Log::Level::Info) };
        std::uint64_t   bytesWritten  = 0;     // approximate, used for rotation checks

        void openIfNeeded() {
            if (path.empty()) return; // stderr-only
            if (fp) return;
            try {
                auto dir = std::filesystem::path(path).parent_path();
                if (!dir.empty()) {
                    std::error_code ec;
                    std::filesystem::create_directories(dir, ec);
                }
            } catch (...) {}
            fp = std::fopen(path.c_str(), "a");
            if (fp) std::setvbuf(fp, nullptr, _IOLBF, 0); // line buffered
            // Initialize bytesWritten from existing size (best effort)
            if (fp) {
                std::error_code ec;
                auto sz = std::filesystem::file_size(path, ec);
                if (!ec) bytesWritten = static_cast<std::uint64_t>(sz);
            }
        }

        static std::string nowTs() {
            using namespace std::chrono;
            auto now = system_clock::now();
            auto t   = system_clock::to_time_t(now);
            std::tm tm {};
            #if defined(_WIN32)
            localtime_s(&tm, &t);
            #else
            localtime_r(&t, &tm);
            #endif
            char buf[64];
            std::snprintf(buf, sizeof(buf),
                          "%04d-%02d-%02d %02d:%02d:%02d",
                          tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                          tm.tm_hour, tm.tm_min, tm.tm_sec);
            return std::string(buf);
        }

        static const char* lvlStr(Log::Level lvl) {
            switch (lvl) {
                case Log::Level::Debug: return "DEBUG";
                case Log::Level::Info:  return "INFO";
                case Log::Level::Warn:  return "WARN";
                case Log::Level::Error: return "ERROR";
            }
            return "INFO";
        }

        void rotateIfNeededUnlocked(std::size_t nextMsgBytes) {
            if (!fp || path.empty() || maxBytes == 0) return;
            if (bytesWritten + nextMsgBytes < maxBytes) return;

            // Close current
            std::fclose(fp);
            fp = nullptr;

            // Rotate: file, file.1, ..., file.N
            try {
                for (int i = rotateCount - 1; i >= 0; --i) {
                    std::filesystem::path from = (i == 0) ? path
                    : (std::filesystem::path(path).string() + "." + std::to_string(i));
                    std::filesystem::path to   = std::filesystem::path(path).string() + "." + std::to_string(i + 1);
                    std::error_code ec;
                    if (std::filesystem::exists(from, ec)) {
                        std::filesystem::rename(from, to, ec);
                        // If rename fails across filesystems, try copy+remove
                        if (ec) {
                            std::filesystem::copy_file(from, to,
                                                       std::filesystem::copy_options::overwrite_existing, ec);
                            if (!ec) std::filesystem::remove(from, ec);
                        }
                    }
                }
            } catch (...) {
                // Best effort; continue
            }

            // Reopen new file
            openIfNeeded();
            bytesWritten = 0;
        }

        void write(Log::Level lvl, const char* fmt, va_list ap) {
            if (static_cast<int>(lvl) < level.load(std::memory_order_relaxed)) return;

            char msgBuf[2048];
            int n = std::vsnprintf(msgBuf, sizeof(msgBuf), fmt, ap);
            if (n < 0) return;
            std::size_t msgLen = (n < static_cast<int>(sizeof(msgBuf))) ? static_cast<std::size_t>(n)
            : sizeof(msgBuf) - 1;

            // Build final line: ts pid tid level msg\n
            char lineBuf[2300];
            const std::string ts = nowTs();
            #if defined(__unix__) || defined(__APPLE__)
            const int pid = static_cast<int>(::getpid());
            #else
            const int pid = 0;
            #endif
            auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());

            int m = std::snprintf(lineBuf, sizeof(lineBuf),
                                  "%s [%d:%zu] %-5s | %.*s\n",
                                  ts.c_str(), pid, static_cast<size_t>(tid),
                                  lvlStr(lvl), (int)msgLen, msgBuf);
            if (m < 0) return;
            std::size_t lineLen = (m < static_cast<int>(sizeof(lineBuf))) ? static_cast<std::size_t>(m)
            : sizeof(lineBuf) - 1;

            std::lock_guard<std::mutex> lk(mtx);

            // Rotation if needed (approximate)
            rotateIfNeededUnlocked(lineLen);

            // File sink
            if (!path.empty()) openIfNeeded();
            if (fp) {
                std::fwrite(lineBuf, 1, lineLen, fp);
                std::fflush(fp);
                bytesWritten += lineLen;
            }

            // Stderr mirror
            if (mirrorStderr) {
                std::fwrite(lineBuf, 1, lineLen, stderr);
                std::fflush(stderr);
            }
        }

        void flush() {
            std::lock_guard<std::mutex> lk(mtx);
            if (fp) std::fflush(fp);
            std::fflush(stderr);
        }

        void close() {
            std::lock_guard<std::mutex> lk(mtx);
            if (fp) {
                std::fflush(fp);
                std::fclose(fp);
                fp = nullptr;
            }
        }
    };

    Logger& logger() {
        static Logger L;
        return L;
    }

} // namespace

// -----------------------------
// Public API
// -----------------------------
namespace Log {

    void Init(const std::string& filePath,
              std::uint64_t maxBytes,
              int rotateCount,
              bool alsoStderr)
    {
        auto& L = logger();
        std::lock_guard<std::mutex> lk(L.mtx);
        L.path         = filePath;
        L.maxBytes     = (maxBytes == 0) ? (5ull * 1024ull * 1024ull) : maxBytes;
        L.rotateCount  = (rotateCount < 0) ? 0 : rotateCount;
        L.mirrorStderr = alsoStderr;

        if (!L.path.empty()) {
            L.openIfNeeded();
        }
    }

    void SetLevel(Level lvl) {
        logger().level.store(static_cast<int>(lvl), std::memory_order_relaxed);
    }

    void SetDebug(bool enable) {
        SetLevel(enable ? Level::Debug : Level::Info);
    }

    Level GetLevel() {
        return static_cast<Level>(logger().level.load(std::memory_order_relaxed));
    }

    void Write(Level lvl, const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        logger().write(lvl, fmt, ap);
        va_end(ap);
    }

    void Flush() {
        logger().flush();
    }

    void Shutdown() {
        logger().close();
    }

} // namespace Log
