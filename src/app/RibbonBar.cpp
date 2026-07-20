#include "bcad/app/RibbonBar.h"

#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

namespace bcad::app {

RibbonBar::RibbonBar(QWidget* parent) : QTabWidget(parent) {
    setDocumentMode(true);
    setTabPosition(QTabWidget::North);
    setFocusPolicy(Qt::NoFocus);
    setMaximumHeight(92); // compact — a full-size ribbon would dominate a small window
}

QHBoxLayout* RibbonBar::layoutForTab(const QString& tabName) {
    auto it = tabLayouts_.find(tabName);
    if (it != tabLayouts_.end()) return it.value();

    auto* page = new QWidget(this);
    auto* layout = new QHBoxLayout(page);
    layout->setContentsMargins(4, 2, 4, 0);
    layout->setSpacing(0);
    layout->addStretch(1); // panels pack to the left; stretch absorbs the rest

    tabLayouts_.insert(tabName, layout);
    addTab(page, tabName);
    return layout;
}

void RibbonBar::addPanel(const QString& tabName, const QString& panelTitle, const QList<QAction*>& actions) {
    QHBoxLayout* tabLayout = layoutForTab(tabName);

    auto* panel = new QFrame(this);
    panel->setObjectName("ribbonPanel");
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(6, 4, 6, 2);
    panelLayout->setSpacing(2);

    auto* buttonRow = new QWidget(panel);
    auto* buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(2);
    for (QAction* action : actions) {
        auto* button = new QToolButton(buttonRow);
        button->setDefaultAction(action);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setAutoRaise(true);
        // No artificial floor here: QToolButton's own sizeHint already
        // fits its label, and a floor narrower than that (the previous
        // bug) is exactly what made "Union"/"Intersection"-style actions
        // elide down to identical, illegible text once a tab held enough
        // buttons to exceed the window width. A generous *cap* is still
        // useful for the two labels that are structurally unbounded
        // (Undo/Redo — QUndoStack keeps rewriting their text to "Undo
        // <last command>", which can run arbitrarily long); eliding those
        // is expected/acceptable, unlike two different fixed actions
        // colliding onto the same truncated string.
        if (action->text().contains("Undo") || action->text().contains("Redo")) {
            button->setMaximumWidth(96);
        }
        button->setToolTip(action->text().remove('&'));
        buttonLayout->addWidget(button);
    }

    auto* caption = new QLabel(panelTitle, panel);
    caption->setObjectName("ribbonPanelCaption");
    caption->setAlignment(Qt::AlignHCenter);

    panelLayout->addWidget(buttonRow);
    panelLayout->addWidget(caption);

    // Insert before the trailing stretch, with a separator ahead of every
    // panel but the first in this tab.
    int insertIndex = tabLayout->count() - 1;
    if (insertIndex > 0) {
        auto* separator = new QFrame(this);
        separator->setObjectName("ribbonSeparator");
        separator->setFrameShape(QFrame::VLine);
        tabLayout->insertWidget(insertIndex++, separator);
    }
    tabLayout->insertWidget(insertIndex, panel);
}

} // namespace bcad::app
