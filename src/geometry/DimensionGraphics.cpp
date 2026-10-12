// Dessin d'une cotation (A-01). Proportions de la norme ISO 129 reprises par
// le style ISO-25 d'AutoCAD, toutes rapportees a la hauteur de texte h :
// fleche h de long et h/3 de large, ecart a l'objet h/4, depassement des
// lignes d'attache h/2, texte pose h/4 au-dessus de la ligne de cote.

#include "bcad/geometry/DimensionGraphics.h"

#include "bcad/geometry/AlignedDimensionEntity.h"
#include "bcad/geometry/AngularDimensionEntity.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/LinearDimensionEntity.h"
#include "bcad/geometry/RadialDimensionEntity.h"
#include "bcad/layout/DimensionStyleRegistry.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace bcad::geom {

namespace {

constexpr double kPi = std::numbers::pi;

struct Dir {
    double x = 1.0, y = 0.0;
};

Dir directionOf(double dx, double dy) {
    const double len = std::hypot(dx, dy);
    if (len < 1e-12) return {};
    return {dx / len, dy / len};
}

Point2 offset(const Point2& p, const Dir& d, double distance) {
    return {p.x_ + d.x * distance, p.y_ + d.y * distance};
}

// Fleche dessinee comme un trace continu (le peintre ne remplit pas) : pointe
// et deux barbes, refermees pour une fleche pleine, ouvertes pour `Open*`,
// remplacees par un petit losange pour `Dot*`, absentes pour `None`.
// `towardTip` va du corps de la fleche vers sa pointe.
void appendArrow(std::vector<Point2>& path, const Point2& tip, const Dir& towardTip, double size,
                 layout::ArrowheadType type) {
    if (type == layout::ArrowheadType::None || size <= 0.0) return;
    const Dir side{-towardTip.y, towardTip.x};
    const Point2 base = offset(tip, towardTip, -size);
    const double w = size / 6.0;                 // demi-largeur (fleche fermee ISO)
    const Point2 left = offset(base, side, w);
    const Point2 right = offset(base, side, -w);

    switch (type) {
        case layout::ArrowheadType::Dot:
        case layout::ArrowheadType::DotSmall: {
            // Petit losange centre sur la pointe (le trace ne remplit pas).
            const double r = size * (type == layout::ArrowheadType::DotSmall ? 0.125 : 0.25);
            const Point2 a = offset(tip, side, r);
            const Point2 b = offset(tip, towardTip, r);
            const Point2 c = offset(tip, side, -r);
            const Point2 d = offset(tip, towardTip, -r);
            for (const Point2& p : {a, b, c, d, a}) path.push_back(p);
            return;
        }
        case layout::ArrowheadType::Open:
        case layout::ArrowheadType::Open90:
        case layout::ArrowheadType::Open30: {
            // Deux barbes reliees a la pointe, sans base : un « V ».
            double half = w;
            if (type == layout::ArrowheadType::Open90) half = size;                  // 2 x 45 degres
            else if (type == layout::ArrowheadType::Open30) half = size * std::tan(kPi / 12.0);  // 2 x 15 degres
            path.push_back(tip);
            path.push_back(offset(base, side, half));
            path.push_back(tip);
            path.push_back(offset(base, side, -half));
            return;
        }
        default:
            break;
    }

    // Fleches fermees (pleines ou vides, grandes ou petites) : meme contour.
    path.push_back(tip);
    path.push_back(left);
    path.push_back(right);
    path.push_back(tip);
}

// Ligne d'attache de l'objet mesure `from` vers le pied `foot` sur la ligne de
// cote : ecart a l'objet, depassement au-dela de la ligne de cote.
void appendExtension(std::vector<Point2>& path, const Point2& from, const Point2& foot, double h,
                     double gap, double extend, bool startAtFoot) {
    const double len = distance(from, foot);
    if (len <= h * 0.25) {
        path.push_back(foot);
        return;
    }
    const Dir d = directionOf(foot.x_ - from.x_, foot.y_ - from.y_);
    const Point2 start = offset(from, d, gap);
    const Point2 end = offset(foot, d, extend);
    if (startAtFoot) {
        path.push_back(foot);
        path.push_back(end);
        path.push_back(start);
    } else {
        path.push_back(start);
        path.push_back(end);
        path.push_back(foot);
    }
}

DimensionLabel labelAlong(const Point2& a, const Point2& b, double h, double textGap,
                          std::string text) {
    DimensionLabel label;
    label.angle = readableAngle(std::atan2(b.y_ - a.y_, b.x_ - a.x_));
    label.height = h;
    label.text = std::move(text);
    const Point2 mid{(a.x_ + b.x_) * 0.5, (a.y_ + b.y_) * 0.5};
    label.position = offset(mid, Dir{-std::sin(label.angle), std::cos(label.angle)}, textGap);
    return label;
}

// Mesures du dessin d'une cotation, en unites du dessin. Le style (registre,
// par nom) donne les ecarts pour sa hauteur de texte ; la hauteur propre a la
// cotation (`dimension.text_height`) les met a l'echelle.
struct Metrics {
    double h;        // hauteur du texte
    double arrow;    // longueur des fleches
    double gap;      // ecart ligne d'attache / objet mesure
    double extend;   // depassement de la ligne d'attache au-dela de la cote
    double textGap;  // ecart texte / ligne de cote
    layout::ArrowheadType arrowhead1;
    layout::ArrowheadType arrowhead2;
};

Metrics metricsOf(const DimensionEntity& dimension) {
    const layout::DimensionStyle& style =
        layout::DimensionStyleRegistry::instance().getOrDefault(dimension.styleName());
    const double h = dimensionTextHeight(dimension);
    const double k = std::isfinite(style.textHeight) && style.textHeight > 0.0 ? h / style.textHeight : 1.0;
    return Metrics{h, h * style.arrowSize, style.extLineOffset * k, style.extLineExtend * k,
                   style.textOffset * k, style.arrowhead1, style.arrowhead2};
}

// Cotation lineaire ou alignee : deux points mesures, leurs pieds sur la
// ligne de cote.
DimensionGraphics straightDimension(const Point2& p1, const Point2& p2, const Point2& d1,
                                    const Point2& d2, const Metrics& m, std::string text) {
    DimensionGraphics g;
    appendExtension(g.path, p1, d1, m.h, m.gap, m.extend, false);
    const Dir outward1 = directionOf(d1.x_ - d2.x_, d1.y_ - d2.y_);
    appendArrow(g.path, d1, outward1, m.arrow, m.arrowhead1);
    appendArrow(g.path, d2, Dir{-outward1.x, -outward1.y}, m.arrow, m.arrowhead2);
    appendExtension(g.path, p2, d2, m.h, m.gap, m.extend, true);
    g.label = labelAlong(d1, d2, m.h, m.textGap, std::move(text));
    return g;
}

DimensionGraphics linearGraphics(const LinearDimensionEntity& dim) {
    const Metrics m = metricsOf(dim);
    const Dir u{std::cos(dim.rotation()), std::sin(dim.rotation())};
    const Point2 loc = dim.dimLineLoc();
    auto foot = [&](const Point2& p) {
        const double t = (p.x_ - loc.x_) * u.x + (p.y_ - loc.y_) * u.y;
        return offset(loc, u, t);
    };
    return straightDimension(dim.defPt1(), dim.defPt2(), foot(dim.defPt1()), foot(dim.defPt2()), m,
                             dim.dimensionText());
}

DimensionGraphics alignedGraphics(const AlignedDimensionEntity& dim) {
    const Metrics m = metricsOf(dim);
    const Point2 p1 = dim.defPt1();
    const Point2 p2 = dim.defPt2();
    const Dir u = directionOf(p2.x_ - p1.x_, p2.y_ - p1.y_);
    const Dir n{-u.y, u.x};
    const double s = (dim.dimLineLoc().x_ - p1.x_) * n.x + (dim.dimLineLoc().y_ - p1.y_) * n.y;
    return straightDimension(p1, p2, offset(p1, n, s), offset(p2, n, s), m,
                             dim.dimensionText());
}

DimensionGraphics angularGraphics(const AngularDimensionEntity& dim) {
    const Metrics m = metricsOf(dim);
    const Point2 v = dim.vertex();
    const double a1 = std::atan2(dim.start().y_ - v.y_, dim.start().x_ - v.x_);
    const double a2 = std::atan2(dim.end().y_ - v.y_, dim.end().x_ - v.x_);
    // Le petit angle, comme measuredValue().
    const double sweep = std::remainder(a2 - a1, 2.0 * kPi);
    if (!std::isfinite(sweep)) return {};
    double radius = distance(v, dim.dimLineLoc());
    if (radius < 1e-9) radius = std::max(distance(v, dim.start()), m.h * 4.0);

    auto onArc = [&](double angle) {
        return Point2{v.x_ + std::cos(angle) * radius, v.y_ + std::sin(angle) * radius};
    };
    const double turn = sweep >= 0.0 ? 1.0 : -1.0;
    auto tangent = [&](double angle) {   // sens de parcours de l'arc
        return Dir{-std::sin(angle) * turn, std::cos(angle) * turn};
    };

    DimensionGraphics g;
    const Point2 arcStart = onArc(a1);
    const Point2 arcEnd = onArc(a1 + sweep);
    appendExtension(g.path, dim.start(), arcStart, m.h, m.gap, m.extend, false);
    const Dir t1 = tangent(a1);
    appendArrow(g.path, arcStart, Dir{-t1.x, -t1.y}, m.arrow, m.arrowhead1);
    const int segments = std::max(4, static_cast<int>(std::ceil(std::abs(sweep) / (kPi / 32.0))));
    for (int i = 1; i < segments; ++i) g.path.push_back(onArc(a1 + sweep * i / segments));
    appendArrow(g.path, arcEnd, tangent(a1 + sweep), m.arrow, m.arrowhead2);
    appendExtension(g.path, dim.end(), arcEnd, m.h, m.gap, m.extend, true);

    const double middle = a1 + sweep * 0.5;
    DimensionLabel label;
    label.angle = readableAngle(middle + kPi / 2.0);
    label.height = m.h;
    label.text = dim.dimensionText();
    const Point2 onMiddle = onArc(middle);
    label.position = offset(onMiddle, Dir{-std::sin(label.angle), std::cos(label.angle)}, m.textGap);
    g.label = std::move(label);
    return g;
}

DimensionGraphics radialGraphics(const RadialDimensionEntity& dim) {
    const Metrics m = metricsOf(dim);
    const Point2 c = dim.center();
    const Point2 edge = dim.chordPoint();
    const Dir out = directionOf(edge.x_ - c.x_, edge.y_ - c.y_);
    DimensionGraphics g;
    Point2 from = c;
    if (dim.radialType() == RadialDimensionEntity::RadialType::Diameter) {
        from = Point2{2.0 * c.x_ - edge.x_, 2.0 * c.y_ - edge.y_};
        appendArrow(g.path, from, Dir{-out.x, -out.y}, m.arrow, m.arrowhead1);
    } else {
        g.path.push_back(c);
    }
    appendArrow(g.path, edge, out, m.arrow, m.arrowhead2);
    g.label = labelAlong(from, edge, m.h, m.textGap, dim.dimensionText());
    return g;
}

} // namespace

