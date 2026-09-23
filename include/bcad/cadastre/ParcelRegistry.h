#pragma once

#include <string>
#include <unordered_set>

namespace bcad::cadastre {

// Registre d'unicité section+numéro (E1)
class ParcelRegistry {
public:
    bool exists(const std::string& section, const std::string& numero) const {
        return keys_.count(key(section, numero)) > 0;
    }
    bool add(const std::string& section, const std::string& numero) {
        return keys_.insert(key(section, numero)).second;
    }
    bool remove(const std::string& section, const std::string& numero) {
        return keys_.erase(key(section, numero)) > 0;
    }
    void clear() { keys_.clear(); }
    size_t size() const { return keys_.size(); }

private:
    static std::string key(const std::string& s, const std::string& n) { return s + "|" + n; }
    std::unordered_set<std::string> keys_;
};

} // namespace bcad::cadastre
