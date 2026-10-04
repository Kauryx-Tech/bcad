// Outils de dessin et de modification de bout en bout, sur le vrai Viewport.
//
// Chaque point est fourni par submitTypedPoint : c'est le meme chemin qu'un clic
// apres accrochage (placePoint), sans dependre d'un ecran ni d'OpenGL. Le test
// verifie pour chaque outil la consigne affichee a chaque etape, l'entite
// produite, et l'aller-retour Annuler/Retablir par la vraie pile Qt.
//
// Regressions couvertes :
//   - Ajout/Suppression rendaient l'entite sous un id neuf : un Retablir d'une
//     transformation posterieure ne la retrouvait plus (perdu sans message) ;
//   - Deplacer ignorait la selection et refusait les coordonnees saisies ;
//   - la polyligne ne pouvait pas etre fermee ;
//   - les cotations visaient un calque inexistant ;
//   - les refus ouvraient une boite modale (bloquante) en anglais.
//
// Cycle de commande AutoCAD (2026-10-04) : au repos la souris selectionne ;
// une commande se termine seule et revient au repos ; la ligne enchaine ses
// segments jusqu'a Entree ; Echap termine la commande puis vide la selection ;
// Entree au repos relance la derniere commande ; une modification lancee sans
// selection fait d'abord designer ses objets a la souris.

#include "Commands.h"
#include "Viewport.h"

#include "bcad/core/Document.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/TextEntity.h"
#include "bcad/geometry/AlignedDimensionEntity.h"
#include "bcad/geometry/AngularDimensionEntity.h"
#include "bcad/geometry/DimensionGraphics.h"
#include "bcad/geometry/LinearDimensionEntity.h"
#include "bcad/geometry/RadialDimensionEntity.h"

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QUndoStack>

#include <cassert>
#include <cmath>
#include <numbers>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace bcad;
using app::ToolMode;
using app::Viewport;

namespace {

bool near(double a, double b, double tol = 1e-9) { return std::abs(a - b) < tol; }

struct Bench {
    core::Document doc;
    QUndoStack stack;
    Viewport viewport;
    QString lastMessage;

    Bench() {
        viewport.setDocument(&doc);
        viewport.setUndoStack(&stack);
        QObject::connect(&viewport, &Viewport::statusMessage,
                         [this](const QString& m) { lastMessage = m; });
    }
    void tool(ToolMode mode) { viewport.setTool(mode); }
    void type(const char* text) { viewport.submitTypedPoint(QString::fromUtf8(text)); }
    void key(int k, const QString& text = {}) {
        QKeyEvent event(QEvent::KeyPress, k, Qt::NoModifier, text);
        QCoreApplication::sendEvent(&viewport, &event);
    }
    size_t count() const { return doc.entities().size(); }
    geom::Entity* last() const { return doc.entities().back().get(); }
    void selectAll() { viewport.selectAll(); }
    // Clic de souris a une position monde (le canevas hors ecran a sa taille
    // par defaut ; la camera convertit).
    void click(double x, double y, Qt::MouseButton button = Qt::LeftButton) {
        const auto screen = viewport.camera().worldToScreen(geom::Point2{x, y});
        const QPointF pos(screen.x, screen.y);
        QMouseEvent press(QEvent::MouseButtonPress, pos, viewport.mapToGlobal(pos), button, button, Qt::NoModifier);
        QCoreApplication::sendEvent(&viewport, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, pos, viewport.mapToGlobal(pos), button, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&viewport, &release);
    }
    void clearSelection() { for (const auto& e : doc.entities()) e->selected = false; }
    void reset() { stack.clear(); doc.clear(); lastMessage.clear(); }
};

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    Bench b;

