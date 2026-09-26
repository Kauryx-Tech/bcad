#pragma once

// Formats ISO 216 vérifiés contre la norme (A0 841×1189 … A4 210×297 mm).
// Sources : ISO 216:2007, papersizes.io, engineeringtoolbox.com.

#include "bcad/layout/Furniture.h"
#include "bcad/layout/GeometryMm.h"
#include "bcad/layout/Viewport.h"

#include <optional>
#include <string>
#include <vector>

namespace bcad::layout {

// Formats normalisés ISO 216
enum class PaperFormat { A4, A3, A2, A1, A0 };
enum class Orientation { Portrait, Paysage };

struct Margins {
    double top = 10.0;    // mm
    double bottom = 10.0;
    double left = 10.0;
    double right = 10.0;
};

// Dimensions en mm (ISO 216)
inline double paperWidth(PaperFormat f) {
    switch (f) {
        case PaperFormat::A4: return 210;
        case PaperFormat::A3: return 297;
        case PaperFormat::A2: return 420;
        case PaperFormat::A1: return 594;
        case PaperFormat::A0: return 841;
    }
    return 210;
}

inline double paperHeight(PaperFormat f) {
    switch (f) {
        case PaperFormat::A4: return 297;
        case PaperFormat::A3: return 420;
        case PaperFormat::A2: return 594;
        case PaperFormat::A1: return 841;
        case PaperFormat::A0: return 1189;
    }
    return 210;
}

inline const char* paperName(PaperFormat f) {
    switch (f) {
        case PaperFormat::A4: return "A4";
        case PaperFormat::A3: return "A3";
        case PaperFormat::A2: return "A2";
        case PaperFormat::A1: return "A1";
        case PaperFormat::A0: return "A0";
    }
    return "A4";
}

// L'autre bout de `paperName`. Le token lu dans un fichier est la donnée ; cette
// fonction ne dit que ce que CET hôte sait en mesurer. Un format national non
// listé ici n'est pas une faute de saisie : il est inconnu, et doit rester écrit.
inline std::optional<PaperFormat> paperFormatOfToken(const std::string& token) {
    if (token == "A4") return PaperFormat::A4;
    if (token == "A3") return PaperFormat::A3;
    if (token == "A2") return PaperFormat::A2;
    if (token == "A1") return PaperFormat::A1;
    if (token == "A0") return PaperFormat::A0;
    return std::nullopt;
}

inline const char* orientationName(Orientation o) {
    return o == Orientation::Portrait ? "Portrait" : "Paysage";
}

inline std::optional<Orientation> orientationOfToken(const std::string& token) {
    if (token == "Portrait") return Orientation::Portrait;
    if (token == "Paysage") return Orientation::Paysage;
    return std::nullopt;
}

// Une feuille du document (ADR-017 décision 1) : un objet Nommé, tenu par son
// format, son orientation, ses marges, ses vues et ses meubles. Elle est hors du
// modèle géométrique — ni `extents()`, ni index spatial, ni tessellation, ni
// picking ne la voient — et rien de son vocabulaire n'est un nom de C++ : le
// cartouche d'un pays est un meuble déclaré, pas une structure de l'hôte.
//
// Les tokens de format et d'orientation sont la donnée, les enums n'en sont que la
// lecture comprise ici. Un token que ce binaire ne connait pas se conserve, rend la
// feuille non mesurable — et non « retombée sur A3 » : une dimension inventée se
// peint, une dimension absente se signale.
class Sheet {
public:
    Sheet(PaperFormat format = PaperFormat::A3,
          Orientation orientation = Orientation::Paysage,
          Margins margins = {})
        : format_(format),
          formatToken_(paperName(format)),
          orientation_(orientation),
          orientationToken_(orientationName(orientation)),
          margins_(margins) {}

    PaperFormat format() const { return format_; }
    Orientation orientation() const { return orientation_; }
    const Margins& margins() const { return margins_; }

