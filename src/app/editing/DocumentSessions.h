#pragma once

// En-tete interne a src/app : il n'est pas installe et n'entre pas dans le SDK.
//
// Les dessins ouverts dans la fenetre, un par onglet. Chacun garde ce qui lui
// est propre — document, historique d'annulation, chemin, etat « modifie »,
// cadrage de la vue — pour qu'en passer de l'un a l'autre ne perde rien et
// qu'un Ctrl+Z n'annule jamais dans le dessin d'a cote.
//
// Logique pure, sans widget : la fenetre ne fait qu'en afficher l'etat.

#include "bcad/core/Document.h"
#include "bcad/render/Camera2D.h"

#include <QString>
#include <QUndoStack>

#include <memory>
#include <optional>
#include <vector>

namespace bcad::app {

struct DocumentSession {
    std::unique_ptr<core::Document> document = std::make_unique<core::Document>();
    std::unique_ptr<QUndoStack> undoStack = std::make_unique<QUndoStack>();
    QString filePath;      // vide tant que le dessin n'a jamais ete enregistre
    QString untitledName;  // « Dessin1 », ou le nom du DXF importe
    bool dirty = false;    // modifie depuis le dernier chargement / enregistrement
    std::optional<render::Camera2D> camera;  // vide = cadrer sur tout a l'activation

    QString displayName() const;
    // Libelle d'onglet : nom, suivi de « * » si le dessin est modifie.
    QString tabLabel() const;
    // Un dessin jamais enregistre, intact et vide : ouvrir un fichier peut le
    // remplacer au lieu d'empiler un onglet inutile.
    bool isPristine() const;
};

class DocumentSessions {
public:
    // Ajoute un dessin vierge nomme « Dessin<n> » et rend son index.
    int addUntitled(const QString& baseName);
    // Ajoute une session deja remplie et rend son index.
    int add(std::unique_ptr<DocumentSession> session);
    void remove(int index);
    void move(int from, int to);

    int count() const { return static_cast<int>(sessions_.size()); }
    DocumentSession& at(int index) { return *sessions_.at(static_cast<size_t>(index)); }
    const DocumentSession& at(int index) const { return *sessions_.at(static_cast<size_t>(index)); }

    // Index du dessin deja ouvert depuis ce fichier (chemins compares apres
    // resolution), -1 sinon : rouvrir un fichier ouvert y ramene au lieu de le
    // dupliquer, ce qui creerait deux versions concurrentes du meme fichier.
    int indexOfPath(const QString& path) const;
    int indexOf(const DocumentSession* session) const;

private:
    std::vector<std::unique_ptr<DocumentSession>> sessions_;
    int untitledCounter_ = 0;
};

} // namespace bcad::app