    // --- Ligne : consigne a chaque etape, coordonnee relative ---
    b.tool(ToolMode::Line);
    assert(b.viewport.prompt().contains("premier point"));
    b.type("0,0");
    assert(b.viewport.prompt().contains("point suivant"));
    b.type("@10,0");
    assert(b.count() == 1);
    auto* line = dynamic_cast<geom::LineEntity*>(b.last());
    assert(line && near(line->end().x_, 10) && near(line->end().y_, 0));
    assert(b.stack.undoText().contains("Ligne"));
    // La ligne enchaine depuis le dernier point, jusqu'a Entree (AutoCAD).
    assert(b.viewport.tool() == ToolMode::Line && b.viewport.prompt().contains("point suivant"));
    b.type("@0,5");
    assert(b.count() == 2);
    auto* chained = dynamic_cast<geom::LineEntity*>(b.last());
    assert(chained && near(chained->start().x_, 10) && near(chained->end().y_, 5));
    b.key(Qt::Key_Return);
    assert(b.viewport.tool() == ToolMode::Select);   // fin de commande : retour au repos
    // Entree au repos relance la derniere commande.
    b.key(Qt::Key_Return);
    assert(b.viewport.tool() == ToolMode::Line && b.viewport.prompt().contains("premier point"));
    b.key(Qt::Key_Escape);
    assert(b.viewport.tool() == ToolMode::Select);

    // --- Cercle : centre puis rayon ---
    b.tool(ToolMode::Circle);
    assert(b.viewport.prompt().contains("centre"));
    b.type("0,0");
    assert(b.viewport.prompt().contains("rayon"));
    b.type("5,0");
    auto* circle = dynamic_cast<geom::CircleEntity*>(b.last());
    assert(circle && near(circle->radius(), 5));
    assert(b.viewport.tool() == ToolMode::Select);   // une commande finie revient au repos

    // --- Arc : centre, depart, fin ---
    b.tool(ToolMode::Arc);
    b.type("0,0"); b.type("5,0");
    assert(b.viewport.prompt().contains("fin"));
    b.type("0,5");
    assert(dynamic_cast<geom::ArcEntity*>(b.last()));

    // --- Rectangle : coin oppose relatif ---
    b.tool(ToolMode::Rectangle);
    b.type("0,0"); b.type("@4,3");
    auto* rect = dynamic_cast<geom::PolylineEntity*>(b.last());
    assert(rect && rect->closed() && rect->vertices().size() == 4);
    assert(near(rect->vertices()[2].x_, 4) && near(rect->vertices()[2].y_, 3));

    // --- Point ---
    b.tool(ToolMode::Point);
    b.type("7,7");
    assert(dynamic_cast<geom::PointEntity*>(b.last()));

    // --- Polyligne ouverte : Entree termine ---
    const size_t beforePoly = b.count();
    b.tool(ToolMode::Polyline);
    b.type("0,0"); b.type("10,0"); b.type("10,10");
    assert(b.viewport.prompt().contains("Entrée"));
    b.key(Qt::Key_Return);
    auto* open = dynamic_cast<geom::PolylineEntity*>(b.last());
    assert(b.count() == beforePoly + 1 && open && !open->closed() && open->vertices().size() == 3);

    assert(b.viewport.tool() == ToolMode::Select);

    // --- Polyligne fermee : touche C, et « C » saisi dans la ligne de commande ---
    b.tool(ToolMode::Polyline);
    b.type("0,0"); b.type("10,0"); b.type("10,10");
    b.key(Qt::Key_C, "c");
    auto* closedByKey = dynamic_cast<geom::PolylineEntity*>(b.last());
    assert(closedByKey && closedByKey->closed() && closedByKey->vertices().size() == 3);
    b.tool(ToolMode::Polyline);
    b.type("0,0"); b.type("5,0"); b.type("5,5");
    b.type("C");
    auto* closedByText = dynamic_cast<geom::PolylineEntity*>(b.last());
    assert(closedByText && closedByText != closedByKey && closedByText->closed());
    // Entree sur une ligne de commande vide termine aussi (comme AutoCAD).
    b.tool(ToolMode::Polyline);
    b.type("0,0"); b.type("3,0");
    const size_t beforeEmpty = b.count();
    b.type("");
    assert(b.count() == beforeEmpty + 1);

