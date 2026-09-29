// CONTRATS layout — ancien spike (ADR-017), converti bloc par bloc : quand un
// obstacle disparaît, son assertion devient un contrat, pas un retrait.
//
//   [1] contrat : une échelle explicite est respectée, jamais écrasée par le
//       peintre (`drawSheet` ne touche pas à `viewport.scale()`).
//   [2] contrat : `applyFittingScale` ne remplit qu'une échelle non choisie ;
//       une échelle d'opérateur survit à l'export.
//   [3] mesure ouverte : rien ne refuse encore une vue qui déborde (`fitsIn`
//       sans appelant dans src/) — l'étape 13 la rendra vraie pour la mise en
//       page, via `IValidator` (décision 5).
//   [4] mesure ouverte : une seule vue peinte, centrée d'office ; la position
//       papier est une donnée (`Viewport::paper()`) que le peintre n'honore
//       pas encore.
//   [5] contrat (ancien bug) : dix champs déclaratifs réservent la bande et
//       sont encrés ; une feuille sans saisie n'en réserve aucune.
//   [6] contrat : le vocabulaire est ouvert — une clé inconnue traverse la
//       résolution et le fichier, et se signale.
//   [7] contrat : plus de liste FR dans l'hôte — la liste vient du profil, et
//       sans liste l'ajustement est exact.
//
// Les appels a effet de bord sont hors des assert() : assert() n'evalue pas son
// argument quand NDEBUG est defini.

#include "bcad/core/Document.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/layout/FieldResolution.h"
#include "bcad/layout/PdfExport.h"

#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QRect>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace bcad;

namespace {

constexpr double kDpi = 96.0;
double mmToPx(double mm) { return mm * kDpi / 25.4; }

int inkedPixels(const QImage& image, const QRect& region) {
    int count = 0;
    for (int y = region.top(); y <= region.bottom() && y < image.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = region.left(); x <= region.right() && x < image.width(); ++x) {
            if (qGray(line[x]) <= 200) ++count;
        }
    }
    return count;
}

layout::PdfExportOptions a3Paysage(const core::Document& document) {
    layout::PdfExportOptions options;
    options.sheet = layout::Sheet(layout::PaperFormat::A3, layout::Orientation::Paysage);
    options.document = &document;
    options.permittedScales = {500, 1000, 2000};
    return options;
}

// Un meuble de dix champs déclaratifs, sans aucun nom connu de l'hôte.
layout::ResolvedFurniture meubleDeDixChamps() {
    layout::FurnitureTemplate gabarit;
    gabarit.columns = 4;
    gabarit.reservedZone = {0, 0, 0, 25.0};
    layout::ResolvedFurniture meuble;
    meuble.gabarit = gabarit;
    for (int i = 0; i < 10; ++i) {
        layout::Field champ;
        champ.label = "Champ " + std::to_string(i);
        champ.key = "dossier.champ_" + std::to_string(i);
        champ.slot = i;
        champ.value.type = properties::PropertyType::String;
        champ.value.value = std::string("valeur");
        meuble.fields.push_back(std::move(champ));
    }
    return meuble;
}

void renderTo(const layout::PdfExportOptions& options, QImage& image) {
    image.fill(Qt::white);
    QPainter painter(&image);
    assert(painter.isActive());
    layout::drawSheet(painter, options);
}

QImage sheetImage(const layout::Sheet& sheet) {
    QImage image(static_cast<int>(mmToPx(sheet.printableWidth())),
                 static_cast<int>(mmToPx(sheet.printableHeight())), QImage::Format_RGB32);
    return image;
}

} // namespace

