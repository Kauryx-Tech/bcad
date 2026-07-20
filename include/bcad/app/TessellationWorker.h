#pragma once

#include "bcad/core/Document.h"
#include "bcad/render/TessellationTypes.h"
#include <QObject>

Q_DECLARE_METATYPE(bcad::geom::BoundingBox)
Q_DECLARE_METATYPE(bcad::render::TessellationResult)

namespace bcad::app {

// Vit sur un QThread dédié. Effectue tout le travail lourd en CPU —
// requête du quadtree, tessellation par entité au LOD courant — en dehors
// du thread GL, en ne touchant que des données brutes (le verrou interne de
// Document rend cela sûr à exécuter en parallèle des modifications sur le
// thread GUI). Le thread GL ne voit jamais cette classe directement ; il ne
// reçoit le résultat terminé que via un signal en file d'attente et
// effectue lui-même les appels glBufferData/draw réels, car un contexte GL
// n'est utilisable que depuis le thread qui le possède.
class TessellationWorker : public QObject {
    Q_OBJECT
public:
    explicit TessellationWorker(QObject* parent = nullptr);

public slots:
    void build(const core::Document* doc, bcad::geom::BoundingBox region, double tolerance);

signals:
    void finished(bcad::render::TessellationResult result);
};

} // namespace bcad::app
