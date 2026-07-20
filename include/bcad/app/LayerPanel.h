#pragma once

#include "bcad/core/Document.h"
#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;

namespace bcad::app {

// Widget ancrable listant tous les calques avec des cases visibilité/verrou,
// une pastille de couleur et un renommage en ligne — le panneau "Layer
// Management System" du document d'architecture, relié directement à
// LayerManager.
class LayerPanel : public QWidget {
    Q_OBJECT
public:
    explicit LayerPanel(QWidget* parent = nullptr);

    void setDocument(core::Document* doc);
    void refresh();

signals:
    void layerActivated(const QString& name);

private slots:
    void onItemChanged(QTreeWidgetItem* item, int column);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onAddLayerClicked();
    void onRemoveLayerClicked();

private:
    QTreeWidgetItem* itemForRow(int row) const;

    core::Document* doc_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    bool updating_ = false;
};

} // namespace bcad::app
