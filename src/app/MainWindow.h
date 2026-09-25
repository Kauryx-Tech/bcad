#pragma once

#include "Viewport.h"
#include "bcad/core/Document.h"
#include "bcad/plugin/Plugin.h"
#include "bcad/plugin/Workbench.h"
#include <QList>
#include <QMainWindow>
#include <QUndoStack>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class QCloseEvent;
class QAction;
class QLabel;
class QLineEdit;
class QMenu;
class QTimer;
class QDockWidget;
class QTreeWidget;

namespace bcad::app {

class LayerPanel;
class PropertiesPanel;
class RibbonBar;

// Fenêtre principale. Ses corps sont repartis sur cinq unites de traduction de
// src/app/, par responsabilite ; la repartition est detaillee a
// src/app/MainWindow.cpp, ou elle explique la sequence d'appels du constructeur.
// L'en-tete Q_OBJECT reste unique (le moc ne voit que lui) et les declarations
// ci-dessous gardent l'ordre historique d'apparition, pas l'ordre des fichiers.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    // Rien d'autre ne préserve le travail : fermer la fenêtre sans
    // confirmation jetterait un document modifié.
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onNew();
    void onOpen();
    void onSave();
    void onSaveAs();
    void onImportDxf();
    void onPrintPreview();
    void onCursorMoved(double x, double y);
    void onToolChanged(ToolMode mode);
    // Donne le focus à la ligne de commande et l'initialise avec le
    // caractère qui l'a déclenchée (voir Viewport::typedInputRequested).
    void onTypedInputRequested(const QString& initialText);
    // Touche Entrée dans la ligne de commande : transmet le texte saisi au
    // viewport comme coordonnée, puis vide le champ et redonne le focus
    // au canevas.
    void onCommandLineSubmitted();
    // Sauvegarde périodique dans un fichier `.bcad.autosave` à côté du
    // projet, seulement s'il y a des modifications non enregistrées et un
    // chemin de projet connu (pas de sauvegarde auto pour un document tout
    // neuf jamais enregistré). Échec silencieux — un raté d'autosave ne
    // doit pas interrompre le dessin, contrairement à un Save explicite.
    void onAutosaveTimeout();

private:
    void buildMenusAndRibbon();
    // Menus et rubans declares par les plugins (workbenches) : l'hote ne
    // connait aucun nom de metier (ADR-005, ADR-016).
    void buildPluginMenus();
    void executeWorkbenchAction(plugin::WorkbenchAction action);
    // Execute les validateurs enregistres par les plugins sur un lot d'entites et
    // remplit le panneau de resultats. L'hote ne porte aucune regle : il affiche
    // les diagnostics tels que les plugins les formulent (ADR-003, ADR-016).
    void runValidation(std::vector<geom::Entity*> scope);
    // Menu « Exporter » dresse depuis les exporteurs enregistres (formats du
    // noyau et d'un module metier) : l'hote ne nomme aucun format, le libelle et
    // l'extension viennent de leur declarant (ADR-016).
    void rebuildExportMenu();
    void runFileExporter(const std::string& id);
    // Faux si l'utilisateur renonce : le document porte des modifications que
    // rien d'autre ne préserve. Appelé avant tout chemin qui écrase le document.
    bool confirmDiscard();
    // Recale les vues qui dépendent du contenu du document : le panneau de
    // propriétés lit le document à chaque rafraîchissement, donc sans cet appel
    // il continue d'afficher les champs d'entités détruites après Nouveau ou
    // Ouvrir.
    void refreshDocumentViews();
    void updateWindowTitle();
    // Les outils ne se décrètent qu'une fois, dans la table kTools du .cpp : le
    // menu, les panneaux du ruban et l'étiquette de la barre d'état lisent la
    // même QAction partagée, ils n'en redéfinissent pas trois copies.
    void buildToolActions();
    QAction* toolAction(ToolMode mode) const;
    void addToolActions(QMenu* menu, std::initializer_list<ToolMode> modes);
    QList<QAction*> toolActionsFor(std::initializer_list<ToolMode> modes) const;
    void buildDockWidgets();
    void buildCommandLine();
    void applyDarkTheme();
    bool saveToPath(const QString& path);
    // Propose de charger la sauvegarde automatique de `path` si elle existe
    // et est plus récente que `path` lui-même (reprise après plantage).
    // Retourne le chemin à charger effectivement : la sauvegarde auto si
    // l'utilisateur accepte, sinon `path` inchangé.
    QString resolveRecoveryPath(const QString& path);
    static QString autosavePathFor(const QString& path) { return path + ".autosave"; }

    std::unique_ptr<core::Document> document_;
    QUndoStack undoStack_;
    Viewport* viewport_ = nullptr;
    RibbonBar* ribbon_ = nullptr;
    LayerPanel* layerPanel_ = nullptr;
    PropertiesPanel* propertiesPanel_ = nullptr;
    QLineEdit* commandLine_ = nullptr;
    QMenu* layerMenu_ = nullptr;
    QMenu* viewMenu_ = nullptr;
    QMenu* helpMenu_ = nullptr;
    QMenu* exportMenu_ = nullptr;
    // Une QAction par entrée de kTools, dans l'ordre de la table : l'état coché
    // est donc partagé entre le menu et le ruban plutôt que dupliqué.
    std::vector<std::pair<ToolMode, QAction*>> toolActions_;
    QLabel* coordLabel_ = nullptr;
    QLabel* toolLabel_ = nullptr;
    QString currentFilePath_;
    QTimer* autosaveTimer_ = nullptr;
    // Marque des changements non enregistrés depuis le dernier
    // chargement/enregistrement — mis à jour sur QUndoStack::indexChanged
    // plutôt que via isClean() pour rester simple (déclenché aussi par un
    // undo qui revient à l'état initial, imprécision acceptée pour ce
    // qui reste une fonctionnalité de sécurité, pas un indicateur UI fin).
    bool dirty_ = false;
    QDockWidget* validationDock_ = nullptr;
    QTreeWidget* validationTree_ = nullptr;
};

} // namespace bcad::app
