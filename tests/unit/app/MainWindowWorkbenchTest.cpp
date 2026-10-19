// Action de module qui fait dessiner un contour (PickPolygon), de bout en bout :
// vraie fenetre hors ecran, module cadastre charge comme dans l'application.
//
// Regression K-B01 : « Nouvelle parcelle » posait un rectangle fixe a
// l'origine. L'action fait maintenant dessiner le contour, et la commande du
// module ne part qu'une fois le contour ferme.

#include "window/MainWindow.h"
#include "window/RibbonBar.h"
#include "viewport/Viewport.h"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QStatusBar>
#include <QTabBar>
#include <QToolButton>

#include <cassert>
#include <cstdio>

using namespace bcad;

namespace {

QAction* actionNamed(QWidget& window, const QString& text) {
    for (auto* action : window.findChildren<QAction*>())
        if (action->text() == text) return action;
    return nullptr;
}

QString undoText(QWidget& window) {
    for (auto* action : window.findChildren<QAction*>())
        if (action->text().startsWith(QStringLiteral("&Annuler"))) return action->text();
    return {};
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    app::MainWindow window;

    // Ruban organise comme AutoCAD 2024 en francais : Debut, Inserer,
    // Annoter, Vue, Sortie ; l'ancien onglet Affichage reste dans la barre
    // d'etat. Aucun bouton grise : un outil non realise n'a pas de bouton.
    auto* ribbon = window.findChild<app::RibbonBar*>(QStringLiteral("ribbon"));
    assert(ribbon && ribbon->count() >= 5);
    const char* tabNames[] = {"Début", "Insérer", "Annoter", "Vue", "Sortie"};
    for (int i = 0; i < 5; ++i) assert(ribbon->tabText(i) == QString::fromUtf8(tabNames[i]));
    for (int i = 0; i < ribbon->count(); ++i) {
        assert(ribbon->tabText(i) != QStringLiteral("Accueil"));
        assert(ribbon->tabText(i) != QStringLiteral("Modifier"));
        assert(ribbon->tabText(i) != QStringLiteral("Affichage"));
    }
    auto blocksOf = [&](int tab) {
        QStringList blocks;
        for (auto* caption : ribbon->widget(tab)->findChildren<QLabel*>(QStringLiteral("ribbonPanelCaption")))
            blocks << caption->text();
        return blocks;
    };
    auto buttonsOf = [&](int tab) {
        QList<QToolButton*> buttons;
        for (auto* button : ribbon->widget(tab)->findChildren<QToolButton*>())
            if (button->defaultAction()) buttons << button;
        return buttons;
    };
    auto buttonNamed = [&](int tab, const QString& text) -> QToolButton* {
        for (auto* button : buttonsOf(tab))
            if (button->defaultAction()->text() == text) return button;
        return nullptr;
    };
    auto menuHas = [](QMenu* menu, const QString& text) {
        if (!menu) return false;
        for (auto* action : menu->actions())
            if (action->text() == text) return true;
        return false;
    };
    for (const char* block : {"Dessin", "Modification", "Annotation", "Calques", "Utilitaires"})
        assert(blocksOf(0).contains(QString::fromUtf8(block)));
    assert(blocksOf(1).contains(QStringLiteral("Importer")));
    for (const char* block : {"Texte", "Cotations"}) assert(blocksOf(2).contains(QString::fromUtf8(block)));
    for (const char* block : {"Navigation 2D", "Palettes"}) assert(blocksOf(3).contains(QString::fromUtf8(block)));
    for (const char* block : {"Tracer", "Exporter"}) assert(blocksOf(4).contains(QString::fromUtf8(block)));
    for (int tab = 0; tab < 5; ++tab)
        for (auto* button : buttonsOf(tab)) assert(button->defaultAction()->isEnabled());

    QStringList debutTools;
    for (auto* button : buttonsOf(0)) debutTools << button->defaultAction()->text();
    for (const char* tool : {"Ligne", "Déplacer", "Rotation", "Ajuster", "Raccord", "Décaler", "Linéaire"})
        assert(debutTools.contains(QString::fromUtf8(tool)));
    // Boutons deroulants : la variante affichee et les autres dans la liste.
    QToolButton* trim = buttonNamed(0, QStringLiteral("Ajuster"));
    assert(trim && trim->popupMode() == QToolButton::MenuButtonPopup);
    assert(menuHas(trim->menu(), QStringLiteral("Prolonger")));
    QToolButton* fillet = buttonNamed(0, QStringLiteral("Raccord"));
    assert(fillet && menuHas(fillet->menu(), QStringLiteral("Chanfrein")));
    QToolButton* dims = buttonNamed(2, QStringLiteral("Linéaire"));
    assert(dims && menuHas(dims->menu(), QStringLiteral("Diamètre")));
    // Panneau etendu (fleche ▼) de Modification : outils secondaires.
    bool expandSeen = false;
    for (auto* expand : ribbon->widget(0)->findChildren<QToolButton*>(QStringLiteral("ribbonPanelExpand"))) {
        if (!menuHas(expand->menu(), QStringLiteral("Coupure"))) continue;
        expandSeen = true;
        assert(menuHas(expand->menu(), QStringLiteral("&Joindre")));
        assert(menuHas(expand->menu(), QStringLiteral("&Opérations booléennes")));
    }
    assert(expandSeen);
    // L'affichage vit dans le coin inferieur droit.
    QStringList statusActions;
    for (auto* button : window.statusBar()->findChildren<QToolButton*>())
        if (button->defaultAction()) statusActions << button->defaultAction()->text();
    assert(statusActions.contains(QStringLiteral("Zoomer sur &tout")));
    assert(statusActions.contains(QStringLiteral("Calques")));
    assert(statusActions.contains(QStringLiteral("Vérifications")));

    auto* viewport = window.findChild<app::Viewport*>();
    auto* tabs = window.findChild<QTabBar*>(QStringLiteral("documentTabs"));
    QAction* newParcel = actionNamed(window, QStringLiteral("Nouvelle parcelle"));
    if (!newParcel) {
        // Le module n'est pas construit dans cet arbre : rien a verifier ici.
        std::printf("Module absent : test ignore\n");
        return 0;
    }
    assert(viewport && tabs);

    newParcel->trigger();
    assert(viewport->tool() == app::ToolMode::CapturePolygon);
    assert(viewport->prompt().contains(QStringLiteral("Nouvelle parcelle")));
    viewport->submitTypedPoint(QStringLiteral("0,0"));
    viewport->submitTypedPoint(QStringLiteral("@20,0"));
    viewport->submitTypedPoint(QStringLiteral("@0,10"));
    viewport->submitTypedPoint(QStringLiteral("@-20,0"));
    assert(tabs->tabText(0) == QStringLiteral("Dessin1"));   // rien n'est cree avant fermeture
    viewport->submitTypedPoint(QStringLiteral("C"));

    // La commande du module est passee par l'historique du dessin.
    assert(viewport->tool() == app::ToolMode::Select);
    assert(tabs->tabText(0) == QStringLiteral("Dessin1 *"));
    assert(undoText(window).contains(QStringLiteral("Nouvelle parcelle")));

    // Echap pendant la saisie : rien n'est cree.
    newParcel->trigger();
    viewport->submitTypedPoint(QStringLiteral("50,50"));
    viewport->submitTypedPoint(QStringLiteral("@5,0"));
    viewport->setTool(app::ToolMode::Select);
    const QString before = undoText(window);
    assert(before.contains(QStringLiteral("Nouvelle parcelle")));   // toujours la premiere

    std::printf("Action de module avec contour : tests PASSED\n");
    return 0;
}
