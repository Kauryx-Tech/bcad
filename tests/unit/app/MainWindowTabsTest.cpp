// Onglets des dessins ouverts, de bout en bout sur la vraie fenetre (hors ecran).
//
// La fenetre est construite comme dans l'application ; les actions passent par
// ses slots (Nouveau, ouverture d'un fichier) et par sa barre d'onglets, comme
// un clic. Le canevas ne rend rien hors ecran, mais il recoit les points saisis.

#include "MainWindow.h"
#include "Viewport.h"

#include "bcad/core/Document.h"
#include "bcad/geometry/Line.h"
#include "bcad/io/Database.h"

#include <QApplication>
#include <QDir>
#include <QTabBar>

#include <cassert>
#include <cstdio>
#include <memory>

using namespace bcad;

namespace {

QString writeDrawing(const QString& path, double x) {
    core::Document doc;
    doc.addEntity(std::make_unique<geom::LineEntity>(geom::Point2{0, 0}, geom::Point2{x, 0}));
    const bool saved = io::Database::save(path.toStdString(), doc);
    assert(saved);
    return path;
}

bool open(app::MainWindow& window, const QString& path) {
    bool ok = false;
    QMetaObject::invokeMethod(&window, "openFile", Q_RETURN_ARG(bool, ok), Q_ARG(QString, path));
    return ok;
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    const QString dir = QDir::tempPath() + QStringLiteral("/bcad_tabs_test");
    QDir(dir).removeRecursively();
    QDir().mkpath(dir);
    const QString a = writeDrawing(dir + QStringLiteral("/a.bcad"), 10);
    const QString b = writeDrawing(dir + QStringLiteral("/b.bcad"), 20);

    app::MainWindow window;
    auto* tabs = window.findChild<QTabBar*>(QStringLiteral("documentTabs"));
    auto* viewport = window.findChild<app::Viewport*>();
    assert(tabs && viewport);

    // Au demarrage : un dessin vierge.
    assert(tabs->count() == 1 && tabs->tabText(0) == QStringLiteral("Dessin1"));
    assert(window.windowTitle().startsWith(QStringLiteral("Dessin1")));

    // Ouvrir un fichier remplace le dessin vierge intact, sans empiler d'onglet.
    bool ok = open(window, a);
    assert(ok);
    assert(tabs->count() == 1 && tabs->tabText(0) == QStringLiteral("a.bcad"));
    assert(tabs->tabToolTip(0).endsWith(QStringLiteral("a.bcad")));

    // Nouveau ajoute un onglet et l'active ; le dessin precedent reste ouvert.
    QMetaObject::invokeMethod(&window, "onNew");
    assert(tabs->count() == 2 && tabs->currentIndex() == 1);
    assert(tabs->tabText(1).startsWith(QStringLiteral("Dessin")));

    // Ce nouveau dessin, encore vierge, cede lui aussi sa place au fichier ouvert.
    ok = open(window, b);
    assert(ok);
    assert(tabs->count() == 2 && tabs->currentIndex() == 1 && tabs->tabText(1) == QStringLiteral("b.bcad"));

    // Rouvrir un fichier deja ouvert ramene a son onglet, sans le dupliquer.
    ok = open(window, dir + QStringLiteral("/../bcad_tabs_test/a.bcad"));
    assert(ok);
    assert(tabs->count() == 2 && tabs->currentIndex() == 0);

    // Dessiner dans b le marque modifie, lui seul.
    tabs->setCurrentIndex(1);
    viewport->setTool(app::ToolMode::Line);
    viewport->submitTypedPoint(QStringLiteral("0,0"));
    viewport->submitTypedPoint(QStringLiteral("@5,5"));
    assert(tabs->tabText(1) == QStringLiteral("b.bcad *"));
    assert(tabs->tabText(0) == QStringLiteral("a.bcad"));
    assert(window.windowTitle().startsWith(QStringLiteral("b.bcad *")));

    // Passer a un autre onglet change le titre ; la marque de b reste.
    tabs->setCurrentIndex(0);
    assert(window.windowTitle().startsWith(QStringLiteral("a.bcad")));
    assert(tabs->tabText(1) == QStringLiteral("b.bcad *"));

    // Un dessin vierge ouvert a cote ne remplace rien tant qu'on n'ouvre rien ;
    // le fermer (intact) ne demande rien et active un voisin.
    QMetaObject::invokeMethod(&window, "onNew");
    assert(tabs->count() == 3 && tabs->currentIndex() == 2);
    emit tabs->tabCloseRequested(2);
    assert(tabs->count() == 2);
    assert(tabs->tabText(0) == QStringLiteral("a.bcad") && tabs->tabText(1) == QStringLiteral("b.bcad *"));

    // Glisser un onglet garde le bon dessin derriere chaque onglet.
    tabs->moveTab(1, 0);
    assert(tabs->tabText(0) == QStringLiteral("b.bcad *"));
    tabs->setCurrentIndex(1);
    assert(window.windowTitle().startsWith(QStringLiteral("a.bcad")));
    tabs->setCurrentIndex(0);
    assert(window.windowTitle().startsWith(QStringLiteral("b.bcad *")));

    QDir(dir).removeRecursively();
    std::printf("Onglets de la fenetre : tests PASSED\n");
    return 0;   // detruire la fenetre ne passe pas par closeEvent : rien a confirmer
}
