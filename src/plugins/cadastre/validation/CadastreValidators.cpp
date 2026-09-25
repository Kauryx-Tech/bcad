#include "CadastreValidators.h"

#include "ParcelIdentifierValidator.h"
#include "ParcelOverlapValidator.h"
#include "../entities/ParcelEntity.h"

#include "bcad/geometry/Polyline.h"
#include "bcad/properties/PropertyMap.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace bcad::cadastre {

namespace {

const geom::PolylineEntity* asParcel(const geom::Entity* entity) {
    if (!isCadastreParcel(entity)) return nullptr;
    return static_cast<const geom::PolylineEntity*>(entity);
}

// Designe la parcelle dans un libelle : section/numero si elle est identifiee,
// sinon son identifiant de document.
std::string describe(const geom::Entity& entity) {
    const auto& props = entity.properties();
    const std::string section = props.getString("cadastre.section");
    const std::string numero = props.getString("cadastre.numero");
    if (!section.empty() || !numero.empty()) {
        return "'" + section + " " + numero + "'";
    }
    return "id " + std::to_string(entity.id());
}

double ringArea(const std::vector<geom::Point2>& v) {
    double total = 0.0;
    for (std::size_t i = 0; i < v.size(); ++i) {
        const auto& p1 = v[i];
        const auto& p2 = v[(i + 1) % v.size()];
        total += p1.x_ * p2.y_ - p2.x_ * p1.y_;
    }
    return std::abs(total * 0.5);
}

bool segmentsIntersect(geom::Point2 a, geom::Point2 b, geom::Point2 c, geom::Point2 d) {
    auto cross = [](geom::Point2 o, geom::Point2 p, geom::Point2 q) {
        return (p.x_ - o.x_) * (q.y_ - o.y_) - (p.y_ - o.y_) * (q.x_ - o.x_);
    };
    auto onSegment = [](geom::Point2 p, geom::Point2 q, geom::Point2 r) {
        return q.x_ <= std::max(p.x_, r.x_) + 1e-9 && q.x_ >= std::min(p.x_, r.x_) - 1e-9 &&
               q.y_ <= std::max(p.y_, r.y_) + 1e-9 && q.y_ >= std::min(p.y_, r.y_) - 1e-9;
    };
    auto sign = [](double value) { return value > 1e-9 ? 1 : (value < -1e-9 ? -1 : 0); };
    const int s1 = sign(cross(c, d, a));
    const int s2 = sign(cross(c, d, b));
    const int s3 = sign(cross(a, b, c));
    const int s4 = sign(cross(a, b, d));
    if (s1 * s2 < 0 && s3 * s4 < 0) return true;
    if (s1 == 0 && onSegment(c, a, d)) return true;
    if (s2 == 0 && onSegment(c, b, d)) return true;
    if (s3 == 0 && onSegment(a, c, b)) return true;
    if (s4 == 0 && onSegment(a, d, b)) return true;
    return false;
}

// Le contour est simple si deux segments non consecutifs ne se rencontrent pas.
bool isClosedRingSimple(const std::vector<geom::Point2>& v) {
    const std::size_t n = v.size();
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 2; j < n; ++j) {
            if (i == 0 && j + 1 == n) continue;  // segments consecutifs de part et d'autre de la fermeture
            if (segmentsIntersect(v[i], v[(i + 1) % n], v[j], v[(j + 1) % n])) return false;
        }
    }
    return true;
}

} // namespace

std::vector<std::string> parcelApplicableTypes() {
    // Le filtre de l'hote est une simple egalite de TypeId : on accepte les deux
    // formes que prend une parcelle, et on tranche ici avec isCadastreParcel().
    return {TypeId_Parcel.value, geom::TypeId_Polyline.value};
}

std::string ParcelTopologyValidator::id() const { return "cadastre.topologie"; }
std::string ParcelTopologyValidator::label() const { return "Topologie des parcelles"; }
std::vector<std::string> ParcelTopologyValidator::applicableTypes() const {
    return parcelApplicableTypes();
}

