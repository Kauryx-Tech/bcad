#pragma once

#include "bcad/core/Document.h"
#include <QWidget>
#include <vector>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QUndoStack;

namespace bcad::app {

// Affiche et modifie les propriétés de la sélection courante : calque,
// couleur de substitution, et informations géométriques en lecture seule
// (longueur/rayon/aire selon le type d'entité — réutilise geom::polygonArea
// pour les polylignes fermées, ce qui donne gratuitement une lecture de
// "surface légale" utilisable par un géomètre). Calqué sur le panneau
// Propriétés des outils de la famille AutoCAD (Général : Couleur/Calque/...).
//
// Se rafraîchit sur Viewport::selectionChanged. Ne s'accroche délibérément
// *pas* à core::Document::onChanged ni à layers::LayerManager::onChanged —
// les deux sont des callbacks std::function à un seul abonné déjà réclamés
// respectivement par Viewport et LayerPanel ; ajouter un second abonné ici
// remplacerait silencieusement le leur. Un signal à abonnés multiples pour
// les deux serait la vraie solution (documentée comme travail à faire),
// hors du périmètre de cette passe.
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

private slots:
    void onCadastreEditFinished();

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

    // Champs cadastre (visibles seulement pour cadastre.parcel)
    QLineEdit* sectionEdit_ = nullptr;
    QLineEdit* numeroEdit_ = nullptr;
    QLineEdit* contenanceEdit_ = nullptr;
    QLineEdit* communeEdit_ = nullptr;
    QLineEdit* proprietaireEdit_ = nullptr;
    QLineEdit* natureEdit_ = nullptr;
    QWidget* cadastreWidget_ = nullptr;

    bool updating_ = false; // protège onLayerChanged pendant que refresh() repeuple le combo
};

} // namespace bcad::app
