// Styles de cote (A-06) : registre par nom et effet du style sur le dessin.
//
// Le style n'existait qu'en structure (`layout/DimensionStyle.h`) sans
// consommateur : la cotation prenait des proportions figees. Le registre donne
// un style par nom, et le dessin (ecarts, fleches, hauteur) en derive.

#include "bcad/geometry/DimensionGraphics.h"
#include "bcad/geometry/LinearDimensionEntity.h"
#include "bcad/layout/DimensionStyle.h"
#include "bcad/layout/DimensionStyleRegistry.h"

#include <cassert>
#include <cstdio>
#include <memory>
#include <numbers>
#include <string>

using namespace bcad;
using namespace bcad::geom;
using bcad::layout::ArrowheadType;
using bcad::layout::DimensionStyle;
using bcad::layout::DimensionStyleRegistry;

namespace {

constexpr double kPi = std::numbers::pi;

bool near(double a, double b, double tol = 1e-9) { return std::abs(a - b) < tol; }

bool passesThrough(const std::vector<Point2>& path, const Point2& p) {
    for (const Point2& q : path)
        if (near(q.x_, p.x_) && near(q.y_, p.y_)) return true;
    return false;
}

} // namespace

int main() {
    DimensionStyleRegistry& reg = DimensionStyleRegistry::instance();
    reg.clear();

    // --- Registre : enregistrer, trouver, lister, retirer ---
    auto grand = std::make_unique<DimensionStyle>("Grand");
    grand->textHeight = 5.0;
    grand->extLineExtend = 5.0;      // DIMEXE, en unites du style
    reg.registerStyle(std::move(grand));

    assert(reg.find("Grand") != nullptr);
    assert(near(reg.find("Grand")->textHeight, 5.0));
    assert(reg.find("Inconnu") == nullptr);

    bool listed = false;
    for (const std::string& n : reg.listNames()) listed = listed || n == "Grand";
    assert(listed);

    // Style inconnu : on retombe sur le standard, jamais sur rien.
    const DimensionStyle& fallback = reg.getOrDefault("Inconnu");
    assert(near(fallback.textHeight, DimensionStyle::standard().textHeight));
    reg.unregisterStyle("Grand");
    assert(reg.find("Grand") == nullptr);

    // --- Le style choisi par la cotation règle la hauteur par défaut ---
    auto petit = std::make_unique<DimensionStyle>("Petit");
    petit->textHeight = 0.5;
    petit->extLineExtend = 1.0;      // pour une hauteur 0.5 : depassement de 1.0
    petit->extLineOffset = 0.5;      // ecart a l'objet
    petit->textOffset = 0.5;         // ecart du texte a la ligne de cote
    reg.registerStyle(std::move(petit));

    LinearDimensionEntity styled({0, 0}, {10, 3}, {5, 8}, 0.0, "Petit");
    assert(near(dimensionTextHeight(styled), 0.5));
    DimensionGraphics g = dimensionGraphics(styled);
    // Le dessin garde les proportions du style, mises a l'echelle de sa hauteur :
    // ecart a l'objet 0.5, depassement 1.0, texte a 0.5 au-dessus de la ligne.
    assert(passesThrough(g.path, {0, 0.5}));
    assert(passesThrough(g.path, {0, 8.0 + 1.0}));
    assert(near(g.label.height, 0.5) && near(g.label.position.y_, 8.0 + 0.5));

    // --- Types de flèche : `None` n'ajoute aucun point, `Dot` / `Open` si ---
    auto sans = std::make_unique<DimensionStyle>("SansFleche");
    sans->textHeight = 1.0;
    sans->arrowhead1 = ArrowheadType::None;
    sans->arrowhead2 = ArrowheadType::None;
    reg.registerStyle(std::move(sans));

    auto ouvert = std::make_unique<DimensionStyle>("FlecheOuverte");
    ouvert->textHeight = 1.0;
    ouvert->arrowhead1 = ArrowheadType::Open;
    ouvert->arrowhead2 = ArrowheadType::Open;
    reg.registerStyle(std::move(ouvert));

    LinearDimensionEntity base({0, 0}, {10, 0}, {5, 3}, 0.0, "Standard");
    base.properties().setDouble(kDimensionTextHeightProperty, 1.0);
    LinearDimensionEntity none({0, 0}, {10, 0}, {5, 3}, 0.0, "SansFleche");
    none.properties().setDouble(kDimensionTextHeightProperty, 1.0);
    LinearDimensionEntity open({0, 0}, {10, 0}, {5, 3}, 0.0, "FlecheOuverte");
    open.properties().setDouble(kDimensionTextHeightProperty, 1.0);

    const std::size_t nBase = dimensionGraphics(base).path.size();
    const std::size_t nNone = dimensionGraphics(none).path.size();
    assert(nNone + 8 == nBase);        // 4 points par flèche, deux flèches
    assert(dimensionGraphics(open).path != dimensionGraphics(base).path);

    // La ligne de cote reste tracée même sans flèche (le trait relie les pieds).
    const auto gn = dimensionGraphics(none);
    assert(passesThrough(gn.path, {0, 3}) && passesThrough(gn.path, {10, 3}));

    // --- Clone : le nom de style est conservé ---
    const auto copy = styled.clone();
    const auto* dc = dynamic_cast<const DimensionEntity*>(copy.get());
    assert(dc != nullptr && dc->styleName() == "Petit");

    reg.clear();
    assert(reg.listNames().empty());

    std::printf("Styles de cote (A-06) : tests PASSED\n");
    return 0;
}
