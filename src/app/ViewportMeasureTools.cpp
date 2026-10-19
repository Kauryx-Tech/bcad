// Outils de mesure (I-01, I-02) : rien n'est cree dans le dessin, le
// resultat s'affiche dans la ligne de commande et la barre d'etat, et
// l'outil reste actif pour enchainer, comme MESUREGEOM / AIRE d'AutoCAD.
//
//   Mesurer : deux points -> distance, dX, dY, angle / axe X ; option A,
//             sommet + deux cotes -> angle ; D revient a la distance.
//   Surface : sommets puis Entree, ou option O, un objet ferme designe ;
//             A / S cumulent dans un total.
//   Coordonnees : X, Y du point, et position relative au precedent.
// Calculs : MeasureDetail.h.

#include "MeasureDetail.h"
#include "Viewport.h"
#include "ViewportTolerances.h"

#include "bcad/core/Document.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/Tolerance.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace bcad::app {

using geom::Point2;

// --- Mesurer (I-01) -------------------------------------------------------

void Viewport::applyMeasure(const Point2& world) {
    toolPoints_.push_back(world);
    if (!measureAngle_ && toolPoints_.size() == 2) {
        const detail::DistanceMeasure m = detail::measureDistance(toolPoints_[0], toolPoints_[1]);
        measureResult_ = tr("Distance = %1 ; ΔX = %2 ; ΔY = %3 ; angle / axe X = %4° (%5 gon)")
                             .arg(m.distance, 0, 'f', 3)
                             .arg(m.dx, 0, 'f', 3)
                             .arg(m.dy, 0, 'f', 3)
                             .arg(m.angleDeg, 0, 'f', 4)
                             .arg(detail::degToGon(m.angleDeg), 0, 'f', 4);
    } else if (measureAngle_ && toolPoints_.size() == 3) {
        const double deg = detail::measureAngleDeg(toolPoints_[0], toolPoints_[1], toolPoints_[2]);
        measureResult_ = tr("Angle = %1° (%2 gon)")
                             .arg(deg, 0, 'f', 4)
                             .arg(detail::degToGon(deg), 0, 'f', 4);
    } else {
        return;
    }
    // L'outil reste actif pour enchainer les mesures, comme MESUREGEOM ;
    // Entree ou Echap le quittent.
    toolPoints_.clear();
    emit statusMessage(measureResult_);
}

bool Viewport::submitMeasureOption(const QString& text) {
    const QString option = text.trimmed().toUpper();
    if (tool_ == ToolMode::Area) {
        // O : designer un objet ; P : revenir aux sommets ; A / S : cumuler
        // en ajoutant / soustrayant (comme AIRE d'AutoCAD), total remis a 0.
        if (option == QLatin1String("O") || option == QLatin1String("OBJET")) areaObject_ = true;
        else if (option == QLatin1String("P") || option == QLatin1String("POINTS")) areaObject_ = false;
        else if (option == QLatin1String("A") || option == QLatin1String("AJOUTER")) {
            if (areaTotalMode_ == 0) areaTotal_ = 0.0;
            areaTotalMode_ = 1;
        } else if (option == QLatin1String("S") || option == QLatin1String("SOUSTRAIRE")) {
            if (areaTotalMode_ == 0) areaTotal_ = 0.0;
            areaTotalMode_ = 2;
        } else {
            return false;
        }
        toolPoints_.clear();
        notifyPrompt();
        update();
        return true;
    }
    if (tool_ != ToolMode::Measure) return false;
    if (option == QLatin1String("A") || option == QLatin1String("ANGLE")) measureAngle_ = true;
    else if (option == QLatin1String("D") || option == QLatin1String("DISTANCE")) measureAngle_ = false;
    else return false;
    toolPoints_.clear();
    notifyPrompt();
    update();
    return true;
}

QString Viewport::measurePrompt() const {
    const std::size_t n = toolPoints_.size();
    QString step;
    if (measureAngle_) {
        step = n == 0 ? tr("Mesurer l'angle — spécifiez le sommet [D distance]")
             : n == 1 ? tr("Mesurer l'angle — un point du premier côté")
                      : tr("Mesurer l'angle — un point du second côté");
    } else {
        step = n == 0 ? tr("Mesurer la distance — premier point [A angle]")
                      : tr("Mesurer la distance — second point");
    }
    if (n == 0 && !measureResult_.isEmpty()) return measureResult_ + QStringLiteral("  |  ") + step;
    return step;
}

// --- Surface et perimetre (I-02) --------------------------------------------

