#include "bcad/commands/CommandRegistry.h"

namespace bcad::commands {

bool CommandRegistry::registerCommand(std::string_view name, CommandFactory factory) {
    std::string key(name);
    auto result = factories_.emplace(std::move(key), std::move(factory));
    return result.second;
}

bool CommandRegistry::unregisterCommand(std::string_view name) {
    return factories_.erase(std::string(name)) > 0;
}

bool CommandRegistry::hasCommand(std::string_view name) const {
    return factories_.find(std::string(name)) != factories_.end();
}

std::unique_ptr<Command> CommandRegistry::createCommand(std::string_view name, const std::vector<std::string>& args) const {
    auto it = factories_.find(std::string(name));
    if (it == factories_.end()) return nullptr;
    return it->second(args);
}

std::vector<std::string> CommandRegistry::getRegisteredCommands() const {
    std::vector<std::string> names;
    names.reserve(factories_.size());
    for (const auto& [name, _] : factories_) {
        names.push_back(name);
    }
    return names;
}

CommandRegistry& CommandRegistry::instance() {
    static CommandRegistry registry;
    return registry;
}

} // namespace bcad::commands