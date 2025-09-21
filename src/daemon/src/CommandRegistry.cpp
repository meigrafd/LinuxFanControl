/*
 * Linux Fan Control — Command registry (implementation)
 * (c) 2025 LinuxFanControl contributors
 */
#include "include/CommandRegistry.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace lfc {

struct CommandRegistry::Impl {
    // name -> (handler, help)
    std::map<std::string, std::pair<CommandRegistry::RpcHandler, std::string>> map;
    mutable std::mutex mtx;
};

CommandRegistry::CommandRegistry()
    : impl_(new Impl) {
    installBuiltins_();
}

CommandRegistry::~CommandRegistry() {
    delete impl_;
}

void CommandRegistry::add(const std::string& name, const std::string& helpText, RpcHandler fn) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    impl_->map[name] = std::make_pair(std::move(fn), helpText);
}

void CommandRegistry::remove(const std::string& name) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    impl_->map.erase(name);
}

void CommandRegistry::clear() {
    {
        std::lock_guard<std::mutex> lock(impl_->mtx);
        impl_->map.clear();
    }
    installBuiltins_();
}

bool CommandRegistry::exists(const std::string& name) const {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    return impl_->map.find(name) != impl_->map.end();
}

size_t CommandRegistry::size() const {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    return impl_->map.size();
}

RpcResult CommandRegistry::call(const RpcRequest& req) {
    RpcHandler fn;
    {
        std::lock_guard<std::mutex> lock(impl_->mtx);
        auto it = impl_->map.find(req.method);
        if (it == impl_->map.end()) {
            throw CommandNotFound(req.method);
        }
        fn = it->second.first; // copy callable; execute without the lock
    }
    return fn(req);
}

std::vector<CommandInfo> CommandRegistry::list() const {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    std::vector<CommandInfo> out;
    out.reserve(impl_->map.size());
    for (const auto& kv : impl_->map) {
        out.push_back(CommandInfo{kv.first, kv.second.second});
    }
    std::sort(out.begin(), out.end(), [](const CommandInfo& a, const CommandInfo& b){
        return a.name < b.name;
    });
    return out;
}

nlohmann::json CommandRegistry::listJson() const {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& ci : list()) {
        arr.push_back({{"name", ci.name}, {"help", ci.help}});
    }
    return arr;
}

std::optional<std::string> CommandRegistry::help(const std::string& name) const {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    auto it = impl_->map.find(name);
    if (it == impl_->map.end()) return std::nullopt;
    return it->second.second;
}

void CommandRegistry::installBuiltins_() {
    // List commands
    add("commands",
        "List available commands",
        [this](const RpcRequest& rq) -> RpcResult {
            (void)rq;
            return RpcResult::makeOk(nullptr, this->listJson());
        });

    // Help for a command: params: {"name": "commandName"}
    add("help",
        "Show help for a command",
        [this](const RpcRequest& rq) -> RpcResult {
            const auto name = rq.params.value("name", std::string{});
            if (name.empty()) {
                return RpcResult::makeError(rq.id, -32602, "missing 'name'");
            }
            auto h = this->help(name);
            if (!h) {
                return RpcResult::makeError(rq.id, -32601, "unknown command", {{"name", name}});
            }
            return RpcResult::makeOk(rq.id, nlohmann::json{{"name", name}, {"help", *h}});
        });
}

} // namespace lfc
