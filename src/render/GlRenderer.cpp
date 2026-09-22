#include "bcad/render/GlRenderer.h"

#include <QMatrix4x4>
#include <QVector4D>

namespace bcad::render {

namespace {

const char* kVertexShader = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
}
)";

const char* kFragmentShader = R"(
#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() {
    FragColor = uColor;
}
)";

} // namespace

GlRenderer::~GlRenderer() {
    if (vbo_.isCreated()) vbo_.destroy();
    if (vao_.isCreated()) vao_.destroy();
}

void GlRenderer::initialize() {
    initializeOpenGLFunctions();

    program_ = std::make_unique<QOpenGLShaderProgram>();
    program_->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader);
    program_->addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader);
    program_->link();

    vao_.create();
    vao_.bind();

    vbo_.create();
    vbo_.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    vbo_.bind();

    program_->bind();
    program_->enableAttributeArray(0);
    program_->setAttributeBuffer(0, GL_FLOAT, 0, 2, 0);

    vao_.release();
    vbo_.release();
    program_->release();

    glLineWidth(1.0f);
    initialized_ = true;
}

void GlRenderer::render(const Camera2D& camera) {
    if (!initialized_) return;

    glViewport(0, 0, camera.viewportWidth(), camera.viewportHeight());

    if (current_.batches.empty()) return;

    // Concatène chaque lot en un seul envoi VBO (un seul appel glBufferData
    // par image) afin que le nombre d'appels de dessin reste proportionnel
    // au nombre de couleurs distinctes visibles, et non au nombre d'entités.
    std::vector<float> combined;
    std::size_t totalFloats = 0;
    for (const auto& b : current_.batches) totalFloats += b.vertices.size();
    combined.reserve(totalFloats);

    std::vector<std::size_t> baseVertex(current_.batches.size());
    for (std::size_t i = 0; i < current_.batches.size(); ++i) {
        baseVertex[i] = combined.size() / 2;
        combined.insert(combined.end(), current_.batches[i].vertices.begin(), current_.batches[i].vertices.end());
    }

    vao_.bind();
    vbo_.bind();
    vbo_.allocate(combined.data(), static_cast<int>(combined.size() * sizeof(float)));

    program_->bind();

    QMatrix4x4 mvp;
    const geom::BoundingBox region = camera.visibleWorldRegion();
    mvp.ortho(static_cast<float>(region.minX), static_cast<float>(region.maxX),
              static_cast<float>(region.minY), static_cast<float>(region.maxY), -1.0f, 1.0f);
    program_->setUniformValue("uMVP", mvp);

    for (std::size_t i = 0; i < current_.batches.size(); ++i) {
        const core::ColorBatch& batch = current_.batches[i];
        if (batch.firsts.empty()) continue;

        program_->setUniformValue("uColor", QVector4D(batch.color.r, batch.color.g, batch.color.b, batch.color.a));

        std::vector<GLint> firsts(batch.firsts.size());
        std::vector<GLsizei> counts(batch.counts.size());
        for (std::size_t k = 0; k < batch.firsts.size(); ++k) {
            firsts[k] = static_cast<GLint>(baseVertex[i] + static_cast<std::size_t>(batch.firsts[k]));
            counts[k] = static_cast<GLsizei>(batch.counts[k]);
        }
        glMultiDrawArrays(GL_LINE_STRIP, firsts.data(), counts.data(), static_cast<GLsizei>(firsts.size()));
    }

    program_->release();
    vbo_.release();
    vao_.release();
}

} // namespace bcad::render
