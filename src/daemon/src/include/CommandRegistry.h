#pragma once
#include <string>
#include <vector>

namespace lfc {

    using RpcHandler = std::function<RpcResult(const RpcRequest&)>;

    struct CommandInfo {
        std::string name;
        std::string help;
    };

    struct RpcRequest {
        std::string method;
        std::vector<std::string> args;
    };

    struct RpcResult {
        bool success;
        std::string message;
    };

    class CommandRegistry {
    public:
        void registerMethod(const std::string& name, const std::string& help, RpcHandler fn);
        bool has(const std::string& name) const;
        RpcResult call(const RpcRequest& req) const;
        std::vector<CommandInfo> list() const;

    private:
        std::vector<CommandInfo> commands;
        std::map<std::string, RpcHandler> handlers;
    };

} // namespace lfc
