// Les outils interactifs : une description, trois lectures.
//
// Un outil de dessin ou de modification se declare une seule fois, dans la table
// kTools ci-dessous. Le menu, le bouton du ruban et l'etiquette de la barre
// d'etat en sont trois lectures et non trois descriptions : c'est le partage de
// l'objet QAction qui fait qu'un choix dans le menu coche le bouton du ruban.
// La table reste privee de cette unite de traduction ; exposee dans un en-tete,
// son `const` lui donnerait une copie par TU et la synchronisation serait
// perdue sans erreur de compilation.

#include "MainWindow.h"

#include "ActionIcons.h"
#include "Viewport.h"

#include <QAction>
#include <QActionGroup>
#include <QLabel>
#include <QMenu>

#include <initializer_list>
#include <optional>
#include <utility>

namespace bcad::app {

// Description d'un outil interactif. Un outil n'existe qu'ici : le menu, le
// bouton du ruban et l'etiquette de la barre d'etat en sont trois lectures. Ils
// etaient decrits a trois endroits differents, ce qui laissait le ruban afficher
// un outil coche pendant que le menu en activait un autre.
// La table doit couvrir chaque ToolMode : Viewport.cpp a un switch exhaustif sur
// l'enum, donc un outil ajoute sans entree ici casse la compilation.
struct ToolSpec {
    ToolMode mode;
    const char* label;        // libelle court : boutons de ruban et entrees de menu
    const char* detail;       // libelle complet : etiquette d'etat et infobulle
    const char* themeIcon;
    QStyle::StandardPixmap fallback;
    // Les alias clavier (C-01) ne sont pas ici : ToolAliases.h, lue aussi
    // par le canevas.
};

const ToolSpec kTools[] = {
    {ToolMode::Select,     "Sélection", "Sélection",             "cursor-arrow",           QStyle::SP_FileDialogContentsView},
    {ToolMode::Move,       "Déplacer",  "Déplacer",              "transform-move",         QStyle::SP_ArrowRight},
    {ToolMode::Copy,       "Copier",    "Copier",                "edit-copy",              QStyle::SP_FileDialogContentsView},
    {ToolMode::Rotate,     "Rotation",  "Rotation",               "object-rotate-right",    QStyle::SP_BrowserReload},
    {ToolMode::Scale,      "Échelle",   "Échelle",               "transform-scale",        QStyle::SP_ArrowUp},
    {ToolMode::Mirror,     "Miroir",    "Miroir",              "object-flip-horizontal", QStyle::SP_ArrowLeft},
    {ToolMode::Trim,       "Ajuster",   "Ajuster (rogner)",                "edit-cut",               QStyle::SP_LineEditClearButton},
    {ToolMode::Extend,     "Prolonger", "Prolonger",             "go-next",                QStyle::SP_ArrowRight},
    {ToolMode::Break,      "Coupure",   "Coupure",               "edit-split",             QStyle::SP_BrowserStop},
    {ToolMode::Fillet,     "Raccord",   "Raccord",               "draw-fillet",            QStyle::SP_BrowserReload},
    {ToolMode::Chamfer,    "Chanfrein", "Chanfrein",             "draw-chamfer",           QStyle::SP_BrowserReload},
    {ToolMode::Offset,     "Décaler",   "Décaler",               "modify-offset",          QStyle::SP_ArrowRight},
    {ToolMode::Stretch,    "Étirer",    "Étirer",                "modify-stretch",         QStyle::SP_ArrowRight},
    {ToolMode::Array,      "Réseau",    "Réseau",                "modify-array",           QStyle::SP_FileDialogListView},
    {ToolMode::Polygon,    "Polygone",  "Polygone",              "draw-polygon",           QStyle::SP_FileDialogListView},
    {ToolMode::Hatch,      "Hachures",  "Hachures",              "draw-hatch",             QStyle::SP_FileDialogDetailedView},
    {ToolMode::Line,       "Ligne",     "Ligne",                 "draw-line",              QStyle::SP_LineEditClearButton},
    {ToolMode::Circle,     "Centre, rayon",    "Cercle centre, rayon",    "draw-circle", QStyle::SP_DialogYesButton},
    {ToolMode::CircleDiameter, "Centre, diamètre", "Cercle centre, diamètre", "draw-circle", QStyle::SP_DialogYesButton},
    {ToolMode::Circle2P,   "2 points",         "Cercle 2 points",         "draw-circle", QStyle::SP_DialogYesButton},
    {ToolMode::Circle3P,   "3 points",         "Cercle 3 points",         "draw-circle", QStyle::SP_DialogYesButton},
    {ToolMode::Arc3P,      "3 points",         "Arc 3 points",            "draw-arc",    QStyle::SP_BrowserReload},
    {ToolMode::Arc,        "Centre, début, fin", "Arc centre, début, fin", "draw-arc",   QStyle::SP_BrowserReload},
    {ToolMode::Polyline,   "Polyligne", "Polyligne",             "draw-polyline",          QStyle::SP_FileDialogListView},
    {ToolMode::Rectangle,  "Rectangle", "Rectangle",             "draw-rectangle",         QStyle::SP_FileDialogDetailedView},
    {ToolMode::Point,      "Point",     "Point",                 "draw-point",             QStyle::SP_DialogOkButton},
    {ToolMode::Text,       "Texte",     "Texte sur une ligne",   "draw-text",              QStyle::SP_FileDialogDetailedView},
    {ToolMode::DimensionLinear,   "Linéaire",  "Cotation linéaire",    "measure",           QStyle::SP_LineEditClearButton},
    {ToolMode::DimensionAligned,  "Alignée",   "Cotation alignée",     "measure-aligned",   QStyle::SP_LineEditClearButton},
    {ToolMode::DimensionAngular,  "Angulaire", "Cotation angulaire",   "measure-angle",     QStyle::SP_BrowserReload},
    {ToolMode::DimensionRadius,   "Rayon",     "Cotation de rayon",    "measure-radius",    QStyle::SP_DialogYesButton},
    {ToolMode::DimensionDiameter, "Diamètre",  "Cotation de diamètre", "measure-diameter",  QStyle::SP_DialogYesButton},
    {ToolMode::Measure,           "Distance",  "Mesurer distance et angle", "measure-distance",      QStyle::SP_FileDialogInfoView},
    {ToolMode::Area,              "Aire",      "Aire et périmètre", "measure-area",      QStyle::SP_FileDialogInfoView},
    {ToolMode::Coordinates,       "ID point",  "Coordonnées d'un point", "measure-coordinates",  QStyle::SP_FileDialogInfoView},
    {ToolMode::ZoomWindow,        "Zoom fenêtre", "Zoom fenêtre",      "zoom-select",       QStyle::SP_FileDialogContentsView},
    {ToolMode::Pan,               "Panoramique", "Panoramique",        "transform-browse",  QStyle::SP_ArrowUp},
    // Pas de bouton ni de menu : ce mode n'est ouvert que par une commande de
    // module (WorkbenchParams::PickPolygon) ; l'entree donne son etiquette d'etat.
    {ToolMode::CapturePolygon,    "Contour",   "Saisie d'un contour",  "draw-polyline",     QStyle::SP_FileDialogListView},
};

const ToolSpec* toolSpec(ToolMode mode) {
    for (const auto& spec : kTools)
        if (spec.mode == mode) return &spec;
    return nullptr;
}

// Une entree de kTools donne une seule QAction, parente de la fenetre et partagee
// par le menu, le ruban et la barre d'acces rapide. Le groupe rend les outils
// exclusifs : le point d'entree de l'etat coche est unique, donc un clic dans le
// menu coche le bouton du ruban.
void MainWindow::buildToolActions() {
    auto* group = new QActionGroup(this);
    for (const auto& spec : kTools) {
        auto* action = new QAction(tr(spec.label), this);
        action->setCheckable(true);
        action->setActionGroup(group);
        action->setToolTip(tr(spec.detail));
        const ToolMode mode = spec.mode;
        connect(action, &QAction::triggered, this, [this, mode] { viewport_->setTool(mode); });
        setActionIcon(this, action, spec.fallback, QString::fromLatin1(spec.themeIcon));
        toolActions_.emplace_back(mode, action);
    }
    if (auto* select = toolAction(ToolMode::Select)) select->setChecked(true);
}

QAction* MainWindow::toolAction(ToolMode mode) const {
    for (const auto& [specMode, action] : toolActions_)
        if (specMode == mode) return action;
    return nullptr;
}

void MainWindow::addToolActions(QMenu* menu, std::initializer_list<ToolMode> modes) {
    for (const auto mode : modes) {
        if (auto* action = toolAction(mode)) menu->addAction(action);
    }
}

QList<QAction*> MainWindow::toolActionsFor(std::initializer_list<ToolMode> modes) const {
    QList<QAction*> actions;
    for (const auto mode : modes) {
        if (auto* action = toolAction(mode)) actions.push_back(action);
    }
    return actions;
}

void MainWindow::onToolChanged(ToolMode mode) {
    const ToolSpec* spec = toolSpec(mode);
    toolLabel_->setText(spec ? tr(spec->detail) : tr("Outil inconnu"));
    // Le changement peut venir du clavier ou du ruban : l'etat coche est
    // partage, il se refllete donc dans les deux lectures a la fois.
    if (auto* action = toolAction(mode)) action->setChecked(true);
}

} // namespace bcad::app
