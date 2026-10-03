#pragma once
#include <string>
#include <string_view>

namespace bcad::cadastre {

// Percent-encoding minimal : encode '|' (séparateur de champ) et '%' (escape)
// pour éviter toute ambiguïté dans les chaînes CSV des entités cadastrales.

inline std::string encodeField(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '%') { out += "%25"; }
        else if (c == '|') { out += "%7C"; }
        else { out += c; }
    }
    return out;
}

inline std::string decodeField(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            const char h1 = s[i + 1], h2 = s[i + 2];
            if (h1 == '2' && h2 == '5') { out += '%'; i += 2; }
            else if (h1 == '7' && (h2 == 'C' || h2 == 'c')) { out += '|'; i += 2; }
            else { out += s[i]; }
        } else {
            out += s[i];
        }
    }
    return out;
}

} // namespace bcad::cadastre