DimensionGraphics buildGraphics(const DimensionEntity& dimension);

double readableAngle(double radians) {
    double a = std::remainder(radians, 2.0 * kPi);   // ]-pi, pi]
    if (a > kPi / 2.0 + 1e-9) a -= kPi;
    else if (a <= -kPi / 2.0 + 1e-9) a += kPi;
    return a;
}

double dimensionTextHeight(const DimensionEntity& dimension) {
    // La hauteur propre a la cotation prime ; a defaut, celle de son style.
    const layout::DimensionStyle& style =
        layout::DimensionStyleRegistry::instance().getOrDefault(dimension.styleName());
    const double h = dimension.properties().getDouble(kDimensionTextHeightProperty, style.textHeight);
    // Bornee : une hauteur absurde lue d'un fichier ferait deborder l'emprise,
    // le cadrage et la grille.
    if (!std::isfinite(h) || h <= 0.0) return kDefaultDimensionTextHeight;
    return std::clamp(h, kMinDimensionTextHeight, kMaxDimensionTextHeight);
}

BoundingBox dimensionBounds(const DimensionEntity& dimension) {
    const DimensionGraphics g = dimensionGraphics(dimension);
    BoundingBox bb;
    for (const auto& p : g.path) bb.expand(p);
    // Texte : boite approchee (0,6 h par caractere), tournee avec lui.
    const double w = 0.6 * g.label.height * static_cast<double>(g.label.text.size());
    const Dir u{std::cos(g.label.angle), std::sin(g.label.angle)};
    const Dir n{-u.y, u.x};
    for (double s : {-0.5, 0.5})
        for (double t : {0.0, 1.0})
            bb.expand(offset(offset(g.label.position, u, s * w), n, t * g.label.height));
    return bb;
}

