#pragma once

#include "bcad/app/SnapEngine.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/BooleanOps.h"
#include "bcad/render/Camera2D.h"
#include "bcad/render/GlRenderer.h"
#include <QOpenGLWidget>
#include <QThread>
#include <QTimer>
#include <optional>
#include <vector>

class QUndoStack;

namespace bcad::app {

class TessellationWorker;

enum class ToolMode { Select, Move, Copy, Rotate, Scale, Mirror, Trim, Extend, Break, Line, Circle, Arc, Polyline };

// La surface de dessin : possède le moteur de rendu GL, la caméra, et la
// machine à états des outils interactifs décrite dans la couche
// "Viewport & Interactive Tools" du document d'architecture. La
// tessellation du document validé s'exécute sur un thread d'arrière-plan
// (TessellationWorker) ; seul le petit aperçu de l'outil en cours est
// construit directement sur le thread GL/UI via QPainter, puisqu'il s'agit
// de quelques points, pas de tout le dessin.
class Viewport : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit Viewport(QWidget* parent = nullptr);
    ~Viewport() override;

    void setDocument(core::Document* doc);
    void setUndoStack(QUndoStack* stack) { undoStack_ = stack; }
    void setTool(ToolMode mode);
    ToolMode tool() const { return tool_; }

    void zoomToFit();

    // Applique une opération booléenne ensembliste aux deux polylignes
    // fermées actuellement sélectionnées, en les remplaçant par le résultat
    // (comme une seule macro d'annulation). Affiche une boîte de message si
    // la sélection actuelle ne convient pas.
    void booleanOperation(geom::BooleanOp op);

public slots:
    void deleteSelected();
    // Remplace chaque PolylineEntity sélectionnée par ses segments
    // LineEntity individuels (une seule macro d'annulation). Les
    // sélections non-polylignes sont laissées telles quelles.
    void explodeSelected();
    // Fusionne les chaînes de LineEntity sélectionnées (extrémités qui se
    // touchent à la tolérance près) en objets PolylineEntity, un par chaîne
    // contiguë. ArcEntity n'est pas encore pris en charge — PolylineEntity
    // n'a pas de segments arc/bulge, donc joindre un arc aplatirait
    // silencieusement sa courbure.
    void joinSelected();
    void selectAll();
    // Sélectionne l'entité ajoutée le plus récemment (id le plus élevé) —
    // le "select last" de l'outil Sélection, sans avoir besoin de pointer.
    void selectLast();
    void toggleSnap() { snapEnabled_ = !snapEnabled_; update(); }
    void toggleGrid() { gridVisible_ = !gridVisible_; update(); }
    void toggleGridSnap() { gridSnapEnabled_ = !gridSnapEnabled_; }
    void toggleOrtho() { orthoEnabled_ = !orthoEnabled_; }
    // Alternative au clavier pour cliquer un point : analyse `text` (voir
    // CoordinateInput.h pour les formats acceptés) par rapport au dernier
    // point placé par l'outil actif, et le fait passer par le même chemin
    // de code qu'un clic de souris utiliserait.
    void submitTypedPoint(const QString& text);

signals:
    void cursorWorldPositionChanged(double x, double y);
    void toolChanged(ToolMode mode);
    // Émis quand l'utilisateur commence à taper une coordonnée directement
    // dans le viewport (un chiffre, '@' ou '-') pendant qu'un outil de
    // dessin est actif, afin que MainWindow puisse donner le focus à sa
    // ligne de commande et l'initialiser avec le caractère déjà tapé —
    // reproduit l'activation de la saisie dynamique d'AutoCAD.
    void typedInputRequested(const QString& initialText);
    // Déclenché chaque fois que l'ensemble des entités sélectionnées change
    // (pointage, fenêtre de sélection, sélectionner tout/dernier) —
    // Document::onChanged ne couvre pas ce cas, puisque `selected` est un
    // indicateur GUI transitoire, pas un état du document. Le panneau de
    // propriétés écoute ce signal pour savoir quand se rafraîchir.
    void selectionChanged();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onTessellationFinished(bcad::render::TessellationResult result);
    void requestTessellationNow();

private:
    void requestTessellation();
    void cancelActiveTool();
    void finishPolyline();
    void commitEntity(std::unique_ptr<geom::Entity> entity, const QString& label);
    // Partagé entre les clics de souris (après accrochage) et la saisie de
    // coordonnées tapées : fournit un point en coordonnées monde à l'outil
    // de dessin actif, quel qu'il soit.
    void placePoint(const geom::Point2& world);
    std::optional<geom::Point2> activeReferencePoint() const;
    std::vector<geom::Entity*> selectedEntities() const;
    void drawToolPreview(class QPainter& painter);
    void drawSnapMarker(class QPainter& painter);
    void drawGrid(class QPainter& painter);
    void drawRubberBand(class QPainter& painter);
    // Partie finale commune à Trim/Extend/Break : entité sous `world` dans
    // la tolérance de pointage, restreinte aux types que ces outils
    // prennent actuellement en charge.
    geom::Entity* pickModifiableEntity(const geom::Point2& world) const;
    geom::Point2 toWorld(QPoint screenPos) const;
    // Position brute du curseur accrochée au point extrémité/milieu/
    // centre/intersection/perpendiculaire/grille le plus proche, s'il y en
    // a un et si l'accrochage est activé.
    geom::Point2 snappedWorld(QPoint screenPos);

    core::Document* doc_ = nullptr;
    QUndoStack* undoStack_ = nullptr;
    render::Camera2D camera_;
    render::GlRenderer renderer_;

    QThread tessThread_;
    TessellationWorker* worker_ = nullptr;
    QTimer tessDebounce_;

    ToolMode tool_ = ToolMode::Select;
    std::vector<geom::Point2> toolPoints_;
    std::optional<geom::Point2> hoverWorld_;

    SnapEngine snapEngine_;
    bool snapEnabled_ = true;
    SnapResult activeSnap_;

    bool gridVisible_ = true;
    bool gridSnapEnabled_ = false;
    // Contraint le prochain point à être horizontal/vertical par rapport
    // au point de référence actif, comme la touche F8 d'AutoCAD. Les
    // accrochages aux objets l'emportent quand même sur Ortho lorsqu'un
    // accrochage est trouvé — correspond à la priorité OSNAP-sur-ORTHO
    // d'AutoCAD.
    bool orthoEnabled_ = false;

    bool panning_ = false;
    QPoint lastMousePos_;

    geom::Entity* moveTarget_ = nullptr;
    std::optional<geom::Point2> moveAnchor_;

    // Fenêtre de sélection de l'outil Sélection : glisser depuis un espace
    // vide démarre une sélection par fenêtre (de gauche à droite,
    // seulement les entités entièrement englobées) ou par capture (de
    // droite à gauche, toute entité touchée), la convention AutoCAD
    // standard distinguée uniquement par la direction du glissement.
    bool rubberBandActive_ = false;
    QPoint rubberBandStartScreen_;
};

} // namespace bcad::app
