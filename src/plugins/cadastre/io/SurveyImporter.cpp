#include "SurveyImporter.h"

#include "entities/SurveyMarkEntity.h"

#include "bcad/core/Document.h"
#include "bcad/properties/PropertyMap.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>

namespace bcad::cadastre {

namespace {

std::string trim(std::string_view text) {
    size_t a = 0, b = text.size();
    while (a < b && std::isspace(static_cast<unsigned char>(text[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(text[b - 1]))) --b;
    return std::string(text.substr(a, b - a));
}

std::vector<std::string> splitLine(const std::string& line, char& separator) {
    if (line.find(';') != std::string::npos) separator = ';';
    else if (line.find('\t') != std::string::npos) separator = '\t';
    else if (line.find(',') != std::string::npos) separator = ',';
    else separator = ' ';
    std::vector<std::string> fields;
    if (separator == ' ') {
        std::istringstream words(line);
        for (std::string word; words >> word;) fields.push_back(word);
    } else {
        std::string current;
        for (char c : line) {
            if (c == separator) { fields.push_back(trim(current)); current.clear(); }
            else current += c;
        }
        fields.push_back(trim(current));
    }
    return fields;
}

std::optional<double> readNumber(std::string text, bool commaIsDecimal) {
    if (commaIsDecimal) std::replace(text.begin(), text.end(), ',', '.');
    if (text.empty()) return std::nullopt;
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end != text.c_str() + text.size() || !std::isfinite(value)) return std::nullopt;
    return value;
}

int markTypeFor(const std::string& code) {
    std::string upper;
    for (char c : code) upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (upper == "REP" || upper == "REPERE" || upper == "REPÈRE") return 1;
    if (upper == "PI") return 2;
    if (upper == "ST" || upper == "STATION") return 3;
    return 0;   // borne
}

} // namespace

SurveyParse parseSurveyText(std::string_view text) {
    SurveyParse result;
    if (text.size() >= 3 && text.substr(0, 3) == "\xEF\xBB\xBF") text.remove_prefix(3);
    std::istringstream lines{std::string(text)};
    int number = 0;
    bool headerAllowed = true;
    for (std::string raw; std::getline(lines, raw);) {
        ++number;
        const std::string line = trim(raw);
        if (line.empty() || line.rfind("#", 0) == 0 || line.rfind("//", 0) == 0) continue;
        char separator = ' ';
        const auto fields = splitLine(line, separator);
        const bool commaIsDecimal = separator != ',';
        const auto x = fields.size() > 1 ? readNumber(fields[1], commaIsDecimal) : std::nullopt;
        const auto y = fields.size() > 2 ? readNumber(fields[2], commaIsDecimal) : std::nullopt;
        if (!x || !y) {
            // Une premiere ligne non numerique est un en-tete (« Matricule;X;Y »).
            if (headerAllowed) { headerAllowed = false; continue; }
            result.rejects.push_back("ligne " + std::to_string(number) + " : " +
                                     (fields.size() < 3 ? std::string("moins de trois champs")
                                                        : std::string("X ou Y illisible")) +
                                     " — « " + line.substr(0, 60) + " »");
            continue;
        }
        headerAllowed = false;
        SurveyPoint point;
        point.name = fields[0];
        point.x = *x;
        point.y = *y;
        size_t next = 3;
        if (fields.size() > 3) {
            if (const auto z = readNumber(fields[3], commaIsDecimal)) {
                point.z = z;
                next = 4;
            }
        }
        for (size_t i = next; i < fields.size(); ++i) {
            if (!point.code.empty()) point.code += ' ';
            point.code += fields[i];
        }
        result.points.push_back(std::move(point));
    }
    return result;
}

bool SurveyImporter::readDocument(core::Document& document, const std::string& path,
                                  std::string* error) const {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error) *error = "lecture impossible : " + path;
        return false;
    }
    std::ostringstream content;
    content << file.rdbuf();
    const SurveyParse parse = parseSurveyText(content.str());
    if (parse.points.empty()) {
        if (error) {
            *error = "aucun point lisible dans " + path;
            if (!parse.rejects.empty()) *error += " (" + parse.rejects.front() + ")";
        }
        return false;
    }
    for (const auto& point : parse.points) {
        auto mark = std::make_unique<SurveyMarkEntity>(geom::Point2{point.x, point.y});
        auto& props = mark->properties();
        props.setString("cadastre.reference", point.name);
        props.setString("cadastre.code", point.code);
        props.setEnum("cadastre.mark_type", markTypeFor(point.code));
        if (point.z) props.setDouble("cadastre.altitude", *point.z);
        document.addEntity(std::move(mark));
    }
    if (!parse.rejects.empty() && error) {
        *error = std::to_string(parse.points.size()) + " point(s) importé(s), " +
                 std::to_string(parse.rejects.size()) + " ligne(s) écartée(s) : ";
        const size_t shown = std::min<size_t>(parse.rejects.size(), 3);
        for (size_t i = 0; i < shown; ++i) *error += (i ? " ; " : "") + parse.rejects[i];
        if (parse.rejects.size() > shown) *error += " ; …";
    }
    return true;
}

} // namespace bcad::cadastre