void Viewport::applyArea(const Point2& world) {
    if (!areaObject_) {
        // Sommets du contour ; Entree (ou clic droit) calcule.
        toolPoints_.push_back(world);
        return;
    }
    const double pickTol = kPickToleranceScreenPx / camera_.pixelsPerUnit();
    geom::Entity* hit = doc_ ? doc_->pickEntity(world, pickTol) : nullptr;
    if (!hit) {
        emit statusMessage(tr("Aire : aucun objet sous le curseur."));
        return;
    }
    if (hit->typeId() == geom::TypeId_Circle) {
        const double r = static_cast<const geom::CircleEntity&>(*hit).radius();
        reportArea(std::numbers::pi * r * r, 2.0 * std::numbers::pi * r, tr("cercle"));
        return;
    }
    // Polylignes, et les types de module qui en derivent : la
    // surface deduit les trous ; une polyligne ouverte est supposee fermee,
    // comme AIRE d'AutoCAD.
    if (const auto* poly = dynamic_cast<const geom::PolylineEntity*>(hit)) {
        if (poly->vertices().size() < 3) {
            emit statusMessage(tr("Aire : il faut au moins trois sommets."));
            return;
        }
        double area = detail::ringArea(poly->vertices());
        for (const auto& hole : poly->holes()) area -= detail::ringArea(hole);
        QString what = poly->closed() ? tr("polyligne") : tr("polyligne ouverte, supposée fermée");
        if (!poly->holes().empty()) what += tr(", %n trou(s) déduit(s)", nullptr, int(poly->holes().size()));
        reportArea(std::max(0.0, area), detail::ringPerimeter(poly->vertices()), what);
        return;
    }
    // Autre objet : accepte s'il se dessine en contour ferme.
    std::vector<Point2> ring = hit->tessellate(pickTol / 4.0);
    if (ring.size() >= 4 && geom::distance(ring.front(), ring.back()) <= geom::Tolerance::kDegenerateLength) {
        ring.pop_back();
        reportArea(detail::ringArea(ring), detail::ringPerimeter(ring), tr("objet"));
        return;
    }
    emit statusMessage(tr("Aire : désignez un objet fermé (polyligne, cercle…)."));
}

void Viewport::finishAreaPolygon() {
    if (toolPoints_.size() < 3) {
        emit statusMessage(tr("Aire : il faut au moins trois sommets."));
        return;
    }
    reportArea(detail::ringArea(toolPoints_), detail::ringPerimeter(toolPoints_),
               tr("%n sommet(s)", nullptr, int(toolPoints_.size())));
    toolPoints_.clear();
    notifyPrompt();
    update();
}

void Viewport::reportArea(double area, double perimeter, const QString& what) {
    measureResult_ = tr("Aire = %1 ; Périmètre = %2 (%3)")
                         .arg(area, 0, 'f', 3)
                         .arg(perimeter, 0, 'f', 3)
                         .arg(what);
    if (areaTotalMode_ != 0) {
        areaTotal_ += areaTotalMode_ == 1 ? area : -area;
        measureResult_ += tr(" ; Total = %1").arg(areaTotal_, 0, 'f', 3);
    }
    emit statusMessage(measureResult_);
}

QString Viewport::areaPrompt() const {
    const std::size_t n = toolPoints_.size();
    QString mode = areaTotalMode_ == 1 ? tr("Aire (ajouter)")
                 : areaTotalMode_ == 2 ? tr("Aire (soustraire)")
                                       : tr("Aire");
    QString step;
    if (areaObject_) {
        step = tr("%1 — désignez un objet fermé [P points, A ajouter, S soustraire]").arg(mode);
    } else if (n == 0) {
        step = tr("%1 — premier sommet [O objet, A ajouter, S soustraire]").arg(mode);
    } else if (n < 3) {
        step = tr("%1 — sommet suivant (%2 sommet(s))").arg(mode).arg(n);
    } else {
        step = tr("%1 — sommet suivant, Entrée pour calculer (%2 sommets)").arg(mode).arg(n);
    }
    if (n == 0 && !measureResult_.isEmpty()) return measureResult_ + QStringLiteral("  |  ") + step;
    return step;
}

// --- Coordonnees d'un point (I-06) -----------------------------------------

void Viewport::applyCoordinates(const Point2& world) {
    measureResult_ = tr("X = %1 ; Y = %2").arg(world.x_, 0, 'f', 3).arg(world.y_, 0, 'f', 3);
    if (idLast_) {
        const detail::DistanceMeasure m = detail::measureDistance(*idLast_, world);
        measureResult_ += tr(" ; depuis le précédent : @%1,%2 = @%3<%4°")
                              .arg(m.dx, 0, 'f', 3)
                              .arg(m.dy, 0, 'f', 3)
                              .arg(m.distance, 0, 'f', 3)
                              .arg(m.angleDeg, 0, 'f', 4);
    }
    idLast_ = world;
    emit statusMessage(measureResult_);
}

} // namespace bcad::app