    // --- Cotations (A-01, A-02) : un objet par cotation, sur un calque qui
    // existe, annulable en un seul pas ---
    b.reset();
    b.tool(ToolMode::DimensionLinear);
    b.type("0,0"); b.type("10,3"); b.type("5,8");          // au-dessus : horizontale
    assert(b.count() == 1);
    auto* horizontal = dynamic_cast<geom::LinearDimensionEntity*>(b.last());
    assert(horizontal && near(horizontal->rotation(), 0) && near(horizontal->measuredValue(), 10));
    assert(horizontal->layer() == "Cotations" && b.doc.layerManager().find("Cotations") != nullptr);
    const double height = geom::dimensionTextHeight(*horizontal);
    assert(height > 0 && horizontal->properties().has(geom::kDimensionTextHeightProperty));
    b.stack.undo();
    assert(b.count() == 0);
    b.stack.redo();
    assert(b.count() == 1);
    // A droite des origines : verticale.
    b.tool(ToolMode::DimensionLinear);
    b.type("0,0"); b.type("10,3"); b.type("15,1");
    auto* vertical = dynamic_cast<geom::LinearDimensionEntity*>(b.last());
    assert(vertical && near(vertical->rotation(), std::numbers::pi / 2) && near(vertical->measuredValue(), 3));
    assert(near(geom::dimensionTextHeight(*vertical), height));   // meme taille que la precedente
    // V force la verticale meme au-dessus.
    b.tool(ToolMode::DimensionLinear);
    b.type("0,0"); b.type("10,3");
    assert(b.viewport.prompt().contains("V verticale"));
    b.type("V");
    assert(b.viewport.prompt().contains("verticale —"));
    b.type("5,8");
    assert(near(b.last()->typeId() == geom::TypeId_LinearDimension
                    ? static_cast<geom::DimensionEntity*>(b.last())->measuredValue() : -1, 3));
    // Cotation nulle (horizontale de deux points superposes en X) : refusee.
    b.tool(ToolMode::DimensionLinear);
    const size_t beforeNull = b.count();
    b.lastMessage.clear();
    b.type("0,0"); b.type("0,5"); b.type("H"); b.type("3,8");
    assert(b.count() == beforeNull && !b.lastMessage.isEmpty());
    b.key(Qt::Key_Escape);
    // Alignee : trois points.
    b.tool(ToolMode::DimensionAligned);
    b.type("0,0"); b.type("3,4"); b.type("0,5");
    auto* aligned = dynamic_cast<geom::AlignedDimensionEntity*>(b.last());
    assert(aligned && near(aligned->measuredValue(), 5));
    // Angulaire : sommet, deux cotes, position de l'arc.
    b.tool(ToolMode::DimensionAngular);
    b.type("0,0"); b.type("5,0"); b.type("0,5"); b.type("3,3");
    auto* angular = dynamic_cast<geom::AngularDimensionEntity*>(b.last());
    assert(angular && near(angular->measuredValue(), 90) && angular->dimensionText() == "90.0\xC2\xB0");
    // Rayon d'un cercle designe : le second point ne donne que la direction.
    b.tool(ToolMode::Circle);
    b.type("20,20"); b.type("24,20");
    b.tool(ToolMode::DimensionRadius);
    b.type("24,20");
    assert(b.viewport.prompt().contains("direction"));
    b.type("20,30");
    auto* radius = dynamic_cast<geom::RadialDimensionEntity*>(b.last());
    assert(radius && near(radius->measuredValue(), 4) && near(radius->chordPoint().y_, 24));
    assert(radius->dimensionText().rfind("R ", 0) == 0);
    // Diametre par centre et point.
    b.tool(ToolMode::DimensionDiameter);
    b.type("0,0"); b.type("2,0");
    auto* diameter = dynamic_cast<geom::RadialDimensionEntity*>(b.last());
    assert(diameter && near(diameter->measuredValue(), 4));
    assert(diameter->dimensionText().rfind("\xC3\x98 ", 0) == 0);
    // Une copie garde calque et taille de texte.
    auto copy = diameter->clone();
    assert(copy->layer() == "Cotations" &&
           near(geom::dimensionTextHeight(static_cast<geom::DimensionEntity&>(*copy)), height));

