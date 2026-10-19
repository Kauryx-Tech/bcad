#include "bcad/geometry/Polyline.h"
#include <cmath>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace bcad::geom {
namespace {

// Decoupe en conservant les champs vides : un champ vide n'est pas une absence
// de donnee, c'est une donnee mal formee, et elle doit etre refusee.
std::vector<std::string_view> splitOn(std::string_view text, char separator) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    while (true) {
        const std::size_t at = text.find(separator, start);
        if (at == std::string_view::npos) {
            out.push_back(text.substr(start));
            return out;
        }
        out.push_back(text.substr(start, at - start));
        start = at + 1;
    }
}

// Un nombre, et rien d'autre. « 10;3 » doit echouer ici plutot que de rendre 10
// : lire un prefixe et jeter le reste est exactement la facon dont une grammaire
// en devore une autre en silence.
bool parseNumber(std::string_view text, double& value) {
    if (text.empty()) return false;
    std::size_t consumed = 0;
    double parsed = 0.0;
    try {
        parsed = std::stod(std::string(text), &consumed);
    } catch (const std::exception&) {
        return false;
    }
    if (consumed != text.size()) return false;
    if (std::isnan(parsed) || std::isinf(parsed)) return false;
    value = parsed;
    return true;
}

// Une suite complete de paires a partir d'un rang donne.
bool parsePairs(const std::vector<std::string_view>& fields,
                std::size_t from,
                std::vector<Point2>& out) {
    if (fields.size() < from || (fields.size() - from) % 2 != 0) return false;
    for (std::size_t i = from; i + 1 < fields.size(); i += 2) {
        double x = 0.0;
        double y = 0.0;
        if (!parseNumber(fields[i], x) || !parseNumber(fields[i + 1], y)) return false;
        out.emplace_back(x, y);
    }
    return true;
}

} // namespace

std::string PolylineEntity::encodeRings(const Rings& rings) {
    std::ostringstream ss;
    ss.precision(17);
    ss << (rings.closed ? 1 : 0);
    for (const auto& v : rings.outer) ss << ',' << v.x_ << ',' << v.y_;
    for (const auto& hole : rings.holes) {
        if (hole.empty()) continue;
        ss << ';';
        for (std::size_t i = 0; i < hole.size(); ++i) {
            if (i != 0) ss << ',';
            ss << hole[i].x_ << ',' << hole[i].y_;
        }
    }
    // Section « @ » des bulges (D-08) : en derniere position, et seulement si
    // elle porte quelque chose — un vecteur vide s'ecrit comme avant D-08.
    if (!rings.bulges.empty()) {
        ss << ";@";
        for (std::size_t i = 0; i < rings.bulges.size(); ++i) {
            if (i != 0) ss << ',';
            ss << rings.bulges[i];
        }
    }
    return ss.str();
}

bool PolylineEntity::decodeRings(std::string_view payload, Rings& out) {
    Rings parsed;
    const std::vector<std::string_view> sections = splitOn(payload, ';');

    const std::vector<std::string_view> outerFields = splitOn(sections.front(), ',');
    double flag = 0.0;
    if (outerFields.empty() || !parseNumber(outerFields.front(), flag)) return false;
    parsed.closed = flag != 0.0;
    if (!parsePairs(outerFields, 1, parsed.outer)) return false;

    for (std::size_t s = 1; s < sections.size(); ++s) {
        if (!sections[s].empty() && sections[s].front() == '@') {
            if (!parsed.bulges.empty()) return false; // section « @ » en double
            const std::string_view numbers = sections[s].substr(1);
            if (!numbers.empty()) {
                for (const std::string_view field : splitOn(numbers, ',')) {
                    double b = 0.0;
                    if (!parseNumber(field, b)) return false;
                    parsed.bulges.push_back(b);
                }
            }
            continue;
        }
        std::vector<Point2> hole;
        if (!parsePairs(splitOn(sections[s], ','), 0, hole)) return false;
        if (hole.empty()) return false;
        parsed.holes.push_back(std::move(hole));
    }
    // Plus de bulges que de sommets : la charge utile contredit sa propre
    // geometrie, et la moitie lire / la moitie ignorer est une facon de
    // corrompre un fichier sans le dire.
    if (parsed.bulges.size() > parsed.outer.size()) return false;

    out = std::move(parsed);
    return true;
}

