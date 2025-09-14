#include "CommandIntrospection.h"
#include "CommandRegistry.h"

namespace lfc {

    std::vector<std::string> rpc_commands() {
        return CommandRegistry::instance().list_rpc();
    }

    std::vector<std::string> shm_commands() {
        return CommandRegistry::instance().list_shm();
    }

} // namespace lfc
