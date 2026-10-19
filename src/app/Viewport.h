#pragma once

#include "GripDetail.h"
#include "SnapEngine.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/DimensionEntity.h"
#include "bcad/geometry/BooleanOps.h"
#include "bcad/render/Camera2D.h"
#include "bcad/render/GlRenderer.h"
#include <QElapsedTimer>
#include <QOpenGLWidget>
#include <QPointF>
#include <QString>
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
    // Variantes de Cercle ▾ et Arc ▾ (AutoCAD) : centre-diametre, 2 points,
    // 3 points ; arc 3 points (le defaut d'ARC dans AutoCAD).
    CircleDiameter, Circle2P, Circle3P, Arc3P,
    DimensionLinear, DimensionAligned, DimensionAngular,
    DimensionRadius, DimensionDiameter,
    Offset, Stretch, Chamfer, Array, Hatch, Polygon, Join, Explode, Erase,
    // Raccord (M-04) : deux objets designes au clic, arc de rayon memorise.
    Fillet,
    // Navigation (V-01, V-03) : zoom par deux coins, main au bouton gauche.
    ZoomWindow, Pan,
    // Mesures (I-01, I-02, I-06) : rien n'est cree, le resultat va dans la ligne de commande.
    Measure, Area, Coordinates,
    // Contour ferme saisi pour le compte d'une commande de module
    // (WorkbenchParams::PickPolygon) : gestes de la polyligne, rien n'est cree
    // par le canevas lui-meme.
    CapturePolygon,
    // Poignee tiree (M-02, « ** ETIRER ** » d'AutoCAD) : lancee par un clic
    // sur une poignee de la selection, jamais retenue comme derniere commande.
    GripStretch
};

