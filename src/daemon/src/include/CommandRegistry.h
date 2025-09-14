#pragma once
#include <string>
#include <vector>
#include <set>
#include <mutex>

namespace lfc {

    /**
     * Thread-safe registry for RPC and SHM command names.
     * Intent: register right after you bind a handler, so introspection
     * remains 100% accurate without manual lists.
     */
    class CommandRegistry {
    public:
        static CommandRegistry& instance();

        void register_rpc(const std::string& name);
        void register_shm(const std::string& name);

        std::vector<std::string> list_rpc() const;
        std::vector<std::string> list_shm() const;

    private:
        CommandRegistry() = default;
        CommandRegistry(const CommandRegistry&) = delete;
        CommandRegistry& operator=(const CommandRegistry&) = delete;

        mutable std::mutex mtx_;
        std::set<std::string> rpc_;
        std::set<std::string> shm_;
    };

    // Small helpers/macros to keep Daemon.cpp clean.
    inline void RegisterRpc(const std::string& name) {
        CommandRegistry::instance().register_rpc(name);
    }
    inline void RegisterShm(const std::string& name) {
        CommandRegistry::instance().register_shm(name);
    }

    // Use these one-liners right after you bind a handler:
    #define LFC_REG_RPC(name_literal)  ::lfc::RegisterRpc(name_literal)
    #define LFC_REG_SHM(name_literal)  ::lfc::RegisterShm(name_literal)

} // namespace lfc
