#pragma once

#include "detail/GeometryFieldDetail.h"

#include <QString>
#include <QWidget>
#include <vector>

class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QUndoStack;
class QVBoxLayout;

namespace bcad::core {
class Document;
}
namespace bcad::geom {
class Entity;
}

namespace bcad::app {

// Nom d'objet stable d'un champ : les tests le retrouvent par findChild sans
// dépendre de l'ordre des lignes de la section.
QString geometryFieldObjectName(detail::GeometryField field);

// Section « Géométrie » de la palette Propriétés (P-01) : les champs qu'une
// entité expose, éditables en ligne — chaque texte repris dans une commande
// annulable unique (EditBatch), chaque refus montré par un bord rouge et un
// message sobre en barre d'état, jamais par une fenêtre de dialogue
// (docs/DESIGN_SYSTEM.md). Les valeurs calculées (longueur, aire, diamètre…)
// sont visibles mais désactivées ; un texte sans champs propres (texte,
// cotation) masque la section.
//
// Se rafraîchit quand l'entité change (écriture, annulation, autre outil) et
// relit alors ses valeurs sur place : reconstruire les lignes volerait le
// focus de la saisie en cours.
class PropertiesGeometrySection : public QWidget {
    Q_OBJECT
public:
    explicit PropertiesGeometrySection(QWidget* parent = nullptr);

    // Affiche les champs de `entity` (masquée sans entité, ou si son type
    // n'expose aucun champ). Les lignes ne sont reconstruites que si l'objet,
    // ses champs ou son nombre de sommets changent.
    void setEntity(core::Document* doc, geom::Entity* entity, QUndoStack* stack);

    // Relit les valeurs de l'entité courante dans les champs existants.
    void updateValues();

signals:
    void statusMessage(const QString& message);
    // Une entité vient d'être remplacée : l'hôte rafraîchit le canevas, et le
    // panneau sa ligne d'information géométrique.
    void entityReplaced();

private:
    // Une ligne de la section. Copiée dans les lambdas : le vecteur est
    // reconstruit à chaque changement d'objet, ses éléments ne tiennent pas.
    struct Row {
        detail::GeometryField field;
        QLineEdit* edit = nullptr;
    };

    static QString labelFor(detail::GeometryField field);
    static QString rejectMessage(detail::GeometryEditReject reject);
    geom::Entity* currentEntity() const;
    void rebuild();
    void commit(Row row);
    void showError(QLineEdit* edit, const QString& message);
    void clearError(QLineEdit* edit);

    core::Document* doc_ = nullptr;
    QUndoStack* stack_ = nullptr;
    int entityId_ = -1;
    std::size_t vertexCount_ = 0;
    int vertex_ = 0; // sommet montré pour une polyligne
    std::vector<detail::GeometryField> fields_;

    QLabel* title_ = nullptr;
    QVBoxLayout* layout_ = nullptr;
    QWidget* rowsHost_ = nullptr;
    QFormLayout* rowsForm_ = nullptr;
    QComboBox* vertexCombo_ = nullptr;
    std::vector<Row> rows_;
};

} // namespace bcad::app
