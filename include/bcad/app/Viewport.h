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

// The drawing surface: owns the GL renderer, the camera, and the
// interactive-tool state machine described in the architecture doc's
// "Viewport & Interactive Tools" layer. Tessellation for the committed
// document runs on a background thread (TessellationWorker); only the
// small in-progress tool preview is built directly on the GL/UI thread via
// QPainter, since it's a handful of points, not the whole drawing.
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

    // Applies a boolean set operation to the two currently-selected closed
    // polylines, replacing them with the result (as one undo macro). Shows
    // a message box if the current selection doesn't qualify.
    void booleanOperation(geom::BooleanOp op);

public slots:
    void deleteSelected();
    // Replaces every selected PolylineEntity with its individual LineEntity
    // segments (one undo macro). Non-polyline selections are left alone.
    void explodeSelected();
    // Merges selected LineEntity chains (endpoints touching within
    // tolerance) into PolylineEntity objects, one per contiguous chain.
    // ArcEntity is not supported yet — PolylineEntity has no arc/bulge
    // segments, so joining an arc in would silently flatten its curvature.
    void joinSelected();
    void selectAll();
    // Selects the most recently added entity (highest id) — Select tool's
    // "select last" without needing a pick.
    void selectLast();
    void toggleSnap() { snapEnabled_ = !snapEnabled_; update(); }
    void toggleGrid() { gridVisible_ = !gridVisible_; update(); }
    void toggleGridSnap() { gridSnapEnabled_ = !gridSnapEnabled_; }
    void toggleOrtho() { orthoEnabled_ = !orthoEnabled_; }
    // Keyboard alternative to clicking a point: parses `text` (see
    // CoordinateInput.h for the accepted formats) relative to the active
    // tool's last placed point, and feeds it through the same code path a
    // mouse click would use.
    void submitTypedPoint(const QString& text);

signals:
    void cursorWorldPositionChanged(double x, double y);
    void toolChanged(ToolMode mode);
    // Emitted when the user starts typing a coordinate directly into the
    // viewport (a digit, '@' or '-') while a drawing tool is active, so
    // MainWindow can focus its command-line input and seed it with the
    // character already typed — mirrors AutoCAD's dynamic input activation.
    void typedInputRequested(const QString& initialText);
    // Fired whenever the set of selected entities changes (pick, rubber
    // band, select all/last) — Document::onChanged doesn't cover this,
    // since `selected` is a transient GUI flag, not document state. The
    // properties panel listens here to know when to refresh.
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
    // Shared by mouse clicks (after snapping) and typed coordinate entry:
    // feeds one world-space point into whichever drawing tool is active.
    void placePoint(const geom::Point2& world);
    std::optional<geom::Point2> activeReferencePoint() const;
    std::vector<geom::Entity*> selectedEntities() const;
    void drawToolPreview(class QPainter& painter);
    void drawSnapMarker(class QPainter& painter);
    void drawGrid(class QPainter& painter);
    void drawRubberBand(class QPainter& painter);
    // Common tail of Trim/Extend/Break: entity under `world` within pick
    // tolerance, restricted to the types those tools currently support.
    geom::Entity* pickModifiableEntity(const geom::Point2& world) const;
    geom::Point2 toWorld(QPoint screenPos) const;
    // Raw cursor position snapped to the nearest endpoint/midpoint/center/
    // intersection/perpendicular/grid point, if any and if enabled.
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
    // Constrains the next point to horizontal/vertical relative to the
    // active reference point, like AutoCAD's F8. Object snaps still win
    // over Ortho when one is found — matches AutoCAD's OSNAP-over-ORTHO
    // precedence.
    bool orthoEnabled_ = false;

    bool panning_ = false;
    QPoint lastMousePos_;

    geom::Entity* moveTarget_ = nullptr;
    std::optional<geom::Point2> moveAnchor_;

    // Select tool rubber-band: dragging from empty space starts a window
    // (left-to-right, only fully-enclosed entities) or crossing
    // (right-to-left, any touched entity) selection, the standard AutoCAD
    // convention distinguished purely by drag direction.
    bool rubberBandActive_ = false;
    QPoint rubberBandStartScreen_;
};

} // namespace bcad::app