BoundingBox PolylineEntity::boundingBox() const {
    BoundingBox bb;
    for (const auto& v : vertices_) bb.expand(v);
    // Un trou valide est intérieur à l'anneau extérieur, donc ses sommets
    // sont déjà dans la boîte. On les inclut quand même : un trou mal formé
    // qui dépasse l'anneau extérieur ne passerait pas inaperçu, et le coût
    // est négligeable.
    for (const auto& hole : holes_)
        for (const auto& v : hole) bb.expand(v);
    // Un segment arc peut sortir bien au-dela de ses deux extremites
    // (demi-cercle, arc majeur) : ses extrema alignes sur les axes entrent
    // dans la boite.
    for (std::size_t i = 0; i < segmentCount(); ++i)
        if (const std::optional<ArcEntity> arc = segmentArc(i)) bb.expand(arc->boundingBox());
    return bb;
}

void PolylineEntity::applyTransform(const Transform2D& t) {
    for (auto& v : vertices_) v = t.transform(v);
    // Les trous suivent l'anneau exterieur : une transformation qui les
    // laisserait en place decalerrait le vide du plein.
    for (auto& hole : holes_)
        for (auto& v : hole) v = t.transform(v);
    // Un miroir inverse le sens de l'arc : le bulge DXF porte ce sens dans
    // son signe. Les rotations et homotheties conservent le bulge ; une
    // deformation non uniforme changerait l'arc en ellipse, que BCAD ne
    // represente pas — le bulge est alors conserve tel quel.
    const auto& m = t.matrix();
    if (m.m00 * m.m11 - m.m01 * m.m10 < 0.0)
        for (double& b : bulges_) b = -b;
}

std::optional<ArcEntity> PolylineEntity::segmentArc(std::size_t i) const {
    if (i >= segmentCount()) return std::nullopt;
    const double b = bulge(i);
    if (b == 0.0) return std::nullopt;
    const std::size_t n = vertices_.size();
    const Point2 a = vertices_[i];
    const Point2 e = vertices_[(i + 1) % n];
    const double c = distance(a, e);
    if (c < Tolerance::kDegenerateLength) return std::nullopt;
    // Centre : milieu de la corde + deplacement normal regle par le bulge.
    // h = (1 - b^2) / (4b) fois la corde, selon la derive de
    // tan(theta/4) = b ; le rayon abs(c(1+b^2)/(4b)) est le rayon de
    // l'arc d'inclusion theta = 4 atan(b).
    const Point2 mid((a.x_ + e.x_) / 2.0, (a.y_ + e.y_) / 2.0);
    const double ux = (e.x_ - a.x_) / c, uy = (e.y_ - a.y_) / c;
    const double h = c * (1.0 - b * b) / (4.0 * b);
    const Point2 center(mid.x_ - uy * h, mid.y_ + ux * h);
    const double r = std::abs(c * (1.0 + b * b) / (4.0 * b));
    const double a0 = angleOf(center, a);
    const double a1 = angleOf(center, e);
    // b > 0 : l'arc va de a vers e en antihoraire (ArcEntity est
    // antihoraire de start vers end) ; b < 0 : le meme arc parcouru en
    // horaire de a vers e est l'antihoraire de e vers a.
    return b > 0.0 ? ArcEntity(center, r, a0, a1) : ArcEntity(center, r, a1, a0);
}

double PolylineEntity::bulgeBetween(const Point2& c, const Point2& a, const Point2& b, bool ccw) {
    double sweep = normalizeAngle(angleOf(c, b) - angleOf(c, a));
    if (!ccw) {
        if (sweep <= Tolerance::kAngular) return 0.0; // points confondus : aucun arc
        sweep -= 2.0 * std::numbers::pi;
    }
    return std::tan(sweep / 4.0);
}

std::vector<Point2> PolylineEntity::tessellate(double maxDeviation) const {
    std::vector<Point2> pts;
    const std::size_t n = vertices_.size();
    if (n == 0) return pts;
    pts.reserve(n + n / 4 + 4);
    pts.push_back(vertices_[0]);
    const std::size_t segs = segmentCount();
    for (std::size_t i = 0; i < segs; ++i) {
        if (const std::optional<ArcEntity> arc = segmentArc(i)) {
            const std::vector<Point2> part = arc->tessellate(maxDeviation);
            // part.front() est le debut du segment, deja pousse.
            pts.insert(pts.end(), part.begin() + 1, part.end());
        } else {
            pts.push_back(vertices_[(i + 1) % n]);
        }
    }
    if (closed_ && !pts.empty()) pts.push_back(pts.front());
    return pts;
}

