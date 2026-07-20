#pragma once

#include "bcad/layers/Layer.h"
#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace bcad::layers {

// Owns the set of layers for a document. Entities reference layers by name
// (matching DXF convention), never by pointer, so layers can be freely
// reordered/renamed without touching entity storage.
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

    // Layer "0" is the DXF default layer and cannot be removed, mirroring
    // AutoCAD/LibreCAD behavior so imported/exported files stay valid.
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

    // Drops every layer except "0" and restores its defaults; used before
    // loading a file into an existing document so stale layers don't linger.
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

    // Fired whenever layers are added/removed/renamed or a property changes,
    // so the GUI's LayerPanel and the Viewport can refresh without polling.
    std::function<void()> onChanged;

private:
    void notifyChanged() { if (onChanged) onChanged(); }

    std::vector<Layer> layers_;
    std::string currentLayer_ = "0";
};

} // namespace bcad::layers
