#pragma once
#include <vector>
#include <string>

namespace lfc {

    /**
     * Thin view layer over the central CommandRegistry.
     * Keep this header-only-facing API stable so the CLI/daemon/main
     * can list commands without depending on internal details.
     */
    std::vector<std::string> rpc_commands();
    std::vector<std::string> shm_commands();

} // namespace lfc
