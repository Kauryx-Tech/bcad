// Consigne de chaque etape d'outil, lue dans la ligne de commande.
//
// Un outil accumule ses points dans `toolPoints_` : la consigne en est une
// lecture, jamais un etat a part. Elle dit donc toujours ce que le prochain
// point va signifier — ce que la ligne de commande d'AutoCAD affiche, et ce
// sans quoi un utilisateur ne peut pas savoir que l'arc veut son centre
// d'abord, ni que la polyligne attend Entree pour se terminer.

#include "Viewport.h"

#include "bcad/geometry/Point.h"

namespace bcad::app {

namespace {

QString commandName(ToolMode mode) {
    switch (mode) {
        case ToolMode::Move: return Viewport::tr("Déplacer");
        case ToolMode::Copy: return Viewport::tr("Copier");
        case ToolMode::Rotate: return Viewport::tr("Tourner");
        case ToolMode::Scale: return Viewport::tr("Échelle");
        case ToolMode::Mirror: return Viewport::tr("Symétrie");
        default: return {};
    }
}

} // namespace

QString Viewport::prompt() const {
    const std::size_t n = toolPoints_.size();
    if (pickingObjects_) {
        return tr("%1 — sélectionnez les objets (%2 sélectionné(s)), puis Entrée ou clic droit")
            .arg(commandName(tool_))
            .arg(selectedEntities().size());
    }
    switch (tool_) {
        case ToolMode::Select:
            return lastCommand_ == ToolMode::Select
                       ? tr("Sélectionnez des objets (clic ou fenêtre), ou choisissez une commande")
                       : tr("Sélectionnez des objets, ou choisissez une commande — Entrée répète la dernière");
        case ToolMode::Move:
            return n == 0 ? tr("Déplacer — spécifiez le point de base")
                          : tr("Déplacer — spécifiez le point de destination");
        case ToolMode::Copy:
            return n == 0 ? tr("Copier — spécifiez le point de base")
                          : tr("Copier — spécifiez le point de destination");
        case ToolMode::Rotate:
            if (n == 0) return tr("Tourner — spécifiez le point de base (pivot)");
            return n == 1 ? tr("Tourner — spécifiez l'angle de référence (un point)")
                          : tr("Tourner — spécifiez le nouvel angle (un point)");
        case ToolMode::Scale:
            if (n == 0) return tr("Échelle — spécifiez le point de base");
            return n == 1 ? tr("Échelle — spécifiez la longueur de référence (un point)")
                          : tr("Échelle — spécifiez la nouvelle longueur (un point)");
        case ToolMode::Mirror:
            return n == 0 ? tr("Symétrie — spécifiez le premier point de l'axe")
                          : tr("Symétrie — spécifiez le deuxième point de l'axe");
        case ToolMode::Trim:
            return tr("Rogner — désignez la partie à supprimer, Entrée pour terminer");
        case ToolMode::Extend:
            return tr("Prolonger — désignez l'extrémité à prolonger, Entrée pour terminer");
        case ToolMode::Break:
            return tr("Scinder — désignez le point de rupture sur l'objet");
        case ToolMode::Line:
            return n == 0 ? tr("Ligne — spécifiez le premier point")
                          : tr("Ligne — point suivant (ex. @10,0 ou @10<45), Entrée pour terminer");
        case ToolMode::Circle:
            return n == 0 ? tr("Cercle — spécifiez le centre")
                          : tr("Cercle — spécifiez le rayon (un point du cercle)");
        case ToolMode::Arc:
            if (n == 0) return tr("Arc — spécifiez le centre");
            return n == 1 ? tr("Arc — spécifiez le point de départ")
                          : tr("Arc — spécifiez le point de fin (sens trigonométrique)");
        case ToolMode::Polyline:
            if (n == 0) return tr("Polyligne — spécifiez le premier point");
            if (n == 1) return tr("Polyligne — spécifiez le point suivant");
            return tr("Polyligne — point suivant, Entrée pour terminer, C pour fermer");
        case ToolMode::Text:
            switch (textStage_) {
                case 0: return tr("Texte — spécifiez le point de départ");
                case 1: return tr("Texte — hauteur (Entrée : %1)").arg(textHeight_);
                case 2: return tr("Texte — angle de rotation en degrés (Entrée : %1)")
                    .arg(textRotation_ * 180.0 / 3.14159265358979323846);
                default: return tr("Texte — tapez le texte puis Entrée ; ligne vide pour terminer");
            }
        case ToolMode::CapturePolygon: {
            const QString what = capturePrompt_.isEmpty()
                                     ? tr("Contour — cliquez les sommets ou tapez x,y")
                                     : capturePrompt_;
            return n < 3 ? tr("%1 (%2 sommet(s))").arg(what).arg(n)
                         : tr("%1 (%2 sommets) — Entrée, C ou clic droit pour fermer").arg(what).arg(n);
        }
        case ToolMode::Rectangle:
            return n == 0 ? tr("Rectangle — spécifiez le premier coin")
                          : tr("Rectangle — spécifiez le coin opposé (ex. @20,10)");
        case ToolMode::Point:
            return tr("Point — spécifiez la position");
        case ToolMode::DimensionLinear:
            if (n == 0) return tr("Cotation linéaire — origine de la première ligne d'attache");
            if (n == 1) return tr("Cotation linéaire — origine de la deuxième ligne d'attache");
            return dimOrientation_ == 1 ? tr("Cotation linéaire horizontale — position de la ligne de cote")
                 : dimOrientation_ == 2 ? tr("Cotation linéaire verticale — position de la ligne de cote")
                 : tr("Cotation linéaire — position de la ligne de cote [H horizontale / V verticale]");
        case ToolMode::DimensionAligned:
            if (n == 0) return tr("Cotation alignée — origine de la première ligne d'attache");
            return n == 1 ? tr("Cotation alignée — origine de la deuxième ligne d'attache")
                          : tr("Cotation alignée — position de la ligne de cote");
        case ToolMode::DimensionAngular:
            if (n == 0) return tr("Cotation angulaire — sommet de l'angle");
            if (n == 1) return tr("Cotation angulaire — un point du premier côté");
            return n == 2 ? tr("Cotation angulaire — un point du second côté")
                          : tr("Cotation angulaire — position de l'arc de cote");
        case ToolMode::DimensionRadius:
        case ToolMode::DimensionDiameter:
            return n == 0 ? tr("Cotation — désignez un cercle ou un arc, ou son centre")
                          : dimRadius_ > 0.0 ? tr("Cotation — direction de la cote")
                                             : tr("Cotation — un point du cercle");
    }
    return {};
}

void Viewport::notifyPrompt() {
    emit promptChanged(prompt());
}

void Viewport::ensureDimensionLayer() {
    if (!doc_) return;
    // Cyan : la couleur d'usage des cotations dans les gabarits AutoCAD.
    doc_->layerManager().createLayer("Cotations", geom::Color::fromRgb255(0, 200, 230));
}

} // namespace bcad::app
