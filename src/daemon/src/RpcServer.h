/*
 * Minimal JSON-RPC 2.0 server over stdin/stdout for lfcd.
 * (c) 2025 meigrafd & contributors - MIT
 */
#pragma once
#include <functional>
#include <map>
#include <string>
#include <nlohmann/json.hpp>

class RpcServer {
public:
    using Handler = std::function<nlohmann::json(const nlohmann::json&)>;

    explicit RpcServer(bool debug = false);
    void on(const std::string& method, Handler h);
    int run(); // blocking loop

private:
    bool debug_ = false;
    std::map<std::string, Handler> handlers_;
    nlohmann::json handleEnvelope(const nlohmann::json& env);
};
