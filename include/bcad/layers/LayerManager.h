#pragma once

#include "bcad/layers/Layer.h"
#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace bcad::layers {

// Possède l'ensemble des calques d'un document. Les entités référencent les
// calques par leur nom (conformément à la convention DXF), jamais par
// pointeur, afin que les calques puissent être librement réordonnés/renommés
// sans toucher au stockage des entités.
class LayerManager {
public:
    LayerManager() { createLayer("0"); }

    Layer& createLayer(const std::string& name, geom::Color color = geom::Color::fromRgb255(255, 255, 255)) {
        if (Layer* existing = find(name)) return *existing;
        Layer layer;
        layer.name = name;
        layer.color = color;
        layers_.push_back(layer);
        notifyChanged();
        return layers_.back();
    }

    // Le calque "0" est le calque par défaut DXF et ne peut pas être
    // supprimé, à l'image du comportement d'AutoCAD/LibreCAD, afin que les
    // fichiers importés/exportés restent valides.
    bool removeLayer(const std::string& name) {
        if (name == "0") return false;
        auto it = std::find_if(layers_.begin(), layers_.end(),
                                [&](const Layer& l) { return l.name == name; });
        if (it == layers_.end()) return false;
        layers_.erase(it);
        if (currentLayer_ == name) currentLayer_ = "0";
        notifyChanged();
        return true;
    }

    bool renameLayer(const std::string& oldName, const std::string& newName) {
        if (oldName == "0" || find(newName) != nullptr) return false;
        Layer* l = find(oldName);
        if (!l) return false;
        l->name = newName;
        if (currentLayer_ == oldName) currentLayer_ = newName;
        notifyChanged();
        return true;
    }

    Layer* find(const std::string& name) {
        for (auto& l : layers_) if (l.name == name) return &l;
        return nullptr;
    }
    const Layer* find(const std::string& name) const {
        for (const auto& l : layers_) if (l.name == name) return &l;
        return nullptr;
    }

    void setVisible(const std::string& name, bool visible) {
        if (Layer* l = find(name)) { l->visible = visible; notifyChanged(); }
    }
    void setLocked(const std::string& name, bool locked) {
        if (Layer* l = find(name)) { l->locked = locked; notifyChanged(); }
    }

    // Supprime tous les calques sauf "0" et restaure ses valeurs par défaut ;
    // utilisé avant de charger un fichier dans un document existant afin
    // qu'aucun calque obsolète ne subsiste.
    void reset() {
        layers_.clear();
        Layer zero;
        zero.name = "0";
        layers_.push_back(zero);
        currentLayer_ = "0";
        notifyChanged();
    }

    const std::vector<Layer>& layers() const { return layers_; }

    const std::string& currentLayerName() const { return currentLayer_; }
    void setCurrentLayer(const std::string& name) {
        if (find(name)) { currentLayer_ = name; notifyChanged(); }
    }

    // Déclenché chaque fois que des calques sont ajoutés/supprimés/renommés
    // ou qu'une propriété change, afin que le LayerPanel et le Viewport de
    // l'interface puissent se rafraîchir sans scrutation (polling).
    std::function<void()> onChanged;

private:
    void notifyChanged() { if (onChanged) onChanged(); }

    std::vector<Layer> layers_;
    std::string currentLayer_ = "0";
};

} // namespace bcad::layers
