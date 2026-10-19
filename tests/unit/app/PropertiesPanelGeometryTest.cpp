// Palette Propriétés (P-01) : champs géométriques éditables sur le vrai
// widget, hors écran. Écriture en une commande annulable unique, refus sans
// fenêtre, valeurs calculées désactivées, et miroir après annulation.

#include "widgets/PropertiesGeometrySection.h"

#include "detail/GeometryFieldDetail.h"
#include "widgets/PropertiesPanel.h"

#include "bcad/core/Document.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/TextEntity.h"

#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QUndoStack>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <numbers>
#include <string>

using namespace bcad;
using app::PropertiesGeometrySection;
using app::PropertiesPanel;
using app::detail::GeometryField;
using app::geometryFieldObjectName;

namespace {
QLineEdit* field(QWidget* root, GeometryField f) {
    return root->findChild<QLineEdit*>(geometryFieldObjectName(f));
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
geom::LineEntity* lineAt(core::Document& doc, int id) {
    return dynamic_cast<geom::LineEntity*>(doc.findEntity(id));
}
} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    core::Document doc;
    auto* firstLine = doc.addEntity(std::make_unique<geom::LineEntity>(geom::Point2{0, 0}, geom::Point2{10, 0}));
    const int lineId = firstLine->id();
    firstLine->selected = true;

    QUndoStack stack;
    PropertiesPanel panel;
    panel.setDocument(&doc);
    panel.setUndoStack(&stack);
    panel.refresh();

    auto* section = panel.findChild<PropertiesGeometrySection*>();
    assert(section);
    assert(!section->isHidden()); // une ligne expose des champs

    QLabel* info = panel.findChild<QLabel*>("propertiesGeometryInfo");
    assert(info && info->text().contains(QStringLiteral("Longueur")));
    const QString infoBefore = info->text();

    QStringList messages;
    QObject::connect(section, &PropertiesGeometrySection::statusMessage,
                     [&messages](const QString& m) { messages << m; });

    // --- Écriture : un seul pas d'annulation, la ligne est remplacée sur place
    QLineEdit* endY = field(section, GeometryField::EndY);
    QLineEdit* length = field(section, GeometryField::Length);
    assert(endY && length);
    assert(length->text() == QStringLiteral("10"));
    assert(!length->isEnabled()); // valeur calculée : visible, désactivée
    endY->setText(QStringLiteral("5"));
    emit endY->editingFinished();
    assert(lineAt(doc, lineId) && near(lineAt(doc, lineId)->end().y_, 5));
    assert(stack.index() == 1);
    assert(endY->text() == QStringLiteral("5"));
    QCoreApplication::processEvents();
    assert(info->text() != infoBefore); // la ligne d'information suit aussi

    // --- Miroir : annuler remet les valeurs affichées, sans clic sur le canevas
    stack.undo();
    QCoreApplication::processEvents();
    assert(lineAt(doc, lineId) && near(lineAt(doc, lineId)->end().y_, 0));
    assert(endY->text() == QStringLiteral("0"));
    assert(info->text() == infoBefore);

    // --- Retaper ce qui est déjà affiché ne pousse rien dans l'historique
    endY->setText(QStringLiteral("0"));
    emit endY->editingFinished();
    assert(stack.index() == 0);
    assert(messages.isEmpty());

    // --- Saisie non numérique : rien ne bouge, bord rouge, message sobre
    endY->setText(QStringLiteral("12a"));
    emit endY->editingFinished();
    assert(stack.index() == 0);
    assert(lineAt(doc, lineId) && near(lineAt(doc, lineId)->end().y_, 0));
    assert(!messages.isEmpty() && messages.last() == QStringLiteral("Valeur non numérique."));
    assert(!endY->styleSheet().isEmpty());

    // --- Échap : la valeur du document revient, le refus s'efface
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(endY, &escape);
    assert(endY->text() == QStringLiteral("0"));
    assert(endY->styleSheet().isEmpty());
    assert(stack.index() == 0);

    // --- Cercle : diamètre calculé, rayon nul refusé puis accepté
    doc.findEntity(lineId)->selected = false;
    auto* freshCircle = doc.addEntity(std::make_unique<geom::CircleEntity>(geom::Point2{0, 0}, 5));
    const int circleId = freshCircle->id();
    freshCircle->selected = true;
    panel.refresh();
    assert(section->isHidden() == false);
    QLineEdit* radius = field(section, GeometryField::Radius);
    QLineEdit* diameter = field(section, GeometryField::Diameter);
    assert(radius && diameter && radius->text() == QStringLiteral("5"));
    assert(diameter->text() == QStringLiteral("10") && !diameter->isEnabled());
    radius->setText(QStringLiteral("0"));
    emit radius->editingFinished();
    assert(near(dynamic_cast<geom::CircleEntity*>(doc.findEntity(circleId))->radius(), 5));
    assert(stack.index() == 0);
    assert(messages.last() == QStringLiteral("Le rayon doit être supérieur à 0."));
    radius->setText(QStringLiteral("8"));
    emit radius->editingFinished();
    assert(near(dynamic_cast<geom::CircleEntity*>(doc.findEntity(circleId))->radius(), 8));
    assert(stack.index() == 1);
    assert(diameter->text() == QStringLiteral("16")); // relue sur place

