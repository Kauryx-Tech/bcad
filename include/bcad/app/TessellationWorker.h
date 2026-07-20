#pragma once

#include "bcad/core/Document.h"
#include "bcad/render/TessellationTypes.h"
#include <QObject>

Q_DECLARE_METATYPE(bcad::geom::BoundingBox)
Q_DECLARE_METATYPE(bcad::render::TessellationResult)

namespace bcad::app {

// Lives on a dedicated QThread. Does all the CPU-heavy work — quadtree
// query, per-entity tessellation at the current LOD — off the GL thread,
// touching only plain data (Document's internal lock makes this safe to run
// concurrently with edits on the GUI thread). The GL thread never sees this
// class directly; it only receives the finished result via a queued signal
// and does the actual glBufferData/draw calls itself, since a GL context is
// not usable from a thread other than the one that owns it.
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
