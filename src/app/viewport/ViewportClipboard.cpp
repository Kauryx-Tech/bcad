// Presse-papiers (E-01), comme AutoCAD : COPYCLIP (Ctrl+C), CUTCLIP
// (Ctrl+X), COPYBASE (Ctrl+Maj+C), PASTECLIP (Ctrl+V), PASTEORIG. Le contenu
// vit dans EntityClipboard, un seul pour l'application : on copie dans un
// onglet et on colle dans un autre. Coller recree les calques manquants du
// dessin cible ; chaque collage est une annulation.

#include "viewport/Viewport.h"

#include "editing/EditBatch.h"
#include "detail/EntityClipboard.h"

#include <QPainter>

namespace bcad::app {

using geom::Point2;

void Viewport::copyClip() {
    if (!doc_) return;
    if (selectedEntities().empty()) {
        setTool(ToolMode::CopyClip);   // designer les objets, puis Entree
        return;
    }
    storeInClipboard(EntityClipboard::lowerLeft(selectedEntities()), /*cut=*/false);
}

void Viewport::cutClip() {
    if (!doc_) return;
    if (selectedEntities().empty()) {
        setTool(ToolMode::CutClip);
        return;
    }
    storeInClipboard(EntityClipboard::lowerLeft(selectedEntities()), /*cut=*/true);
}

void Viewport::copyBase() {
    if (doc_) setTool(ToolMode::CopyBase);
}

void Viewport::pasteClip() {
    if (!doc_) return;
    if (EntityClipboard::instance().empty()) {
        emit statusMessage(tr("Coller : le presse-papiers est vide."));
        return;
    }
    setTool(ToolMode::Paste);
}

void Viewport::pasteOriginal() {
    if (!doc_) return;
    if (EntityClipboard::instance().empty()) {
        emit statusMessage(tr("Coller : le presse-papiers est vide."));
        return;
    }
    applyPaste(EntityClipboard::instance().base());
}

void Viewport::storeInClipboard(const Point2& base, bool cut) {
    const std::vector<geom::Entity*> selected = selectedEntities();
    if (selected.empty()) return;
    EntityClipboard::instance().store(selected, base, doc_->layerManager());
    if (cut) {
        EditBatch batch(doc_, undoStack_, tr("Couper"));
        for (geom::Entity* e : selected) batch.remove(e);
    }
    emit statusMessage(cut ? tr("%n objet(s) coupé(s) dans le presse-papiers.", nullptr, int(selected.size()))
                           : tr("%n objet(s) copié(s) dans le presse-papiers.", nullptr, int(selected.size())));
    if (cut) emit selectionChanged();
    update();
}

bool Viewport::finishClipboardPick() {
    switch (tool_) {
        case ToolMode::CopyClip:
            storeInClipboard(EntityClipboard::lowerLeft(selectedEntities()), false);
            break;
        case ToolMode::CutClip:
            storeInClipboard(EntityClipboard::lowerLeft(selectedEntities()), true);
            break;
        case ToolMode::CopyBase:
            if (toolPoints_.empty()) return false;
            storeInClipboard(toolPoints_.front(), false);
            break;
        default:
            return false;
    }
    endCommand();
    return true;
}

void Viewport::applyCopyBase(const Point2& world) {
    // Comme COPYBASE : le point de base d'abord, puis les objets — sauf s'ils
    // sont deja selectionnes.
    if (!selectedEntities().empty()) {
        storeInClipboard(world, false);
        endCommand();
        return;
    }
    toolPoints_ = {world};
    pickingObjects_ = true;
    notifyPrompt();
}

void Viewport::applyPaste(const Point2& at) {
    const EntityClipboard& clipboard = EntityClipboard::instance();
    clipboard.ensureLayers(doc_->layerManager());
    {
        EditBatch batch(doc_, undoStack_, tr("Coller"));
        for (auto& copy : clipboard.clonesAt(at)) batch.add(std::move(copy));
    }
    emit statusMessage(tr("%n objet(s) collé(s).", nullptr, int(clipboard.size())));
    if (tool_ == ToolMode::Paste) endCommand();
    update();
}

bool Viewport::drawPastePreview(QPainter& painter) {
    if (tool_ != ToolMode::Paste || !hoverWorld_) return false;
    // Les objets suivent le curseur par leur point de base.
    for (const auto& copy : EntityClipboard::instance().clonesAt(*hoverWorld_)) drawGhost(painter, *copy);
    return true;
}

} // namespace bcad::app
