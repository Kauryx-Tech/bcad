#pragma once

#include <string>

namespace bcad::layout {

struct Cartouche {
    std::string commune;
    std::string section;
    std::string echelle;      // ex: "1:500"
    std::string date;         // ex: "23/09/2026"
    std::string geometre;
    std::string dossier;
    double heightMm = 25.0;   // hauteur du cartouche en bas de feuille

    bool isValid() const { return !commune.empty() || !section.empty(); }

    std::string title() const {
        std::string t;
        if (!commune.empty()) t += commune;
        if (!section.empty()) t += (t.empty() ? "" : " - ") + std::string("Section ") + section;
        if (!echelle.empty()) t += "  (" + echelle + ")";
        return t;
    }
};

} // namespace bcad::layout
