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

QString Viewport::prompt() const {
    const std::size_t n = toolPoints_.size();
    switch (tool_) {
        case ToolMode::Select:
            return tr("Sélectionnez des objets : clic, ou fenêtre glissée");
        case ToolMode::Move:
            if (moveTarget_) return tr("Déplacer — spécifiez le point de destination");
            if (selectedEntities().empty())
                return tr("Déplacer — désignez l'objet, ou sélectionnez d'abord puis choisissez Déplacer");
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
            return tr("Rogner — désignez la partie de l'objet à supprimer");
        case ToolMode::Extend:
            return tr("Prolonger — désignez l'extrémité de l'objet à prolonger");
        case ToolMode::Break:
            return tr("Scinder — désignez le point de rupture sur l'objet");
        case ToolMode::Line:
            return n == 0 ? tr("Ligne — spécifiez le premier point")
                          : tr("Ligne — spécifiez le point suivant (ex. @10,0 ou @10<45)");
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
        case ToolMode::Rectangle:
            return n == 0 ? tr("Rectangle — spécifiez le premier coin")
                          : tr("Rectangle — spécifiez le coin opposé (ex. @20,10)");
        case ToolMode::Point:
            return tr("Point — spécifiez la position");
        case ToolMode::DimensionLinear:
            if (n == 0) return tr("Cotation linéaire — origine de la première ligne d'attache");
            return n == 1 ? tr("Cotation linéaire — origine de la deuxième ligne d'attache")
                          : tr("Cotation linéaire — position de la ligne de cote");
        case ToolMode::DimensionAligned:
            return n == 0 ? tr("Cotation alignée — premier point")
                          : tr("Cotation alignée — deuxième point");
        case ToolMode::DimensionAngular:
            if (n == 0) return tr("Cotation angulaire — sommet de l'angle");
            return n == 1 ? tr("Cotation angulaire — un point du premier côté")
                          : tr("Cotation angulaire — un point du second côté");
        case ToolMode::DimensionRadius:
        case ToolMode::DimensionDiameter:
            return n == 0 ? tr("Cotation — centre du cercle ou de l'arc")
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
