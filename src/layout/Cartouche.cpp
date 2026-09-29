// Sérialisation des signatures du cartouche.
//
// Les signatures voyagent dans le même sac de paires clé/valeur que le reste du
// cartouche : un document n'a pas de table pour elles. Elles sont donc aplaties
// en une seule valeur JSON sous la clé SIGNATURES.
//
// Le format est volontairement strict — pas de tolerant, pas de sous-ensemble :
// une valeur qu'on ne sait pas relire vaut mieux qu'une valeur mal relue, et le
// lecteur jette ce qu'il ne comprend pas au lieu de deviner. L'échappement est
// fait à la main, et il est complet : un nom de signataire peut contenir un
// guillemet, une barre inverse ou un accent (les accents ne sont pas échappés en
// JSON, ils sont émis tels quels en UTF-8).

#include "bcad/layout/Cartouche.h"

#include <cstddef>

namespace bcad::layout {

namespace {

// Valeur rendue par hexToByte quand les deux caractères ne sont pas un octet
// hexadécimal — et jamais un code 0, qui ne produirait qu'un octet nul.
constexpr unsigned kNoByte = 0x100;

// Quatre caractères hexadécimal tenant sur un octet, ou kNoByte. std::stoul
// leverait ici sur une séquence abîmée, et l'exception remonterait jusqu'au
// lecteur de fichier.
unsigned hexToByte(const std::string& hex) {
    if (hex.size() != 4) return kNoByte;
    unsigned code = 0;
    for (const char c : hex) {
        code <<= 4;
        if (c >= '0' && c <= '9') code |= static_cast<unsigned>(c - '0');
        else if (c >= 'a' && c <= 'f') code |= static_cast<unsigned>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') code |= static_cast<unsigned>(c - 'A' + 10);
        else return kNoByte;
    }
    // Au-delà d'un octet, ce n'est pas le format qu'on a écrit : ne pas deviner.
    return (code == 0 || code >= 0x100) ? kNoByte : code;
}

// Échappe une chaîne pour un littéral JSON, guillemets compris.
std::string escapeJson(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (const char c : value) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    static const char* hex = "0123456789abcdef";
                    out += "\\u00";
                    out += hex[(static_cast<unsigned char>(c) >> 4) & 0xF];
                    out += hex[static_cast<unsigned char>(c) & 0xF];
                } else {
                    out += c;
                }
        }
    }
    return out;
}

std::string unescapeJson(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '\\' || i + 1 >= value.size()) {
            out += value[i];
            continue;
        }
        switch (value[++i]) {
            case '"':  out += '"';  break;
            case '\\': out += '\\'; break;
            case 'b':  out += '\b'; break;
            case 'f':  out += '\f'; break;
            case 'n':  out += '\n'; break;
            case 'r':  out += '\r'; break;
            case 't':  out += '\t'; break;
            case 'u': {
                // \u00XX : quatre chiffres, dont le code tient sur un octet. Tout
                // autre \u est laissé tel quel plutôt que deviné — et sans lever :
                // une valeur illisible se remplace, elle ne doit pas faire
                // échouer la lecture entière.
                if (i + 4 < value.size()) {
                    const unsigned code = hexToByte(value.substr(i + 1, 4));
                    if (code != kNoByte) {
                        out += static_cast<char>(code);
                        i += 4;
                        break;
                    }
                }
                out += "\\u";
                break;
            }
            default:
                out += '\\';
                out += value[i];
        }
    }
    return out;
}

// Avance jusqu'au caractère `c` en ignorant ceux échappés par un antislash.
// Renvoie npos si `c` n'apparaît pas.
std::size_t findUnescaped(const std::string& s, char c, std::size_t from) {
    for (std::size_t i = from; i < s.size(); ++i) {
        if (s[i] == '\\') {
            ++i;
            continue;
        }
        if (s[i] == c) return i;
    }
    return std::string::npos;
}

// Lit un littéral JSON. `pos` doit pointer sur le guillemet ouvrant.
bool readJsonString(const std::string& s, std::size_t& pos, std::string& out) {
    if (pos >= s.size() || s[pos] != '"') return false;
    ++pos;
    const std::size_t end = findUnescaped(s, '"', pos);
    if (end == std::string::npos) return false;
    out = unescapeJson(s.substr(pos, end - pos));
    pos = end + 1;
    return true;
}

// Ignore les espaces.
void skipSpaces(const std::string& s, std::size_t& pos) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' ||
                              s[pos] == '\n' || s[pos] == '\r')) {
        ++pos;
    }
}

}  // namespace

std::string Cartouche::serializeSignatures(const std::vector<Signature>& signatures) {
    if (signatures.empty()) return {};

    std::string out = "[";
    for (std::size_t i = 0; i < signatures.size(); ++i) {
        if (i > 0) out += ",";
        out += "{\"nom\":\"" + escapeJson(signatures[i].nom);
        out += "\",\"role\":\"" + escapeJson(signatures[i].role);
        out += "\",\"date\":\"" + escapeJson(signatures[i].date);
        out += "\",\"signaturePath\":\"" + escapeJson(signatures[i].signaturePath);
        out += "\"}";
    }
    out += "]";
    return out;
}

std::vector<Cartouche::Signature> Cartouche::deserializeSignatures(const std::string& json) {
    std::vector<Signature> out;
    if (json.empty()) return out;

    std::size_t pos = 0;
    skipSpaces(json, pos);
    if (pos >= json.size() || json[pos] != '[') return out;
    ++pos;

    while (true) {
        skipSpaces(json, pos);
        if (pos >= json.size()) return out;
        if (json[pos] == ']') return out;
        if (json[pos] == ',') {
            ++pos;
            continue;
        }
        if (json[pos] != '{') return out;
        ++pos;

        Signature sig;
        while (true) {
            skipSpaces(json, pos);
            if (pos >= json.size()) return out;
            if (json[pos] == '}') {
                ++pos;
                break;
            }
            if (json[pos] == ',') {
                ++pos;
                continue;
            }

            std::string key;
            if (!readJsonString(json, pos, key)) return out;
            skipSpaces(json, pos);
            if (pos >= json.size() || json[pos] != ':') return out;
            ++pos;
            skipSpaces(json, pos);

            if (key == "nom") {
                if (!readJsonString(json, pos, sig.nom)) return out;
            } else if (key == "role") {
                if (!readJsonString(json, pos, sig.role)) return out;
            } else if (key == "date") {
                if (!readJsonString(json, pos, sig.date)) return out;
            } else if (key == "signaturePath") {
                if (!readJsonString(json, pos, sig.signaturePath)) return out;
            } else {
                // Clé inconnue : on saute sa valeur sans l'interpréter. Le
                // format peut gagner un champ sans rendre les vieux fichiers
                // illisibles, dans les deux sens.
                if (pos < json.size() && json[pos] == '"') {
                    std::string ignored;
                    if (!readJsonString(json, pos, ignored)) return out;
                } else if (pos < json.size() && (json[pos] == '{' || json[pos] == '[')) {
                    const char open = json[pos];
                    const char close = open == '{' ? '}' : ']';
                    const std::size_t end = findUnescaped(json, close, pos + 1);
                    if (end == std::string::npos) return out;
                    pos = end + 1;
                } else {
                    return out;
                }
            }
        }

        // Une signature sans nom n'identifie personne : elle ne mérite pas de
        // ligne dans le tableau.
        if (!sig.nom.empty()) out.push_back(std::move(sig));
    }
}

}  // namespace bcad::layout
