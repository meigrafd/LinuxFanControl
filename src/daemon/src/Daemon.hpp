#pragma once
#include "JsonRpc.hpp"
#include "Engine.hpp"
#include "Hwmon.hpp"
#include <unordered_map>

class Daemon {
public:
  Daemon();
  void run();
private:
  JsonRpcServer rpc_;
  Engine engine_;
  std::unordered_map<std::string,std::string> coupling_;

  std::string listSensors(const std::string&);
  std::string listPwms(const std::string&);
  std::string enumerate(const std::string&);
  std::string detectCalibrate(const std::string&);
  std::string listChannels(const std::string&);
  std::string createChannel(const std::string&);
  std::string deleteChannel(const std::string&);
  std::string setChannelMode(const std::string&);
  std::string setChannelManual(const std::string&);
  std::string setChannelCurve(const std::string&);
  std::string setChannelHystTau(const std::string&);
  std::string engineStart(const std::string&);
  std::string engineStop(const std::string&);
  std::string deleteCoupling(const std::string&);
  std::string noop(const std::string&);
};
