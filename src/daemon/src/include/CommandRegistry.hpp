/*
 * Linux Fan Control — Command registry (RPC command table)
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace lfc {

/* Lightweight command metadata. */
struct CommandInfo {
    std::string name;
    std::string help;   // short description (one line)
};

/* JSON-RPC request model */
struct RpcRequest {
    // JSON-RPC 2.0 fields we use:
    // - id:      number | string | null
    // - method:  string
    // - params:  object | array | null
    nlohmann::json id;     // keeps original type
    std::string    method;
    nlohmann::json params;
};

/* JSON-RPC result model */
struct RpcResult {
    // ok=true  -> result populated
    // ok=false -> error.* populated (JSON-RPC error object fields)
    bool            ok{true};
    nlohmann::json  id;
    nlohmann::json  result;        // when ok=true
    int             code{0};       // when ok=false
    std::string     message;       // when ok=false
    nlohmann::json  data;          // optional when ok=false

    static inline RpcResult makeOk(const nlohmann::json& id, const nlohmann::json& res) {
        RpcResult r; r.ok = true; r.id = id; r.result = res; return r;
    }
    static inline RpcResult makeError(const nlohmann::json& id, int code, const std::string& msg, const nlohmann::json& data = {}) {
        RpcResult r; r.ok = false; r.id = id; r.code = code; r.message = msg; r.data = data; return r;
    }
    inline nlohmann::json toJson() const {
        if (ok) {
            return nlohmann::json{
                {"jsonrpc","2.0"},
                {"id", id},
                {"result", result}
            };
        } else {
            nlohmann::json err{{"code", code}, {"message", message}};
            if (!data.is_null() && !data.empty()) err["data"] = data;
            return nlohmann::json{
                {"jsonrpc","2.0"},
                {"id", id},
                {"error", err}
            };
        }
    }
};

/* Error thrown when a command is not found. */
class CommandNotFound : public std::runtime_error {
public:
    explicit CommandNotFound(const std::string& n)
        : std::runtime_error("Unknown command: " + n) {}
};

/*
 * Thread-safe command registry.
 * - Stores name -> (RpcHandler, help).
 * - No locks are held while executing the callable.
 * - Provides built-ins "commands" and "help".
 */
class CommandRegistry {
public:
    using RpcHandler = std::function<RpcResult(const RpcRequest&)>;

    CommandRegistry();
    ~CommandRegistry();

    /* Register or replace a command. */
    void add(const std::string& name, const std::string& help, RpcHandler fn);

    /* Remove a command if present. */
    void remove(const std::string& name);

    /* Remove all commands (built-ins are reinstalled afterwards). */
    void clear();

    /* Returns true if the command exists. */
    bool exists(const std::string& name) const;

    /* Number of registered commands. */
    size_t size() const;

    /* Invoke a command; throws CommandNotFound if missing. */
    RpcResult call(const RpcRequest& req);

    /* List all commands (sorted by name). */
    std::vector<CommandInfo> list() const;

    /* JSON list helper: [{name, help}, ...] sorted. */
    nlohmann::json listJson() const;

    /* Optional: get help text for a command. */
    std::optional<std::string> help(const std::string& name) const;

private:
    struct Impl;
    Impl* impl_;

    void installBuiltins_();
};

} // namespace lfc