    // --- Deplacer la selection, au clavier, puis Annuler/Retablir x2 ---
    b.reset();
    b.tool(ToolMode::Line);
    b.type("0,0"); b.type("10,0");
    b.selectAll();
    b.tool(ToolMode::Move);
    assert(b.viewport.prompt().contains("point de base"));
    b.type("0,0");
    assert(b.viewport.prompt().contains("destination"));
    b.type("@100,0");
    line = dynamic_cast<geom::LineEntity*>(b.last());
    assert(line && near(line->start().x_, 100) && near(line->end().x_, 110));
    b.stack.undo();                       // annule le deplacement
    b.stack.undo();                       // annule la ligne
    assert(b.count() == 0);
    b.stack.redo();                       // la ligne revient...
    b.stack.redo();                       // ...et le deplacement la retrouve
    line = dynamic_cast<geom::LineEntity*>(b.last());
    assert(line && near(line->start().x_, 100));

    // --- Copier, Tourner, Echelle, Symetrie sur la selection ---
    b.selectAll();
    b.tool(ToolMode::Copy);
    b.type("0,0"); b.type("@0,10");
    assert(b.count() == 2);
    b.clearSelection();
    b.doc.entities().front()->selected = true;
    b.tool(ToolMode::Rotate);
    b.type("100,0"); b.type("110,0"); b.type("100,10");   // +90 degres autour de (100,0)
    line = dynamic_cast<geom::LineEntity*>(b.doc.entities().front().get());
    assert(line && near(line->end().x_, 100, 1e-6) && near(line->end().y_, 10, 1e-6));
    b.tool(ToolMode::Scale);
    b.type("100,0"); b.type("100,10"); b.type("100,20");  // facteur 2
    assert(near(line->end().y_, 20, 1e-6));
    b.tool(ToolMode::Mirror);
    b.type("0,0"); b.type("0,1");                          // symetrie par l'axe Y
    assert(b.count() == 3);
    auto* mirrored = dynamic_cast<geom::LineEntity*>(b.last());
    assert(mirrored && near(mirrored->start().x_, -100, 1e-6));

    // Sans selection, la commande fait d'abord designer ses objets a la souris
    // (les coordonnees tapees ne designent rien), puis Entree valide.
    b.clearSelection();
    b.tool(ToolMode::Copy);
    assert(b.viewport.prompt().contains("sélectionnez les objets"));
    b.type("0,0"); b.type("1,1");
    assert(b.count() == 3);
    b.key(Qt::Key_Return);                                 // rien de designe : on reste a designer
    assert(b.viewport.prompt().contains("sélectionnez les objets"));
    const geom::Point2 mid{(mirrored->start().x_ + mirrored->end().x_) / 2,
                           (mirrored->start().y_ + mirrored->end().y_) / 2};
    b.click(mid.x_, mid.y_);
    assert(mirrored->selected);
    assert(b.viewport.prompt().contains("1 sélectionné"));
    b.click(mid.x_, mid.y_, Qt::RightButton);              // clic droit = Entree
    assert(b.viewport.prompt().contains("point de base"));
    b.type("0,0"); b.type("@0,-50");
    assert(b.count() == 4);
    assert(b.viewport.tool() == ToolMode::Select);

