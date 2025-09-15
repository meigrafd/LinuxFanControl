#include "Log.hpp"
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>
#include <unistd.h>

using namespace lfc;

Logger& Logger::instance() {
    static Logger g;
    return g;
}

Logger::Logger() = default;
Logger::~Logger() {
    std::lock_guard<std::mutex> lk(mtx_);
    if (fp_) std::fclose(fp_);
}

void Logger::configure(const std::string& filePath, std::size_t maxBytes, int maxFiles, bool debugEnabled) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (fp_) { std::fclose(fp_); fp_ = nullptr; }
    path_ = filePath;
    maxBytes_ = maxBytes ? maxBytes : (5*1024*1024);
    maxFiles_ = maxFiles > 0 ? maxFiles : 3;
    debugEnabled_ = debugEnabled;
    ensureOpen();
}

void Logger::setLevel(LogLevel lvl) { level_ = lvl; }
LogLevel Logger::level() const { return level_; }

void Logger::ensureOpen() {
    if (path_.empty()) return;
    ::umask(0022);
    fp_ = std::fopen(path_.c_str(), "a");
}

void Logger::rotateIfNeeded(std::size_t addBytes) {
    if (!fp_ || path_.empty()) return;
    long cur = std::ftell(fp_);
    if (cur < 0) return;
    std::size_t next = static_cast<std::size_t>(cur) + addBytes;
    if (next <= maxBytes_) return;

    std::fclose(fp_); fp_ = nullptr;
    // rotate: file.log.N ... file.log.1
    for (int i = maxFiles_-1; i >= 1; --i) {
        std::ostringstream src, dst;
        src << path_ << "." << i;
        dst << path_ << "." << (i+1);
        ::rename(src.str().c_str(), dst.str().c_str());
    }
    std::string dst1 = path_ + ".1";
    ::rename(path_.c_str(), dst1.c_str());
    ensureOpen();
}

std::string Logger::nowStamp() {
    using namespace std::chrono;
    auto t  = system_clock::to_time_t(system_clock::now());
    std::tm tm{};
    localtime_r(&t, &tm);
    std::ostringstream os;
    os<<std::put_time(&tm,"%Y-%m-%d %H:%M:%S");
    return os.str();
}

void Logger::write(LogLevel lvl, const std::string& msg) {
    if (!fp_) ensureOpen();
    if (!fp_) return;

    const char* tag = (lvl==LogLevel::Debug?"DEBUG":
    lvl==LogLevel::Info ?"INFO ":
    lvl==LogLevel::Warn ?"WARN ":
    "ERROR");
    std::string line = nowStamp() + " [" + tag + "] " + msg + "\n";
    rotateIfNeeded(line.size());
    std::fwrite(line.data(), 1, line.size(), fp_);
    std::fflush(fp_);
}

void Logger::writef(LogLevel lvl, const char* fmt, ...) {
    char buf[4096];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    write(lvl, std::string(buf));
}
