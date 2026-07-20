#include "bcad/app/PropertiesPanel.h"

#include "bcad/app/Commands.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/BooleanOps.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"
#include <QColorDialog>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QUndoStack>
#include <QVBoxLayout>
#include <cmath>

namespace bcad::app {

namespace {
bool colorEquals(const std::optional<geom::Color>& a, const std::optional<geom::Color>& b) {
    if (a.has_value() != b.has_value()) return false;
    if (!a) return true;
    return a->r == b->r && a->g == b->g && a->b == b->b && a->a == b->a;
}
} // namespace

PropertiesPanel::PropertiesPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    headerLabel_ = new QLabel(tr("No selection"), this);
    headerLabel_->setObjectName("propertiesHeader");
    layout->addWidget(headerLabel_);

    auto* form = new QFormLayout();
    layerCombo_ = new QComboBox(this);
    connect(layerCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &PropertiesPanel::onLayerChanged);
    form->addRow(tr("Layer"), layerCombo_);

    auto* colorRow = new QWidget(this);
    auto* colorRowLayout = new QHBoxLayout(colorRow);
    colorRowLayout->setContentsMargins(0, 0, 0, 0);
    colorButton_ = new QPushButton(tr("ByLayer"), this);
    connect(colorButton_, &QPushButton::clicked, this, &PropertiesPanel::onColorButtonClicked);
    byLayerButton_ = new QPushButton(tr("Reset"), this);
    byLayerButton_->setToolTip(tr("Use the layer's color instead of a per-entity override"));
    connect(byLayerButton_, &QPushButton::clicked, this, &PropertiesPanel::onByLayerClicked);
    colorRowLayout->addWidget(colorButton_);
    colorRowLayout->addWidget(byLayerButton_);
    form->addRow(tr("Color"), colorRow);

    layout->addLayout(form);

    geometryInfoLabel_ = new QLabel(this);
    geometryInfoLabel_->setObjectName("propertiesGeometryInfo");
    geometryInfoLabel_->setWordWrap(true);
    geometryInfoLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    layout->addWidget(geometryInfoLabel_);

    layout->addStretch(1);

    layerCombo_->setEnabled(false);
    colorButton_->setEnabled(false);
    byLayerButton_->setEnabled(false);
}

void PropertiesPanel::setDocument(core::Document* doc) {
    doc_ = doc;
    refresh();
}

std::vector<geom::Entity*> PropertiesPanel::selectedEntities() const {
    std::vector<geom::Entity*> out;
    if (!doc_) return out;
    for (const auto& e : doc_->entities()) {
        if (e->selected) out.push_back(e.get());
    }
    return out;
}

QString PropertiesPanel::geometryInfoFor(const geom::Entity& e) const {
    switch (e.type()) {
        case geom::EntityType::Line: {
            const auto& l = static_cast<const geom::LineEntity&>(e);
            return tr("Line\nLength: %1").arg(l.length(), 0, 'f', 3);
        }
        case geom::EntityType::Circle: {
            const auto& c = static_cast<const geom::CircleEntity&>(e);
            return tr("Circle\nRadius: %1\nCenter: (%2, %3)")
                .arg(c.radius(), 0, 'f', 3)
                .arg(CGAL::to_double(c.center().x()), 0, 'f', 3)
                .arg(CGAL::to_double(c.center().y()), 0, 'f', 3);
        }
        case geom::EntityType::Arc: {
            const auto& a = static_cast<const geom::ArcEntity&>(e);
            return tr("Arc\nRadius: %1\nSweep: %2°")
                .arg(a.radius(), 0, 'f', 3)
                .arg(geom::toDegrees(a.sweep()), 0, 'f', 1);
        }
        case geom::EntityType::Polyline: {
            const auto& p = static_cast<const geom::PolylineEntity&>(e);
            QString s = tr("Polyline (%1)\nVertices: %2\nLength: %3")
                            .arg(p.closed() ? tr("closed") : tr("open"))
                            .arg(p.vertices().size())
                            .arg(p.length(), 0, 'f', 3);
            if (p.closed()) s += tr("\nArea: %1").arg(std::abs(geom::polygonArea(p)), 0, 'f', 3);
            return s;
        }
        case geom::EntityType::Point: {
            const auto& pt = static_cast<const geom::PointEntity&>(e);
            return tr("Point\n(%1, %2)")
                .arg(CGAL::to_double(pt.position().x()), 0, 'f', 3)
                .arg(CGAL::to_double(pt.position().y()), 0, 'f', 3);
        }
    }
    return {};
}

