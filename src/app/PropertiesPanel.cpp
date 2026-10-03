#include "PropertiesPanel.h"

#include "Commands.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/BooleanOps.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/properties/PropertyMap.h"
#include "bcad/properties/PropertyTypes.h"
#include <QColorDialog>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <cmath>
#include <algorithm>

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

    headerLabel_ = new QLabel(tr("Aucune sélection"), this);
    headerLabel_->setObjectName("propertiesHeader");
    layout->addWidget(headerLabel_);

    auto* form = new QFormLayout();
    layerCombo_ = new QComboBox(this);
    connect(layerCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &PropertiesPanel::onLayerChanged);
    form->addRow(tr("Calque"), layerCombo_);

    auto* colorRow = new QWidget(this);
    auto* colorRowLayout = new QHBoxLayout(colorRow);
    colorRowLayout->setContentsMargins(0, 0, 0, 0);
    colorButton_ = new QPushButton(tr("Par calque"), this);
    connect(colorButton_, &QPushButton::clicked, this, &PropertiesPanel::onColorButtonClicked);
    byLayerButton_ = new QPushButton(tr("Réinitialiser"), this);
    byLayerButton_->setToolTip(tr("Utiliser la couleur du calque"));
    connect(byLayerButton_, &QPushButton::clicked, this, &PropertiesPanel::onByLayerClicked);
    colorRowLayout->addWidget(colorButton_);
    colorRowLayout->addWidget(byLayerButton_);
    form->addRow(tr("Couleur"), colorRow);

    layout->addLayout(form);

    geometryInfoLabel_ = new QLabel(this);
    geometryInfoLabel_->setObjectName("propertiesGeometryInfo");
    geometryInfoLabel_->setWordWrap(true);
    geometryInfoLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    layout->addWidget(geometryInfoLabel_);

    propertyWidget_ = new QWidget(this);
    propertyForm_ = new QFormLayout(propertyWidget_);
    propertyWidget_->hide();
    layout->addWidget(propertyWidget_);

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
    return QString::fromStdString(e.geometryInfo());
}

void PropertiesPanel::refresh() {
    if (!doc_) return;
    std::vector<geom::Entity*> selected = selectedEntities();
    updating_ = true;
    const QSignalBlocker blockerLayer(layerCombo_);

    if (selected.empty()) {
        headerLabel_->setText(tr("Aucune sélection"));
        layerCombo_->clear();
        layerCombo_->setEnabled(false);
        colorButton_->setEnabled(false);
        colorButton_->setText(tr("Par calque"));
        colorButton_->setIcon(QIcon());
        byLayerButton_->setEnabled(false);
        geometryInfoLabel_->clear();
        propertyWidget_->hide();
        updating_ = false;
        return;
    }

    headerLabel_->setText(selected.size() == 1 ? tr("1 entité sélectionnée")
                                                : tr("%1 entités sélectionnées").arg(selected.size()));

    // --- Combo des calques : une entrée par calque du document, plus un
    // espace réservé en tête si la sélection couvre plusieurs calques.
    layerCombo_->clear();
    std::string firstLayer = selected.front()->layer();
    bool mixedLayer = false;
    for (geom::Entity* e : selected) {
        if (e->layer() != firstLayer) {
            mixedLayer = true;
            break;
        }
    }
    // Le calque cible est porte par la donnee de l'entree, pas par son texte :
    // l'espace reserve « (Mixte) » n'en a pas, et un calque reellement nomme
    // « (Mixte) » reste choisissable.
    if (mixedLayer) layerCombo_->addItem(tr("(Mixte)"));
    int matchIndex = -1;
    for (const auto& layer : doc_->layerManager().layers()) {
        const QString name = QString::fromStdString(layer.name);
        layerCombo_->addItem(name, name);
        if (!mixedLayer && layer.name == firstLayer) matchIndex = layerCombo_->count() - 1;
    }
    layerCombo_->setEnabled(true);
    layerCombo_->setCurrentIndex(mixedLayer ? 0 : std::max(matchIndex, 0));

    // --- Couleur : substitution commune, ByLayer commun, ou mixte.
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
        colorText = tr("(Mixte)");
    } else if (firstOverride) {
        swatch = *firstOverride;
        colorText = tr("Personnalisée");
    } else {
        const layers::Layer* layer = mixedLayer ? nullptr : doc_->layerManager().find(firstLayer);
        if (layer) swatch = layer->color;
        colorText = tr("Par calque");
    }
    QPixmap pix(16, 16);
    pix.fill(QColor::fromRgbF(swatch.r, swatch.g, swatch.b));
    colorButton_->setIcon(QIcon(pix));
    colorButton_->setText(colorText);
    colorButton_->setEnabled(true);
    byLayerButton_->setEnabled(true);

    // Surface géométrique vs contenance déclarée : geometryInfo() donne
    // l'aire calculée ("Surface géométrique"), contenance métier séparée.
    {
        QString info = selected.size() == 1 ? geometryInfoFor(*selected.front()) : QString();
        geometryInfoLabel_->setText(info);
    }

    if (selected.size() == 1) {
        rebuildPropertyEditors(selected.front());
    } else {
        propertyWidget_->hide();
    }

    updating_ = false;
}

