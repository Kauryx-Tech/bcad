#pragma once

#include <regex>
#include <string>

namespace bcad::cadastre {

// Identification d'une parcelle : la forme de la section et du numero est une
// regle de profil, pas une constante du code. Les motifs par defaut sont ceux
// du profil historique ; un gabarit installe (Templates.cpp) peut les remplacer.
class ParcelIdentifierValidator {
public:
    struct Result {
        bool valid = true;
        std::string error;
    };

    explicit ParcelIdentifierValidator(
        std::string sectionPattern = "^[A-Z]{1,3}$",
        std::string numberPattern = "^[0-9]+$");

    Result validate(const std::string& section, const std::string& numero) const;

    const std::string& sectionPattern() const { return sectionPattern_; }
    const std::string& numberPattern() const { return numberPattern_; }

private:
    std::string sectionPattern_;
    std::string numberPattern_;
    // Compiles a la construction : la regle est creee une fois au chargement du
    // module, pas par entite verifiee.
    std::regex sectionRegex_;
    std::regex numberRegex_;
};

} // namespace bcad::cadastre
