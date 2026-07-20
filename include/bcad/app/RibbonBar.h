#pragma once

#include <QHash>
#include <QList>
#include <QTabWidget>

class QHBoxLayout;
class QAction;

namespace bcad::app {

// Une approximation légère du ruban ("ribbon") utilisé par les versions
// récentes d'AutoCAD : des onglets (Accueil / Modifier / Affichage) contenant
// des "panneaux" groupés de boutons d'outils, chaque panneau étant légendé
// en dessous — plutôt qu'une seule barre d'outils plate. Construit sur les
// QTabWidget/QToolButton standard (pas de bibliothèque de ruban externe),
// donc ça reste simple : addPanel() est toute l'API, appelée une fois par
// groupe logique d'actions depuis MainWindow.
class RibbonBar : public QTabWidget {
    Q_OBJECT
public:
    explicit RibbonBar(QWidget* parent = nullptr);

    // Ajoute un panneau légendé de boutons (un par action) à l'onglet donné,
    // en créant l'onglet lors de la première utilisation. Les actions sont
    // des objets QAction partagés, donc déclencher un bouton du ruban fait
    // exactement ce que fait l'élément de menu ou le raccourci équivalent —
    // aucune logique séparée à maintenir synchronisée.
    void addPanel(const QString& tabName, const QString& panelTitle, const QList<QAction*>& actions);

private:
    QHBoxLayout* layoutForTab(const QString& tabName);

    QHash<QString, QHBoxLayout*> tabLayouts_;
};

} // namespace bcad::app
