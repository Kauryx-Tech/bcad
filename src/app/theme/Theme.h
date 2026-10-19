// Design system de BCAD : la seule source des couleurs, tailles et espacements
// de l'interface (docs/DESIGN_SYSTEM.md). La feuille de style (dark.qss) est un
// gabarit qui ne cite que des jetons @nom@ ; le code C++ lit theme::color().
// Une valeur en dur ailleurs est refusee par scripts/check_arch.sh.

#pragma once

#include <QColor>
#include <QString>

namespace bcad::app::theme {

enum class Token {
    // Surfaces, de la plus enfoncee a la plus elevee (theme sombre d'AutoCAD).
    SurfaceSunken, Field, Canvas, Window, ScrollTrack, Toolbar, Caption, Panel,
    PanelAlt, Ribbon,
    // Controles et bordures.
    BorderPanel, Control, Border, ControlHover, ControlRaised, BorderHover, ControlRaisedHover,
    // Textes.
    TextDisabled, TextCaption, TextSecondary, TextDock, TextPrimary, TextStrong,
    // Accent (bleu AutoCAD) et etat coche.
    Accent, AccentSoft,
    // Canevas : texte, apercu d'outil, accrochage, grille, fenetres de selection.
    CanvasText, Preview, PreviewText, Snap, GridDot, GridAxis, WindowSelect, CrossingSelect,
    // Poignees de la selection (M-02) : froide, survolee, chaude (AutoCAD
    // GRIPCOLOR 150, GRIPHOVER 11, GRIPHOT 12).
    Grip, GripHover, GripHot,
    // Liste des calques : ampoule, cadenas.
    LayerOn, LayerOff, LockOn, LockOff, BulbBase,
    // Gravite des diagnostics.
    SeverityError, SeverityWarning, SeverityInfo,
    // Palette des icones SVG (src/app/icons) : check_arch n'y accepte que ces
    // valeurs, lues ici. Geometrie, action, point, danger, succes, contrastes.
    IconGeometry, IconAction, IconPoint, IconDanger, IconSuccess, IconContrastLight, IconContrastDark,
};

// Valeur d'un jeton.
QColor color(Token token);
// Nom du jeton dans la feuille de style (« accent-soft » pour @accent-soft@).
const char* name(Token token);
// Feuille de style de l'application : le gabarit :/theme/dark.qss dont chaque
// @jeton@ est remplace par sa valeur. Un jeton inconnu est laisse tel quel et
// signale (qWarning) : la feuille ne peut pas citer une couleur hors du systeme.
QString styleSheet();

// Tailles d'icones en px logiques (AutoCAD 2024 ; barres d'acces rapide et
// d'etat resserrees a 14 px, comme leur densite dans AutoCAD).
inline constexpr int kIconRibbonLarge = 32;
inline constexpr int kIconRibbonSmall = 16;
inline constexpr int kIconStatusBar = 14;
inline constexpr int kIconQuickAccess = 14;
inline constexpr int kIconAppButton = 22;
inline constexpr int kIconChevron = 10;

// Typographie (pt), calee sur AutoCAD 2024 (Segoe UI) : corps (menus, barre
// d'etat, palettes, ligne de commande), legendes (titres de panneau et
// libelles du ruban), glyphe isole (« + » de nouvel onglet). Police d'interface
// d'AutoCAD d'abord, puis les plus proches sous Linux ; chasse fixe pour la
// ligne de commande.
inline constexpr const char* kFontUi = "\"Segoe UI\", \"Liberation Sans\", \"DejaVu Sans\", sans-serif";
inline constexpr int kFontBodyPt = 10;
inline constexpr int kFontCaptionPt = 9;
inline constexpr int kFontGlyphPt = 13;
inline constexpr const char* kFontMono = "\"Consolas\", \"DejaVu Sans Mono\", monospace";

// Espacements (px), multiples de 2, et rayons des coins.
// Poignees du canevas (M-02) : cote du carre en px logiques (GRIPSIZE 5
// d'AutoCAD, carre de 2 x 5 + 1) ; au-dela de kGripObjectLimit objets
// selectionnes, aucune poignee (GRIPOBJLIMIT 100).
inline constexpr int kGripSizePx = 11;
inline constexpr int kGripObjectLimit = 100;

inline constexpr int kSpaceXs = 2;
inline constexpr int kSpaceS = 4;
inline constexpr int kSpaceM = 6;
inline constexpr int kSpaceL = 8;
inline constexpr int kRadiusSmall = 2;   // cases a cocher
inline constexpr int kRadius = 3;        // boutons, champs, panneaux
inline constexpr int kRadiusRound = 4;   // poignees de defilement

} // namespace bcad::app::theme
