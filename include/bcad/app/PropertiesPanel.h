#pragma once

#include "bcad/core/Document.h"
#include <QWidget>
#include <vector>

class QComboBox;
class QLabel;
class QPushButton;
class QUndoStack;

namespace bcad::app {

// Shows and edits properties of the current selection: layer, color
// override, and read-only geometry info (length/radius/area depending on
// entity type — reuses geom::polygonArea for closed polylines, giving a
// géomètre-usable "surface légale" readout for free). Modeled on the
// Propriétés panel of AutoCAD-family tools (Général: Couleur/Calque/...).
//
// Refreshes on Viewport::selectionChanged. Deliberately does *not* hook
// core::Document::onChanged or layers::LayerManager::onChanged — both are
// single-subscriber std::function callbacks already claimed by Viewport and
// LayerPanel respectively; adding a second subscriber here would silently
// replace theirs. A multi-subscriber signal for both would be the proper
// fix (documented as follow-up work), out of scope for this pass.
class PropertiesPanel : public QWidget {
    Q_OBJECT
public:
    explicit PropertiesPanel(QWidget* parent = nullptr);

    void setDocument(core::Document* doc);
    void setUndoStack(QUndoStack* stack) { undoStack_ = stack; }

public slots:
    void refresh();

private slots:
    void onLayerChanged(int index);
    void onColorButtonClicked();
    void onByLayerClicked();

private:
    std::vector<geom::Entity*> selectedEntities() const;
    QString geometryInfoFor(const geom::Entity& e) const;

    core::Document* doc_ = nullptr;
    QUndoStack* undoStack_ = nullptr;

    QLabel* headerLabel_ = nullptr;
    QComboBox* layerCombo_ = nullptr;
    QPushButton* colorButton_ = nullptr;
    QPushButton* byLayerButton_ = nullptr;
    QLabel* geometryInfoLabel_ = nullptr;

    bool updating_ = false; // guards onLayerChanged while refresh() repopulates the combo
};

} // namespace bcad::app
