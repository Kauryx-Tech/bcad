#pragma once

#include "DocumentSessions.h"
#include "Viewport.h"
#include "bcad/core/Document.h"
#include "bcad/plugin/Plugin.h"
#include "bcad/plugin/Workbench.h"
#include <QList>
#include <QMainWindow>
#include <QUndoGroup>
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
class QTabBar;
class QTreeWidget;

namespace bcad::app {

class LayerPanel;
class PropertiesPanel;
class RibbonBar;

// Fenêtre principale. Ses corps sont repartis sur plusieurs unites de traduction de
// src/app/, par responsabilite ; la repartition est detaillee a
// src/app/MainWindow.cpp, ou elle explique la sequence d'appels du constructeur.
// L'en-tete Q_OBJECT reste unique (le moc ne voit que lui) et les declarations
// ci-dessous gardent l'ordre historique d'apparition, pas l'ordre des fichiers.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

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
    void onExportDxf();
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
    // Ouvre un fichier .bcad dans un onglet : ramene a l'onglet s'il est deja
    // ouvert, reutilise un dessin vierge intact plutot que d'empiler. Slot pour
    // pouvoir etre appele sans boite de dialogue (glisser-deposer, ligne de
    // commande, tests).
    bool openFile(const QString& path);

private:
    void buildMenusAndRibbon();
    // Menus et rubans declares par les plugins (workbenches) : l'hote ne
    // connait aucun nom de metier (ADR-005, ADR-016).
    void buildPluginMenus();
    void executeWorkbenchAction(plugin::WorkbenchAction action);
    // Cree la commande du module avec ses arguments et l'execute (historique,
    // etat modifie, compte de selection) : la fin commune a toutes les
    // strategies, appelee aussi a la fermeture d'un contour saisi.
    void runWorkbenchCommand(const plugin::WorkbenchAction& action,
                             const std::vector<std::string>& args);
    // Execute les validateurs enregistres par les plugins sur un lot d'entites et
    // remplit le panneau de resultats. L'hote ne porte aucune regle : il affiche
    // les diagnostics tels que les plugins les formulent (ADR-003, ADR-016).
    void runValidation(std::vector<geom::Entity*> scope);
    // Menu « Exporter » dresse depuis les exporteurs enregistres (formats du
    // noyau et d'un module metier) : l'hote ne nomme aucun format, le libelle et
    // l'extension viennent de leur declarant (ADR-016).
    void rebuildExportMenu();
    void rebuildImportMenu();
    void runFileImporter(const std::string& id);
    // Crée dans doc les calques déclarés par les IStyleProvider enregistrés.
    // Premier consommateur du point d'extension IStyleProvider (§7.1).
    void applyStyleProvidersToDocument(core::Document& doc);
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

    // --- Dessins ouverts, un par onglet (MainWindowSessions.cpp) ---
    QWidget* buildDocumentTabs();
    // Branche une session (historique -> etat modifie) et lui ajoute son onglet.
    int addSessionTab(int sessionIndex);
    int newUntitledSession();
    // Rend le dessin actif : document, historique, vue et panneaux suivent.
    void activateSession(int index);
    // Ferme un onglet apres confirmation ; un dernier onglet ferme laisse un
    // dessin vierge, la fenetre n'est jamais sans document.
    bool closeSession(int index);
    void updateTabLabel(int index);
    DocumentSession& session() { return sessions_.at(activeSession_); }
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

    // Dessins ouverts. `document_` et `undoStack_` designent ceux de l'onglet
    // actif : le reste de la fenetre et les panneaux ne voient que celui-la.
    DocumentSessions sessions_;
    int activeSession_ = -1;
    core::Document* document_ = nullptr;
    QUndoStack* undoStack_ = nullptr;
    // Les actions Annuler / Retablir suivent l'historique du dessin actif.
    QUndoGroup undoGroup_;
    QTabBar* documentTabs_ = nullptr;
    Viewport* viewport_ = nullptr;
    RibbonBar* ribbon_ = nullptr;
    LayerPanel* layerPanel_ = nullptr;
    PropertiesPanel* propertiesPanel_ = nullptr;
    QLineEdit* commandLine_ = nullptr;
    QMenu* layerMenu_ = nullptr;
    QMenu* viewMenu_ = nullptr;
    QMenu* helpMenu_ = nullptr;
    QMenu* importMenu_ = nullptr;
    QMenu* exportMenu_ = nullptr;
    // Une QAction par entrée de kTools, dans l'ordre de la table : l'état coché
    // est donc partagé entre le menu et le ruban plutôt que dupliqué.
    std::vector<std::pair<ToolMode, QAction*>> toolActions_;
    QLabel* coordLabel_ = nullptr;
    QLabel* toolLabel_ = nullptr;
    QTimer* autosaveTimer_ = nullptr;
    QDockWidget* layersDock_ = nullptr;
    QDockWidget* propertiesDock_ = nullptr;
    QDockWidget* validationDock_ = nullptr;
    QTreeWidget* validationTree_ = nullptr;
};

} // namespace bcad::app
