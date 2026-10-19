// Saisie au clavier : ce que la ligne de commande transmet au canevas.
// Chaque outil lit d'abord ses options et ses valeurs (rayon d'un raccord,
// angle d'une rotation, lettre d'option…) ; ce qui reste est un point —
// coordonnees (CoordinateInput.h) ou, comme AutoCAD, une distance seule
// posee dans la direction du curseur. Au repos, un alias lance l'outil.

#include "viewport/Viewport.h"
#include "viewport/ToolAliases.h"
#include "detail/CoordinateInput.h"

namespace bcad::app {

using geom::Point2;

// Coordonnee ou option tapee dans la ligne de commande.
void Viewport::submitTypedPoint(const QString& text) {
    // Hauteur, angle et lignes de l'outil Texte se tapent dans la ligne de
    // commande : ce ne sont pas des coordonnees.
    if (tool_ == ToolMode::Text && textStage_ >= 1) {
        submitTextValue(text);
        return;
    }
    if (submitGripOption(text)) return;
    if (submitDimensionOption(text)) return;
    if (submitMeasureOption(text)) return;
    if (submitFilletOption(text)) return;
    if (submitOffsetOption(text)) return;
    if (submitTransformValue(text)) return;
    if (submitCircleOption(text)) return;
    if (submitModifyOption(text)) return;
    // Ligne vide = Entree (valider, terminer, ou relancer la derniere commande) ;
    // « C » clot la polyligne, comme dans AutoCAD.
    const QString option = text.trimmed();
    if (option.isEmpty()) {
        pressEnter(/*fromRightClick=*/false);
        update();
        return;
    }
    if ((tool_ == ToolMode::Polyline || tool_ == ToolMode::Polygon ||
         tool_ == ToolMode::CapturePolygon) &&
        option.compare(QStringLiteral("C"), Qt::CaseInsensitive) == 0) {
        finishPolyline(true);
        return;
    }
    if (pickingObjects_) return;   // les objets se designent a la souris
    // Au repos, un alias de commande lance l'outil (C-01) : « L », « PL »,
    // « DLI »… Seuls les outils livres ont un alias ; le reste tombe comme
    // avant dans l'analyse de coordonnees, qui l'ignore. Pendant une commande
    // (outil actif, polyligne, cote, texte), les lettres gardent leur sens
    // d'option (C clot, H/V oriente) et ne changent pas d'outil.
    if (tool_ == ToolMode::Select) {
            if (std::optional<ToolMode> alias = toolModeForAlias(option)) {
            setTool(*alias);
            update();
            return;
        }
        // Boite Options (S-15), comme OP / OPTIONS d'AutoCAD.
        if (option.compare(QStringLiteral("OP"), Qt::CaseInsensitive) == 0 ||
            option.compare(QStringLiteral("OPTIONS"), Qt::CaseInsensitive) == 0) {
            emit optionsRequested();
            return;
        }
        // Zoom precedent / suivant (V-02) : immediats, pas d'outil.
        if (option.compare(QStringLiteral("ZE"), Qt::CaseInsensitive) == 0) {
            zoomToFit();
            return;
        }
        if (option.compare(QStringLiteral("ZP"), Qt::CaseInsensitive) == 0) {
            zoomPrevious();
            return;
        }
        if (option.compare(QStringLiteral("ZS"), Qt::CaseInsensitive) == 0) {
            zoomNext();
            return;
        }
        // Alias d'un outil pas encore livré : le dire, avec sa tâche, plutôt
        // que d'ignorer la saisie sans un mot.
        {
            if (const char* missing = futureToolForAlias(option)) {
                emit statusMessage(tr("%1 : outil pas encore livré").arg(QString::fromLatin1(missing)));
                return;
            }
        }
    }
    const std::string raw = text.toStdString();

    std::optional<Point2> parsed = parseCoordinateInput(raw, activeReferencePoint());
    // Saisie directe de distance : un nombre seul, apres un premier point, est
    // une distance vers le curseur (rayon du cercle, longueur du segment…).
    if (!parsed) parsed = parseDirectDistance(raw, activeReferencePoint(), hoverWorld_);
    if (!parsed && tool_ == ToolMode::Select) {
        emit statusMessage(tr("Commande inconnue : « %1 ».").arg(text.trimmed()));
        return;
    }
    if (!parsed) {
        emit statusMessage(tr("Saisie non reconnue : « %1 » (point X,Y, @dx,dy, distance<angle, "
                              "ou une distance après le premier point).").arg(text.trimmed()));
        return;
    }
    activeSnap_ = {};
    placePoint(*parsed);
}

} // namespace bcad::app
