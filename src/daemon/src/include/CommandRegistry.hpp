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
#include <stdexcept>
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
    bool            ok{true};
    nlohmann::json  id;
    nlohmann::json  result;        // when ok=true
    int             code{0};       // when ok=false
    std::string     message;       // when ok=false
    nlohmann::json  data;          // optional when ok=false
    std::string     method;        // needed to put method at top-level in error payload

    // success: pass the final payload object via 'res'
    static inline RpcResult makeOk(const nlohmann::json& id, const nlohmann::json& res) {
        RpcResult r; r.ok = true; r.id = id; r.result = res; return r;
    }

    // error: capture method explicitly so we can surface it at top-level. 4-arg version
    static inline RpcResult makeError(const nlohmann::json& id, const std::string& method, int code, const std::string& msg, const nlohmann::json& data = {}) {
        RpcResult r; r.ok = false; r.id = id; r.method = method; r.code = code; r.message = msg; r.data = data; return r;
    }

    inline nlohmann::json toJson() const {
        if (ok) {
            // JSON-RPC: put your envelope into "result"
            return nlohmann::json{
                {"jsonrpc","2.0"},
                {"id", id},
                {"result", result}
            };
        } else {
            // JSON-RPC: still use "result" and keep your envelope shape
            nlohmann::json payload = {
                {"success", false},
                {"method",  method},
                {"error",   nlohmann::json{{"code", code}, {"message", message}}}
            };
            if (!data.is_null() && !data.empty()) {
                payload["data"] = data; // optional extra info
            }
            return nlohmann::json{
                {"jsonrpc","2.0"},
                {"id", id},
                {"result", payload}
            };
        }
    }
};

// --- JSON-RPC convenience helpers (shared by all Rpc*.cpp) ------------------
inline RpcResult ok_(const RpcRequest& rq,
                     const char* method,
                     const nlohmann::json& data = nlohmann::json::object()) {
    return RpcResult::makeOk(rq.id, nlohmann::json{
        {"success", true},
        {"method",  method},
        {"data",    data}
    });
}

inline RpcResult err_(const RpcRequest& rq,
                      const char* method,
                      int code,
                      const std::string& message,
                      const nlohmann::json& extra = nlohmann::json::object()) {
    return RpcResult::makeError(rq.id, method, code, message, extra);
}

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

/*
 * Normalize RPC params to JSON and object-shape.
 * Safe to include in headers (inline). No API changes to existing code.
 */
inline nlohmann::json paramsToJson(const RpcRequest& rq) {
    // If your RpcRequest already stores JSON in rq.params, just return it.
    return rq.params;
}

inline nlohmann::json paramsAsObject(const nlohmann::json& p) {
    if (p.is_object()) {
        return p;
    }
    // JSON-RPC allows array-style params: [ { ... } ]
    if (p.is_array() && p.size() == 1 && p[0].is_object()) {
        return p[0];
    }
    // Anything else → empty object to keep handlers robust
    return nlohmann::json::object();
}

// Overload for convenience when a handler has the whole request:
inline nlohmann::json paramsAsObject(const RpcRequest& rq) {
    return paramsAsObject(paramsToJson(rq));
}

inline std::string paramsToString(const nlohmann::json& j) {
    try {
        return j.dump(); // compact for logging
    } catch (...) {
        return "{}";
    }
}

inline std::string paramsToString(const RpcRequest& rq) {
    return paramsToString(rq.params);
}

} // namespace lfc
