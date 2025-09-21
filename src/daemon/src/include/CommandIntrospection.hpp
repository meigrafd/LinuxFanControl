/*
 * Linux Fan Control — Command Introspection (declarations)
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <string>

namespace lfc {
class CommandRegistry;

// Preferred, namespaced symbol
std::string BuildIntrospectionJson(const CommandRegistry& reg);
} // namespace lfc

// (Optional legacy) If some code still expects the global symbol,
// CommandIntrospection.cpp provides it as a thin forwarder.
// std::string BuildIntrospectionJson(const lfc::CommandRegistry& reg);
