#pragma once

#include "SnapEngine.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/DimensionEntity.h"
#include "bcad/geometry/BooleanOps.h"
#include "bcad/render/Camera2D.h"
#include "bcad/render/GlRenderer.h"
#include <QOpenGLWidget>
#include <QThread>
#include <QTimer>
#include <functional>
#include <optional>
#include <vector>
#include <memory>

class QMouseEvent;
class QUndoStack;

class QColor;
namespace bcad::geom { struct DimensionLabel; }

namespace bcad::app {

class TessellationWorker;

enum class ToolMode {
    Select, Move, Copy, Rotate, Scale, Mirror, Trim, Extend, Break,
    Line, Circle, Arc, Polyline, Rectangle, Point, Text,
    DimensionLinear, DimensionAligned, DimensionAngular,
    DimensionRadius, DimensionDiameter,
    // Contour ferme saisi pour le compte d'une commande de module
    // (WorkbenchParams::PickPolygon) : gestes de la polyligne, rien n'est cree
    // par le canevas lui-meme.
    CapturePolygon
};

// La surface de dessin : possède le moteur de rendu GL, la caméra, et la
// machine à états des outils interactifs décrite dans la couche
// "Viewport & Interactive Tools" du document d'architecture. La
// tessellation du document validé s'exécute sur un thread d'arrière-plan
// (TessellationWorker) ; seul le petit aperçu de l'outil en cours est
// construit directement sur le thread GL/UI via QPainter, puisqu'il s'agit
// de quelques points, pas de tout le dessin.
//
// La classe est répartie sur sept unités de traduction de src/app/, par
// responsabilité. L'en-tête Q_OBJECT reste unique — seuls les corps changent
// de fichier ; la répartition est détaillée dans src/app/Viewport.cpp.
class Viewport : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit Viewport(QWidget* parent = nullptr);
    ~Viewport() override;

    void setDocument(core::Document* doc);
    void setUndoStack(QUndoStack* stack) { undoStack_ = stack; }
    void setTool(ToolMode mode);
    ToolMode tool() const { return tool_; }
    // Consigne de l'etape en cours de l'outil actif (« Spécifiez le centre »),
    // affichee dans la ligne de commande comme le fait AutoCAD.
    QString prompt() const;

