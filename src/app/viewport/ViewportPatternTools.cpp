// Les outils de repetition : reseau rectangulaire et hachures. Meme
// contrat que les outils de modification (ViewportModifyTools) : selection
// prealable, points dans `toolPoints_`, une macro d'annulation par
// operation, aucun dialogue.

#include "viewport/Viewport.h"

#include "editing/Commands.h"
#include "editing/EditBatch.h"
#include "detail/ModifyDetail.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/PointEntity.h"
#include <QUndoStack>
#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>

namespace bcad::app {

using geom::Point2;

void Viewport::applyArray(const Point2& world) {
    // Apres une premiere pose, l'outil reste actif : trois nouveaux points
    // ajoutent un reseau superpose (nouvelle macro), sans annuler le precedent.
    // L'utilisateur controle l'historique par Ctrl+Z.
    toolPoints_.push_back(world);
    if (toolPoints_.size() < 3) return;
    std::vector<geom::Entity*> selected = selectedEntities();
    if (selected.empty()) {
        emit statusMessage(tr("Sélectionnez d'abord les objets du réseau."));
        arrayAdjusting_ = false;
        endCommand();
        return;
    }
    arrayAdjusting_ = false;
    std::vector<geom::Transform2D> trs =
        detail::arrayTransforms(arrayPolar_, toolPoints_[0], toolPoints_[1], toolPoints_[2],
                               arrayCount_);
    if (trs.empty()) {
        emit statusMessage(tr(arrayPolar_ ? "Réseau polaire : angle ou rayon trop petit."
                                          : "Réseau : pas trop petit."));
        endCommand();
        return;
    }
    {
        EditBatch batch(doc_, undoStack_, tr("Réseau"));
        for (geom::Entity* e : selected) {
            for (const auto& t : trs) {
                auto clone = e->clone();
                clone->applyTransform(t);
                clone->selected = false;
                batch.add(std::move(clone));
            }
        }
    }
    if (undoStack_) {
        arrayAdjusting_ = true;
        toolPoints_.clear();
        notifyPrompt();
        update();
        return;
    }
    endCommand();
}

bool Viewport::submitModifyOption(const QString& text) {
    // Options des outils de repetition, avant tout point : « P » / « R »
    // pour le mode du reseau, un nombre pour le nombre d'objets (polaire),
    // de colonnes (rectangulaire) ou le pas des hachures. Comme les options
    // de cotation (H/V), elles ne valent que pendant la commande.
    const QString option = text.trimmed();
    if (option.isEmpty()) return false;
    if (tool_ == ToolMode::Chamfer && toolPoints_.empty()) {
        if (option.compare(QStringLiteral("R"), Qt::CaseInsensitive) == 0 ||
            option.compare(QStringLiteral("CONGE"), Qt::CaseInsensitive) == 0) {
            chamferFillet_ = true;
            notifyPrompt();
            update();
            return true;
        }
        if (option.compare(QStringLiteral("D"), Qt::CaseInsensitive) == 0 ||
            option.compare(QStringLiteral("DROIT"), Qt::CaseInsensitive) == 0 ||
            option.compare(QStringLiteral("CHANFREIN"), Qt::CaseInsensitive) == 0) {
            chamferFillet_ = false;
            notifyPrompt();
            update();
            return true;
        }
    }
    if (tool_ == ToolMode::Chamfer) {
        bool ok = false;
        double value = option.toDouble(&ok);
        if (ok && value > 0.0) {
            if (chamferFillet_) {
                filletRadius_ = std::min(value, 10000.0);
            } else {
                chamferDist_ = std::min(value, 10000.0);
            }
            notifyPrompt();
            update();
            return true;
        }
    }
    if (tool_ == ToolMode::Array && toolPoints_.empty()) {
        if (option.compare(QStringLiteral("P"), Qt::CaseInsensitive) == 0 ||
            option.compare(QStringLiteral("POLAIRE"), Qt::CaseInsensitive) == 0) {
            arrayPolar_ = true;
            notifyPrompt();
            update();
            return true;
        }
        if (option.compare(QStringLiteral("R"), Qt::CaseInsensitive) == 0 ||
            option.compare(QStringLiteral("RECTANGLE"), Qt::CaseInsensitive) == 0 ||
            option.compare(QStringLiteral("RECTANGULAIRE"), Qt::CaseInsensitive) == 0) {
            arrayPolar_ = false;
            notifyPrompt();
            update();
            return true;
        }
    }
    if (tool_ == ToolMode::Hatch) {
        if (option.compare(QStringLiteral("LIGNES"), Qt::CaseInsensitive) == 0) {
            hatchPattern_ = 0;
            notifyPrompt();
            update();
            return true;
        }
        if (option.compare(QStringLiteral("CROIX"), Qt::CaseInsensitive) == 0) {
            hatchPattern_ = 1;
            notifyPrompt();
            update();
            return true;
        }
        if (option.compare(QStringLiteral("POINTS"), Qt::CaseInsensitive) == 0) {
            hatchPattern_ = 2;
            notifyPrompt();
            update();
            return true;
        }
        if (option.size() > 1 &&
            (option.startsWith(QStringLiteral("A"), Qt::CaseInsensitive))) {
            bool ok = false;
            double deg = option.mid(1).toDouble(&ok);
            if (ok) {
                hatchAngleDeg_ = std::fmod(std::abs(deg), 180.0);
                notifyPrompt();
                update();
                return true;
            }
        }
    }
    if (tool_ == ToolMode::Array || tool_ == ToolMode::Hatch) {
        bool ok = false;
        double value = option.toDouble(&ok);
        if (ok && value > 0.0) {
            if (tool_ == ToolMode::Array) {
                arrayCount_ = std::clamp(static_cast<int>(std::round(value)), 2, 64);
            } else {
                hatchSpacing_ = std::min(value, 1000.0);
            }
            notifyPrompt();
            update();
            return true;
        }
    }
    return false;
}

void Viewport::applyHatch(const Point2& world) {
    // Hachures : balayage dans le repere tourne d'hatchAngleDeg_, regle
    // pair-impair sur l'anneau exterieur moins les trous (holes()), motifs
    // LIGNES (une passe), CROIX (deux passes perpendiculaires), POINTS
    // (ticks sur grille). Contours = polylignes fermees selectionnees,
    // sinon celles du document qui contiennent le point clique. Pas force
    // par un nombre tape, sinon min(largeur, hauteur)/25, borne (ADR-016).
    std::vector<const geom::PolylineEntity*> bounds;
    for (geom::Entity* e : selectedEntities()) {
        if (e->typeId() != geom::TypeId_Polyline) continue;
        const auto& p = static_cast<const geom::PolylineEntity&>(*e);
        if (p.closed() && p.vertices().size() >= 3) bounds.push_back(&p);
    }
    // Point interieur : le plus petit contour autour du point, ilots exclus.
    std::vector<std::vector<Point2>> islands;
    if (bounds.empty() && selectedEntities().empty() && doc_) {
        detail::HatchBoundary picked = detail::hatchBoundaryAt(doc_->entities(), world);
        if (picked.outer) bounds.push_back(picked.outer);
        islands = std::move(picked.islands);
    }
    // Comme HACHURES d'AutoCAD : l'outil reste ouvert pour d'autres points
    // interieurs jusqu'a Entree ; une selection prealable est hachuree d'un coup.
    const bool fromSelection = !selectedEntities().empty();
    if (bounds.empty()) {
        emit statusMessage(tr("Hachures : aucun contour fermé autour de ce point."));
        if (fromSelection) endCommand();
        return;
    }
    double baseAngle = hatchAngleDeg_ * std::numbers::pi / 180.0;
    int passes = (hatchPattern_ == 1) ? 2 : 1;
    EditBatch batch(doc_, undoStack_, tr("Hachures"));
    auto commitSeg = [&](Point2 a, Point2 b, const std::string& layer) {
        if (geom::distance(a, b) < geom::Tolerance::kDegenerateLength) return;
        auto seg = std::make_unique<geom::LineEntity>(a, b);
        seg->setLayer(layer);
        seg->selected = false;
        batch.add(std::move(seg));
    };
    // Intervalles interieurs d'un anneau a la hauteur y (repere tourne).
    auto ringIntervals = [](const std::vector<Point2>& q, double y) {
        std::vector<double> xs;
        std::size_t n = q.size();
        for (std::size_t i = 0; i < n; ++i) {
            const Point2& a = q[i];
            const Point2& b = q[(i + 1) % n];
            if ((a.y_ <= y && b.y_ > y) || (b.y_ <= y && a.y_ > y)) {
                double t = (y - a.y_) / (b.y_ - a.y_);
                xs.push_back(a.x_ + t * (b.x_ - a.x_));
            }
        }
        std::sort(xs.begin(), xs.end());
        std::vector<std::pair<double, double>> iv;
        for (std::size_t k = 0; k + 1 < xs.size(); k += 2) iv.emplace_back(xs[k], xs[k + 1]);
        return iv;
    };
    for (const auto* poly : bounds) {
        for (int pass = 0; pass < passes; ++pass) {
            double angle = baseAngle + pass * std::numbers::pi / 2.0;
            double ca = std::cos(-angle), sa = std::sin(-angle);
            auto rot = [&](const Point2& p) {
                return Point2(p.x_ * ca - p.y_ * sa, p.x_ * sa + p.y_ * ca);
            };
            auto unrot = [&](const Point2& p) {
                return Point2(p.x_ * ca + p.y_ * sa, -p.x_ * sa + p.y_ * ca);
            };
            const auto& v = poly->vertices();
            std::vector<Point2> q;
            q.reserve(v.size());
            for (const auto& p : v) q.push_back(rot(p));
            std::vector<std::vector<Point2>> qholes;
            for (const auto& island : islands) {
                std::vector<Point2> qi;
                for (const auto& p : island) qi.push_back(rot(p));
                qholes.push_back(std::move(qi));
            }
            for (const auto& h : poly->holes()) {
                if (h.size() < 3) continue;
                std::vector<Point2> qh;
                qh.reserve(h.size());
                for (const auto& p : h) qh.push_back(rot(p));
                qholes.push_back(std::move(qh));
            }
            double yMin = q[0].y_, yMax = q[0].y_, xMin = q[0].x_, xMax = q[0].x_;
            for (const auto& p : q) {
                yMin = std::min(yMin, p.y_);
                yMax = std::max(yMax, p.y_);
                xMin = std::min(xMin, p.x_);
                xMax = std::max(xMax, p.x_);
            }
            double step = (hatchSpacing_ > 0.0)
                                ? hatchSpacing_
                                : std::min(xMax - xMin, yMax - yMin) / 25.0;
            step = std::clamp(step, 0.1, 1000.0);
            if (hatchPattern_ == 2) {
                // Points reels (PointEntity) sur grille, testes interieur/extremite.
                for (double y = yMin + step / 2.0; y < yMax; y += step) {
                    auto outer = ringIntervals(q, y);
                    std::vector<std::vector<std::pair<double, double>>> holeIv;
                    for (const auto& qh : qholes) holeIv.push_back(ringIntervals(qh, y));
                    for (double x = xMin + step / 2.0; x < xMax; x += step) {
                        bool in = false;
                        for (const auto& [a, b] : outer) {
                            if (x >= a && x <= b) {
                                in = true;
                                break;
                            }
                        }
                        if (!in) continue;
                        for (const auto& hiv : holeIv) {
                            for (const auto& [a, b] : hiv) {
                                if (x >= a && x <= b) {
                                    in = false;
                                    break;
                                }
                            }
                            if (!in) break;
                        }
                        if (!in) continue;
                        Point2 c = unrot(Point2(x, y));
                        auto pt = std::make_unique<geom::PointEntity>(c);
                        pt->setLayer(poly->layer());
                        pt->selected = false;
                        batch.add(std::move(pt));
                    }
                }
                continue;
            }
            for (double y = yMin + step; y < yMax; y += step) {
                auto outer = ringIntervals(q, y);
                std::vector<std::pair<double, double>> holeAll;
                for (const auto& qh : qholes) {
                    auto hiv = ringIntervals(qh, y);
                    holeAll.insert(holeAll.end(), hiv.begin(), hiv.end());
                }
                std::sort(holeAll.begin(), holeAll.end());
                for (const auto& [a, b] : outer) {
                    // Soustrait les trous chevauchant [a, b].
                    double cur = a;
                    for (const auto& [ha, hb] : holeAll) {
                        if (hb <= cur || ha >= b) continue;
                        if (ha > cur) commitSeg(unrot(Point2(cur, y)), unrot(Point2(std::min(ha, b), y)),
                                               poly->layer());
                        cur = std::max(cur, hb);
                        if (cur >= b) break;
                    }
                    if (cur < b) commitSeg(unrot(Point2(cur, y)), unrot(Point2(b, y)), poly->layer());
                }
            }
        }
    }
    if (fromSelection) endCommand();
}

} // namespace bcad::app
