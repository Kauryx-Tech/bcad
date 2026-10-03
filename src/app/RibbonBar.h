#pragma once

#include <QHash>
#include <QList>
#include <QTabWidget>

class QHBoxLayout;
class QAction;
class QMenu;
class QScrollArea;

namespace bcad::app {

// Une approximation légère du ruban des versions récentes d'AutoCAD : des
// onglets contenant des panneaux légendés. Dans un panneau, les premières
// actions sont de grands boutons (icône au-dessus du libellé), les suivantes de
// petits boutons empilés par trois en colonnes — la disposition du panneau
// « Dessin » d'AutoCAD. Construit sur QTabWidget/QToolButton, sans
// bibliothèque externe.
class RibbonBar : public QTabWidget {
    Q_OBJECT
public:
    explicit RibbonBar(QWidget* parent = nullptr);

    // Ajoute un panneau légendé à l'onglet donné, en créant l'onglet à la
    // première utilisation. `largeCount` : nombre d'actions en grands boutons,
    // les suivantes sont petites ; -1 (défaut) les rend toutes grandes. Les
    // actions sont les QAction partagées des menus : un bouton du ruban fait
    // exactement ce que fait l'entrée de menu équivalente.
    void addPanel(const QString& tabName, const QString& panelTitle,
                  const QList<QAction*>& actions, int largeCount = -1);

    // Bouton d'application à gauche des onglets, qui déroule `menu`.
    void setApplicationMenu(QMenu* menu);

private:
    QHBoxLayout* layoutForTab(const QString& tabName);

    QHash<QString, QHBoxLayout*> tabLayouts_;
    QHash<QString, QScrollArea*> tabAreas_;
};

} // namespace bcad::app
