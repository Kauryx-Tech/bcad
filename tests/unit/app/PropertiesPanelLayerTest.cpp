// Liste des calques du panneau Propriétés, sur le vrai widget (hors écran).
//
// Régression B-01 : pour une sélection sur plusieurs calques, l'entrée en tête
// « (Mixte) » est un espace réservé. Le garde comparait le texte à « (Mixed) » :
// il ne protégeait rien, et choisir l'entrée déplaçait la sélection sur un
// calque inexistant nommé « (Mixte) ».

#include "widgets/PropertiesPanel.h"

#include "bcad/core/Document.h"
#include "bcad/geometry/Line.h"
#include "bcad/properties/PropertyMap.h"

#include <QApplication>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
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

    // Sans selection : les attributs du dessin, sous le libelle declare par
    // leur module, modifiables et annulables (cartouche du dossier).
    for (const auto& e : doc.entities()) e->selected = false;
    doc.properties().addString("module.dossier.geometre", "")->setDescription("Géomètre");
    panel.refresh();
    QLineEdit* field = nullptr;
    for (auto* edit : panel.findChildren<QLineEdit*>())
        if (edit->isVisibleTo(&panel)) field = edit;
    QLabel* label = nullptr;
    for (auto* form : panel.findChildren<QFormLayout*>())
        if (auto* l = qobject_cast<QLabel*>(form->labelForField(field))) label = l;
    assert(field);
    assert(label && label->text() == QStringLiteral("Géomètre"));
    const int before = stack.count();
    field->setText(QStringLiteral("Kossi A."));
    emit field->editingFinished();
    assert(doc.properties().getString("module.dossier.geometre") == "Kossi A.");
    assert(stack.count() == before + 1);
    stack.undo();
    assert(doc.properties().getString("module.dossier.geometre").empty());

    std::printf("Panneau Proprietes, calques : tests PASSED\n");
    return 0;
}
