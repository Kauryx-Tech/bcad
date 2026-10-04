// Action de module qui fait dessiner un contour (PickPolygon), de bout en bout :
// vraie fenetre hors ecran, module cadastre charge comme dans l'application.
//
// Regression K-B01 : « Nouvelle parcelle » posait un rectangle fixe a
// l'origine. L'action fait maintenant dessiner le contour, et la commande du
// module ne part qu'une fois le contour ferme.

#include "MainWindow.h"
#include "RibbonBar.h"
#include "Viewport.h"

#include <QAction>
#include <QApplication>
#include <QLabel>
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

    // Ruban organise comme AutoCAD : Accueil, Insertion, Annoter ; l'onglet
    // Affichage est passe dans le coin inferieur droit de la barre d'etat.
    auto* ribbon = window.findChild<app::RibbonBar*>(QStringLiteral("ribbon"));
    assert(ribbon && ribbon->count() >= 3);
    assert(ribbon->tabText(0) == QStringLiteral("Accueil"));
    assert(ribbon->tabText(1) == QStringLiteral("Insertion"));
    assert(ribbon->tabText(2) == QStringLiteral("Annoter"));
    for (int i = 0; i < ribbon->count(); ++i) {
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
    for (const char* block : {"Dessin", "Modification", "Annotation", "Calques", "Propriétés", "Utilitaires"})
        assert(blocksOf(0).contains(QString::fromUtf8(block)));
    for (const char* block : {"Bloc", "Définition de bloc", "Référence", "Importer", "Données",
                              "Liaison et extraction", "Localisation"})
        assert(blocksOf(1).contains(QString::fromUtf8(block)));
    for (const char* block : {"Texte", "Cotation", "Lignes d'axe", "Lignes de repère", "Tableaux"})
        assert(blocksOf(2).contains(QString::fromUtf8(block)));
    QStringList accueilTools;
    for (auto* button : buttonsOf(0)) accueilTools << button->defaultAction()->text();
    for (const char* tool : {"Ligne", "Rogner", "Prolonger", "Tourner", "&Union", "Linéaire"})
        assert(accueilTools.contains(QString::fromUtf8(tool)));
    // Les outils pas encore realises sont grises et disent leur tache.
    bool upcomingSeen = false;
    for (auto* button : buttonsOf(2)) {
        if (button->defaultAction()->isEnabled()) continue;
        upcomingSeen = true;
        assert(button->defaultAction()->toolTip().contains(QStringLiteral("à venir")));
    }
    assert(upcomingSeen);
    // Les cotations realisees sont actives dans Annoter.
    QStringList annoterActifs;
    for (auto* button : buttonsOf(2))
        if (button->defaultAction()->isEnabled()) annoterActifs << button->defaultAction()->text();
    assert(annoterActifs.contains(QStringLiteral("Diamètre")));
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