int main(int argc, char** argv) {
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    std::cout << std::unitbuf << "=== CONTRATS layout ===\n";

    // 1. Une echelle explicite est respectee, par la composition comme par le
    //    peintre : "regler 1:500" tient jusqu'au papier.
    {
        layout::Viewport viewport;
        viewport.setSource(geom::BoundingBox{0, 0, 40, 30});
        viewport.setScale(500);
        const auto composition = layout::composeSheet(
            layout::Sheet(layout::PaperFormat::A3, layout::Orientation::Paysage), viewport,
            {500, 1000});
        std::cout << std::unitbuf << "[1] composition respecte une echelle explicite : scale="
                  << composition.mapping.scale << " plan=" << composition.mapping.rect.w << "x"
                  << composition.mapping.rect.h << " mm (attendu 500, 80x60)\n";
        assert(composition.mapping.scale == 500);
        assert(std::abs(composition.mapping.rect.w - 80.0) < 1e-9);
        assert(std::abs(composition.mapping.rect.h - 60.0) < 1e-9);
    }

    // 2. Le seul remplissage automatique ne remplace pas un choix : sans
    //    echelle il ajuste, avec echelle il laisse.
    {
        core::Document document;
        auto options = a3Paysage(document);
        options.viewport.setSource(geom::BoundingBox{0, 0, 40, 30});
        layout::applyFittingScale(options);
        std::cout << std::unitbuf << "[2] sans choix : 1:" << options.viewport.scale() << "\n";
        assert(options.viewport.scale() == 500);

        auto avecChoix = a3Paysage(document);
        avecChoix.viewport.setSource(geom::BoundingBox{0, 0, 40, 30});
        avecChoix.viewport.setScale(1000);
        auto image = sheetImage(avecChoix.sheet);
        renderTo(avecChoix, image);
        std::cout << std::unitbuf << "[2] avec choix 1:1000 : scale=" << avecChoix.viewport.scale()
                  << " (le peintre ne touche pas a l'echelle)\n";
        assert(avecChoix.viewport.scale() == 1000);
    }

    // 3. Mesure ouverte : un 1:500 sur un ilot de 300 m déborde et personne ne
    //    le refuse encore — `fitsIn` le voit, sans appelant dans src/.
    {
        layout::Viewport viewport;
        viewport.setSource(geom::BoundingBox{0, 0, 300, 200});
        viewport.setScale(500);
        const auto sheet = layout::Sheet(layout::PaperFormat::A3, layout::Orientation::Paysage);
        const auto composition = layout::composeSheet(sheet, viewport, {500, 1000});
        const bool deborde = composition.mapping.rect.right() > composition.printable.right() + 1e-9
                          || composition.mapping.rect.bottom() > composition.printable.bottom() + 1e-9;
        std::cout << std::unitbuf << "[3] 300x200 m a 1:500 = " << composition.mapping.rect.w << "x"
                  << composition.mapping.rect.h << " mm : deborde=" << deborde
                  << ", fitsIn=" << viewport.fitsIn(sheet) << " (jamais appele par src/)\n";
        assert(deborde);
        assert(!viewport.fitsIn(sheet));
    }

    // 4. Mesure ouverte : la position papier est une donnée (`isPlaced()`), que
    //    le peintre n'honore pas encore — une seule vue, centrée d'office.
    {
        const auto sheet = layout::Sheet(layout::PaperFormat::A3, layout::Orientation::Paysage);
        layout::Viewport vue1;
        vue1.setSource(geom::BoundingBox{0, 0, 40, 30});
        vue1.setScale(1000);
        vue1.setPaper({10.0, 10.0, 80.0, 60.0});
        layout::Viewport vue2;
        vue2.setSource(geom::BoundingBox{100, 0, 110, 10});
        vue2.setScale(1000);
        const auto c1 = layout::composeSheet(sheet, vue1, {500, 1000});
        const auto c2 = layout::composeSheet(sheet, vue2, {500, 1000});
        std::cout << std::unitbuf << "[4] vue placee : isPlaced=" << vue1.isPlaced()
                  << " ; ancrages composition (" << c1.mapping.rect.x << "," << c1.mapping.rect.y
                  << ") et (" << c2.mapping.rect.x << "," << c2.mapping.rect.y << ")\n";
        assert(vue1.isPlaced());
        assert(!vue2.isPlaced());
        assert(c1.mapping.rect.contains(c2.mapping.rect));
    }

    // 5. Contrat (ancien bug, ancien obstacle 5) : dix champs déclaratifs sans
    //    aucun nom connu de l'hôte réservent la bande ET sont encrés. Zéro
    //    champ : zéro bande.
    {
        core::Document document;
        document.addEntity(std::make_unique<geom::PolylineEntity>(
            std::vector<geom::Point2>{{0, 0}, {100, 0}, {100, 60}, {0, 60}}, true));

        auto options = a3Paysage(document);
        options.viewport.setSource(document.extents());
        options.viewport.setScale(1000);
        options.meubles.push_back(meubleDeDixChamps());
        double bande = 0;
        for (const auto& meuble : options.meubles) bande += meuble.gabarit.reservedZone.h;
        const auto composition = layout::composeSheet(options.sheet, options.viewport,
                                                      options.permittedScales, bande);
        auto image = sheetImage(options.sheet);
        renderTo(options, image);
        const QRect zone(0, static_cast<int>(image.height() - mmToPx(22.0)),
                         image.width(), static_cast<int>(mmToPx(20.0)));
        const int encre = inkedPixels(image, zone);
        std::cout << std::unitbuf << "[5] dix champs declares : bande=" << composition.cartouche.h
                  << " mm, encre=" << encre << " px\n";
        assert(std::abs(composition.cartouche.h - 25.0) < 1e-9);
        assert(encre > 2 * zone.height() + 50);

        auto sans = a3Paysage(document);
        sans.viewport.setSource(document.extents());
        sans.viewport.setScale(1000);
        const auto vide = layout::composeSheet(sans.sheet, sans.viewport, sans.permittedScales);
        std::cout << std::unitbuf << "[5] sans meuble : bande=" << vide.cartouche.h << " mm\n";
        assert(vide.cartouche.h == 0.0);
    }

    // 6. Contrat : le vocabulaire est ouvert. Une clé que personne n'a
    //    déclarée traverse la résolution conservée et signalée.
    {
        layout::FurnitureTemplate gabarit;
        gabarit.id = "profil_national.cartouche";
        layout::Furniture meuble("profil_national.cartouche");
        layout::Field inconnu;
        inconnu.label = "Indice";
        inconnu.key = "PROFIL_NATIONAL";
        inconnu.slot = 3;
        inconnu.value.type = properties::PropertyType::String;
        inconnu.value.value = std::string("togo");
        meuble.addField(inconnu);

        properties::PropertyMap dossier;
        const layout::FieldScope scope{&dossier, {}};
        std::vector<validation::Diagnostic> diagnostics;
        const std::vector<layout::Field> resolus =
            layout::resolveFields(meuble, gabarit, scope, diagnostics);
        bool conserve = false, signale = false;
        for (const auto& champ : resolus) {
            if (champ.key != "PROFIL_NATIONAL") continue;
            conserve = std::get<std::string>(champ.value.value) == "togo";
        }
        for (const auto& diag : diagnostics) {
            if (diag.message.find("PROFIL_NATIONAL") != std::string::npos) signale = true;
        }
        std::cout << std::unitbuf << "[6] cle non declaree : conservee=" << conserve
                  << ", signalee=" << signale << "\n";
        assert(conserve);
        assert(signale);
    }

    // 7. Contrat : aucune liste nationale dans l'hôte. La liste vient du
    //    profil ; sans liste, l'ajustement est exact.
    {
        const std::vector<int> profil{500, 1000, 2000};
        const double choisie =
            layout::permittedScaleFor(geom::BoundingBox{0, 0, 200, 100}, layout::RectMm{0, 0, 400, 252}, profil);
        const double exacte =
            layout::permittedScaleFor(geom::BoundingBox{0, 0, 200, 100}, layout::RectMm{0, 0, 400, 252}, {});
        std::cout << std::unitbuf << "[7] profil {500,1000,2000} : 1:" << choisie
                  << " ; sans liste : 1:" << exacte << "\n";
        assert(choisie == 500);
        assert(exacte == 500);
    }

    std::cout << std::unitbuf << "=== contrats tenus : 5 contrats, 2 mesures ouvertes ===\n";
    return 0;
}
