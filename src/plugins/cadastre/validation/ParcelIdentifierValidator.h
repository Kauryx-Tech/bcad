#pragma once

#include <string>

namespace bcad::cadastre {

class ParcelIdentifierValidator {
public:
    struct Result {
        bool valid = true;
        std::string error;
    };

    Result validate(const std::string& section, const std::string& numero) const;
};

} // namespace bcad::cadastre
