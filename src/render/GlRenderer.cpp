#include "bcad/render/GlRenderer.h"

#include <QMatrix4x4>
#include <QOpenGLContext>
#include <QtGlobal>
#include <QVector2D>
#include <QVector4D>

#include <algorithm>
#include <cmath>

namespace bcad::render {

namespace {

// Distance cumulee (unites du dessin) -> pixels dans le fragment shader, pour
// des tirets de longueur constante a l'ecran quel que soit le zoom.
const char* kVertexShader = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in float aDist;
uniform mat4 uMVP;
out float vDist;
void main() {
    gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
    vDist = aDist;
}
)";

// Chaque segment devient une bande de uWidthPx pixels, prolongee d'une
// demi-largeur a chaque bout pour boucher les angles (extremites carrees).
const char* kWideGeometryShader = R"(
#version 330 core
layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;
uniform vec2 uViewport;
uniform float uWidthPx;
in float vDist[];
out float fDist;
void main() {
    vec2 hv = uViewport * 0.5;
    vec2 s0 = gl_in[0].gl_Position.xy / gl_in[0].gl_Position.w * hv;
    vec2 s1 = gl_in[1].gl_Position.xy / gl_in[1].gl_Position.w * hv;
    vec2 dir = s1 - s0;
    float len = length(dir);
    dir = len > 1e-6 ? dir / len : vec2(1.0, 0.0);
    vec2 n = vec2(-dir.y, dir.x) * (uWidthPx * 0.5);
    vec2 e = dir * (uWidthPx * 0.5);
    fDist = vDist[0]; gl_Position = vec4((s0 - e + n) / hv, 0.0, 1.0); EmitVertex();
    fDist = vDist[0]; gl_Position = vec4((s0 - e - n) / hv, 0.0, 1.0); EmitVertex();
    fDist = vDist[1]; gl_Position = vec4((s1 + e + n) / hv, 0.0, 1.0); EmitVertex();
    fDist = vDist[1]; gl_Position = vec4((s1 + e - n) / hv, 0.0, 1.0); EmitVertex();
    EndPrimitive();
}
)";

// Types de ligne (layers::Layer::LineType) : 0 continu, 1 tirets,
// 2 pointille, 3 tiret-point. Motifs en pixels, etires avec l'epaisseur.
const char* kFragmentTemplate = R"(
#version 330 core
uniform vec4 uColor;
uniform int uLineType;
uniform float uPxPerUnit;
uniform float uPatternScale;
in float DIST;
out vec4 FragColor;
void main() {
    float d = DIST * uPxPerUnit / uPatternScale;
    if (uLineType == 1) {
        if (mod(d, 18.0) > 12.0) discard;
    } else if (uLineType == 2) {
        if (mod(d, 6.0) > 2.0) discard;
    } else if (uLineType == 3) {
        float m = mod(d, 24.0);
        if ((m > 12.0 && m < 16.0) || m > 18.0) discard;
    }
    FragColor = uColor;
}
)";

QString fragmentShader(const char* distName) {
    return QString::fromLatin1(kFragmentTemplate).replace(QStringLiteral("DIST"), QLatin1String(distName));
}

// Epaisseur a l'ecran : 96 ppp, sans descendre sous 1 pixel ; 0,25 mm (la
// valeur par defaut) reste donc un trait d'un pixel.
float widthPx(float lineWeightMm) {
    return std::max(1.0f, std::round(lineWeightMm * 96.0f / 25.4f));
}

} // namespace

GlRenderer::~GlRenderer() {
    if (vbo_.isCreated()) vbo_.destroy();
    if (distVbo_.isCreated()) distVbo_.destroy();
    if (vao_.isCreated()) vao_.destroy();
}

