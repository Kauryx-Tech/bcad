#pragma once

#include "bcad/app/Viewport.h"
#include "bcad/core/Document.h"
#include <QMainWindow>
#include <QUndoStack>
#include <memory>

class QLabel;
class QLineEdit;
class QTimer;

namespace bcad::app {

class LayerPanel;
class PropertiesPanel;
class RibbonBar;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onNew();
    void onOpen();
    void onSave();
    void onSaveAs();
    void onImportDxf();
    void onExportDxf();
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
};

} // namespace bcad::app
