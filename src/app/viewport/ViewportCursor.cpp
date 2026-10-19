// Pointeur du canevas (S-11), comme AutoCAD : le pointeur du systeme est
// masque et remplace par un reticule en croix, dessine en dernier. Trois
// formes : au repos, croix et cible (on pointe un objet ou on lance une
// commande) ; quand un point est attendu, croix seule ; quand des objets sont
// a designer, cible seule. Taille, couleur et cible viennent de la boite
// Options (detail::CursorStyle) ; la cible est la tolerance de clic.

#include "viewport/Viewport.h"

#include "theme/Theme.h"

#include <QEnterEvent>
#include <QPainter>

namespace bcad::app {

void Viewport::setCursorStyle(const detail::CursorStyle& style) {
    cursorStyle_ = style.clamped();
    applyCanvasCursor();
    update();
}

detail::CursorShape Viewport::cursorShape() const {
    if (pickingObjects_) return detail::CursorShape::Box;
    switch (tool_) {
        case ToolMode::Select:
            return detail::CursorShape::CrosshairAndBox;
        // Outils qui designent un objet au clic, sans accrochage.
        case ToolMode::Trim:
        case ToolMode::Extend:
        case ToolMode::Break:
        case ToolMode::Fillet:
            return detail::CursorShape::Box;
        default:
            return detail::CursorShape::Crosshair;
    }
}

void Viewport::applyCanvasCursor() {
    if (tool_ == ToolMode::Pan) setCursor(Qt::OpenHandCursor);
    else if (cursorStyle_.systemPointer) unsetCursor();
    else setCursor(Qt::BlankCursor);
}

void Viewport::enterEvent(QEnterEvent* event) {
    cursorInside_ = true;
    QOpenGLWidget::enterEvent(event);
    update();
}

void Viewport::leaveEvent(QEvent* event) {
    // Hors du canevas, ni reticule ni survol.
    cursorInside_ = false;
    QOpenGLWidget::leaveEvent(event);
    update();
}

void Viewport::drawCursor(QPainter& painter) {
    if (!cursorInside_ || cursorStyle_.systemPointer || tool_ == ToolMode::Pan) return;
    const QPointF c(cursorScreen_);
    const QColor color = cursorStyle_.crosshairRgb ? QColor::fromRgb(*cursorStyle_.crosshairRgb)
                                                   : theme::color(theme::Token::Crosshair);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(color, 0));   // trait d'un pixel, quel que soit le zoom
    painter.setBrush(Qt::NoBrush);
    const detail::CursorShape shape = cursorShape();
    if (shape != detail::CursorShape::Box) {
        const double half = cursorStyle_.crosshairHalfLength(width(), height());
        painter.drawLine(QPointF(c.x() - half, c.y()), QPointF(c.x() + half, c.y()));
        painter.drawLine(QPointF(c.x(), c.y() - half), QPointF(c.x(), c.y() + half));
    }
    if (shape != detail::CursorShape::Crosshair) {
        const double box = cursorStyle_.pickboxPx;
        painter.drawRect(QRectF(c.x() - box, c.y() - box, 2 * box, 2 * box));
    }
    painter.restore();
}

} // namespace bcad::app