void GlRenderer::initialize() {
    // Sans contexte GL 3.3 utilisable (pilote absent, plateforme hors ecran),
    // le renderer reste inactif : le canevas est vide mais l'application ne
    // plante pas en appelant des fonctions GL non chargees.
    if (!QOpenGLContext::currentContext() || !initializeOpenGLFunctions()) {
        qWarning("bcad[render]: OpenGL 3.3 indisponible, dessin desactive");
        return;
    }

    program_ = std::make_unique<QOpenGLShaderProgram>();
    const bool baseOk =
        program_->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader) &&
        program_->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader("vDist"));
    program_->bindAttributeLocation("aPos", 0);
    program_->bindAttributeLocation("aDist", 1);
    if (!baseOk || !program_->link()) {
        qWarning("bcad[render]: shaders de trait refuses, dessin desactive : %s",
                 qPrintable(program_->log()));
        program_.reset();
        return;
    }

    wideProgram_ = std::make_unique<QOpenGLShaderProgram>();
    const bool wideOk =
        wideProgram_->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader) &&
        wideProgram_->addShaderFromSourceCode(QOpenGLShader::Geometry, kWideGeometryShader) &&
        wideProgram_->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader("fDist")) &&
        wideProgram_->link();
    if (!wideOk) {
        qWarning("bcad[render]: traits epais indisponibles (geometry shader) : %s",
                 qPrintable(wideProgram_->log()));
        wideProgram_.reset();
    }

    vao_.create();
    vao_.bind();

    vbo_.create();
    vbo_.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    vbo_.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    distVbo_.create();
    distVbo_.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    distVbo_.bind();
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, nullptr);

    vao_.release();
    distVbo_.release();
    vbo_.release();

    glLineWidth(1.0f);
    initialized_ = true;
}

void GlRenderer::render(const Camera2D& camera) {
    if (!initialized_) return;

    glViewport(0, 0, camera.viewportWidth(), camera.viewportHeight());

    if (current_.batches.empty()) return;

    // Concatène chaque lot en un seul envoi VBO (un seul appel glBufferData
    // par image) afin que le nombre d'appels de dessin reste proportionnel
    // au nombre de styles de trait visibles, et non au nombre d'entités.
    std::vector<float> combined;
    std::vector<float> distances;
    std::size_t totalFloats = 0;
    for (const auto& b : current_.batches) totalFloats += b.vertices.size();
    combined.reserve(totalFloats);
    distances.reserve(totalFloats / 2);

    std::vector<std::size_t> baseVertex(current_.batches.size());
    for (std::size_t i = 0; i < current_.batches.size(); ++i) {
        const auto& b = current_.batches[i];
        baseVertex[i] = combined.size() / 2;
        combined.insert(combined.end(), b.vertices.begin(), b.vertices.end());
        // Un lot sans distances (producteur ancien) se dessine en continu.
        if (b.distances.size() == b.vertices.size() / 2) {
            distances.insert(distances.end(), b.distances.begin(), b.distances.end());
        } else {
            distances.resize(combined.size() / 2, 0.0f);
        }
    }

    vao_.bind();
    vbo_.bind();
    vbo_.allocate(combined.data(), static_cast<int>(combined.size() * sizeof(float)));
    distVbo_.bind();
    distVbo_.allocate(distances.data(), static_cast<int>(distances.size() * sizeof(float)));

    QMatrix4x4 mvp;
    const geom::BoundingBox region = camera.visibleWorldRegion();
    mvp.ortho(static_cast<float>(region.minX), static_cast<float>(region.maxX),
              static_cast<float>(region.minY), static_cast<float>(region.maxY), -1.0f, 1.0f);

    for (std::size_t i = 0; i < current_.batches.size(); ++i) {
        const core::ColorBatch& batch = current_.batches[i];
        if (batch.firsts.empty()) continue;

        const float width = widthPx(batch.lineWeightMm);
        QOpenGLShaderProgram* program = (width > 1.0f && wideProgram_) ? wideProgram_.get() : program_.get();
        program->bind();
        program->setUniformValue("uMVP", mvp);
        program->setUniformValue("uColor", QVector4D(batch.color.r, batch.color.g, batch.color.b, batch.color.a));
        program->setUniformValue("uLineType", static_cast<GLint>(batch.lineType));
        program->setUniformValue("uPxPerUnit", static_cast<GLfloat>(camera.pixelsPerUnit()));
        program->setUniformValue("uPatternScale", std::max(1.0f, width * 0.5f));
        if (program == wideProgram_.get()) {
            program->setUniformValue("uViewport", QVector2D(static_cast<float>(camera.viewportWidth()),
                                                           static_cast<float>(camera.viewportHeight())));
            program->setUniformValue("uWidthPx", width);
        }

        std::vector<GLint> firsts(batch.firsts.size());
        std::vector<GLsizei> counts(batch.counts.size());
        for (std::size_t k = 0; k < batch.firsts.size(); ++k) {
            firsts[k] = static_cast<GLint>(baseVertex[i] + static_cast<std::size_t>(batch.firsts[k]));
            counts[k] = static_cast<GLsizei>(batch.counts[k]);
        }
        glMultiDrawArrays(GL_LINE_STRIP, firsts.data(), counts.data(), static_cast<GLsizei>(firsts.size()));
        program->release();
    }

    distVbo_.release();
    vbo_.release();
    vao_.release();
}

} // namespace bcad::render