    // Au repos, la souris selectionne sans choisir d'outil ; Echap vide la selection.
    b.clearSelection();
    b.click(mid.x_, mid.y_);
    assert(mirrored->selected);
    b.key(Qt::Key_Escape);
    assert(!mirrored->selected);

    // --- Rogner, Prolonger, Scinder ---
    b.reset();
    b.doc.addEntity(std::make_unique<geom::LineEntity>(geom::Point2{0, 0}, geom::Point2{20, 0}));
    b.doc.addEntity(std::make_unique<geom::LineEntity>(geom::Point2{10, -5}, geom::Point2{10, 5}));
    b.tool(ToolMode::Trim);
    b.type("15,0");                                        // garde le cote gauche
    line = dynamic_cast<geom::LineEntity*>(b.last());
    assert(line && near(line->start().x_, 0) && near(line->end().x_, 10));
    b.stack.undo();
    assert(b.count() == 2);
    b.stack.redo();

    b.reset();
    b.doc.addEntity(std::make_unique<geom::LineEntity>(geom::Point2{0, 0}, geom::Point2{5, 0}));
    b.doc.addEntity(std::make_unique<geom::LineEntity>(geom::Point2{10, -5}, geom::Point2{10, 5}));
    b.tool(ToolMode::Extend);
    b.type("4,0");
    line = dynamic_cast<geom::LineEntity*>(b.last());
    assert(line && near(line->end().x_, 10));

    b.reset();
    b.doc.addEntity(std::make_unique<geom::LineEntity>(geom::Point2{0, 0}, geom::Point2{10, 0}));
    b.tool(ToolMode::Break);
    b.type("4,0");
    assert(b.count() == 2);
    b.stack.undo();
    assert(b.count() == 1);

    // Rogner sans arete de coupe : message, pas de boite modale.
    b.lastMessage.clear();
    b.tool(ToolMode::Trim);
    b.type("5,0");
    assert(b.count() == 1 && !b.lastMessage.isEmpty());

    // --- Supprimer, Exploser, Joindre ---
    b.reset();
    b.tool(ToolMode::Rectangle);
    b.type("0,0"); b.type("4,3");
    b.selectAll();
    b.viewport.explodeSelected();
    assert(b.count() == 4);
    b.selectAll();
    b.viewport.joinSelected();
    assert(b.count() == 1);
    b.selectAll();
    b.viewport.deleteSelected();
    assert(b.count() == 0);
    b.stack.undo();
    assert(b.count() == 1);

    // --- Saisie d'un contour pour une commande de module (PickPolygon) ---
    {
        std::vector<geom::Point2> received;
        int calls = 0;
        auto done = [&](std::vector<geom::Point2> v) { received = std::move(v); ++calls; };
        const size_t before = b.count();
        b.viewport.capturePolygon(QStringLiteral("Contour du module"), done);
        assert(b.viewport.prompt().contains(QStringLiteral("Contour du module")));
        b.type("0,0"); b.type("@10,0");
        b.key(Qt::Key_Return);                       // deux sommets : refuse, on reste
        assert(calls == 0 && !b.lastMessage.isEmpty());
        assert(b.viewport.tool() == ToolMode::CapturePolygon);
        b.type("@0,5");
        b.key(Qt::Key_Return);
        assert(calls == 1 && received.size() == 3);
        assert(near(received[1].x_, 10) && near(received[2].y_, 5));
        assert(b.count() == before);                 // le canevas ne cree rien lui-meme
        assert(b.viewport.tool() == ToolMode::Select);
        // Entree au repos ne relance pas une saisie de contour.
        b.key(Qt::Key_Return);
        assert(b.viewport.tool() != ToolMode::CapturePolygon);
        b.key(Qt::Key_Escape);
        // « C » ferme aussi ; Echap renonce sans rappel.
        b.viewport.capturePolygon(QString(), done);
        b.type("0,0"); b.type("4,0"); b.type("4,4"); b.type("C");
        assert(calls == 2 && received.size() == 3);
        b.viewport.capturePolygon(QString(), done);
        b.type("0,0"); b.type("4,0"); b.type("4,4");
        b.key(Qt::Key_Escape);
        assert(calls == 2 && b.viewport.tool() == ToolMode::Select);
    }

