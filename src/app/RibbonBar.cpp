#include "bcad/app/RibbonBar.h"

#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

namespace bcad::app {

RibbonBar::RibbonBar(QWidget* parent) : QTabWidget(parent) {
    setDocumentMode(true);
    setTabPosition(QTabWidget::North);
    setFocusPolicy(Qt::NoFocus);
    setMaximumHeight(92); // compact — un ruban en taille normale dominerait une petite fenêtre
}

QHBoxLayout* RibbonBar::layoutForTab(const QString& tabName) {
    auto it = tabLayouts_.find(tabName);
    if (it != tabLayouts_.end()) return it.value();

    auto* page = new QWidget(this);
    auto* layout = new QHBoxLayout(page);
    layout->setContentsMargins(4, 2, 4, 0);
    layout->setSpacing(0);
    layout->addStretch(1); // les panneaux se regroupent à gauche ; le stretch absorbe le reste

    tabLayouts_.insert(tabName, layout);
    addTab(page, tabName);
    return layout;
}

void RibbonBar::addPanel(const QString& tabName, const QString& panelTitle, const QList<QAction*>& actions) {
    QHBoxLayout* tabLayout = layoutForTab(tabName);

    auto* panel = new QFrame(this);
    panel->setObjectName("ribbonPanel");
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(6, 4, 6, 2);
    panelLayout->setSpacing(2);

    auto* buttonRow = new QWidget(panel);
    auto* buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(2);
    for (QAction* action : actions) {
        auto* button = new QToolButton(buttonRow);
        button->setDefaultAction(action);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setAutoRaise(true);
        // Pas de largeur minimale artificielle ici : le sizeHint propre de
        // QToolButton s'ajuste déjà à son libellé, et une largeur minimale
        // plus étroite que ça (le bug précédent) est exactement ce qui
        // faisait que des actions du style "Union"/"Intersection" étaient
        // tronquées en un texte identique et illisible dès qu'un onglet
        // contenait assez de boutons pour dépasser la largeur de la
        // fenêtre. Un plafond *généreux* reste en revanche utile pour les
        // deux libellés structurellement non bornés (Undo/Redo —
        // QUndoStack réécrit sans cesse leur texte en "Undo
        // <dernière commande>", qui peut devenir arbitrairement long) ;
        // les tronquer est attendu/acceptable, contrairement à deux
        // actions fixes différentes qui entrent en collision sur la même
        // chaîne tronquée.
        if (action->text().contains("Undo") || action->text().contains("Redo")) {
            button->setMaximumWidth(96);
        }
        button->setToolTip(action->text().remove('&'));
        buttonLayout->addWidget(button);
    }

    auto* caption = new QLabel(panelTitle, panel);
    caption->setObjectName("ribbonPanelCaption");
    caption->setAlignment(Qt::AlignHCenter);

    panelLayout->addWidget(buttonRow);
    panelLayout->addWidget(caption);

    // Insère avant le stretch final, avec un séparateur devant chaque
    // panneau sauf le premier de cet onglet.
    int insertIndex = tabLayout->count() - 1;
    if (insertIndex > 0) {
        auto* separator = new QFrame(this);
        separator->setObjectName("ribbonSeparator");
        separator->setFrameShape(QFrame::VLine);
        tabLayout->insertWidget(insertIndex++, separator);
    }
    tabLayout->insertWidget(insertIndex, panel);
}

} // namespace bcad::app
