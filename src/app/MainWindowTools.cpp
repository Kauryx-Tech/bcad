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
    // Alias de commande clavier (C-01) : tape au repos, il lance l'outil.
    // Seuls les outils livres ont un alias ; un outil a venir (DECALER,
    // RACCORD, RESEAU, HACHURES…) n'en recoit qu'avec son bouton, sinon la
    // commande accepterait un outil qui n'existe pas encore.
    const char* alias = nullptr;
};

const ToolSpec kTools[] = {
    {ToolMode::Select,     "Sélection", "Sélection",             "cursor-arrow",           QStyle::SP_FileDialogContentsView},
    {ToolMode::Move,       "Déplacer",  "Déplacer",              "transform-move",         QStyle::SP_ArrowRight,          "M"},
    {ToolMode::Copy,       "Copier",    "Copier",                "edit-copy",              QStyle::SP_FileDialogContentsView, "CO"},
    {ToolMode::Rotate,     "Tourner",   "Tourner",               "object-rotate-right",    QStyle::SP_BrowserReload,       "RO"},
    {ToolMode::Scale,      "Échelle",   "Échelle",               "transform-scale",        QStyle::SP_ArrowUp,             "SC"},
    {ToolMode::Mirror,     "Symétrie",  "Symétrie",              "object-flip-horizontal", QStyle::SP_ArrowLeft,           "MI"},
    {ToolMode::Trim,       "Rogner",    "Rogner",                "edit-cut",               QStyle::SP_LineEditClearButton, "TR"},
    {ToolMode::Extend,     "Prolonger", "Prolonger",             "go-next",                QStyle::SP_ArrowRight,          "EX"},
    {ToolMode::Break,      "Scinder",   "Scinder",               "edit-split",             QStyle::SP_BrowserStop,         "BR"},
    {ToolMode::Fillet,     "Raccord",   "Raccord",               "draw-fillet",            QStyle::SP_BrowserReload,       "RA"},
    {ToolMode::Line,       "Ligne",     "Ligne",                 "draw-line",              QStyle::SP_LineEditClearButton, "L"},
    {ToolMode::Circle,     "Cercle",    "Cercle",                "draw-circle",            QStyle::SP_DialogYesButton,     "C"},
    {ToolMode::Arc,        "Arc",       "Arc",                   "draw-arc",               QStyle::SP_BrowserReload,       "A"},
    {ToolMode::Polyline,   "Polyligne", "Polyligne",             "draw-polyline",          QStyle::SP_FileDialogListView,  "PL"},
    {ToolMode::Rectangle,  "Rectangle", "Rectangle",             "draw-rectangle",         QStyle::SP_FileDialogDetailedView, "REC"},
    {ToolMode::Point,      "Point",     "Point",                 "draw-point",             QStyle::SP_DialogOkButton,      "PO"},
    {ToolMode::Text,       "Texte",     "Texte sur une ligne",   "draw-text",              QStyle::SP_FileDialogDetailedView, "T"},
    {ToolMode::DimensionLinear,   "Linéaire",  "Cotation linéaire",    "measure",           QStyle::SP_LineEditClearButton, "DLI"},
    {ToolMode::DimensionAligned,  "Alignée",   "Cotation alignée",     "measure-aligned",   QStyle::SP_LineEditClearButton, "DAL"},
    {ToolMode::DimensionAngular,  "Angulaire", "Cotation angulaire",   "measure-angle",     QStyle::SP_BrowserReload,      "DAN"},
    {ToolMode::DimensionRadius,   "Rayon",     "Cotation de rayon",    "measure-radius",    QStyle::SP_DialogYesButton,     "DRA"},
    {ToolMode::DimensionDiameter, "Diamètre",  "Cotation de diamètre", "measure-diameter",  QStyle::SP_DialogYesButton,     "DDI"},
    {ToolMode::Measure,           "Mesurer",   "Mesurer distance et angle", "measure",      QStyle::SP_FileDialogInfoView,  "DI"},
    {ToolMode::Area,              "Surface",   "Surface et périmètre", "measure-area",      QStyle::SP_FileDialogInfoView,  "AA"},
    {ToolMode::Coordinates,       "Coordonnées", "Coordonnées d'un point", "measure-point",  QStyle::SP_FileDialogInfoView,  "ID"},
    // Pas de bouton ni de menu : ce mode n'est ouvert que par une commande de
    // module (WorkbenchParams::PickPolygon) ; l'entree donne son etiquette d'etat.
    {ToolMode::CapturePolygon,    "Contour",   "Saisie d'un contour",  "draw-polyline",     QStyle::SP_FileDialogListView},
};

