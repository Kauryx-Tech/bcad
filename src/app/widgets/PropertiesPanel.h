#pragma once

#include "bcad/properties/PropertyTypes.h"

#include <QWidget>
#include <functional>
#include <string>
#include <vector>

namespace bcad::core { class Document; }
namespace bcad::geom { class Entity; }
namespace bcad::properties { class PropertyMap; }

class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QWidget;
class QUndoStack;

namespace bcad::app {

class PropertiesGeometrySection;

// Affiche et modifie les propriétés de la sélection courante : calque,
// couleur de substitution, champs géométriques éditables (section « Géométrie »,
// PropertiesGeometrySection) et informations géométriques en lecture seule.
// Pour une polyligne fermée on affiche "Surface géométrique" (calculée) et, si
// le domaine détient une propriété de surface déclarée, celle-ci séparément — ne
// jamais appeler l'aire géométrique "surface légale".
// Calqué sur le panneau Propriétés AutoCAD (Général : Couleur/Calque/...).
// Sans sélection : les attributs du dessin (Document::properties), que les
// modules déclarent (un cartouche, un dossier) — l'hôte n'en nomme aucun.
//
// Se rafraîchit sur Viewport::selectionChanged, et sur QUndoStack::indexChanged
// de la pile courante : annuler une écriture hors du canevas (palette, ligne de
// commande) remet les valeurs affichées sans attendre un clic sur le dessin.
// Ne s'accroche délibérément *pas* à layers::LayerManager::onChanged
// (std::function mono-abonné déjà réclamé par LayerPanel) ; Document notifie
// via events::EventBus multi-abonnés.
class PropertiesPanel : public QWidget {
    Q_OBJECT
public:
    explicit PropertiesPanel(QWidget* parent = nullptr);

    void setDocument(core::Document* doc);
    // Connecte le panneau à la pile du dessin courant (et se déconnecte de la
    // précédente) : hors-ligne, testable sans fenêtre.
    void setUndoStack(QUndoStack* stack);

public slots:
    void refresh();

signals:
    void statusMessage(const QString& message);
    // Une entité a été remplacée depuis la section Géométrie : l'hôte
    // rafraîchit le canevas (viewport_->refreshDrawing()).
    void entityReplaced();

private slots:
    void onLayerChanged(int index);
    void onColorButtonClicked();
    void onByLayerClicked();
    // Rafraîchissement différé, en un seul appel par événement de pile :
    // le champ qui vient de pousser une commande est encore dans son signal.
    void queueRefresh();

private:
    std::vector<geom::Entity*> selectedEntities() const;
    QString geometryInfoFor(const geom::Entity& e) const;
    // Un editeur par propriete d'une PropertyMap ; `apply` porte la valeur
    // (commande annulable de l'objet ou du dessin).
    using PropertyApply = std::function<void(const std::string&, const properties::PropertyValue&)>;
    void rebuildPropertyEditors(const properties::PropertyMap& map, const PropertyApply& apply);
    void rebuildEntityEditors(geom::Entity* entity);
    void rebuildDocumentEditors();

    core::Document* doc_ = nullptr;
    QUndoStack* undoStack_ = nullptr;
    bool refreshQueued_ = false;

    QLabel* headerLabel_ = nullptr;
    QComboBox* layerCombo_ = nullptr;
    QPushButton* colorButton_ = nullptr;
    QPushButton* byLayerButton_ = nullptr;
    QLabel* geometryInfoLabel_ = nullptr;
    PropertiesGeometrySection* geometrySection_ = nullptr;

    QWidget* propertyWidget_ = nullptr;
    QFormLayout* propertyForm_ = nullptr;

    bool updating_ = false; // protège onLayerChanged pendant que refresh() repeuple le combo
};

} // namespace bcad::app