    void zoomToFit();
    // Cadrage d'un dessin, garde quand on passe a un autre onglet. setCamera
    // conserve la taille courante du canevas : seuls centre et echelle changent.
    render::Camera2D camera() const { return camera_; }
    void setCamera(const render::Camera2D& camera);
    // Fait dessiner un contour ferme (au moins trois sommets) avec la consigne
    // donnee, puis le remet a `done`. Echap, ou un autre outil, y renonce :
    // `done` n'est alors pas appele.
    void capturePolygon(const QString& prompt,
                        std::function<void(std::vector<geom::Point2>)> done);
    // Modifier le contenu d'un texte (D-01b) : boite de saisie, puis commande
    // annulable. editSelectedText prend le premier texte de la selection.
    void editText(int entityId);
    void editSelectedText();

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
    bool snapEnabled() const { return snapEnabled_; }
    bool gridVisible() const { return gridVisible_; }
    bool gridSnapEnabled() const { return gridSnapEnabled_; }
    bool orthoEnabled() const { return orthoEnabled_; }
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
    // La consigne de l'outil a change (nouvel outil, nouvelle etape).
    void promptChanged(const QString& prompt);
    // Un refus ou une information non bloquante (« Sélectionnez d'abord… ») :
    // la fenetre l'affiche dans la barre d'etat, sans boite modale qui
    // interromprait le trace.
    void statusMessage(const QString& message);
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
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private slots:
    void onTessellationFinished(bcad::core::TessellationResult result);
    void requestTessellationNow();

private:
    void requestTessellation();
    void cancelActiveTool();
    // Termine la polyligne (ouverte, ou fermee par C) et la commande.
    void finishPolyline(bool closed = false);
    // Fin de commande, comme AutoCAD : retour a l'etat de repos, ou la souris
    // selectionne. Retenue comme derniere commande, qu'Entree relance.
    void endCommand();
    // Entree, ligne de commande vide ou clic droit : valide la selection
    // d'objets en cours, termine la polyligne ou la ligne, ou — au repos —
    // relance la derniere commande (pas sur clic droit).
    void pressEnter(bool fromRightClick);
    // Clic de selection, au repos comme pendant la designation des objets
    // d'une commande. `addByDefault` : pendant une commande, chaque clic
    // ajoute (Maj retire), comme AutoCAD.
    void selectAt(QMouseEvent* event, bool addByDefault);
    void notifyPrompt();
    // Calque des cotations, cree a la demande : sans lui, Document::addEntity
    // reversait silencieusement les cotations sur le calque courant.
    void ensureDimensionLayer();
    void commitEntity(std::unique_ptr<geom::Entity> entity, const QString& label);
    // Partagé entre les clics de souris (après accrochage) et la saisie de
    // coordonnées tapées : fournit un point en coordonnées monde à l'outil
    // de dessin actif, quel qu'il soit. Dispatche ensuite sur le gestionnaire
    // de l'outil, qui accumule ses points puis commite.
    void placePoint(const geom::Point2& world);
    // Un gestionnaire par outil. Les outils de tracé créent de la géométrie
    // (place*), ceux de manipulation la transforment ou la coupent (apply*).
    // Chacun se comporte exactement comme le `case` dont il vient, y compris
    // sur les sorties anticipées : `placePoint` seul déclenche le `update()`.
    void placeLine(const geom::Point2& world);
    void placeCircle(const geom::Point2& world);
    void placeArc(const geom::Point2& world);
    void placeRectangle(const geom::Point2& world);
    void placePointEntity(const geom::Point2& world);
    // Outil Texte (D-01), etapes comme la commande TEXTE d'AutoCAD : point de
    // depart, hauteur, angle, puis lignes de texte jusqu'a une ligne vide.
    void placeText(const geom::Point2& world);
    void submitTextValue(const QString& text);
    // Cotations (A-01, A-02), ViewportDimensionTools.cpp : un objet par cotation,
    // construit des points saisis — aussi pour l'apercu sous le curseur.
    bool isDimensionTool() const;
    void placeDimension(const geom::Point2& world);
    std::unique_ptr<geom::DimensionEntity> dimensionFromPoints(const std::vector<geom::Point2>& points) const;
    double newDimensionTextHeight() const;
    bool submitDimensionOption(const QString& text);
    void applyMove(const geom::Point2& world);
    void applyCopy(const geom::Point2& world);
    void applyRotate(const geom::Point2& world);
    void applyScale(const geom::Point2& world);
    void applyMirror(const geom::Point2& world);
    void applyTrim(const geom::Point2& world);
    void applyExtend(const geom::Point2& world);
    void applyBreak(const geom::Point2& world);
    std::optional<geom::Point2> activeReferencePoint() const;
    std::vector<geom::Entity*> selectedEntities() const;
    // Surimpression 2D peinte par paintGL, chacune dans son rôle propre.
    void drawEntityTexts(class QPainter& painter);
    void drawDimensionLabel(class QPainter& painter, const geom::DimensionLabel& label, const QColor& color);
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
    std::unique_ptr<bcad::events::SubscriptionGuard> docSub_;
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

    // Commande de modification lancee sans selection : la souris designe
    // d'abord les objets, Entree ou clic droit valide.
    bool pickingObjects_ = false;
    ToolMode lastCommand_ = ToolMode::Select;
    // Outil Texte : etape (0 point, 1 hauteur, 2 angle, 3 contenu), et les
    // valeurs gardees d'un texte a l'autre comme AutoCAD.
    int textStage_ = 0;
    // Cotation lineaire : 0 selon la position, 1 horizontale (H), 2 verticale (V).
    int dimOrientation_ = 0;
    // Rayon du cercle ou de l'arc designe pour une cotation de rayon/diametre.
    double dimRadius_ = 0.0;
    // Hauteur de texte de la cotation en cours, choisie a son premier point.
    double dimTextHeight_ = 0.0;
    double textHeight_ = 2.5;
    double textRotation_ = 0.0;   // radians
    // Saisie de contour pour une commande de module (capturePolygon).
    QString capturePrompt_;
    std::function<void(std::vector<geom::Point2>)> captureDone_;

    // Fenêtre de sélection de l'outil Sélection : glisser depuis un espace
    // vide démarre une sélection par fenêtre (de gauche à droite,
    // seulement les entités entièrement englobées) ou par capture (de
    // droite à gauche, toute entité touchée), la convention AutoCAD
    // standard distinguée uniquement par la direction du glissement.
    bool rubberBandActive_ = false;
    QPoint rubberBandStartScreen_;
};

} // namespace bcad::app
