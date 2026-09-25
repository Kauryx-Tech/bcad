#include "ParcelIdentifierValidator.h"

#include <utility>

namespace bcad::cadastre {

namespace {

// Motif invalide dans un gabarit : la regle refuse alors toute valeur, ce qui
// se voit dans les diagnostics plutot que de laisser passer n'importe quoi.
std::regex compilePattern(const std::string& pattern) {
    try {
        return std::regex(pattern);
    } catch (const std::regex_error&) {
        return std::regex("(?!a)a");
    }
}

} // namespace

ParcelIdentifierValidator::ParcelIdentifierValidator(std::string sectionPattern,
                                                     std::string numberPattern)
    : sectionPattern_(std::move(sectionPattern)),
      numberPattern_(std::move(numberPattern)),
      sectionRegex_(compilePattern(sectionPattern_)),
      numberRegex_(compilePattern(numberPattern_)) {}

ParcelIdentifierValidator::Result ParcelIdentifierValidator::validate(
    const std::string& section, const std::string& numero) const {
    // Le libelle cite le motif en vigueur : il vient du gabarit, il serait faux
    // de le decrire en toutes lettres depuis le code.
    if (!std::regex_match(section, sectionRegex_)) {
        return {false, "section « " + section + " » non conforme au motif " + sectionPattern_};
    }
    if (!std::regex_match(numero, numberRegex_)) {
        return {false, "numéro « " + numero + " » non conforme au motif " + numberPattern_};
    }
    return {};
}

} // namespace bcad::cadastre
