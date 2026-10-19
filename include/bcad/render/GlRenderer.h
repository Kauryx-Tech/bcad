#pragma once

#include "bcad/core/TessellationTypes.h"
#include "bcad/geometry/Types.h"
#include "bcad/render/Camera2D.h"
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <memory>

namespace bcad::render {

// Rendu de lignes léger en GL moderne (core 3.3). Ne doit être manipulé que
// depuis le thread propriétaire du contexte GL (le thread GUI/principal de
// Qt pour un QOpenGLWidget) — le gros du travail (la tessellation) se fait
// ailleurs, voir TessellationWorker ; cette classe se contente d'envoyer des
// données de sommets déjà calculées et d'émettre les appels de dessin.
class GlRenderer : protected QOpenGLFunctions_3_3_Core {
public:
    GlRenderer() = default;
    ~GlRenderer();

    void initialize();
    // Dessine la tessellation courante par-dessus le contenu déjà présent
    // dans le framebuffer — n'efface pas. L'appelant (Viewport) possède le
    // remplissage de fond/la grille, peints via QPainter avant d'entrer dans
    // le rendu natif, car un clear GL ici effacerait ce fond.
    void render(const Camera2D& camera);

    // Appelé sur le thread GL une fois qu'une passe de tessellation en
    // arrière-plan se termine.
    void setTessellation(core::TessellationResult result) { current_ = std::move(result); }

    // Afficher les epaisseurs de ligne (LWDISPLAY d'AutoCAD) : sinon tout est
    // dessine en trait d'un pixel ; les types de ligne restent appliques.
    void setShowLineWeights(bool show) { showLineWeights_ = show; }
    bool showLineWeights() const { return showLineWeights_; }

private:
    // Traits fins (GL_LINE_STRIP) et traits epais (bandes construites par un
    // geometry shader : glLineWidth > 1 n'est pas garanti en profil core).
    // Les deux appliquent le type de ligne (tirets) dans le fragment shader.
    // Sans geometry shader (pilote limite), tout est dessine en trait fin.
    bool initialized_ = false;
    std::unique_ptr<QOpenGLShaderProgram> program_;
    std::unique_ptr<QOpenGLShaderProgram> wideProgram_;
    QOpenGLVertexArrayObject vao_;
    QOpenGLBuffer vbo_{ QOpenGLBuffer::VertexBuffer };
    QOpenGLBuffer distVbo_{ QOpenGLBuffer::VertexBuffer };
    core::TessellationResult current_;
    bool showLineWeights_ = true;
};

} // namespace bcad::render