void PropertiesPanel::onLayerChanged(int index) {
    if (updating_ || !doc_ || index < 0) return;
    const QVariant target = layerCombo_->itemData(index);
    if (!target.isValid()) return; // espace reserve « (Mixte) », pas un calque

    std::string newLayer = target.toString().toStdString();
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

void PropertiesPanel::rebuildPropertyEditors(geom::Entity* entity) {
    while (QLayoutItem* item = propertyForm_->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    std::vector<std::string> names = entity->properties().listNames();
    std::sort(names.begin(), names.end());
    for (const std::string& name : names) {
        const properties::Property* property = entity->properties().find(name);
        if (!property) continue;
        if (property->isReadOnly()) continue; // UI générique : pas d'éditeur pour read-only
        const QString label = QString::fromStdString(name);
        if (property->type() == properties::PropertyType::String) {
            auto* edit = new QLineEdit(QString::fromStdString(property->asString()), propertyWidget_);
            connect(edit, &QLineEdit::editingFinished, this, [this, entity, name, edit] {
                const std::string value = edit->text().toStdString();
                if (value == entity->properties().getString(name)) return;
                if (undoStack_) undoStack_->push(new SetEntityPropertyCommand(doc_, entity, name, properties::PropertyValue(value), tr("Edit property")));
                else {
                    entity->properties().setString(name, value);
                    doc_->notifyEntityChanged(entity);
                }
                refresh();
            });
            propertyForm_->addRow(label, edit);
        } else if (property->type() == properties::PropertyType::Enum) {
            auto* combo = new QComboBox(propertyWidget_);
            for (const auto& value : property->enumValues()) combo->addItem(QString::fromStdString(value));
            combo->setCurrentIndex(property->asEnum());
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                    [this, entity, name](int value) {
                        if (value == entity->properties().getEnum(name)) return;
                        if (undoStack_) undoStack_->push(new SetEntityPropertyCommand(doc_, entity, name, properties::PropertyValue(properties::EnumIndex(value)), tr("Edit property")));
                        else {
                            entity->properties().setEnum(name, value);
                            doc_->notifyEntityChanged(entity);
                        }
                        refresh();
                    });
            propertyForm_->addRow(label, combo);
        } else if (property->type() == properties::PropertyType::Double) {
            auto* spin = new QDoubleSpinBox(propertyWidget_);
            spin->setDecimals(6);
            spin->setValue(property->asDouble());
            if (property->hasRange()) {
                spin->setMinimum(property->min());
                spin->setMaximum(property->max());
            }
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                    [this, entity, name](double value) {
                        if (value == entity->properties().getDouble(name)) return;
                        if (undoStack_) undoStack_->push(new SetEntityPropertyCommand(doc_, entity, name, properties::PropertyValue(value), tr("Edit property")));
                        else {
                            entity->properties().setDouble(name, value);
                            doc_->notifyEntityChanged(entity);
                        }
                        refresh();
                    });
            propertyForm_->addRow(label, spin);
        } else if (property->type() == properties::PropertyType::Int) {
            auto* spin = new QSpinBox(propertyWidget_);
            spin->setValue(property->asInt());
            if (property->hasRange()) {
                spin->setMinimum(static_cast<int>(property->min()));
                spin->setMaximum(static_cast<int>(property->max()));
            }
            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                    [this, entity, name](int value) {
                        if (value == entity->properties().getInt(name)) return;
                        if (undoStack_) undoStack_->push(new SetEntityPropertyCommand(doc_, entity, name, properties::PropertyValue(value), tr("Edit property")));
                        else {
                            entity->properties().setInt(name, value);
                            doc_->notifyEntityChanged(entity);
                        }
                        refresh();
                    });
            propertyForm_->addRow(label, spin);
        } else if (property->type() == properties::PropertyType::Bool) {
            auto* check = new QCheckBox(propertyWidget_);
            check->setChecked(property->asBool());
            connect(check, &QCheckBox::toggled, this,
                    [this, entity, name](bool value) {
                        if (value == entity->properties().getBool(name)) return;
                        if (undoStack_) undoStack_->push(new SetEntityPropertyCommand(doc_, entity, name, properties::PropertyValue(value), tr("Edit property")));
                        else {
                            entity->properties().setBool(name, value);
                            doc_->notifyEntityChanged(entity);
                        }
                        refresh();
                    });
            propertyForm_->addRow(label, check);
        }
    }
    propertyWidget_->setVisible(propertyForm_->rowCount() > 0);
}

} // namespace bcad::app
