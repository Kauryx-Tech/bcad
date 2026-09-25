// SPIKE — il ne decide rien et ne repare rien : il mesure ce que l'API de
// mise en page refuse aujourd'hui, avant qu'un format de fichier ne gele ces
// choix. Chaque bloc printe un obstacle et l'assert qui le prouve.
//
// Ce qui est teste ici est le comportement REEL de src/layout ; si une
// assertion casse, c'est que l'obstacle a cesse d'exister — pas que le test
// est faux.
//
// Les appels a effet de bord sont hors des assert() : assert() n'evalue pas son
// argument quand NDEBUG est defini.

#include "bcad/core/Document.h"
#include "bcad/geometry/Polyline.h"
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
    return options;
}

layout::Cartouche rempliHorsVocabulaire() {
    layout::Cartouche cartouche;
    cartouche.auteur = "Atelier topographique";
    cartouche.geometre = "K. Mensah";
    cartouche.date = "25/09/2026";
    cartouche.dossier = "TOG-2026-0114";
    cartouche.phase = "APD";
    cartouche.lotNumber = "LOT 3";
    cartouche.referencePlan = "Cadastre digital, fl. 12";
    cartouche.revision = "B";
    cartouche.proprietaire = "S. Aho";
    cartouche.nature = "parcelle batie";
    return cartouche;
}

void renderTo(const layout::PdfExportOptions& options, QImage& image) {
    image.fill(Qt::white);
    QPainter painter(&image);
    assert(painter.isActive());
    layout::drawSheet(painter, options);
}

