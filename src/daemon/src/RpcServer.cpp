/*
 * Minimal JSON-RPC 2.0 server over stdin/stdout for lfcd.
 * (c) 2025 meigrafd & contributors - MIT
 */
#include "RpcServer.h"
#include <iostream>

RpcServer::RpcServer(bool debug) : debug_(debug) {}

void RpcServer::on(const std::string& method, Handler h) {
    handlers_[method] = std::move(h);
}

nlohmann::json RpcServer::handleEnvelope(const nlohmann::json& env) {
    nlohmann::json out;
    out["jsonrpc"] = "2.0";
    if (env.contains("id")) out["id"] = env["id"];

    try {
        auto it = env.find("method");
        if (it == env.end() || !it->is_string())
            throw std::runtime_error("invalid request: no method");
        const std::string m = *it;

        auto hit = handlers_.find(m);
        if (hit == handlers_.end())
            throw std::runtime_error("method not found: " + m);

        const nlohmann::json params = env.value("params", nlohmann::json::object());
        out["result"] = hit->second(params);
    } catch (const std::exception& ex) {
        out["error"] = { {"code", -32603}, {"message", ex.what()} };
    }
    return out;
}

int RpcServer::run() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        try {
            auto doc = nlohmann::json::parse(line);
            if (doc.is_array()) {
                nlohmann::json arr = nlohmann::json::array();
                for (auto& env : doc) {
                    arr.push_back(handleEnvelope(env));
                }
                std::cout << arr.dump() << "\n" << std::flush;
            } else if (doc.is_object()) {
                auto resp = handleEnvelope(doc);
                std::cout << resp.dump() << "\n" << std::flush;
            } else {
                nlohmann::json err = {{"jsonrpc","2.0"},{"error", {{"code",-32600},{"message","invalid"} } }};
                std::cout << err.dump() << "\n" << std::flush;
            }
        } catch (const std::exception& ex) {
            nlohmann::json err = {{"jsonrpc","2.0"},{"error", {{"code",-32700},{"message",ex.what()} } }};
            std::cout << err.dump() << "\n" << std::flush;
        }
    }
    return 0;
}
