#pragma once

#include <QHash>
#include <QList>
#include <QTabWidget>

class QHBoxLayout;
class QAction;

namespace bcad::app {

// A light approximation of the ribbon UI recent AutoCAD versions use:
// tabs (Home / Modify / View) containing grouped "panels" of tool buttons,
// each panel captioned underneath — instead of one flat toolbar. Built on
// stock QTabWidget/QToolButton (no external ribbon library), so it stays
// simple: addPanel() is the whole API, called once per logical group of
// actions from MainWindow.
class RibbonBar : public QTabWidget {
    Q_OBJECT
public:
    explicit RibbonBar(QWidget* parent = nullptr);

    // Adds a captioned panel of buttons (one per action) to the given tab,
    // creating the tab on first use. Actions are shared QAction objects, so
    // triggering a ribbon button does exactly what the equivalent menu item
    // or shortcut does — no separate logic to keep in sync.
    void addPanel(const QString& tabName, const QString& panelTitle, const QList<QAction*>& actions);

private:
    QHBoxLayout* layoutForTab(const QString& tabName);

    QHash<QString, QHBoxLayout*> tabLayouts_;
};

} // namespace bcad::app