double dimensionDistance(const DimensionEntity& dimension, const Point2& p) {
    const DimensionGraphics g = dimensionGraphics(dimension);
    // Distance a la boite du texte (0,6 h par caractere), dans le repere du texte.
    const double w = 0.6 * g.label.height * static_cast<double>(g.label.text.size());
    const double rx = p.x_ - g.label.position.x_, ry = p.y_ - g.label.position.y_;
    const double along = rx * std::cos(g.label.angle) + ry * std::sin(g.label.angle);
    const double across = -rx * std::sin(g.label.angle) + ry * std::cos(g.label.angle);
    const double outAlong = std::max(0.0, std::abs(along) - w / 2.0);
    const double outAcross = std::max({0.0, -across, across - g.label.height});
    double best = std::hypot(outAlong, outAcross);
    for (std::size_t i = 1; i < g.path.size(); ++i)
        best = std::min(best, distance(p, closestPointOnSegment(p, g.path[i - 1], g.path[i])));
    return best;
}

void writeDimensionDxf(std::ostream& f, const DimensionEntity& dimension, const std::string& layer,
                       const std::optional<Color>& colorOverride) {
    const DimensionGraphics g = dimensionGraphics(dimension);
    auto writeColor = [&] {
        if (!colorOverride) return;   // couleur du calque (DuCalque)
        const int r = static_cast<int>(colorOverride->r * 255);
        const int gr = static_cast<int>(colorOverride->g * 255);
        const int b = static_cast<int>(colorOverride->b * 255);
        f << "62\n" << ((r == gr && gr == b) ? std::clamp(r / 8, 1, 255) : 7) << "\n";
    };
    for (std::size_t i = 1; i < g.path.size(); ++i) {
        const Point2& a = g.path[i - 1];
        const Point2& b = g.path[i];
        if (distance(a, b) < 1e-12) continue;
        f << "0\nLINE\n8\n" << layer << "\n";
        writeColor();
        f << "10\n" << a.x_ << "\n20\n" << a.y_ << "\n11\n" << b.x_ << "\n21\n" << b.y_ << "\n";
    }
    if (g.label.text.empty()) return;
    // Texte centre (72 = 1) sur son point d'alignement (11/21), angle en degres.
    f << "0\nTEXT\n8\n" << layer << "\n";
    writeColor();
    f << "10\n" << g.label.position.x_ << "\n20\n" << g.label.position.y_ << "\n"
      << "40\n" << g.label.height << "\n1\n" << g.label.text << "\n"
      << "50\n" << g.label.angle * 180.0 / kPi << "\n72\n1\n"
      << "11\n" << g.label.position.x_ << "\n21\n" << g.label.position.y_ << "\n";
}

