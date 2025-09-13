#pragma once
#include <string>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <atomic>

class JsonRpcServer {
public:
  using Handler = std::function<std::string(const std::string&)>;
  JsonRpcServer();
  void registerMethod(const std::string& name, Handler h);
  void runStdio();   // blocking
  void stop();
private:
  std::unordered_map<std::string, Handler> handlers_;
  std::mutex mtx_;
  std::atomic<bool> running_{false};
};
