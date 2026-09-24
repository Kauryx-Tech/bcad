#pragma once

#include <QWidget>
#include <vector>

namespace bcad::core { class Document; }
namespace bcad::geom { class Entity; }

class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QWidget;
class QUndoStack;

namespace bcad::app {

// Affiche et modifie les propriétés de la sélection courante : calque,
// couleur de substitution, et informations géométriques en lecture seule.
// Pour une polyligne fermée on affiche "Surface géométrique" (calculée) et,
// si la propriété métier "cadastre.contenance" existe, "Contenance déclarée"
// séparément — ne jamais appeler l'aire géométrique "contenance légale".
// Calqué sur le panneau Propriétés AutoCAD (Général : Couleur/Calque/...).
//
// Se rafraîchit sur Viewport::selectionChanged. Ne s'accroche délibérément
// *pas* à layers::LayerManager::onChanged (std::function mono-abonné déjà
// réclamé par LayerPanel) ; Document notifie via events::EventBus multi-abonnés.
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
    void rebuildPropertyEditors(geom::Entity* entity);

    core::Document* doc_ = nullptr;
    QUndoStack* undoStack_ = nullptr;

    QLabel* headerLabel_ = nullptr;
    QComboBox* layerCombo_ = nullptr;
    QPushButton* colorButton_ = nullptr;
    QPushButton* byLayerButton_ = nullptr;
    QLabel* geometryInfoLabel_ = nullptr;

    QWidget* propertyWidget_ = nullptr;
    QFormLayout* propertyForm_ = nullptr;

    bool updating_ = false; // protège onLayerChanged pendant que refresh() repeuple le combo
};

} // namespace bcad::app
