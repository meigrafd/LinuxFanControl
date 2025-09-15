#pragma once
// LinuxFanControl - Logger
// - Thread-safe file logger
// - Size-based rotation with max files
// - Simple printf-like formatting via std::ostringstream

#include <string>
#include <mutex>
#include <cstdio>
#include <cstdarg>

namespace lfc {

    enum class LogLevel { Debug=0, Info=1, Warn=2, Error=3 };

    class Logger {
    public:
        static Logger& instance();

        // Open/rotate policy
        void configure(const std::string& filePath,
                       std::size_t maxBytes,
                       int maxFiles,
                       bool debugEnabled);

        void setLevel(LogLevel lvl);
        LogLevel level() const;

        void write(LogLevel lvl, const std::string& msg);
        void writef(LogLevel lvl, const char* fmt, ...);

        // convenience
        inline void debug(const std::string& m) { if (debugEnabled_) write(LogLevel::Debug, m); }
        inline void info (const std::string& m) { write(LogLevel::Info, m); }
        inline void warn (const std::string& m) { write(LogLevel::Warn, m); }
        inline void error(const std::string& m) { write(LogLevel::Error, m); }

        // Expose for daemon unit tests / tools
        const std::string& path() const { return path_; }

    private:
        Logger();
        ~Logger();
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        void ensureOpen();
        void rotateIfNeeded(std::size_t addBytes);
        static std::string nowStamp();

    private:
        std::mutex mtx_;
        std::string path_;
        std::FILE* fp_{nullptr};
        std::size_t maxBytes_{5*1024*1024};
        int maxFiles_{3};
        LogLevel level_{LogLevel::Info};
        bool debugEnabled_{false};
    };

} // namespace lfc

// Shorthand macros
#define LFC_LOGD(...) ::lfc::Logger::instance().writef(::lfc::LogLevel::Debug, __VA_ARGS__)
#define LFC_LOGI(...) ::lfc::Logger::instance().writef(::lfc::LogLevel::Info,  __VA_ARGS__)
#define LFC_LOGW(...) ::lfc::Logger::instance().writef(::lfc::LogLevel::Warn,  __VA_ARGS__)
#define LFC_LOGE(...) ::lfc::Logger::instance().writef(::lfc::LogLevel::Error, __VA_ARGS__)
