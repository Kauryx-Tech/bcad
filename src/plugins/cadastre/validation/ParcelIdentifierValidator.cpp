#include "ParcelIdentifierValidator.h"
#include <regex>

namespace bcad::cadastre {

ParcelIdentifierValidator::Result ParcelIdentifierValidator::validate(
    const std::string& section, const std::string& numero) const {
    static const std::regex sectionPattern("^[A-Z]{1,3}$");
    static const std::regex numberPattern("^[0-9]+$");
    if (!std::regex_match(section, sectionPattern)) {
        return {false, "La section doit contenir une à trois lettres majuscules"};
    }
    if (!std::regex_match(numero, numberPattern)) {
        return {false, "Le numéro doit contenir uniquement des chiffres"};
    }
    return {};
}

} // namespace bcad::cadastre