double PolylineEntity::distanceTo(const Point2& p) const {
    double best = std::numeric_limits<double>::infinity();
    const std::size_t n = vertices_.size();
    if (n == 1) return distance(p, vertices_[0]);
    for (std::size_t i = 0; i < segmentCount(); ++i) {
        if (const std::optional<ArcEntity> arc = segmentArc(i)) {
            best = std::min(best, arc->distanceTo(p));
        } else {
            best = std::min(best, distance(p, closestPointOnSegment(p, vertices_[i], vertices_[(i + 1) % n])));
        }
    }
    // Les trous sont des anneaux toujours fermés, toujours droits : accrocher
    // un bord de trou est aussi utile que d'accrocher l'anneau extérieur.
    auto measureHole = [&](const std::vector<Point2>& ring) {
        const std::size_t rn = ring.size();
        if (rn == 0) return;
        if (rn == 1) { best = std::min(best, distance(p, ring[0])); return; }
        for (std::size_t i = 0; i < rn; ++i)
            best = std::min(best, distance(p, closestPointOnSegment(p, ring[i], ring[(i + 1) % rn])));
    };
    for (const auto& hole : holes_) measureHole(hole);
    return best;
}

double PolylineEntity::length() const {
    double total = 0.0;
    const std::size_t n = vertices_.size();
    if (n < 2) return 0.0;
    for (std::size_t i = 0; i < segmentCount(); ++i) {
        if (const std::optional<ArcEntity> arc = segmentArc(i)) total += arc->radius() * arc->sweep();
        else total += distance(vertices_[i], vertices_[(i + 1) % n]);
    }
    return total;
}

double PolylineEntity::signedArea() const {
    // Lacet referme sur l'anneau exterieur (l'ouvert est suppose ferme,
    // comme AIRE d'AutoCAD), puis correction circulaire de chaque segment
    // arc : aire entre corde et arc, signee par l'angle central — le meme
    // signe que le bulge. Les trous sont deduits en valeur absolue, comme
    // polygonArea (§0.7).
    double sum = 0.0;
    const std::size_t n = vertices_.size();
    for (std::size_t i = 0; i < n; ++i) {
        const Point2& a = vertices_[i];
        const Point2& b = vertices_[(i + 1) % n];
        sum += a.x_ * b.y_ - b.x_ * a.y_;
    }
    double area = sum / 2.0;
    for (std::size_t i = 0; i < segmentCount(); ++i) {
        const double b = bulge(i);
        if (b == 0.0) continue;
        const std::optional<ArcEntity> arc = segmentArc(i);
        if (!arc) continue;
        const double theta = 4.0 * std::atan(b);
        const double r = arc->radius();
        area += (r * r / 2.0) * (theta - std::sin(theta));
    }
    const double sign = area >= 0.0 ? 1.0 : -1.0;
    for (const auto& hole : holes_) {
        double h = 0.0;
        const std::size_t hn = hole.size();
        for (std::size_t i = 0; i < hn; ++i) {
            const Point2& a = hole[i];
            const Point2& b = hole[(i + 1) % hn];
            h += a.x_ * b.y_ - b.x_ * a.y_;
        }
        area -= sign * std::abs(h / 2.0);
    }
    return area;
}

std::string PolylineEntity::geometryInfo() const {
    std::ostringstream ss;
    ss.precision(3);
    ss << "Polyligne (" << (closed_ ? "fermée" : "ouverte") << ")\nSommets : " << vertices_.size()
       << "\nLongueur : " << length();
    if (closed_) ss << "\nSurface : " << std::abs(signedArea());
    return ss.str();
}

void PolylineEntity::doAddSnapCandidates(const Point2& cursor, SnapCallback add) const {
    auto midpoint = [](const Point2& a, const Point2& b) {
        return Point2((a.x_ + b.x_) / 2.0, (a.y_ + b.y_) / 2.0);
    };
    const std::size_t n = vertices_.size();
    for (std::size_t i = 0; i < n; ++i) add(vertices_[i], SnapPointType::Endpoint);
    for (std::size_t i = 0; i < segmentCount(); ++i) {
        if (const std::optional<ArcEntity> arc = segmentArc(i)) {
            // Milieu de l'arc et quadrants tombant dans son balayage, comme
            // les snaps d'un arc isole — un segment arc d'une polyligne offre
            // les memes points qu'une entite Arc.
            const double am = arc->startAngle() + arc->sweep() / 2.0;
            add(Point2(arc->center().x_ + arc->radius() * std::cos(am),
                       arc->center().y_ + arc->radius() * std::sin(am)),
                SnapPointType::Midpoint);
            for (double ang : {0.0, std::numbers::pi / 2.0, std::numbers::pi, 3.0 * std::numbers::pi / 2.0}) {
                if (normalizeAngle(ang - arc->startAngle()) <= arc->sweep() + Tolerance::kAngular) {
                    add(Point2(arc->center().x_ + arc->radius() * std::cos(ang),
                               arc->center().y_ + arc->radius() * std::sin(ang)),
                        SnapPointType::Quadrant);
                }
            }
        } else {
            add(midpoint(vertices_[i], vertices_[(i + 1) % n]), SnapPointType::Midpoint);
        }
    }
}

} // namespace bcad::geom