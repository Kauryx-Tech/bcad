#include "bcad/app/TessellationWorker.h"

namespace bcad::app {

TessellationWorker::TessellationWorker(QObject* parent) : QObject(parent) {
    qRegisterMetaType<geom::BoundingBox>("bcad::geom::BoundingBox");
    qRegisterMetaType<core::TessellationResult>("bcad::core::TessellationResult");
}

void TessellationWorker::build(const core::Document* doc, geom::BoundingBox region, double tolerance) {
    emit finished(doc->buildTessellation(region, tolerance));
}

} // namespace bcad::app
