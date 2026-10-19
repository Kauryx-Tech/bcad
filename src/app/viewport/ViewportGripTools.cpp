// Poignees (M-02), comme AutoCAD : petits carres bleus sur les objets
// selectionnes au repos. Le survol rosit la poignee ; Maj+clic en rend
// plusieurs chaudes (rouges) ; un clic lance « ** ETIRER ** » avec la poignee
// cliquee — et toutes les chaudes si elle en fait partie. Les poignees des
// autres objets selectionnes au meme endroit chauffent ensemble (deux lignes
// qui partagent une extremite restent jointes).
//
// Ici : designation des poignees, dessin, objets avant / apres et apercu.
// Modes, options et application : ViewportGripCommands.cpp. Ce que devient
// chaque objet : detail::moveGrip / gripTransform (GripDetail.h).

#include "viewport/Viewport.h"

#include "theme/Theme.h"

#include <QPainter>
#include <cmath>
#include <map>

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

bool Viewport::beginGrip(QPoint screenPos, bool shift) {
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

    // Poignees de chaque objet selectionne a cet endroit (une par objet).
    std::vector<std::pair<int, detail::Grip>> here;
    for (geom::Entity* e : selected) {
        for (const detail::Grip& g : detail::gripsOf(*e)) {
            if (!samePlace(g.at, *at)) continue;
            here.emplace_back(e->id(), g);
            break;
        }
    }
    const auto isHot = [this](int id, const Point2& p) {
        for (const auto& [hid, h] : hotGrips_)
            if (hid == id && samePlace(h.at, p)) return true;
        return false;
    };

    // Maj+clic : la poignee chauffe ou refroidit, sans lancer de commande.
    if (shift) {
        const bool wasHot = !here.empty() && isHot(here.front().first, *at);
        std::erase_if(hotGrips_, [&](const auto& h) { return samePlace(h.second.at, *at); });
        if (!wasHot) hotGrips_.insert(hotGrips_.end(), here.begin(), here.end());
        update();
        return true;
    }

    // Clic sur une poignee chaude : toutes les chaudes partent ensemble.
    std::vector<std::pair<int, detail::Grip>> hot = here;
    if (!here.empty() && isHot(here.front().first, *at)) {
        hot = hotGrips_;
        for (const auto& h : here)
            if (!isHot(h.first, h.second.at)) hot.push_back(h);
    }
    setTool(ToolMode::GripStretch);
    hotGrips_ = std::move(hot);
    toolPoints_ = {*at};   // point de base : @dx,dy, ortho et polaire en partent
    gripPressScreen_ = screenPos;
    gripDragArmed_ = true;
    gripMode_ = detail::GripMode::Stretch;
    gripCopy_ = gripAwaitBase_ = gripAwaitRef_ = gripAddVertex_ = false;
    gripRef_.reset();
    gripCopies_ = 0;
    notifyPrompt();
    update();
    return true;
}

bool Viewport::finishGripDrag(QPoint screenPos) {
    // Appui sur la poignee, glisser, relacher : le relachement est le point.
    // Un simple clic laisse la poignee chaude jusqu'au clic suivant.
    if (tool_ != ToolMode::GripStretch || !gripDragArmed_) return false;
    gripDragArmed_ = false;   // un seul relachement compte
    if ((screenPos - gripPressScreen_).manhattanLength() <= 3) return false;
    placePoint(snappedWorld(screenPos));
    return true;
}

Viewport::GripChanges Viewport::stretchChanges(const Point2& world) const {
    GripChanges out;
    if (!doc_ || toolPoints_.empty()) return out;
    const double dx = world.x_ - toolPoints_.front().x_;
    const double dy = world.y_ - toolPoints_.front().y_;
    std::map<int, std::vector<detail::Grip>> byEntity;
    for (const auto& [id, grip] : hotGrips_) byEntity[id].push_back(grip);
    for (const auto& [id, grips] : byEntity) {
        geom::Entity* e = doc_->findEntity(id);
        if (!e) continue;
        if (auto moved = detail::moveGrips(*e, grips, dx, dy)) out.emplace_back(e, std::move(moved));
    }
    return out;
}

Viewport::GripChanges Viewport::transformChanges(const geom::Transform2D& transform) const {
    // Deplacer, Rotation, Echelle, Miroir s'appliquent a toute la selection.
    GripChanges out;
    for (geom::Entity* e : selectedEntities()) {
        std::unique_ptr<geom::Entity> copy = e->clone();
        copy->applyTransform(transform);
        out.emplace_back(e, std::move(copy));
    }
    return out;
}

std::optional<geom::Transform2D> Viewport::gripPointTransform(const Point2& world) const {
    if (toolPoints_.empty()) return std::nullopt;
    return detail::gripTransform(gripMode_, toolPoints_.front(), world, gripRef_);
}

std::optional<std::pair<geom::Entity*, detail::Grip>> Viewport::singlePolylineGrip() const {
    if (!doc_ || hotGrips_.size() != 1) return std::nullopt;
    geom::Entity* e = doc_->findEntity(hotGrips_.front().first);
    const detail::Grip& g = hotGrips_.front().second;
    if (!dynamic_cast<geom::PolylineEntity*>(e)) return std::nullopt;
    if (g.role != detail::GripRole::Vertex && g.role != detail::GripRole::Segment) return std::nullopt;
    return std::make_pair(e, g);
}

void Viewport::drawSelection(QPainter& painter) {
    // Comme AutoCAD : l'objet choisi s'entoure d'un halo et passe en bleu ;
    // au repos, celui que la souris survole prend le halo (ce qu'un clic
    // choisirait). Dessine par-dessus le rendu GL, sans le retracer.
    if (!doc_) return;
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(Qt::NoBrush);
    QPen glow(theme::color(theme::Token::SelectionGlow), theme::kSelectionGlowPx);
    glow.setCapStyle(Qt::RoundCap);
    glow.setJoinStyle(Qt::RoundJoin);
    QPen line(theme::color(theme::Token::Selection), theme::kSelectionLinePx);
    const std::vector<geom::Entity*> selected = selectedEntities();
    for (const geom::Entity* e : selected) {
        painter.setPen(glow);
        drawGhost(painter, *e);
        painter.setPen(line);
        drawGhost(painter, *e);
    }
    const bool atRest = tool_ == ToolMode::Select || pickingObjects_;
    if (atRest && hoverWorld_ && !rubberBandActive_) {
        const geom::Entity* hover = pickEntityAt(toWorld(cursorScreen_));
        if (hover && !hover->selected) {
            painter.setPen(glow);
            drawGhost(painter, *hover);
        }
    }
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
    // Elastique depuis le point de base, et les objets tels qu'ils seront.
    painter.drawLine(toScreenF(toolPoints_.front()), toScreenF(*hoverWorld_));
    if (gripAwaitBase_ || gripAwaitRef_) return true;
    if (gripAddVertex_) {
        if (const auto target = singlePolylineGrip())
            if (auto out = detail::insertVertex(*target->first, target->second, *hoverWorld_)) drawGhost(painter, *out);
        return true;
    }
    GripChanges changes;
    if (gripMode_ == detail::GripMode::Stretch) {
        changes = stretchChanges(*hoverWorld_);
    } else if (const auto t = gripPointTransform(*hoverWorld_)) {
        changes = transformChanges(*t);
    }
    for (const auto& change : changes) drawGhost(painter, *change.second);
    return true;
}

} // namespace bcad::app