std::vector<validation::Diagnostic> ParcelTopologyValidator::validate(
    const std::vector<geom::Entity*>& entities) const {
    std::vector<validation::Diagnostic> out;
    for (const auto* entity : entities) {
        const auto* parcel = asParcel(entity);
        if (!parcel) continue;
        const auto vertices = parcel->vertices();
        const std::vector<int> ids{entity->id()};
        if (vertices.size() < 3) {
            out.push_back({validation::Severity::Error,
                           "Parcelle " + describe(*entity) + " : au moins 3 sommets requis", ids});
            continue;
        }
        // Un contour qui se croise a une aire algébrique nulle : rendre les deux
        // diagnostics répéterait la même cause. La perte de simplicité prime, et
        // l'aire nulle ne qualifie que les contours simples (sommets alignés).
        if (!isClosedRingSimple(vertices)) {
            out.push_back({validation::Severity::Error,
                           "Parcelle " + describe(*entity) + " : polygone auto-intersectant", ids});
        } else if (ringArea(vertices) < 1e-9) {
            out.push_back({validation::Severity::Error,
                           "Parcelle " + describe(*entity) + " : aire nulle (sommets colinéaires)", ids});
        }
    }
    return out;
}

std::string ParcelOverlapRuleValidator::id() const { return "cadastre.recouvrement"; }
std::string ParcelOverlapRuleValidator::label() const { return "Emprises communes"; }
std::vector<std::string> ParcelOverlapRuleValidator::applicableTypes() const {
    return parcelApplicableTypes();
}

std::vector<validation::Diagnostic> ParcelOverlapRuleValidator::validate(
    const std::vector<geom::Entity*>& entities) const {
    std::vector<validation::Diagnostic> out;
    ParcelOverlapValidator overlap;
    for (std::size_t i = 0; i < entities.size(); ++i) {
        const auto* first = asParcel(entities[i]);
        if (!first) continue;
        for (std::size_t j = i + 1; j < entities.size(); ++j) {
            const auto* second = asParcel(entities[j]);
            if (!second) continue;
            const auto result = overlap.check(*first, *second);
            if (!result.overlap) continue;
            out.push_back({validation::Severity::Error,
                           "Parcelles " + describe(*first) + " et " + describe(*second) +
                               " : " + result.details,
                           {first->id(), second->id()}});
        }
    }
    return out;
}

ParcelIdentifierRuleValidator::ParcelIdentifierRuleValidator(std::string sectionPattern,
                                                             std::string numberPattern)
    : identifier_(std::move(sectionPattern), std::move(numberPattern)) {}

std::string ParcelIdentifierRuleValidator::id() const { return "cadastre.identification"; }
std::string ParcelIdentifierRuleValidator::label() const { return "Identification des parcelles"; }
std::vector<std::string> ParcelIdentifierRuleValidator::applicableTypes() const {
    return parcelApplicableTypes();
}

std::vector<validation::Diagnostic> ParcelIdentifierRuleValidator::validate(
    const std::vector<geom::Entity*>& entities) const {
    std::vector<validation::Diagnostic> out;
    std::map<std::string, std::vector<const geom::Entity*>> byIdentifier;

    for (const auto* entity : entities) {
        if (!asParcel(entity)) continue;
        const auto& props = entity->properties();
        const std::string section = props.getString("cadastre.section");
        const std::string numero = props.getString("cadastre.numero");
        const std::vector<int> ids{entity->id()};

        if (section.empty() && numero.empty()) {
            out.push_back({validation::Severity::Warning,
                           "Parcelle " + describe(*entity) + " : section et numéro non renseignés", ids});
            continue;
        }
        const auto result = identifier_.validate(section, numero);
        if (!result.valid) {
            out.push_back({validation::Severity::Error,
                           "Parcelle " + describe(*entity) + " : " + result.error, ids});
            continue;
        }
        byIdentifier[section + " " + numero].push_back(entity);
    }

    // Deux parcelles ne peuvent pas porter la meme identification.
    for (const auto& [key, owners] : byIdentifier) {
        if (owners.size() < 2) continue;
        std::vector<int> ids;
        for (const auto* entity : owners) ids.push_back(entity->id());
        out.push_back({validation::Severity::Error,
                       "Identification '" + key + "' portée par " + std::to_string(owners.size()) +
                           " parcelles",
                       ids});
    }
    return out;
}

} // namespace bcad::cadastre
