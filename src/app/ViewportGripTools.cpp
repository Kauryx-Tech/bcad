// Poignees (M-02), comme AutoCAD : petits carres bleus sur les objets
// selectionnes au repos. Le survol rosit la poignee ; un clic la rend chaude
// (rouge) et lance « ** ETIRER ** » : le point suivant — clique, accroche,
// tape (@dx,dy depuis la poignee) ou relachement d'un glisser — etire
// l'objet. Les poignees des autres objets selectionnes au meme endroit
// chauffent ensemble (deux lignes qui partagent une extremite restent
// jointes). Une seule annulation ; la selection reste, poignees comprises.
// Ce que devient chaque objet : detail::moveGrip (GripDetail.h).

#include "Viewport.h"

#include "EditBatch.h"
#include "theme/Theme.h"

#include <QPainter>
#include <cmath>

namespace bcad::app {

using geom::Point2;

namespace {

// Le carre d'une poignee contient-il ce point de l'ecran ?
bool underCursor(const QPointF& grip, const QPointF& cursor) {
    const double half = theme::kGripSizePx / 2.0;
    return std::abs(grip.x() - cursor.x()) <= half && std::abs(grip.y() - cursor.y()) <= half;
}

bool samePlace(const Point2& a, const Point2& b) {
    return geom::distance(a, b) < geom::Tolerance::kDegenerateLength;
}

} // namespace

bool Viewport::beginGrip(QPoint screenPos) {
    if (!doc_) return false;
    const std::vector<geom::Entity*> selected = selectedEntities();
    if (selected.empty() || selected.size() > static_cast<std::size_t>(theme::kGripObjectLimit)) return false;

    // La poignee la plus proche sous le curseur.
    const QPointF cursor(screenPos);
    std::optional<Point2> at;
    double best = 0.0;
    for (geom::Entity* e : selected) {
        for (const detail::Grip& g : detail::gripsOf(*e)) {
            const QPointF s = toScreenF(g.at);
            if (!underCursor(s, cursor)) continue;
            const double d = std::hypot(s.x() - cursor.x(), s.y() - cursor.y());
            if (!at || d < best) {
                at = g.at;
                best = d;
            }
        }
    }
    if (!at) return false;

    // Chaque objet selectionne qui a une poignee a cet endroit y participe.
    std::vector<std::pair<int, detail::Grip>> hot;
    for (geom::Entity* e : selected) {
        for (const detail::Grip& g : detail::gripsOf(*e)) {
            if (!samePlace(g.at, *at)) continue;
            hot.emplace_back(e->id(), g);
            break;
        }
    }
    setTool(ToolMode::GripStretch);
    hotGrips_ = std::move(hot);
    toolPoints_ = {*at};   // point de base : @dx,dy, ortho et polaire en partent
    gripPressScreen_ = screenPos;
    notifyPrompt();
    update();
    return true;
}

bool Viewport::finishGripDrag(QPoint screenPos) {
    // Appui sur la poignee, glisser, relacher : le relachement est le point.
    // Un simple clic laisse la poignee chaude jusqu'au clic suivant.
    if (tool_ != ToolMode::GripStretch || toolPoints_.size() != 1) return false;
    if ((screenPos - gripPressScreen_).manhattanLength() <= 3) return false;
    placePoint(snappedWorld(screenPos));
    return true;
}

void Viewport::applyGripStretch(const Point2& world) {
    if (toolPoints_.empty() || samePlace(world, toolPoints_.front())) {
        endCommand();
        return;
    }
    // Objets d'abord calcules : rien n'est pousse dans l'historique si
    // aucun ne peut prendre la nouvelle forme (un pas vide a annuler).
    std::vector<std::pair<geom::Entity*, std::unique_ptr<geom::Entity>>> changes;
    for (const auto& [id, grip] : hotGrips_) {
        geom::Entity* e = doc_->findEntity(id);
        if (!e) continue;
        if (auto out = detail::moveGrip(*e, grip, world)) changes.emplace_back(e, std::move(out));
    }
    if (changes.empty()) {
        emit statusMessage(tr("Étirer : forme impossible (rayon nul ou points alignés)."));
    } else {
        EditBatch batch(doc_, undoStack_, tr("Étirer"));
        for (auto& [original, replacement] : changes) batch.replace(original, std::move(replacement));
    }
    endCommand();
    emit selectionChanged();
}

void Viewport::drawGrips(QPainter& painter) {
    if (!doc_ || pickingObjects_ || (tool_ != ToolMode::Select && tool_ != ToolMode::GripStretch)) return;
    const std::vector<geom::Entity*> selected = selectedEntities();
    if (selected.empty() || selected.size() > static_cast<std::size_t>(theme::kGripObjectLimit)) return;

    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(Qt::NoPen);
    const double size = theme::kGripSizePx;
    const QPointF cursor(cursorScreen_);
    for (geom::Entity* e : selected) {
        for (const detail::Grip& g : detail::gripsOf(*e)) {
            const QPointF s = toScreenF(g.at);
            bool hot = false;
            for (const auto& [id, h] : hotGrips_) hot = hot || (id == e->id() && samePlace(h.at, g.at));
            const bool hover = !hot && tool_ == ToolMode::Select && hoverWorld_ && underCursor(s, cursor);
            const theme::Token token = hot ? theme::Token::GripHot
                                     : hover ? theme::Token::GripHover : theme::Token::Grip;
            painter.fillRect(QRectF(s.x() - size / 2.0, s.y() - size / 2.0, size, size), theme::color(token));
        }
    }
}

bool Viewport::drawGripPreview(QPainter& painter) {
    if (tool_ != ToolMode::GripStretch || toolPoints_.empty() || !hoverWorld_ || !doc_) return false;
    // Elastique depuis la poignee, et l'objet tel qu'il sera.
    painter.drawLine(toScreenF(toolPoints_.front()), toScreenF(*hoverWorld_));
    for (const auto& [id, grip] : hotGrips_) {
        const geom::Entity* e = doc_->findEntity(id);
        if (!e) continue;
        if (auto out = detail::moveGrip(*e, grip, *hoverWorld_)) drawGhost(painter, *out);
    }
    return true;
}

} // namespace bcad::app
