/*
 * Linux Fan Control — Command Introspection (implementation)
 * Uses nlohmann::json to serialize introspection.
 * (c) 2025 LinuxFanControl contributors
 */
#include "include/CommandIntrospection.hpp"
#include "include/CommandRegistry.hpp"
#include <nlohmann/json.hpp>

using nlohmann::json;

// Global symbol (legacy)
std::string BuildIntrospectionJson(const lfc::CommandRegistry& reg) {
    json methods = json::array();
    for (const auto& m : reg.list()) {
        methods.push_back({{"name", m.name}, {"help", m.help}});
    }
    json root = {{"methods", methods}};
    return root.dump();
}

// Namespaced shim to satisfy callers expecting lfc::
namespace lfc {
std::string BuildIntrospectionJson(const CommandRegistry& reg) {
    return ::BuildIntrospectionJson(reg);
}
} // namespace lfc
