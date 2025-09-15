/*
 * Linux Fan Control — Engine (implementation)
 * - Periodic sensor readout and telemetry
 * - Optional PWM control (disabled by default)
 * (c) 2025 LinuxFanControl contributors
 */
#include "Engine.hpp"
#include "Hwmon.hpp"
#include "Log.hpp"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace lfc {

  Engine::Engine() = default;
  Engine::~Engine() { stop(); }

  void Engine::setSnapshot(const HwmonSnapshot& snap) {
    snap_ = snap;
  }

  bool Engine::initShm(const std::string& path) {
    shmPath_ = path;
    int fd = ::shm_open(shmPath_.c_str(), O_CREAT | O_RDWR, 0660);
    if (fd < 0) return false;
    if (::ftruncate(fd, static_cast<off_t>(kShmSize)) != 0) {
      ::close(fd);
      return false;
    }
    void* p = ::mmap(nullptr, kShmSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ::close(fd);
    if (p == MAP_FAILED) return false;
    shmPtr_ = static_cast<char*>(p);
    std::memset(shmPtr_, 0, kShmSize);
    return true;
  }

  void Engine::start() {
    running_ = true;
    lastTick_ = std::chrono::steady_clock::now();
  }

  void Engine::stop() {
    running_ = false;
    if (shmPtr_) {
      ::munmap(shmPtr_, kShmSize);
      shmPtr_ = nullptr;
    }
  }

  void Engine::enableControl(bool on) {
    controlEnabled_ = on;
  }

  static int clampPercent(int v) {
    if (v < 0) return 0;
    if (v > 100) return 100;
    return v;
  }

  void Engine::tick() {
    if (!running_) return;

    auto now = std::chrono::steady_clock::now();
    if (now - lastTick_ < std::chrono::milliseconds(200)) return;
    lastTick_ = now;

    // Read temperatures and fans.
    std::vector<std::pair<std::string, int>> temps;
    temps.reserve(snap_.temps.size());
    for (const auto& t : snap_.temps) {
      auto v = Hwmon::readMilliC(t);
      if (v) {
        temps.emplace_back(t.label.empty() ? t.path_input : t.label, *v);
      }
    }

    std::vector<std::pair<std::string, int>> fans;
    fans.reserve(snap_.fans.size());
    for (const auto& f : snap_.fans) {
      auto v = Hwmon::readRpm(f);
      if (v) {
        fans.emplace_back(f.path_input, *v);
      }
    }

    // Compute naive target from max temp (only applied if control is enabled).
    int maxMilliC = 0;
    for (const auto& kv : temps) {
      if (kv.second > maxMilliC) maxMilliC = kv.second;
    }
    int target = 0; // 0..100
    if (maxMilliC <= 40000) {
      target = 20;
    } else if (maxMilliC >= 80000) {
      target = 100;
    } else {
      double x = (maxMilliC - 40000) / 40000.0;
      target = static_cast<int>(20 + x * 80);
    }
    target = clampPercent(target);

    // Apply only if explicitly enabled.
    if (controlEnabled_) {
      for (const auto& p : snap_.pwms) {
        Hwmon::setManual(p);
        Hwmon::setPercent(p, target);
      }
      lastPercent_ = target;
    }

    // Telemetry JSON.
    std::ostringstream tele;
    tele << "{";

    tele << "\"control\":" << (controlEnabled_ ? "true" : "false") << ",";
    tele << "\"max_mC\":" << maxMilliC << ",";

    tele << "\"temps\":[";
    {
      bool first = true;
      for (const auto& kv : temps) {
        if (!first) {
          tele << ",";
        }
        first = false;
        tele << "{\"name\":\"";
        for (char c : kv.first) {
          if (c == '\"' || c == '\\') tele << '\\';
          tele << c;
        }
        tele << "\",\"mC\":" << kv.second << "}";
      }
    }
    tele << "],";

    tele << "\"fans\":[";
    {
      bool first = true;
      for (const auto& kv : fans) {
        if (!first) {
          tele << ",";
        }
        first = false;
        tele << "{\"path\":\"";
        for (char c : kv.first) {
          if (c == '\"' || c == '\\') tele << '\\';
          tele << c;
        }
        tele << "\",\"rpm\":" << kv.second << "}";
      }
    }
    tele << "],";

    if (controlEnabled_) {
      tele << "\"pwm\":{\"percent\":" << target << "}";
    } else {
      tele << "\"pwm\":null";
    }
    tele << "}";

    const std::string s = tele.str();
    if (shmPtr_) {
      std::size_t n = s.size();
      if (n >= kShmSize - 1) n = kShmSize - 1;
      std::memcpy(shmPtr_, s.data(), n);
      shmPtr_[n] = '\0';
    }
  }

} // namespace lfc
