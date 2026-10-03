// Liste des calques du panneau Propriétés, sur le vrai widget (hors écran).
//
// Régression B-01 : pour une sélection sur plusieurs calques, l'entrée en tête
// « (Mixte) » est un espace réservé. Le garde comparait le texte à « (Mixed) » :
// il ne protégeait rien, et choisir l'entrée déplaçait la sélection sur un
// calque inexistant nommé « (Mixte) ».

#include "PropertiesPanel.h"

#include "bcad/core/Document.h"
#include "bcad/geometry/Line.h"

#include <QApplication>
#include <QComboBox>
#include <QSignalBlocker>
#include <QUndoStack>

#include <cassert>
#include <cstdio>
#include <memory>

using namespace bcad;

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    core::Document doc;
    doc.layerManager().createLayer("A");
    doc.layerManager().createLayer("B");
    auto* first = doc.addEntity(std::make_unique<geom::LineEntity>(geom::Point2{0, 0}, geom::Point2{1, 0}));
    auto* second = doc.addEntity(std::make_unique<geom::LineEntity>(geom::Point2{0, 1}, geom::Point2{1, 1}));
    first->setLayer("A");
    second->setLayer("B");
    first->selected = second->selected = true;

    QUndoStack stack;
    app::PropertiesPanel panel;
    panel.setDocument(&doc);
    panel.setUndoStack(&stack);
    panel.refresh();

    auto* combo = panel.findChild<QComboBox*>();
    assert(combo);
    assert(combo->currentIndex() == 0 && combo->itemText(0) == QStringLiteral("(Mixte)"));

    // Choisir l'espace reserve ne touche a rien : on quitte l'entree sans
    // signal, puis on y revient pour que le changement soit reellement emis.
    {
        const QSignalBlocker quiet(combo);
        combo->setCurrentIndex(1);
    }
    combo->setCurrentIndex(0);
    assert(first->layer() == "A" && second->layer() == "B");
    assert(stack.count() == 0);

    // Choisir un vrai calque deplace toute la selection, en une etape annulable.
    const int indexB = combo->findText(QStringLiteral("B"));
    assert(indexB > 0);
    combo->setCurrentIndex(indexB);
    assert(doc.findEntity(first->id())->layer() == "B");
    assert(doc.findEntity(second->id())->layer() == "B");
    stack.undo();
    assert(doc.findEntity(first->id())->layer() == "A");

    // Un calque reellement nomme « (Mixte) » reste choisissable.
    doc.layerManager().createLayer("(Mixte)");
    doc.findEntity(first->id())->selected = true;
    doc.findEntity(second->id())->selected = true;
    panel.refresh();
    const int real = combo->findData(QStringLiteral("(Mixte)"));
    assert(real > 0);
    combo->setCurrentIndex(real);
    assert(doc.findEntity(first->id())->layer() == "(Mixte)");

    std::printf("Panneau Proprietes, calques : tests PASSED\n");
    return 0;
}