// Alias de commande clavier (C-01) : ToolAliases.h, la seule table.

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
    // Derniere commande (Entree au repos la relance) et commandes recentes,
    // la plus recente d'abord : menu du clic droit (« Repeter », « Entree
    // recente »).
    ToolMode lastCommand() const { return lastCommand_; }
    const std::vector<ToolMode>& recentCommands() const { return recentCommands_; }
    bool hasSelection() const { return !selectedEntities().empty(); }
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
    // Zoom precedent / suivant (V-02) : vues gardees a chaque changement de
    // cadrage ; actions immediates, la commande en cours continue.
    void zoomPrevious();
    void zoomNext();
    // Retrace le dessin (styles de calque modifies hors d'une commande).
    void refreshDrawing() { requestTessellation(); update(); }
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
    void deselectAll();
    // Sélectionne l'entité ajoutée le plus récemment (id le plus élevé) —
    // le "select last" de l'outil Sélection, sans avoir besoin de pointer.
    void selectLast();
    void toggleSnap() { snapEnabled_ = !snapEnabled_; update(); }
    void toggleGrid() { gridVisible_ = !gridVisible_; update(); }
    void toggleGridSnap() { gridSnapEnabled_ = !gridSnapEnabled_; }
    void toggleOrtho() { orthoEnabled_ = !orthoEnabled_; }
    // Reperage polaire (F10) et son increment en degres (POLARANG).
    void togglePolar() { polarEnabled_ = !polarEnabled_; update(); }
    void setPolarIncrement(double degrees) { polarIncrementDeg_ = degrees; update(); }
    // Reperage d'objet (F11) : points acquis au survol d'un accrochage.
    void toggleObjectTracking() { objectTracking_ = !objectTracking_; acquired_.clear(); update(); }
    void toggleLineWeights() { renderer_.setShowLineWeights(!renderer_.showLineWeights()); update(); }
    // Saisie dynamique (F12, DYNMODE d'AutoCAD) : consigne et valeurs pres du
    // curseur pendant une commande ; le texte tape s'y affiche.
    void toggleDynamicInput() { dynamicInput_ = !dynamicInput_; update(); }
    void setDynamicInputText(const QString& text) { dynamicText_ = text; update(); }
    bool lineWeightsShown() const { return renderer_.showLineWeights(); }
    bool dynamicInputEnabled() const { return dynamicInput_; }
    bool snapEnabled() const { return snapEnabled_; }
    bool gridVisible() const { return gridVisible_; }
    bool gridSnapEnabled() const { return gridSnapEnabled_; }
    bool orthoEnabled() const { return orthoEnabled_; }
    bool polarEnabled() const { return polarEnabled_; }
    bool objectTrackingEnabled() const { return objectTracking_; }
    double polarIncrement() const { return polarIncrementDeg_; }
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
    // Clic droit au repos : la fenetre ouvre le menu contextuel d'AutoCAD.
    void contextMenuRequested(const QPoint& globalPos);

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
    // Variantes de cercle et d'arc (ViewportCircleTools.cpp), leurs options
    // tapees (2P, 3P, D dans CERCLE ; C dans ARC) et leur apercu.
    void placeCircleVariant(const geom::Point2& world);
    bool submitCircleOption(const QString& text);
    bool drawCircleVariantPreview(class QPainter& painter);
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
    // Mesurer (I-01), ViewportMeasureTools.cpp : distance par deux points,
    // ou angle (option A) par sommet et deux cotes. L'outil reste actif.
    void applyMeasure(const geom::Point2& world);
    bool submitMeasureOption(const QString& text);
    QString measurePrompt() const;
    // Surface et perimetre (I-02) : par sommets (Entree calcule) ou, option O,
    // par un objet ferme designe ; A / S cumulent dans un total.
    void applyArea(const geom::Point2& world);
    void finishAreaPolygon();
    void reportArea(double area, double perimeter, const QString& what);
    QString areaPrompt() const;
    // Coordonnees d'un point (I-06) : X, Y, et position relative au point
    // identifie precedemment (cartesienne et polaire).
    void applyCoordinates(const geom::Point2& world);
    bool submitModifyOption(const QString& text);
    // Options tapees des outils de repetition (ViewportPatternTools.cpp) :
    // « P » / « R » basculent le reseau en polaire / rectangulaire, un nombre
    // donne le nombre d'objets (polaire) ou de colonnes (rectangulaire), ou
    // le pas des hachures selon l'outil actif.
    void applyMove(const geom::Point2& world);
    void applyCopy(const geom::Point2& world);
    void applyRotate(const geom::Point2& world);
    // Tourner / Echelle (M-05) : valeur tapee, option C copie ; application
    // commune, en place ou sur des copies.
    bool submitTransformValue(const QString& text);
    void transformSelection(const geom::Transform2D& transform, const QString& label);
    void applyScale(const geom::Point2& world);
    void applyMirror(const geom::Point2& world);
    void applyTrim(const geom::Point2& world);
    void applyExtend(const geom::Point2& world);
    void applyBreak(const geom::Point2& world);
    // Decaler (M-01), ViewportOffsetTools.cpp : distance tapee (ou P, par
    // un point), objet designe, cote clique ; repete jusqu'a Entree.
    void applyOffset(const geom::Point2& world);
    bool submitOffsetOption(const QString& text);
    QString offsetPrompt() const;
    bool drawOffsetPreview(class QPainter& painter);
    void applyStretch(const geom::Point2& world);
    // Objets concernes par Etirer : la selection prealable, sinon ceux que
    // la fenetre (toolPoints_[0..1]) touche.
    std::vector<geom::Entity*> stretchTargets() const;
    void applyChamfer(const geom::Point2& world);
    void applyArray(const geom::Point2& world);
    void applyHatch(const geom::Point2& world);
    // Raccord (M-04), ViewportFilletTools.cpp : premier objet, second objet ;
    // options R (rayon), A (ajuster ou non), M (plusieurs).
    void applyFillet(const geom::Point2& world);
    bool submitFilletOption(const QString& text);
    QString filletPrompt() const;
    bool drawFilletPreview(class QPainter& painter);
    // Navigation (ViewportNavigation.cpp). rememberView garde la vue courante
    // avant un changement ; `coalesce` regroupe une rafale (molette) en un pas.
    void rememberView(bool coalesce);
    void applyZoomWindow(const geom::Point2& world);
    // Poignees (M-02), ViewportGripTools.cpp : affichees sur la selection au
    // repos ; un clic sur l'une la rend chaude (toutes celles au meme endroit
    // aussi) et lance GripStretch ; le point suivant — clique, accroche,
    // tape ou relachement d'un glisser — termine.
    bool beginGrip(QPoint screenPos);
    bool finishGripDrag(QPoint screenPos);
    void applyGripStretch(const geom::Point2& world);
    void drawGrips(class QPainter& painter);
    bool drawGripPreview(class QPainter& painter);
    std::optional<geom::Point2> activeReferencePoint() const;
    std::vector<geom::Entity*> selectedEntities() const;
    // Surimpression 2D peinte par paintGL, chacune dans son rôle propre.
    void drawEntityTexts(class QPainter& painter);
    void drawDimensionLabel(class QPainter& painter, const geom::DimensionLabel& label, const QColor& color);
    void drawToolPreview(class QPainter& painter);
    // Bulle de saisie dynamique et reperage polaire (ViewportDynamicInput.cpp).
    void drawDynamicInput(class QPainter& painter);
    void drawTracking(class QPainter& painter);
    // Apercu des outils de modification/repetition (ViewportModifyPreview.cpp) :
    // vrai s'il a dessine (et donc termine l'apercu), faux sinon.
    bool drawModifyPreview(class QPainter& painter);
    void drawSnapMarker(class QPainter& painter);
    void drawGrid(class QPainter& painter);
    void drawRubberBand(class QPainter& painter);
    // Partie finale commune à Trim/Extend/Break : entité sous `world` dans
    // la tolérance de pointage, restreinte aux types que ces outils
    // prennent actuellement en charge.
    geom::Entity* pickModifiableEntity(const geom::Point2& world) const;
    geom::Point2 toWorld(QPoint screenPos) const;
    // --- Aides communes des outils (docs/REUSE_RULES.md) : a utiliser plutot
    // que de recopier le calcul.
    // Tolerance de designation en unites du dessin (kPickToleranceScreenPx).
    double pickTolerance() const;
    // Entite sous un point monde, a la tolerance de designation ; nullptr sinon.
    geom::Entity* pickEntityAt(const geom::Point2& world) const;
    // Point monde -> point ecran du canevas.
    QPointF toScreenF(const geom::Point2& world) const;
    // Fantome d'apercu : la polyligne de l'entite, au stylo courant du peintre.
    void drawGhost(class QPainter& painter, const geom::Entity& entity) const;
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
    // Reperage polaire : actif par defaut, increment 90° (POLARANG d'AutoCAD) ;
    // dernier repere trouve, dessine par drawTracking.
    bool polarEnabled_ = true;
    double polarIncrementDeg_ = 90.0;
    std::optional<geom::Point2> polarRef_;
    std::optional<geom::Point2> polarPoint_;
    double polarDistance_ = 0.0;
    double polarAngleDeg_ = 0.0;
    // Reperage d'objet : actif par defaut (AutoCAD) ; points acquis pendant la
    // commande (7 au plus, comme AutoCAD) ; alignement en cours.
    bool objectTracking_ = true;
    std::vector<geom::Point2> acquired_;
    std::optional<geom::Point2> trackPoint_;
    std::optional<geom::Point2> trackHorizontalFrom_;
    std::optional<geom::Point2> trackVerticalFrom_;

    bool panning_ = false;
    QPoint lastMousePos_;
    // Historique des vues (V-02) : precedentes, suivantes ; rafale en cours.
    std::vector<render::Camera2D> viewBack_;
    std::vector<render::Camera2D> viewForward_;
    QElapsedTimer viewBurst_;

    // Commande de modification lancee sans selection : la souris designe
    // d'abord les objets, Entree ou clic droit valide.
    bool pickingObjects_ = false;
    ToolMode lastCommand_ = ToolMode::Select;
    std::vector<ToolMode> recentCommands_;
    bool dynamicInput_ = true;
    QString dynamicText_;
    // Outil Texte : etape (0 point, 1 hauteur, 2 angle, 3 contenu), et les
    // valeurs gardees d'un texte a l'autre comme AutoCAD.
    int textStage_ = 0;
    // Cotation lineaire : 0 selon la position, 1 horizontale (H), 2 verticale (V).
    int dimOrientation_ = 0;
    // Rayon du cercle ou de l'arc designe pour une cotation de rayon/diametre.
    double dimRadius_ = 0.0;
    // Hauteur de texte de la cotation en cours, choisie a son premier point.
    double dimTextHeight_ = 0.0;
    // Mesurer : false distance (defaut), true angle ; dernier resultat,
    // repris dans la consigne pour rester lisible apres la mesure.
    bool measureAngle_ = false;
    QString measureResult_;
    // Surface : designation d'objet (O) ; cumul 0 aucun, 1 ajouter, 2 soustraire.
    bool areaObject_ = false;
    int areaTotalMode_ = 0;
    double areaTotal_ = 0.0;
    // Dernier point identifie (I-06), pour la position relative.
    std::optional<geom::Point2> idLast_;
    double textHeight_ = 2.5;
    double textRotation_ = 0.0;   // radians
    // Reseau : false rectangulaire (defaut), true polaire ; nombre d'objets
    // (polaire) ou de colonnes (rectangulaire, 2 rangees).
    bool arrayPolar_ = false;
    int arrayCount_ = 6;
    // L'outil reste actif apres la pose : trois nouveaux points ajoutent un
    // reseau superpose (nouvelle macro). L'utilisateur controle l'historique
    // par Ctrl+Z.
    bool arrayAdjusting_ = false;
    // Chanfrein : false droit (defaut), true conge arrondi ; distance
    // symetrique forcee (> 0) ou rayon du conge.
    bool chamferFillet_ = false;
    double chamferDist_ = 0.0;
    double filletRadius_ = 0.0;
    // Raccord : rayon et ajustement gardes d'une commande a l'autre, comme
    // FILLETRAD / TRIMMODE d'AutoCAD ; premier objet designe ; M multiple ;
    // R attend le rayon tape.
    double filletRadiusMemo_ = 0.0;
    // Decaler : distance, mode par point, effacer la source, calque courant
    // — gardes d'une commande a l'autre (OFFSETDIST…) ; objet designe et
    // mode multiple, propres a la commande.
    // Tourner / Echelle : option Copie (C), propre a la commande.
    bool transformCopy_ = false;
    double offsetDistMemo_ = 0.0;
    bool offsetThrough_ = false;
    bool offsetErase_ = false;
    bool offsetCurrentLayer_ = false;
    int offsetObjectId_ = -1;
    bool offsetMultiple_ = false;
    bool filletTrim_ = true;
    int filletFirstId_ = -1;
    bool filletMultiple_ = false;
    bool filletAwaitRadius_ = false;
    // Hachures : pas force (> 0), 0 = automatique selon la taille.
    double hatchSpacing_ = 0.0;
    // Angle du motif en degres (0-180) et motif : 0 lignes, 1 croix, 2 points.
    double hatchAngleDeg_ = 45.0;
    int hatchPattern_ = 0;
    // Poignees chaudes (objet, poignee) de la commande GripStretch ; position
    // du curseur a l'ecran (survol des poignees) ; appui sur la poignee.
    std::vector<std::pair<int, detail::Grip>> hotGrips_;
    QPoint cursorScreen_;
    QPoint gripPressScreen_;
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


