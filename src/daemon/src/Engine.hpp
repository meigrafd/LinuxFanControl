#pragma once
#include "Curve.hpp"
#include "Hwmon.hpp"
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>

struct Channel {
  std::string name;
  std::string sensor_path;
  std::string output_label;
  Curve curve;
  SchmittSlew ss;
  std::string mode="Auto";
  double manual=0.0;
};

class Engine {
public:
  void setSensors(std::vector<TempSensor> s){ std::lock_guard<std::mutex> lk(mx_); sensors_=std::move(s); }
  void setOutputs(std::vector<PwmOutput> p){ std::lock_guard<std::mutex> lk(mx_); outputs_=std::move(p); }
  const std::vector<TempSensor>& sensors() const { return sensors_; }
  const std::vector<PwmOutput>& outputs() const { return outputs_; }
  std::vector<Channel>& channels(){ return channels_; }

  void start();
  void stop();
  bool running() const { return running_.load(); }

private:
  std::vector<TempSensor> sensors_;
  std::vector<PwmOutput> outputs_;
  std::vector<Channel> channels_;
  std::thread th_;
  std::atomic<bool> running_{false};
  mutable std::mutex mx_;
};