const ToolSpec* toolSpec(ToolMode mode) {
    for (const auto& spec : kTools)
        if (spec.mode == mode) return &spec;
    return nullptr;
}

// Recherche d'un outil par son alias de commande clavier (C-01), insensible
// à la casse. Sans alias (Sélection, Contour) ou nom inconnu : rien.
std::optional<ToolMode> toolModeForAlias(const QString& text) {
    if (text.isEmpty())
        return std::nullopt;

    // Extraire le premier token
    QString token = text.trimmed();
    int sepIdx = token.indexOf(QChar(' '));
    int tabIdx = token.indexOf(QChar('\t'));
    if (tabIdx >= 0 && (sepIdx < 0 || tabIdx < sepIdx)) sepIdx = tabIdx;
    if (sepIdx >= 0)
        token = token.left(sepIdx);

    if (token.isEmpty())
        return std::nullopt;

    token = token.toUpper();

    // Longest-match d'abord
    if (token == "DLI") return ToolMode::DimensionLinear;
    if (token == "MEA") return ToolMode::Measure;
    if (token == "DI")  return ToolMode::Measure;
    if (token == "AA")  return ToolMode::Area;
    if (token == "ID")  return ToolMode::Coordinates;
    if (token == "RAC") return ToolMode::Fillet;
    if (token == "RA")  return ToolMode::Fillet;
    if (token == "F")   return ToolMode::Fillet;
    if (token == "DAL") return ToolMode::DimensionAligned;
    if (token == "DAN") return ToolMode::DimensionAngular;
    if (token == "DRA") return ToolMode::DimensionRadius;
    if (token == "DDI") return ToolMode::DimensionDiameter;
    if (token == "REC") return ToolMode::Rectangle;
    if (token == "CHA") return ToolMode::Chamfer;
    if (token == "POL") return ToolMode::Polygon;
    if (token == "PL")  return ToolMode::Polyline;
    if (token == "PO")  return ToolMode::Point;
    if (token == "CO")  return ToolMode::Copy;
    if (token == "RO")  return ToolMode::Rotate;
    if (token == "SC")  return ToolMode::Scale;
    if (token == "MI")  return ToolMode::Mirror;
    if (token == "TR")  return ToolMode::Trim;
    if (token == "EX")  return ToolMode::Extend;
    if (token == "BR")  return ToolMode::Break;
    if (token == "AR")  return ToolMode::Array;
    if (token == "L")   return ToolMode::Line;
    if (token == "C")   return ToolMode::Circle;
    if (token == "A")   return ToolMode::Arc;
    if (token == "T")   return ToolMode::Text;
    if (token == "M")   return ToolMode::Move;
    if (token == "O")   return ToolMode::Offset;
    if (token == "S")   return ToolMode::Stretch;
    if (token == "H")   return ToolMode::Hatch;
    if (token == "J")   return ToolMode::Join;
    if (token == "X")   return ToolMode::Explode;
    if (token == "E")   return ToolMode::Erase;

    return std::nullopt;
}

namespace {
// Outils pas encore livrés, par alias de commande (C-01) : taper l'alias
// annonce l'outil et sa tâche au lieu de ne rien faire. L'alias migre dans
// ToolSpec avec le bouton, le jour de la livraison. « F » n'y figure pas :
// la touche sert déjà au zoom sur tout (V-21).
constexpr struct {
    const char* alias;
    const char* detail;
} kFutureTools[] = {
    {"O", "Décaler (M-01)"},
    {"S", "Étirer (M-03)"},
    {"CHA", "Chanfrein (M-07)"},
    {"AR", "Réseau (M-06)"},
    {"H", "Hachures (D-02)"},
    {"POL", "Polygone (D-06)"},
    {"J", "Joindre (M-08)"},
    {"X", "Éclater (E-01)"},
    {"E", "Effacer (E-02)"},
};
} // namespace

const char* futureToolForAlias(const QString& name) {
    for (const auto& tool : kFutureTools) {
        if (name.compare(QString::fromLatin1(tool.alias), Qt::CaseInsensitive) == 0)
            return tool.detail;
    }
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
