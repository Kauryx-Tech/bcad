// Consigne de chaque etape d'outil, lue dans la ligne de commande.
//
// Un outil accumule ses points dans `toolPoints_` : la consigne en est une
// lecture, jamais un etat a part. Elle dit donc toujours ce que le prochain
// point va signifier — ce que la ligne de commande d'AutoCAD affiche, et ce
// sans quoi un utilisateur ne peut pas savoir que l'arc veut son centre
// d'abord, ni que la polyligne attend Entree pour se terminer.

#include "viewport/Viewport.h"

#include "bcad/geometry/Point.h"
#include <algorithm>

namespace bcad::app {

namespace {

QString commandName(ToolMode mode) {
    switch (mode) {
        case ToolMode::Move: return Viewport::tr("Déplacer");
        case ToolMode::Copy: return Viewport::tr("Copier");
        case ToolMode::Rotate: return Viewport::tr("Rotation");
        case ToolMode::Scale: return Viewport::tr("Échelle");
        case ToolMode::Mirror: return Viewport::tr("Miroir");
        case ToolMode::CopyClip: return Viewport::tr("Copier dans le presse-papiers");
        case ToolMode::CutClip: return Viewport::tr("Couper");
        case ToolMode::CopyBase: return Viewport::tr("Copier avec point de base");
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
        case ToolMode::CopyBase:
            return tr("Copier avec point de base — spécifiez le point de base");
        case ToolMode::Paste:
            return tr("Coller — spécifiez le point d'insertion");
        case ToolMode::GripStretch:
            return gripPrompt();
        case ToolMode::Move:
            return n == 0 ? tr("Déplacer — spécifiez le point de base")
                          : tr("Déplacer — spécifiez le point de destination");
        case ToolMode::Copy:
            return n == 0 ? tr("Copier — spécifiez le point de base")
                          : tr("Copier — spécifiez le point de destination");
        case ToolMode::Rotate: {
            const QString name = transformCopy_ ? tr("Rotation (copie)") : tr("Rotation");
            if (n == 0) return tr("%1 — spécifiez le point de base (pivot) [C copie]").arg(name);
            return n == 1 ? tr("%1 — angle en degrés, ou point de référence [C copie]").arg(name)
                          : tr("%1 — nouvel angle : un point, ou des degrés").arg(name);
        }
        case ToolMode::Scale: {
            const QString name = transformCopy_ ? tr("Échelle (copie)") : tr("Échelle");
            if (n == 0) return tr("%1 — spécifiez le point de base [C copie]").arg(name);
            return n == 1 ? tr("%1 — facteur, ou point de la longueur de référence [C copie]").arg(name)
                          : tr("%1 — nouvelle longueur : un point, ou une valeur").arg(name);
        }
        case ToolMode::Mirror:
            return n == 0 ? tr("Miroir — spécifiez le premier point de l'axe")
                          : tr("Miroir — spécifiez le deuxième point de l'axe");
        case ToolMode::Trim:
            return tr("Ajuster — désignez la partie à supprimer, Entrée pour terminer");
        case ToolMode::Extend:
            return tr("Prolonger — désignez l'extrémité à prolonger, Entrée pour terminer");
        case ToolMode::Break:
            return tr("Coupure — désignez le point de rupture sur l'objet");
        case ToolMode::Offset:
            return offsetPrompt();
        case ToolMode::Stretch:
            if (n == 0) return tr("Étirer — premier coin de la fenêtre de capture");
            if (n == 1) return tr("Étirer — coin opposé de la fenêtre");
            return n == 2 ? tr("Étirer — point de base")
                          : tr("Étirer — point de destination (ex. @5,0)");
        case ToolMode::Chamfer: {
            if (chamferFillet_) {
                if (filletRadius_ > 0.0)
                    return tr("Congé R=%1 — spécifiez un point sur chaque ligne").arg(filletRadius_);
                return tr("Congé — tapez le rayon (R), puis un point sur chaque ligne [D droit]");
            }
            if (chamferDist_ > 0.0)
                return tr("Chanfrein D=%1 — spécifiez un point sur chaque ligne ou arc").arg(chamferDist_);
            return tr("Chanfrein — un point par ligne/arc, nombre=distance, R=congé");
        }
        case ToolMode::Array:
            if (arrayAdjusting_ && n == 0)
                return tr("Réseau posé — trois nouveaux points pour changer les pas, Entrée pour terminer");
            if (arrayPolar_) {
                if (n == 0) return tr("Réseau polaire (%1 objets) — spécifiez le centre [R rectangulaire]").arg(arrayCount_);
                return n == 1 ? tr("Réseau polaire — spécifiez le point de référence (angle de départ)")
                              : tr("Réseau polaire — spécifiez le second point (pas angulaire)");
            }
            if (n == 0) return tr("Réseau %1x2 — spécifiez le point de base [P polaire]").arg(std::max(2, arrayCount_));
            return n == 1 ? tr("Réseau — spécifiez le pas des colonnes (second point)")
                          : tr("Réseau — spécifiez le pas des rangées (troisième point)");
        case ToolMode::Hatch: {
            const char* motif = hatchPattern_ == 1 ? "croix" : hatchPattern_ == 2 ? "points" : "lignes";
            if (hatchSpacing_ > 0.0)
                return tr("Hachures %1 %2° pas %3 — point intérieur, Entrée pour terminer")
                    .arg(QString::fromLatin1(motif)).arg(hatchAngleDeg_).arg(hatchSpacing_);
            return tr("Hachures %1 %2° pas auto — point intérieur [nombre=pas, A=angle, motif], Entrée pour terminer")
                .arg(QString::fromLatin1(motif)).arg(hatchAngleDeg_);
        }
        case ToolMode::Polygon:
            if (n == 0) return tr("Polygone — spécifiez le premier sommet");
            if (n == 1) return tr("Polygone — spécifiez le sommet suivant");
            return tr("Polygone — sommet suivant, Entrée ou C pour fermer");
        case ToolMode::Join:
            return tr("Joindre — cliquez pour fusionner les lignes sélectionnées");
        case ToolMode::Explode:
            return tr("Éclater — cliquez pour éclater la sélection");
        case ToolMode::Erase:
            return tr("Effacer — cliquez pour supprimer la sélection");
        case ToolMode::Measure:
            return measurePrompt();
        case ToolMode::Area:
            return areaPrompt();
        case ToolMode::Fillet:
            return filletPrompt();
        case ToolMode::ZoomWindow:
            return toolPoints_.empty() ? tr("Zoom fenêtre — premier coin (ou faites glisser)")
                                       : tr("Zoom fenêtre — coin opposé");
        case ToolMode::Pan:
            return tr("Panoramique — faites glisser la vue, Entrée ou Échap pour terminer");
        case ToolMode::Coordinates: {
            const QString step = tr("Coordonnées — spécifiez un point, Entrée pour terminer");
            return measureResult_.isEmpty() ? step : measureResult_ + QStringLiteral("  |  ") + step;
        }
        case ToolMode::Line:
            return n == 0 ? tr("Ligne — spécifiez le premier point")
                          : tr("Ligne — point suivant (ex. @10,0 ou @10<45), Entrée pour terminer");
        case ToolMode::Circle:
            return n == 0 ? tr("Cercle — spécifiez le centre [2P, 3P]")
                          : tr("Cercle — spécifiez le rayon (un point du cercle) [D diamètre]");
        case ToolMode::CircleDiameter:
            return n == 0 ? tr("Cercle — spécifiez le centre")
                          : tr("Cercle — spécifiez le diamètre (un point à cette distance)");
        case ToolMode::Circle2P:
            return n == 0 ? tr("Cercle 2 points — première extrémité du diamètre")
                          : tr("Cercle 2 points — seconde extrémité du diamètre");
        case ToolMode::Circle3P:
            if (n == 0) return tr("Cercle 3 points — premier point");
            return n == 1 ? tr("Cercle 3 points — deuxième point") : tr("Cercle 3 points — troisième point");
        case ToolMode::Arc3P:
            if (n == 0) return tr("Arc — spécifiez le point de départ [C centre]");
            return n == 1 ? tr("Arc — spécifiez le deuxième point de l'arc")
                          : tr("Arc — spécifiez le point final");
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
