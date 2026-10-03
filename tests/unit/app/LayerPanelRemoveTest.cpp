// Suppression d'un calque depuis le panneau Calques, sur le vrai widget.
//
// Regression B-02 : supprimer un calque laissait ses objets sur un calque qui
// n'existait plus. Comme dans AutoCAD, un calque occupe ne se supprime pas ;
// le refus passe par un message non bloquant, un calque vide part normalement.

#include "LayerPanel.h"

#include "bcad/core/Document.h"
#include "bcad/geometry/Line.h"

#include <QApplication>
#include <QPushButton>
#include <QTreeWidget>

#include <cassert>
#include <cstdio>
#include <memory>

using namespace bcad;

namespace {

QTreeWidgetItem* rowFor(QTreeWidget* tree, const QString& name) {
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = tree->topLevelItem(i);
        for (int c = 0; c < tree->columnCount(); ++c)
            if (item->data(c, Qt::UserRole).toString() == name) return item;
    }
    return nullptr;
}

QPushButton* buttonWithText(QWidget& parent, const QString& text) {
    for (auto* button : parent.findChildren<QPushButton*>())
        if (button->text() == text) return button;
    return nullptr;
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    core::Document doc;
    doc.layerManager().createLayer("Occupe");
    doc.layerManager().createLayer("Vide");
    auto* line = doc.addEntity(std::make_unique<geom::LineEntity>(geom::Point2{0, 0}, geom::Point2{1, 0}));
    line->setLayer("Occupe");

    app::LayerPanel panel;
    panel.setDocument(&doc);
    QString message;
    QObject::connect(&panel, &app::LayerPanel::statusMessage,
                     [&](const QString& m) { message = m; });

    auto* tree = panel.findChild<QTreeWidget*>();
    QPushButton* remove = buttonWithText(panel, QStringLiteral("- Calque"));
    assert(tree && remove);

    // Calque occupe : refuse, l'objet garde un calque existant.
    QTreeWidgetItem* busy = rowFor(tree, QStringLiteral("Occupe"));
    assert(busy);
    tree->setCurrentItem(busy);
    remove->click();
    assert(doc.layerManager().find("Occupe") != nullptr);
    assert(line->layer() == "Occupe");
    assert(message.contains(QStringLiteral("Occupe")) && message.contains(QStringLiteral("1 objet")));

    // Calque 0 : jamais supprime, et l'operateur sait pourquoi.
    message.clear();
    tree->setCurrentItem(rowFor(tree, QStringLiteral("0")));
    remove->click();
    assert(doc.layerManager().find("0") != nullptr && !message.isEmpty());

    // Calque vide : supprime sans question.
    message.clear();
    QTreeWidgetItem* empty = rowFor(tree, QStringLiteral("Vide"));
    assert(empty);
    tree->setCurrentItem(empty);
    remove->click();
    assert(doc.layerManager().find("Vide") == nullptr);
    assert(message.isEmpty());

    std::printf("Panneau Calques, suppression : tests PASSED\n");
    return 0;
}
