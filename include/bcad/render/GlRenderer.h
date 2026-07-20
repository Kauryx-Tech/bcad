#pragma once

#include "bcad/geometry/Types.h"
#include "bcad/render/Camera2D.h"
#include "bcad/render/TessellationTypes.h"
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <memory>

namespace bcad::render {

// Thin, modern-GL (3.3 core) line renderer. Must only ever be touched from
// the thread that owns the GL context (Qt's GUI/main thread for a
// QOpenGLWidget) — the heavy lifting (tessellation) happens elsewhere, see
// TessellationWorker; this class only uploads already-computed vertex data
// and issues draw calls.
class GlRenderer : protected QOpenGLFunctions_3_3_Core {
public:
    GlRenderer() = default;
    ~GlRenderer();

    void initialize();
    // Draws the current tessellation over whatever is already in the
    // framebuffer — does not clear. The caller (Viewport) owns the
    // background fill/grid, painted via QPainter before entering native
    // painting, since a GL clear here would wipe that out.
    void render(const Camera2D& camera);

    // Called on the GL thread once a background tessellation pass completes.
    void setTessellation(TessellationResult result) { current_ = std::move(result); }

private:
    bool initialized_ = false;
    std::unique_ptr<QOpenGLShaderProgram> program_;
    QOpenGLVertexArrayObject vao_;
    QOpenGLBuffer vbo_{ QOpenGLBuffer::VertexBuffer };
    TessellationResult current_;
};

} // namespace bcad::render
