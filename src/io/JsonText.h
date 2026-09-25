#pragma once

// Ecriture et lecture du JSON minimal partage par les ecrivains de src/io
// (GeoJSON, et le `value_json` des proprietes). Interne a src/io : ce fichier
// n'est ni installe ni expose, et n'a pas vocation a devenir un parseur JSON
// general.

#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace bcad::io {

inline std::string jsonEscape(const std::string& text) {
    std::ostringstream out;
    for (const unsigned char c : text) {
        switch (c) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 0x20)
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << c
                    << std::dec << std::setfill(' ') << std::setw(0);
            else
                out << static_cast<char>(c);
        }
    }
    return out.str();
}

inline std::string jsonString(const std::string& text) {
    return "\"" + jsonEscape(text) + "\"";
}

inline std::string jsonNumber(double value) {
    std::ostringstream out;
    out << std::setprecision(17) << value;
    return out.str();
}

inline bool parseJsonNumber(std::string_view text, double& out) {
    std::string token;
    for (const char c : text) {
        if (!std::isspace(static_cast<unsigned char>(c))) token += c;
    }
    if (token.empty()) return false;
    std::size_t consumed = 0;
    try {
        out = std::stod(token, &consumed);
    } catch (const std::exception&) {
        return false;
    }
    return consumed == token.size();
}

// Inverse de jsonEscape pour la seule grammaire emise ci-dessus. Une sequence
// inconnue est rendue telle quelle : un fichier abime ne doit pas inventer un
// caractere.
inline std::string jsonUnescape(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c != '\\' || i + 1 >= text.size()) {
            out += c;
            continue;
        }
        const char next = text[++i];
        switch (next) {
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        default: out += c; out += next; break;
        }
    }
    return out;
}

} // namespace bcad::io
