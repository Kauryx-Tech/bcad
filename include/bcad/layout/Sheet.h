#pragma once

// Formats ISO 216 vérifiés contre la norme (A0 841×1189 … A4 210×297 mm).
// Sources : ISO 216:2007, papersizes.io, engineeringtoolbox.com.

#include <string>

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
    return 297;
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

class Sheet {
public:
    Sheet(PaperFormat format = PaperFormat::A3,
          Orientation orientation = Orientation::Paysage,
          Margins margins = {})
        : format_(format), orientation_(orientation), margins_(margins) {}

    PaperFormat format() const { return format_; }
    Orientation orientation() const { return orientation_; }
    const Margins& margins() const { return margins_; }

    void setFormat(PaperFormat f) { format_ = f; }
    void setOrientation(Orientation o) { orientation_ = o; }
    void setMargins(Margins m) { margins_ = m; }

    // Dimensions totales (avec orientation)
    double width() const {
        return orientation_ == Orientation::Portrait ? paperWidth(format_) : paperHeight(format_);
    }
    double height() const {
        return orientation_ == Orientation::Portrait ? paperHeight(format_) : paperWidth(format_);
    }

    // Zone imprimable (hors marges)
    double printableWidth() const { return width() - margins_.left - margins_.right; }
    double printableHeight() const { return height() - margins_.top - margins_.bottom; }

    std::string name() const {
        std::string n = paperName(format_);
        n += (orientation_ == Orientation::Portrait ? " Portrait" : " Paysage");
        return n;
    }

    bool isValid() const { return printableWidth() > 0 && printableHeight() > 0; }

private:
    PaperFormat format_;
    Orientation orientation_;
    Margins margins_;
};

} // namespace bcad::layout