void PropertiesPanel::refresh() {
    if (!doc_) return;
    std::vector<geom::Entity*> selected = selectedEntities();
    updating_ = true;

    if (selected.empty()) {
        headerLabel_->setText(tr("No selection"));
        layerCombo_->clear();
        layerCombo_->setEnabled(false);
        colorButton_->setEnabled(false);
        colorButton_->setText(tr("ByLayer"));
        colorButton_->setIcon(QIcon());
        byLayerButton_->setEnabled(false);
        geometryInfoLabel_->clear();
        updating_ = false;
        return;
    }

    headerLabel_->setText(selected.size() == 1 ? tr("1 entity selected")
                                                : tr("%1 entities selected").arg(selected.size()));

    // --- Layer combo: one entry per document layer, plus a placeholder at
    // the front if the selection spans more than one layer.
    layerCombo_->clear();
    std::string firstLayer = selected.front()->layer();
    bool mixedLayer = false;
    for (geom::Entity* e : selected) {
        if (e->layer() != firstLayer) {
            mixedLayer = true;
            break;
        }
    }
    if (mixedLayer) layerCombo_->addItem(tr("(Mixed)"));
    int matchIndex = -1;
    for (const auto& layer : doc_->layerManager().layers()) {
        layerCombo_->addItem(QString::fromStdString(layer.name));
        if (!mixedLayer && layer.name == firstLayer) matchIndex = layerCombo_->count() - 1;
    }
    layerCombo_->setEnabled(true);
    layerCombo_->setCurrentIndex(mixedLayer ? 0 : std::max(matchIndex, 0));

    // --- Color: common override, common ByLayer, or mixed.
    std::optional<geom::Color> firstOverride = selected.front()->colorOverride();
    bool mixedColor = false;
    for (geom::Entity* e : selected) {
        if (!colorEquals(e->colorOverride(), firstOverride)) {
            mixedColor = true;
            break;
        }
    }
    geom::Color swatch = geom::Color::fromRgb255(255, 255, 255);
    QString colorText;
    if (mixedColor) {
        swatch = geom::Color::fromRgb255(140, 140, 140);
        colorText = tr("(Mixed)");
    } else if (firstOverride) {
        swatch = *firstOverride;
        colorText = tr("Custom");
    } else {
        const layers::Layer* layer = mixedLayer ? nullptr : doc_->layerManager().find(firstLayer);
        if (layer) swatch = layer->color;
        colorText = tr("ByLayer");
    }
    QPixmap pix(16, 16);
    pix.fill(QColor::fromRgbF(swatch.r, swatch.g, swatch.b));
    colorButton_->setIcon(QIcon(pix));
    colorButton_->setText(colorText);
    colorButton_->setEnabled(true);
    byLayerButton_->setEnabled(true);

    geometryInfoLabel_->setText(selected.size() == 1 ? geometryInfoFor(*selected.front()) : QString());

    updating_ = false;
}

void PropertiesPanel::onLayerChanged(int index) {
    if (updating_ || !doc_ || index < 0) return;
    QString text = layerCombo_->itemText(index);
    if (text == tr("(Mixed)")) return; // placeholder, not a real target

    std::string newLayer = text.toStdString();
    std::vector<geom::Entity*> selected = selectedEntities();
    if (selected.empty()) return;

    if (undoStack_) undoStack_->beginMacro(tr("Change Layer"));
    for (geom::Entity* e : selected) {
        if (e->layer() == newLayer) continue;
        if (undoStack_) {
            undoStack_->push(new SetLayerCommand(doc_, e, newLayer, tr("Change Layer")));
        } else {
            e->setLayer(newLayer);
            doc_->notifyEntityChanged(e);
        }
    }
    if (undoStack_) undoStack_->endMacro();
    refresh();
}

void PropertiesPanel::onColorButtonClicked() {
    if (!doc_) return;
    std::vector<geom::Entity*> selected = selectedEntities();
    if (selected.empty()) return;

    geom::Color initial = selected.front()->colorOverride().value_or(geom::Color::fromRgb255(255, 255, 255));
    QColor chosen = QColorDialog::getColor(QColor::fromRgbF(initial.r, initial.g, initial.b), this, tr("Entity Color"));
    if (!chosen.isValid()) return;

    geom::Color newColor{ static_cast<float>(chosen.redF()), static_cast<float>(chosen.greenF()),
                           static_cast<float>(chosen.blueF()), static_cast<float>(chosen.alphaF()) };
    if (undoStack_) undoStack_->beginMacro(tr("Change Color"));
    for (geom::Entity* e : selected) {
        if (undoStack_) {
            undoStack_->push(new SetColorOverrideCommand(doc_, e, newColor, tr("Change Color")));
        } else {
            e->setColorOverride(newColor);
            doc_->notifyEntityChanged(e);
        }
    }
    if (undoStack_) undoStack_->endMacro();
    refresh();
}

void PropertiesPanel::onByLayerClicked() {
    if (!doc_) return;
    std::vector<geom::Entity*> selected = selectedEntities();
    if (selected.empty()) return;

    if (undoStack_) undoStack_->beginMacro(tr("Use Layer Color"));
    for (geom::Entity* e : selected) {
        if (undoStack_) {
            undoStack_->push(new SetColorOverrideCommand(doc_, e, std::nullopt, tr("Use Layer Color")));
        } else {
            e->setColorOverride(std::nullopt);
            doc_->notifyEntityChanged(e);
        }
    }
    if (undoStack_) undoStack_->endMacro();
    refresh();
}

} // namespace bcad::app
