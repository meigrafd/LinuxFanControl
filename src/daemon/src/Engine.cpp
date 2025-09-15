/*
 * Linux Fan Control — Engine (implementation)
 * - Periodic sensor readout and PWM updates
 * - SHM telemetry writer
 * - Snapshot-driven device inventory
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

static int clampPercent(int v) {
  if (v < 0) return 0;
  if (v > 100) return 100;
  return v;
}

void Engine::tick() {
  if (!running_) return;

  // Simple fixed cadence.
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

  // Naive control: map max temp to a PWM percent.
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
    // Linear 40C..80C -> 20..100
    double x = (maxMilliC - 40000) / 40000.0;
    target = static_cast<int>(20 + x * 80);
  }
  target = clampPercent(target);

  // Apply to all PWMs.
  for (const auto& p : snap_.pwms) {
    Hwmon::setManual(p);
    Hwmon::setPercent(p, target);
  }

  // Telemetry JSON.
  std::ostringstream tele;
  tele << "{";

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

  tele << "\"pwm\":{\"percent\":" << target << "}";
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
