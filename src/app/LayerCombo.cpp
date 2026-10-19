#include "theme/Theme.h"
#include "LayerCombo.h"

#include "bcad/core/Document.h"

#include <QAbstractItemView>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QStylePainter>

namespace bcad::app {

namespace {

constexpr int kVisibleRole = Qt::UserRole + 1;
constexpr int kLockedRole = Qt::UserRole + 2;
constexpr int kColorRole = Qt::UserRole + 3;
constexpr int kCurrentRole = Qt::UserRole + 4;

// Ampoule allumee (jaune) ou eteinte (bleu gris), comme AutoCAD.
void paintBulb(QPainter& p, const QRect& r, bool on) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const QColor glass = on ? theme::color(theme::Token::LayerOn) : theme::color(theme::Token::LayerOff);
    p.setPen(QPen(glass.darker(140), 1));
    p.setBrush(glass);
    p.drawEllipse(QRectF(r.x() + 3, r.y() + 1, 10, 10));
    p.setBrush(theme::color(theme::Token::BulbBase));
    p.setPen(Qt::NoPen);
    p.drawRect(QRectF(r.x() + 5.5, r.y() + 11, 5, 3.5));
    p.restore();
}

// Cadenas ouvert ou ferme.
void paintLock(QPainter& p, const QRect& r, bool locked) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const QColor c = locked ? theme::color(theme::Token::LockOn) : theme::color(theme::Token::LockOff);
    p.setPen(QPen(c, 1.4));
    p.setBrush(Qt::NoBrush);
    QPainterPath shackle;
    shackle.moveTo(r.x() + 5, r.y() + 7.5);
    shackle.lineTo(r.x() + 5, r.y() + 5);
    shackle.arcTo(QRectF(r.x() + 5, r.y() + 1.5, 6, 7), 180, -180);
    if (locked) shackle.lineTo(r.x() + 11, r.y() + 7.5);
    p.drawPath(shackle);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawRoundedRect(QRectF(r.x() + 3, r.y() + 7.5, 10, 7), 1.5, 1.5);
    p.restore();
}

void paintRow(QPainter& p, const QRect& rect, const QModelIndex& index, const QPalette& pal) {
    const int top = rect.y() + (rect.height() - LayerCombo::kZone) / 2;
    paintBulb(p, QRect(rect.x() + LayerCombo::kBulbX, top, LayerCombo::kZone, LayerCombo::kZone),
              index.data(kVisibleRole).toBool());
    paintLock(p, QRect(rect.x() + LayerCombo::kLockX, top, LayerCombo::kZone, LayerCombo::kZone),
              index.data(kLockedRole).toBool());
    p.fillRect(QRect(rect.x() + LayerCombo::kColorX + 2, top + 2, 12, 12), index.data(kColorRole).value<QColor>());
    p.setPen(pal.color(QPalette::Text));
    QFont font = p.font();
    font.setBold(index.data(kCurrentRole).toBool());
    p.setFont(font);
    p.drawText(QRect(rect.x() + LayerCombo::kTextX, rect.y(), rect.width() - LayerCombo::kTextX, rect.height()),
               Qt::AlignVCenter | Qt::AlignLeft, index.data(Qt::DisplayRole).toString());
}

class LayerRowDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        if (option.state & QStyle::State_Selected) p->fillRect(option.rect, option.palette.highlight());
        else if (option.state & QStyle::State_MouseOver) p->fillRect(option.rect, option.palette.midlight());
        paintRow(*p, option.rect, index, option.palette);
    }
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QSize s = QStyledItemDelegate::sizeHint(option, index);
        s.setHeight(std::max(s.height(), 22));
        return s;
    }
};

} // namespace

LayerCombo::LayerCombo(QWidget* parent) : QComboBox(parent) {
    setObjectName("ribbonLayerCombo");
    setToolTip(tr("Calque courant — l'ampoule allume ou éteint, le cadenas verrouille"));
    setMinimumWidth(180);
    setMaxVisibleItems(20);
    model_ = new QStandardItemModel(this);
    setModel(model_);
    setItemDelegate(new LayerRowDelegate(this));
    view()->setMouseTracking(true);
    view()->viewport()->installEventFilter(this);
    connect(this, qOverload<int>(&QComboBox::activated), this, &LayerCombo::onActivated);
}

void LayerCombo::setDocument(core::Document* doc) {
    doc_ = doc;
    refresh();
}

void LayerCombo::refresh() {
    if (!doc_ || updating_) return;
    const auto& manager = doc_->layerManager();
    const auto& layers = manager.layers();
    bool sameNames = model_->rowCount() == static_cast<int>(layers.size());
    for (int i = 0; sameNames && i < model_->rowCount(); ++i)
        sameNames = model_->item(i)->text() == QString::fromStdString(layers[i].name);
    if (!sameNames) {
        const QSignalBlocker quiet(this);
        model_->clear();
        for (const auto& layer : layers) model_->appendRow(new QStandardItem(QString::fromStdString(layer.name)));
    }
    int current = 0;
    for (int i = 0; i < static_cast<int>(layers.size()); ++i) {
        const auto& layer = layers[i];
        QStandardItem* item = model_->item(i);
        item->setData(layer.visible, kVisibleRole);
        item->setData(layer.locked, kLockedRole);
        item->setData(QColor::fromRgbF(layer.color.r, layer.color.g, layer.color.b), kColorRole);
        const bool isCurrent = layer.name == manager.currentLayerName();
        item->setData(isCurrent, kCurrentRole);
        if (isCurrent) current = i;
    }
    const QSignalBlocker quiet(this);
    setCurrentIndex(current);
    update();
}

void LayerCombo::onActivated(int row) {
    if (!doc_ || row < 0 || row >= model_->rowCount()) return;
    doc_->layerManager().setCurrentLayer(model_->item(row)->text().toStdString());
    refresh();
}

bool LayerCombo::eventFilter(QObject* watched, QEvent* event) {
    // Clic sur l'ampoule ou le cadenas d'une ligne de la liste ouverte : on
    // bascule et on garde la liste ouverte, comme AutoCAD.
    if (watched == view()->viewport() && doc_ &&
        (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease)) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        const QModelIndex index = view()->indexAt(mouse->position().toPoint());
        if (index.isValid()) {
            const int x = mouse->position().toPoint().x() - view()->visualRect(index).x();
            const bool onBulb = x >= kBulbX && x < kBulbX + kZone;
            const bool onLock = x >= kLockX && x < kLockX + kZone;
            if (onBulb || onLock) {
                if (event->type() == QEvent::MouseButtonRelease) {
                    const std::string name = index.data(Qt::DisplayRole).toString().toStdString();
                    auto& manager = doc_->layerManager();
                    if (const auto* layer = manager.find(name)) {
                        if (onBulb) manager.setVisible(name, !layer->visible);
                        else manager.setLocked(name, !layer->locked);
                    }
                    refresh();
                }
                return true;
            }
        }
    }
    return QComboBox::eventFilter(watched, event);
}

void LayerCombo::paintEvent(QPaintEvent*) {
    // Liste fermee : la ligne du calque courant, avec ses icones.
    QStylePainter painter(this);
    QStyleOptionComboBox option;
    initStyleOption(&option);
    option.currentText.clear();
    option.currentIcon = QIcon();
    painter.drawComplexControl(QStyle::CC_ComboBox, option);
    const QModelIndex index = model_->index(currentIndex(), 0);
    if (!index.isValid()) return;
    const QRect field = style()->subControlRect(QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxEditField, this);
    paintRow(painter, field, index, palette());
}

} // namespace bcad::app