DimensionGraphics dimensionGraphics(const DimensionEntity& dimension) {
    DimensionGraphics g = buildGraphics(dimension);
    // Coordonnees non finies (fichier corrompu ou malveillant) : rien a dessiner.
    auto finite = [](const Point2& p) { return std::isfinite(p.x_) && std::isfinite(p.y_); };
    if (!std::all_of(g.path.begin(), g.path.end(), finite) || !finite(g.label.position) ||
        !std::isfinite(g.label.angle))
        return {};
    return g;
}

DimensionGraphics buildGraphics(const DimensionEntity& dimension) {
    switch (dimension.dimensionType()) {
        case DimensionEntityType::Linear:
            if (auto* d = dynamic_cast<const LinearDimensionEntity*>(&dimension)) return linearGraphics(*d);
            break;
        case DimensionEntityType::Aligned:
            if (auto* d = dynamic_cast<const AlignedDimensionEntity*>(&dimension)) return alignedGraphics(*d);
            break;
        case DimensionEntityType::Angular:
            if (auto* d = dynamic_cast<const AngularDimensionEntity*>(&dimension)) return angularGraphics(*d);
            break;
        case DimensionEntityType::Radius:
        case DimensionEntityType::Diameter:
            if (auto* d = dynamic_cast<const RadialDimensionEntity*>(&dimension)) return radialGraphics(*d);
            break;
    }
    return {};
}

} // namespace bcad::geom