    void setFormat(PaperFormat f) {
        format_ = f;
        formatKnown_ = true;
        formatToken_ = paperName(f);
    }
    void setOrientation(Orientation o) {
        orientation_ = o;
        orientationKnown_ = true;
        orientationToken_ = orientationName(o);
    }
    void setMargins(Margins m) { margins_ = m; }

    // Nom que l'opérateur donne à la feuille. Vide tant qu'il n'a rien nommé :
    // c'est un objet du document, pas une chaîne d'affichage calculée.
    const std::string& title() const { return title_; }
    void setTitle(const std::string& title) { title_ = title; }

    // Le format et l'orientation tels qu'écrits, connus ou non.
    const std::string& formatToken() const { return formatToken_; }
    const std::string& orientationToken() const { return orientationToken_; }
    bool formatIsKnown() const { return formatKnown_; }
    bool orientationIsKnown() const { return orientationKnown_; }

    // Un token inconnu ne fait pas sauter le fichier : il est garde, et la
    // feuille devient non mesurable jusqu'a ce qu'un hote le comprenne.
    void setFormatToken(const std::string& token) {
        formatToken_ = token;
        if (const auto f = paperFormatOfToken(token)) {
            format_ = *f;
            formatKnown_ = true;
        } else {
            formatKnown_ = false;
        }
    }
    void setOrientationToken(const std::string& token) {
        orientationToken_ = token;
        if (const auto o = orientationOfToken(token)) {
            orientation_ = *o;
            orientationKnown_ = true;
        } else {
            orientationKnown_ = false;
        }
    }

    // Nom affiché : celui que l'opérateur a donné, sinon le format et
    // l'orientation. Le second cas est celui de tout le code écrit avant
    // l'ADR-017, donc rien de visible ne change faute d'un nom choisi.
    std::string name() const {
        if (!title_.empty()) return title_;
        std::string n = formatKnown_ ? paperName(format_) : formatToken_;
        n += " ";
        n += orientationKnown_ ? orientationName(orientation_) : orientationToken_;
        return n;
    }

    // Dimensions totales (avec orientation). Nullles si un des deux tokens est
    // inconnu : ce n'est pas une feuille de 0×0 mm, c'est une feuille que cet
    // executable ne sait pas mesurer.
    double width() const {
        if (!formatKnown_ || !orientationKnown_) return 0.0;
        return orientation_ == Orientation::Portrait ? paperWidth(format_) : paperHeight(format_);
    }
    double height() const {
        if (!formatKnown_ || !orientationKnown_) return 0.0;
        return orientation_ == Orientation::Portrait ? paperHeight(format_) : paperWidth(format_);
    }

    // Zone imprimable (hors marges)
    double printableWidth() const { return width() - margins_.left - margins_.right; }
    double printableHeight() const { return height() - margins_.top - margins_.bottom; }

    bool isValid() const {
        return formatKnown_ && orientationKnown_ && printableWidth() > 0 && printableHeight() > 0;
    }

    // Ce que la feuille contient : des vues du dessin, a une echelle choisie et a
    // une place donnee, et des meubles dont le vocabulaire vient d'un module.
    std::vector<Viewport>& views() { return views_; }
    const std::vector<Viewport>& views() const { return views_; }
    std::vector<Furniture>& furniture() { return furniture_; }
    const std::vector<Furniture>& furniture() const { return furniture_; }

private:
    std::string title_;
    PaperFormat format_;
    std::string formatToken_;
    bool formatKnown_ = true;
    Orientation orientation_;
    std::string orientationToken_;
    bool orientationKnown_ = true;
    Margins margins_;
    std::vector<Viewport> views_;
    std::vector<Furniture> furniture_;
};

// Corps rendu ici, et non dans Viewport.h : la vue se mesure a la feuille, et la
// feuille porte des vues. Sans ce renversement les deux en-tetes se boucleraient.
inline bool Viewport::fitsIn(const Sheet& sheet) const {
    return scale_ > 0 &&
           widthOnSheet() <= sheet.printableWidth() &&
           heightOnSheet() <= sheet.printableHeight();
}

} // namespace bcad::layout