    // --- Texte (D-01) : point, hauteur, angle, lignes jusqu'a une ligne vide ---
    {
        b.reset();
        b.tool(ToolMode::Text);
        assert(b.viewport.prompt().contains("point de départ"));
        b.type("0,0");
        assert(b.viewport.prompt().contains("hauteur"));
        b.type("abc");                                     // refuse, on reste sur la hauteur
        assert(!b.lastMessage.isEmpty() && b.viewport.prompt().contains("hauteur"));
        b.type("5");
        assert(b.viewport.prompt().contains("angle"));
        b.type("");                                        // angle par defaut : 0
        assert(b.viewport.prompt().contains("tapez le texte"));
        b.type("Bonjour");
        b.type("Ligne 2");
        assert(b.count() == 2);
        auto* first = dynamic_cast<geom::TextEntity*>(b.doc.entities()[0].get());
        auto* second = dynamic_cast<geom::TextEntity*>(b.doc.entities()[1].get());
        assert(first && first->text() == "Bonjour" && near(first->height(), 5) && near(first->rotation(), 0));
        assert(second && near(second->position().x_, 0) && near(second->position().y_, -7.5));  // 1,5 h plus bas
        b.type("");                                        // ligne vide : fin
        assert(b.viewport.tool() == ToolMode::Select && b.count() == 2);
        assert(b.stack.undoText().contains("Texte"));

        // Hauteur gardee, angle tape en degres ; ligne suivante perpendiculaire.
        b.tool(ToolMode::Text);
        b.type("10,10");
        b.key(Qt::Key_Return);                             // garde 5
        b.type("90");
        b.type("V");
        b.type("W");
        auto* v = dynamic_cast<geom::TextEntity*>(b.doc.entities()[2].get());
        auto* w = dynamic_cast<geom::TextEntity*>(b.doc.entities()[3].get());
        assert(v && near(v->height(), 5) && near(v->rotation(), std::numbers::pi / 2));
        assert(w && near(w->position().x_, 17.5, 1e-9) && near(w->position().y_, 10, 1e-9));
        b.key(Qt::Key_Escape);

        // Modifier le texte (D-01b) : commande annulable.
        b.stack.push(new app::SetTextCommand(&b.doc, first, "Bonsoir", QStringLiteral("Modifier le texte")));
        assert(first->text() == "Bonsoir");
        b.stack.undo();
        assert(first->text() == "Bonjour");
        b.stack.redo();
        assert(first->text() == "Bonsoir");
    }

    // --- Import annulable (RecordedAdditionCommand) ---
    {
        b.reset();
        const int a = b.doc.addEntity(std::make_unique<geom::PointEntity>(geom::Point2{1, 1}))->id();
        const int c = b.doc.addEntity(std::make_unique<geom::PointEntity>(geom::Point2{2, 2}))->id();
        b.stack.push(new app::RecordedAdditionCommand(&b.doc, {a, c}, QStringLiteral("Importer")));
        assert(b.count() == 2);                  // le push ne duplique rien
        b.stack.undo();
        assert(b.count() == 0);
        b.stack.redo();
        assert(b.count() == 2 && b.doc.findEntity(a) && b.doc.findEntity(c));   // memes ids
    }

    // --- Echap termine la commande en cours et revient au repos ---
    b.tool(ToolMode::Line);
    b.type("0,0");
    b.key(Qt::Key_Escape);
    assert(b.viewport.tool() == ToolMode::Select);
    assert(b.viewport.prompt().contains("Sélectionnez des objets"));

    std::printf("Outils du viewport : tests PASSED\n");
    return 0;
}
