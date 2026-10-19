// Saisie dynamique (F12, DYNMODE d'AutoCAD) et reperage polaire (F10).
// Saisie dynamique : pendant une commande, une
// bulle suit le curseur avec la consigne de l'etape et, en guise de champs,
// les coordonnees (premier point) ou la longueur et l'angle depuis le point
// precedent ; ce que l'utilisateur tape (dans la ligne de commande) s'y
// affiche aussi. Lecture seule : la saisie passe toujours par
// submitTypedPoint, une seule voie pour le clic et le clavier.

#include "Viewport.h"

#include "MeasureDetail.h"
#include "theme/Theme.h"

#include <QFontMetrics>
#include <QPainter>

#include <cmath>

namespace bcad::app {

void Viewport::drawDynamicInput(QPainter& painter) {
    if (!dynamicInput_ || tool_ == ToolMode::Select || !hoverWorld_) return;

    // Consigne courte, comme la bulle d'AutoCAD : l'etape apres « Outil — »,
    // sans options [..], exemples (..) ni « , Entree pour terminer ».
    QString step = prompt();
    if (const int dash = step.lastIndexOf(QStringLiteral(" — ")); dash >= 0) step = step.mid(dash + 3);
    for (const QChar stop : {QLatin1Char('['), QLatin1Char('('), QLatin1Char(',')})
        if (const int at = step.indexOf(stop); at > 0) step = step.left(at).trimmed();

    // Valeurs : longueur et angle depuis le point de reference, sinon X, Y.
    QString values;
    if (const auto ref = activeReferencePoint()) {
        const auto m = detail::measureDistance(*ref, *hoverWorld_);
        values = QStringLiteral("%1  <  %2°").arg(m.distance, 0, 'f', 3).arg(m.angleDeg, 0, 'f', 1);
    } else {
        values = QStringLiteral("%1 , %2").arg(hoverWorld_->x_, 0, 'f', 3).arg(hoverWorld_->y_, 0, 'f', 3);
    }
    const QString typed = dynamicText_.isEmpty() ? values : dynamicText_;

    const QFontMetrics fm(painter.font());
    const int pad = theme::kSpaceS;
    const int width = std::max(fm.horizontalAdvance(step), fm.horizontalAdvance(typed)) + 2 * pad + 4;
    const int lineH = fm.height();
    QPointF origin = toScreenF(*hoverWorld_) + QPointF(18, 18);
    // Reste dans le canevas pres des bords.
    if (origin.x() + width > this->width()) origin.rx() -= width + 36;
    if (origin.y() + 2 * lineH + 3 * pad > this->height()) origin.ry() -= 2 * lineH + 3 * pad + 36;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF promptBox(origin, QSizeF(width, lineH + 2 * pad));
    painter.setPen(QPen(theme::color(theme::Token::Border), 1));
    painter.setBrush(theme::color(theme::Token::Panel));
    painter.drawRoundedRect(promptBox, theme::kRadius, theme::kRadius);
    painter.setPen(theme::color(theme::Token::TextPrimary));
    painter.drawText(promptBox.adjusted(pad, 0, -pad, 0), Qt::AlignVCenter | Qt::AlignLeft, step);

    const QRectF field(origin + QPointF(0, lineH + 2 * pad + 2), QSizeF(width, lineH + 2 * pad));
    painter.setPen(QPen(theme::color(dynamicText_.isEmpty() ? theme::Token::Border : theme::Token::Accent), 1));
    painter.setBrush(theme::color(theme::Token::Field));
    painter.drawRoundedRect(field, theme::kRadius, theme::kRadius);
    painter.setPen(theme::color(dynamicText_.isEmpty() ? theme::Token::TextSecondary : theme::Token::TextStrong));
    painter.drawText(field.adjusted(pad, 0, -pad, 0), Qt::AlignVCenter | Qt::AlignLeft, typed);
    painter.restore();
}

void Viewport::drawPolarTracking(QPainter& painter) {
    // Rayon pointille de la direction reperee, jusqu'au bord du canevas, et
    // bulle « Polaire : longueur < angle », comme AutoCAD.
    if (!polarPoint_ || !polarRef_ || tool_ == ToolMode::Select) return;
    const QPointF from = toScreenF(*polarRef_);
    const QPointF to = toScreenF(*polarPoint_);
    const QPointF dir = to - from;
    const double len = std::hypot(dir.x(), dir.y());
    if (len < 1.0) return;
    const double reach = std::hypot(width(), height());
    painter.save();
    QPen pen(theme::color(theme::Token::Snap), 1, Qt::DotLine);
    painter.setPen(pen);
    painter.drawLine(from, from + dir / len * reach);
    const QString label = tr("Polaire : %1 < %2°").arg(polarDistance_, 0, 'f', 3).arg(polarAngleDeg_, 0, 'f', 0);
    const QFontMetrics fm(painter.font());
    const QRectF box(to + QPointF(16, -fm.height() - 18), QSizeF(fm.horizontalAdvance(label) + 2 * theme::kSpaceS,
                                                                fm.height() + theme::kSpaceS));
    painter.setPen(QPen(theme::color(theme::Token::Border), 1));
    painter.setBrush(theme::color(theme::Token::Panel));
    painter.drawRoundedRect(box, theme::kRadius, theme::kRadius);
    painter.setPen(theme::color(theme::Token::TextPrimary));
    painter.drawText(box, Qt::AlignCenter, label);
    painter.restore();
}

} // namespace bcad::app
