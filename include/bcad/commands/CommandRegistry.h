#pragma once

#include "bcad/commands/Command.h"
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace bcad::commands {

// Factory function for creating commands from plugin arguments
using CommandFactory = std::function<std::unique_ptr<Command>(const std::vector<std::string>& args)>;

// Registry for command types. Allows plugins to register new command types
// without modifying Core. Maps command name to factory function.
class CommandRegistry {
public:
    CommandRegistry() = default;
    ~CommandRegistry() = default;
    CommandRegistry(const CommandRegistry&) = delete;
    CommandRegistry& operator=(const CommandRegistry&) = delete;
    CommandRegistry(CommandRegistry&&) = default;
    CommandRegistry& operator=(CommandRegistry&&) = default;

    // Register a command type with its factory
    // Returns false if command name already registered
    bool registerCommand(std::string_view name, CommandFactory factory);

    // Unregister a command type
    bool unregisterCommand(std::string_view name);

    // Check if a command is registered
    bool hasCommand(std::string_view name) const;

    // Create a command from its name and arguments
    // Returns null if command not registered or args invalid
    std::unique_ptr<Command> createCommand(std::string_view name, const std::vector<std::string>& args) const;

    // Get all registered command names
    std::vector<std::string> getRegisteredCommands() const;

    // Singleton access
    static CommandRegistry& instance();

private:
    std::unordered_map<std::string, CommandFactory> factories_;
};

} // namespace bcad::commands