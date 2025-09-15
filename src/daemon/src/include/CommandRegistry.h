#pragma once
// Dynamic JSON-RPC registry

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace lfc {

    struct RpcRequest {
        std::string method;
        std::string id;          // may be empty for notifications
        std::string paramsJson;  // raw json (object or array)
    };

    struct RpcResult {
        bool ok{true};
        std::string json; // already JSON (object/array/primitive)
    };

    struct CommandInfo {
        std::string name;
        std::string help;
    };

    using RpcHandler = std::function<RpcResult(const RpcRequest&)>;

    class CommandRegistry {
    public:
        void registerMethod(const std::string& name, const std::string& help, RpcHandler fn);
        bool has(const std::string& name) const;
        RpcResult call(const RpcRequest& req) const;
        std::vector<CommandInfo> list() const;

    private:
        std::unordered_map<std::string, std::pair<std::string, RpcHandler>> map_;
    };

} // namespace lfc
