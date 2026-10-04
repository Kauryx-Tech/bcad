// Extension garantie a l'enregistrement.
//
// Regression : « Enregistrer sous » avec un nom tape sans extension (« 1 »,
// « fichier1 ») creait un fichier sans « .bcad », invisible ensuite dans la
// boite d'ouverture filtree sur *.bcad.

#include "SaveDialog.h"

#include <QCoreApplication>

#include <cassert>
#include <cstdio>

using bcad::app::withExtension;

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    const QString bcad = QStringLiteral("bcad");
    assert(withExtension(QStringLiteral("/tmp/plan"), bcad) == QStringLiteral("/tmp/plan.bcad"));
    assert(withExtension(QStringLiteral("/tmp/fichier1"), bcad) == QStringLiteral("/tmp/fichier1.bcad"));
    // Deja la bonne extension, quelle que soit la casse : inchange.
    assert(withExtension(QStringLiteral("/tmp/plan.bcad"), bcad) == QStringLiteral("/tmp/plan.bcad"));
    assert(withExtension(QStringLiteral("/tmp/PLAN.BCAD"), bcad) == QStringLiteral("/tmp/PLAN.BCAD"));
    // Une autre extension n'est pas la bonne : on complete.
    assert(withExtension(QStringLiteral("/tmp/plan.v2"), bcad) == QStringLiteral("/tmp/plan.v2.bcad"));
    // Un point dans un dossier ne compte pas comme extension.
    assert(withExtension(QStringLiteral("/tmp/dossier.2026/plan"), bcad) ==
           QStringLiteral("/tmp/dossier.2026/plan.bcad"));
    // Renoncement ou extension inconnue : rien n'est invente.
    assert(withExtension(QString(), bcad).isEmpty());
    assert(withExtension(QStringLiteral("/tmp/plan"), QString()) == QStringLiteral("/tmp/plan"));
    // Exports : l'extension declaree par l'exporteur.
    assert(withExtension(QStringLiteral("/tmp/plan"), QStringLiteral("dxf")) == QStringLiteral("/tmp/plan.dxf"));
    assert(withExtension(QStringLiteral("/tmp/plan.GPKG"), QStringLiteral("gpkg")) == QStringLiteral("/tmp/plan.GPKG"));

    std::printf("Extension a l'enregistrement : tests PASSED\n");
    return 0;
}