    // --- Arc : angles en degrés éditables, longueur d'arc désactivée
    doc.findEntity(circleId)->selected = false;
    auto* freshArc = doc.addEntity(std::make_unique<geom::ArcEntity>(geom::Point2{0, 0}, 2, 0.0, std::numbers::pi / 2));
    const int arcId = freshArc->id();
    freshArc->selected = true;
    panel.refresh();
    QLineEdit* startAngle = field(section, GeometryField::StartAngle);
    QLineEdit* arcLength = field(section, GeometryField::ArcLength);
    assert(startAngle && arcLength && startAngle->text() == QStringLiteral("0"));
    assert(!arcLength->isEnabled());
    startAngle->setText(QStringLiteral("45"));
    emit startAngle->editingFinished();
    const auto* turned = dynamic_cast<geom::ArcEntity*>(doc.findEntity(arcId));
    assert(turned && near(turned->startAngle(), std::numbers::pi / 4));
    assert(stack.index() == 2);

    // --- Polyligne : un sommet à la fois, longueur et aire désactivées
    doc.findEntity(arcId)->selected = false;
    auto* freshPoly = doc.addEntity(std::make_unique<geom::PolylineEntity>(
        std::vector<geom::Point2>{{0, 0}, {10, 0}, {10, 10}, {0, 10}}, true));
    const int polyId = freshPoly->id();
    freshPoly->selected = true;
    panel.refresh();
    auto* selector = section->findChild<QComboBox*>("geometryFieldVertexSelector");
    QLineEdit* vertexX = field(section, GeometryField::VertexX);
    QLineEdit* vertexY = field(section, GeometryField::VertexY);
    QLineEdit* polyLength = field(section, GeometryField::Length);
    QLineEdit* area = field(section, GeometryField::Area);
    assert(selector && selector->count() == 4);
    assert(vertexX && vertexY && polyLength && area);
    assert(!polyLength->isEnabled() && !area->isEnabled());
    assert(area->text() == QStringLiteral("100"));
    selector->setCurrentIndex(2);
    assert(vertexX->text() == QStringLiteral("10") && vertexY->text() == QStringLiteral("10"));
    vertexX->setText(QStringLiteral("12"));
    emit vertexX->editingFinished();
    const auto* polygon = dynamic_cast<geom::PolylineEntity*>(doc.findEntity(polyId));
    assert(polygon && near(polygon->vertices()[2].x_, 12));
    assert(stack.index() == 3);
    assert(area->text() == QStringLiteral("110")); // aire relue après écriture
    stack.undo();
    QCoreApplication::processEvents();
    assert(area->text() == QStringLiteral("100"));
    assert(vertexX->text() == QStringLiteral("10"));

    // --- Point : position seule
    doc.findEntity(polyId)->selected = false;
    auto* freshPoint = doc.addEntity(std::make_unique<geom::PointEntity>(geom::Point2{3, 4}));
    const int pointId = freshPoint->id();
    freshPoint->selected = true;
    panel.refresh();
    QLineEdit* posX = field(section, GeometryField::PositionX);
    assert(posX && posX->text() == QStringLiteral("3"));
    assert(section->findChild<QComboBox*>("geometryFieldVertexSelector") == nullptr);
    posX->setText(QStringLiteral("7"));
    emit posX->editingFinished();
    const auto* moved = dynamic_cast<geom::PointEntity*>(doc.findEntity(pointId));
    assert(moved && near(moved->position().x_, 7));

    // --- Texte : aucun champ propre, la section se masque
    doc.findEntity(pointId)->selected = false;
    auto* freshText = doc.addEntity(std::make_unique<geom::TextEntity>(geom::Point2{1, 1}, std::string("A")));
    freshText->selected = true;
    panel.refresh();
    assert(section->isHidden());

    // --- Deux objets désignés : pas de champs non plus
    freshText->selected = false;
    doc.findEntity(lineId)->selected = true;
    auto* secondLine = doc.addEntity(std::make_unique<geom::LineEntity>(geom::Point2{0, 1}, geom::Point2{1, 1}));
    secondLine->selected = true;
    panel.refresh();
    assert(section->isHidden());
    assert(field(section, GeometryField::EndY) == nullptr);

    std::printf("Panneau Proprietes, champs geometrie : tests PASSED\n");
    return 0;
}
