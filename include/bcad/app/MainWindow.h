#pragma once

#include "bcad/app/Viewport.h"
#include "bcad/core/Document.h"
#include <QMainWindow>
#include <QUndoStack>
#include <memory>

class QLabel;
class QLineEdit;

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

private:
    void buildMenusAndRibbon();
    void buildDockWidgets();
    void buildCommandLine();
    void applyDarkTheme();
    bool saveToPath(const QString& path);

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
};

} // namespace bcad::app
