#include "CommandRegistry.h"

namespace lfc {

    CommandRegistry& CommandRegistry::instance() {
        static CommandRegistry g;
        return g;
    }

    void CommandRegistry::register_rpc(const std::string& name) {
        std::lock_guard<std::mutex> lk(mtx_);
        rpc_.insert(name);
    }

    void CommandRegistry::register_shm(const std::string& name) {
        std::lock_guard<std::mutex> lk(mtx_);
        shm_.insert(name);
    }

    std::vector<std::string> CommandRegistry::list_rpc() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return std::vector<std::string>(rpc_.begin(), rpc_.end());
    }

    std::vector<std::string> CommandRegistry::list_shm() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return std::vector<std::string>(shm_.begin(), shm_.end());
    }

} // namespace lfc
