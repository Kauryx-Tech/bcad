#pragma once

#include "bcad/core/Document.h"
#include <QWidget>
#include <QTreeWidget>
#include <QComboBox>
#include <QLineEdit>
#include <string>
#include <vector>

class QPushButton;

namespace bcad::app {

// Panneau de gestion des calques, enrichi d'après les pratiques AutoCAD
// (Layer Properties Manager) : recherche par nom, filtres de propriété
// (visibilité/verrouillage), colonnes épaisseur/type de trait, calque
// courant, menu contextuel (isoler, propriétés), états de calque
// sauvegardés via QSettings.
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
    void onSearchTextChanged(const QString& text);
    void onFilterChanged(int index);
    void onAddLayerClicked();
    void onRemoveLayerClicked();
    void onContextMenuRequested(const QPoint& pos);
    void onSaveStateClicked();
    void onRestoreStateClicked();

private:
    enum FilterMode { FilterAll, FilterVisible, FilterHidden, FilterLocked, FilterUnlocked };

    bool passesFilter(const layers::Layer& layer) const;
    void applyFilters();
    void setCurrentLayer(const std::string& name);
    void isolateLayer(const std::string& name);
    void showAllLayers();
    void showLayerProperties(const std::string& name);
    static std::string serializeStates(const std::vector<layers::Layer>& layers);
    void applyState(const std::string& data);

    core::Document* doc_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    QLineEdit* searchEdit_ = nullptr;
    QComboBox* filterCombo_ = nullptr;
    QPushButton* saveStateBtn_ = nullptr;
    QPushButton* restoreStateBtn_ = nullptr;
    bool updating_ = false;
};

} // namespace bcad::app