// Paires libelle -> valeur hors apparence : toKeyValuePairs emit toujours
// BORDER_WIDTH, FONT_NAME et FONT_SIZE_MM, qui ne sont pas des attributs.
std::vector<std::pair<std::string, std::string>> champsDeclaratifs(const layout::Cartouche& c) {
    static const std::vector<std::string> style = {"BORDER_WIDTH", "FONT_NAME", "FONT_SIZE_MM"};
    std::vector<std::pair<std::string, std::string>> metier;
    for (const auto& paire : c.toKeyValuePairs()) {
        if (std::find(style.begin(), style.end(), paire.first) == style.end())
            metier.push_back(paire);
    }
    return metier;
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

    std::cout << std::unitbuf << "=== SPIKE layout : obstacles mesures ===\n";

    // 1. L'echelle choisie EST respectee par la composition : "regler 1:500"
    //    n'est donc pas un manque de src/layout, c'est un manque d'entree.
    {
        layout::Viewport viewport;
        viewport.setSource(geom::BoundingBox{0, 0, 40, 30});
        viewport.setScale(500);
        const auto composition = layout::composeSheet(
            layout::Sheet(layout::PaperFormat::A3, layout::Orientation::Paysage), viewport,
            layout::Cartouche{});
        std::cout << std::unitbuf << "[1] composition respecte une echelle explicite : scale="
                  << composition.mapping.scale << " plan=" << composition.mapping.rect.w << "x"
                  << composition.mapping.rect.h << " mm (attendu 500, 80x60)\n";
        assert(composition.mapping.scale == 500);
        assert(std::abs(composition.mapping.rect.w - 80.0) < 1e-9);
        assert(std::abs(composition.mapping.rect.h - 60.0) < 1e-9);
    }

    // 2. Obstacle : le seul chemin de l'hote ecrase ce choix. Les deux appelants
    //    de applySuggestedScale (aperçu, commande plugin) ne laissent aucune
    //    place a une echelle decidee par l'operateur.
    {
        core::Document document;
        auto options = a3Paysage(document);
        options.viewport.setSource(geom::BoundingBox{0, 0, 40, 30});
        options.viewport.setScale(500);
        layout::applySuggestedScale(options);
        std::cout << std::unitbuf << "[2] applySuggestedScale remplace 1:500 par 1:" << options.viewport.scale()
                  << " (cartouche " << options.cartouche.echelle << ") — PdfExport.cpp:411\n";
        assert(options.viewport.scale() != 500);
        assert(options.cartouche.echelle == "1:200");
    }

    // 3. Obstacle : rien ne verifie qu'une vue tient sur la feuille. Le garde
    //    existe (Viewport::fitsIn, Viewport.h:33) et n'a aucun appelant dans
    //    src/ : l'hote ne peut donc pas refuser un 1:500 sur un ilot de 300 m.
    {
        layout::Viewport viewport;
        viewport.setSource(geom::BoundingBox{0, 0, 300, 200});
        viewport.setScale(500);
        const auto sheet = layout::Sheet(layout::PaperFormat::A3, layout::Orientation::Paysage);
        const auto composition = layout::composeSheet(sheet, viewport, layout::Cartouche{});
        const bool deborde = composition.mapping.rect.right() > composition.printable.right() + 1e-9
                          || composition.mapping.rect.bottom() > composition.printable.bottom() + 1e-9;
        std::cout << std::unitbuf << "[3] 300x200 m a 1:500 = " << composition.mapping.rect.w << "x"
                  << composition.mapping.rect.h << " mm pour une zone imprimable de "
                  << sheet.printableWidth() << "x" << sheet.printableHeight()
                  << " mm : deborde=" << deborde << ", fitsIn=" << viewport.fitsIn(sheet)
                  << " (jamais appele par src/)\n";
        assert(deborde);
        assert(!viewport.fitsIn(sheet));
    }

    // 4. Obstacle : une seule vue par feuille, et aneree a l'office. Deux vues
    //    distinctes tombent l'une sur l'autre — aucun ancrage papier n'existe,
    //    la position n'est pas une donnee de la vue.
    {
        const auto sheet = layout::Sheet(layout::PaperFormat::A3, layout::Orientation::Paysage);
        layout::Viewport vue1;
        vue1.setSource(geom::BoundingBox{0, 0, 40, 30});
        vue1.setScale(1000);
        layout::Viewport vue2;
        vue2.setSource(geom::BoundingBox{100, 0, 110, 10});
        vue2.setScale(1000);
        const auto c1 = layout::composeSheet(sheet, vue1, layout::Cartouche{});
        const auto c2 = layout::composeSheet(sheet, vue2, layout::Cartouche{});
        const bool memePlace = std::abs(c1.mapping.rect.x - c2.mapping.rect.x) < 1e-6
                            && std::abs(c1.mapping.rect.y - c2.mapping.rect.y) < 1e-6;
        std::cout << std::unitbuf << "[4] deux vues sur la meme feuille : ancrages (" << c1.mapping.rect.x << ","
                  << c1.mapping.rect.y << ") et (" << c2.mapping.rect.x << "," << c2.mapping.rect.y
                  << ") — meme coin=" << memePlace
                  << ", la petite tient dans la grande=" << c1.mapping.rect.contains(c2.mapping.rect)
                  << " ; un PdfExportOptions ne porte qu'un Viewport (PdfExport.h:29)\n";
        assert(!memePlace);
        // Pire que deplacees : la seconde vue est incluse dans l'emprise de la
        // premiere, donc les deux plans se peindraient l'un sur l'autre.
        assert(c1.mapping.rect.contains(c2.mapping.rect));
    }

    // 5. Obstacle : le mobilier n'est pas declare, il est devine du contenu.
    //    Un cartouche riche de dix champs d'attributs mais sans commune, section
    //    ni projet est `isValid()==false` : la bande n'est pas reservee ET n'est
    //    pas peinte (PdfExport.cpp:383). Mesure a l'encre, pas a l'idee.
    {
        core::Document document;
        document.addEntity(std::make_unique<geom::PolylineEntity>(
            std::vector<geom::Point2>{{0, 0}, {100, 0}, {100, 60}, {0, 60}}, true));

        struct Mesure {
            double hauteurBande = 0;
            int encreBande = 0;
            int hauteurBandePx = 0;
        };

        const auto bande = [&document](const layout::Cartouche& cartouche) {
            auto options = a3Paysage(document);
            options.viewport.setSource(document.extents());
            options.viewport.setScale(1000);
            options.cartouche = cartouche;
            const auto composition = layout::composeSheet(
                options.sheet, options.viewport, options.cartouche,
                options.parcelTable.size() > 0 ? layout::kParcelTableWidthMm : 0.0);
            auto image = sheetImage(options.sheet);
            renderTo(options, image);
            Mesure mesure;
            mesure.hauteurBande = composition.cartouche.h;
            // Les 22 mm au-dessus du cadre : ce qui reste quand la bande du
            // cartouche n'est pas reservee, le plan (60 mm de haut, centre)
            // n'y descend jamais a 1:1000.
            const QRect zone(0, static_cast<int>(image.height() - mmToPx(22.0)),
                             image.width(), static_cast<int>(mmToPx(20.0)));
            mesure.hauteurBandePx = zone.height();
            mesure.encreBande = inkedPixels(image, zone);
            return mesure;
        };

        layout::Cartouche avecVocabulaire;
        avecVocabulaire.commune = "Lome";
        const auto avec = bande(avecVocabulaire);
        const auto sans = bande(rempliHorsVocabulaire());

        std::cout << std::unitbuf << "[5] bande reservee : " << avec.hauteurBande << " mm avec un "
                     "seul champ du vocabulaire hote, "
                  << sans.hauteurBande << " mm avec dix champs d'attributs\n"
                  << "    encre dans cette bande : " << avec.encreBande << " px contre "
                  << sans.encreBande << " px (" << 2 * sans.hauteurBandePx
                  << " = les deux montants du cadre, donc zero cartouche)\n";
        assert(std::abs(avec.hauteurBande - avecVocabulaire.heightMm) < 1e-9);
        assert(sans.hauteurBande == 0.0);
        // Le cartouche rempli d'attributs ne laisse sur la feuille que les
        // montants du cadre : deux traits verticaux, rien d'autre.
        assert(sans.encreBande <= 4 * sans.hauteurBandePx);
        assert(avec.encreBande - sans.encreBande >= 50);

        const int champsMetier = static_cast<int>(champsDeclaratifs(rempliHorsVocabulaire()).size());
        std::cout << std::unitbuf << "[5] isValid() = " << std::boolalpha
                  << rempliHorsVocabulaire().isValid() << " pour " << champsMetier
                  << " champs renseignes : la condition regarde trois noms, pas le contenu\n";
        assert(!rempliHorsVocabulaire().isValid());
        assert(champsMetier == 10);
    }

    // 6. Obstacle : le vocabulaire du cartouche est une structure fermee, et
    //    son propre serializeur perd ce qu'il ne connait pas. Un champ declare
    //    par un profil national n'entre pas dans la round-trip hote.
    {
        const std::vector<std::pair<std::string, std::string>> paires{
            {"COMMUNE", "Lome"}, {"PROFIL_NATIONAL", "togo"}, {"INDICE_CADASTRAL", "TOGO-2026"}};
        const auto cartouche = layout::Cartouche::fromKeyValuePairs(paires);
        const auto restitue = cartouche.toKeyValuePairs();
        bool retrouveProfil = false, retrouveIndice = false;
        for (const auto& [cle, valeur] : restitue) {
            if (cle == "PROFIL_NATIONAL") retrouveProfil = true;
            if (cle == "INDICE_CADASTRAL") retrouveIndice = true;
        }
        const int ressortisMetier = static_cast<int>(champsDeclaratifs(cartouche).size());
        std::cout << std::unitbuf << "[6] aller-retour d'un champ declare par un profil : COMMUNE conserve="
                  << (cartouche.commune == "Lome") << ", PROFIL_NATIONAL=" << retrouveProfil
                  << ", INDICE_CADASTRAL=" << retrouveIndice << " ; " << paires.size()
                  << " champs entres, " << ressortisMetier << " attributs ressortis\n";
        assert(cartouche.commune == "Lome");
        assert(!retrouveProfil && !retrouveIndice);  // perte silencieuse
        assert(ressortisMetier == 1);
    }

    // 7. Obstacle annexe, hors cartouche : les noms de champs, la liste
    //    d'echelles et le pas de grille sont du vocabulaire cadastral FR dans
    //    l'API publique de l'hote, et la grille de feuille n'a aucun peintre.
    {
        const auto ladder = layout::kStandardScales;
        std::cout << std::unitbuf << "[7] echelles : " << ladder.size() << " valeurs etiquetees \"FR (BOFiP)\" "
                  "dans include/bcad/layout/Scale.h ; gridStepMm(500) = "
                  << layout::gridStepMm(500)
                  << " mm, sans appelant dans src/ (aucun peintre de grille de feuille, "
                     "aucune legende)\n";
        assert(layout::gridStepMm(500) == 20.0);
    }

    std::cout << std::unitbuf << "=== spike termine : 7 obstacles mesures, 0 decision prise ===\n";
    return 0;
}
