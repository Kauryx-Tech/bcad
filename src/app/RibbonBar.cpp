#include "RibbonBar.h"

#include <QAction>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace bcad::app {

namespace {

constexpr int kSmallRows = 3;

QToolButton* makeButton(QAction* action, QWidget* parent, bool large) {
    auto* button = new QToolButton(parent);
    button->setDefaultAction(action);
    button->setAutoRaise(true);
    button->setToolTip(action->toolTip().remove('&'));
    if (large) {
        button->setObjectName("ribbonLargeButton");
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIconSize(QSize(32, 32));
        // Le sizeHint de QToolButton ignore le padding de la feuille de style :
        // sans cette largeur mesuree, « Polyligne » s'affichait « Po…ne ».
        const int textWidth = QFontMetrics(button->font()).horizontalAdvance(action->iconText());
        button->setMinimumWidth(std::max(56, textWidth + 20));
        button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    } else {
        button->setObjectName("ribbonSmallButton");
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIconSize(QSize(16, 16));
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    // QUndoStack réécrit sans cesse le libellé d'Annuler/Rétablir en
    // « Annuler <dernière commande> », arbitrairement long : on plafonne ces
    // deux-là plutôt que de laisser un bouton dévorer le panneau.
    if (action->text().contains("Annuler") || action->text().contains("Rétablir")
        || action->text().contains("Undo") || action->text().contains("Redo")) {
        button->setMaximumWidth(large ? 96 : 140);
    }
    return button;
}

} // namespace

RibbonBar::RibbonBar(QWidget* parent) : QTabWidget(parent) {
    setObjectName("ribbon");
    setDocumentMode(true);
    setTabPosition(QTabWidget::North);
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void RibbonBar::setApplicationMenu(QMenu* menu) {
    auto* button = new QToolButton(this);
    button->setObjectName("ribbonAppButton");
    button->setIcon(QIcon(QStringLiteral(":/icons/bcad-logo.svg")));
    button->setIconSize(QSize(22, 22));
    button->setPopupMode(QToolButton::InstantPopup);
    button->setMenu(menu);
    button->setToolTip(menu->title().remove('&'));
    setCornerWidget(button, Qt::TopLeftCorner);
}

QHBoxLayout* RibbonBar::layoutForTab(const QString& tabName) {
    auto it = tabLayouts_.find(tabName);
    if (it != tabLayouts_.end()) return it.value();

    // Un onglet trop large pour la fenetre defile au lieu d'imposer sa largeur
    // a toute la fenetre : un ecran 1366 x 768 (poste de reference, ADR-016)
    // ne pouvait plus afficher l'onglet « Accueil » sans deborder.
    auto* area = new QScrollArea(this);
    area->setObjectName("ribbonScroll");
    area->setFrameShape(QFrame::NoFrame);
    area->setWidgetResizable(true);
    area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* page = new QWidget(area);
    page->setObjectName("ribbonPage");
    auto* layout = new QHBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addStretch(1); // les panneaux se regroupent à gauche
    area->setWidget(page);

    tabLayouts_.insert(tabName, layout);
    tabAreas_.insert(tabName, area);
    addTab(area, tabName);
    return layout;
}

void RibbonBar::addPanel(const QString& tabName, const QString& panelTitle,
                         const QList<QAction*>& actions, int largeCount) {
    QHBoxLayout* tabLayout = layoutForTab(tabName);
    if (largeCount < 0 || largeCount > actions.size()) largeCount = actions.size();

    auto* panel = new QFrame(this);
    panel->setObjectName("ribbonPanel");
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(0);

    auto* body = new QWidget(panel);
    auto* bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(4, 4, 4, 2);
    bodyLayout->setSpacing(2);

    for (int i = 0; i < largeCount; ++i)
        bodyLayout->addWidget(makeButton(actions[i], body, true));

    QGridLayout* grid = nullptr;
    for (int i = largeCount; i < actions.size(); ++i) {
        if (!grid) {
            grid = new QGridLayout();
            grid->setContentsMargins(0, 0, 0, 0);
            grid->setHorizontalSpacing(2);
            grid->setVerticalSpacing(1);
            bodyLayout->addLayout(grid);
        }
        const int index = i - largeCount;
        grid->addWidget(makeButton(actions[i], body, false), index % kSmallRows, index / kSmallRows);
    }
    if (grid) {
        // Une colonne incomplète reste alignée en haut, comme dans AutoCAD.
        grid->setRowStretch(kSmallRows, 1);
    }

    auto* caption = new QLabel(panelTitle, panel);
    caption->setObjectName("ribbonPanelCaption");
    caption->setAlignment(Qt::AlignCenter);

    panelLayout->addWidget(body, 1);
    panelLayout->addWidget(caption);

    tabLayout->insertWidget(tabLayout->count() - 1, panel);

    // Hauteur fixe : celle des panneaux plus la place de la barre de defilement,
    // pour que son apparition ne rogne pas le bas des panneaux.
    QScrollArea* area = tabAreas_.value(tabName);
    const int height = area->widget()->sizeHint().height()
                       + area->horizontalScrollBar()->sizeHint().height();
    area->setFixedHeight(height);
}

} // namespace bcad::app
