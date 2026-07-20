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
    void setTessellation(TessellationResult result) { current_ = std::move(result); }

private:
    bool initialized_ = false;
    std::unique_ptr<QOpenGLShaderProgram> program_;
    QOpenGLVertexArrayObject vao_;
    QOpenGLBuffer vbo_{ QOpenGLBuffer::VertexBuffer };
    TessellationResult current_;
};

} // namespace bcad::render
