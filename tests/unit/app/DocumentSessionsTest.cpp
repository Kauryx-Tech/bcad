// Dessins ouverts dans des onglets : la logique des sessions, sans fenetre.
//
// Ce que la fenetre en attend : des noms « Dessin1, Dessin2 » ; un libelle
// marque « * » quand le dessin est modifie ; un fichier deja ouvert retrouve
// meme ecrit autrement (chemin relatif, « .. ») ; un dessin vierge reconnu
// comme remplacable ; et surtout un historique d'annulation PROPRE a chaque
// dessin — Annuler dans un onglet ne touche jamais le dessin d'a cote.

#include "DocumentSessions.h"

#include "bcad/geometry/Line.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QUndoCommand>
#include <QUndoGroup>

#include <cassert>
#include <cstdio>
#include <memory>

using namespace bcad;
using app::DocumentSession;
using app::DocumentSessions;

namespace {

// Commande minimale : ajoute une ligne, la retire a l'annulation.
class AddLine : public QUndoCommand {
public:
    explicit AddLine(core::Document& doc) : doc_(doc) {}
    void redo() override {
        id_ = doc_.addEntity(std::make_unique<geom::LineEntity>(geom::Point2{0, 0}, geom::Point2{1, 1}))->id();
    }
    void undo() override { doc_.removeEntity(id_); }

private:
    core::Document& doc_;
    int id_ = -1;
};

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    DocumentSessions sessions;
    const int first = sessions.addUntitled(QStringLiteral("Dessin"));
    const int second = sessions.addUntitled(QStringLiteral("Dessin"));
    assert(sessions.count() == 2);
    assert(sessions.at(first).displayName() == QStringLiteral("Dessin1"));
    assert(sessions.at(second).displayName() == QStringLiteral("Dessin2"));

    // Libelle et dessin vierge.
    assert(sessions.at(first).isPristine());
    sessions.at(first).dirty = true;
    assert(sessions.at(first).tabLabel() == QStringLiteral("Dessin1 *"));
    assert(!sessions.at(first).isPristine());
    sessions.at(first).dirty = false;
    sessions.at(first).document->addEntity(
        std::make_unique<geom::LineEntity>(geom::Point2{0, 0}, geom::Point2{2, 0}));
    assert(!sessions.at(first).isPristine());   // du contenu, meme non marque

    // Un dessin enregistre porte le nom de son fichier.
    const QString dir = QDir::tempPath() + QStringLiteral("/bcad_sessions_test");
    QDir().mkpath(dir);
    const QString file = dir + QStringLiteral("/plan.bcad");
    { QFile touch(file); bool opened = touch.open(QIODevice::WriteOnly); assert(opened); }
    sessions.at(second).filePath = file;
    assert(sessions.at(second).displayName() == QStringLiteral("plan.bcad"));

    // Retrouver un fichier deja ouvert, meme ecrit autrement.
    assert(sessions.indexOfPath(file) == second);
    assert(sessions.indexOfPath(dir + QStringLiteral("/../bcad_sessions_test/plan.bcad")) == second);
    assert(sessions.indexOfPath(dir + QStringLiteral("/autre.bcad")) == -1);
    assert(sessions.indexOfPath(QString()) == -1);   // un dessin sans fichier ne « matche » rien

    // Deplacer un onglet deplace sa session ; retirer decale les suivants.
    const int third = sessions.addUntitled(QStringLiteral("Dessin"));
    DocumentSession* thirdSession = &sessions.at(third);
    sessions.move(third, 0);
    assert(sessions.indexOf(thirdSession) == 0);
    assert(sessions.at(0).displayName() == QStringLiteral("Dessin3"));
    sessions.remove(0);
    assert(sessions.count() == 2 && sessions.indexOf(thirdSession) == -1);
    assert(sessions.at(0).displayName() == QStringLiteral("Dessin1"));

    // Historiques isoles : le groupe n'annule que dans le dessin actif.
    DocumentSession& a = sessions.at(0);
    DocumentSession& b = sessions.at(1);
    QUndoGroup group;
    group.addStack(a.undoStack.get());
    group.addStack(b.undoStack.get());
    const size_t aBefore = a.document->entities().size();
    a.undoStack->push(new AddLine(*a.document));
    b.undoStack->push(new AddLine(*b.document));
    assert(a.document->entities().size() == aBefore + 1);
    assert(b.document->entities().size() == 1);
    group.setActiveStack(b.undoStack.get());
    group.undo();
    assert(b.document->entities().empty());
    assert(a.document->entities().size() == aBefore + 1);   // le dessin voisin n'a pas bouge
    group.setActiveStack(a.undoStack.get());
    group.undo();
    assert(a.document->entities().size() == aBefore);

    QFile::remove(file);
    QDir().rmdir(dir);
    std::printf("Sessions de dessins : tests PASSED\n");
    return 0;
}
