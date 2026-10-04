// Commandes de parcelle de bout en bout : chacune est obtenue par sa fabrique
// publique avec les arguments que l'hote rassemble pour son action de ruban,
// puis executee, annulee et REFAITE comme le fait la pile d'annulation.
// Regression : l'original etait remis avec un id neuf, et Retablir apres
// Annuler ne trouvait plus la parcelle — Scinder, Fusionner et Lotir ne
// faisaient alors plus rien, sans message.

#include "ParcelOps.h"
#include "entities/ParcelEntity.h"

#include "bcad/commands/Command.h"
#include "bcad/core/Document.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace bcad::cadastre {
std::unique_ptr<commands::Command> makeSubdivideParcel(const std::vector<std::string>& args);
std::unique_ptr<commands::Command> makeSplitParcel(const std::vector<std::string>& args);
std::unique_ptr<commands::Command> makeMergeParcels(const std::vector<std::string>& args);
std::unique_ptr<commands::Command> makeCreateParcel(const std::vector<std::string>& args);
}

using namespace bcad;

namespace {

constexpr double kCutLoss = 0.01;

std::vector<std::string> argsFor(int id, const std::string& count) {
    // Emprise 0..40 x 0..10 : mediane x = 20, debordant d'une unite.
    return {std::to_string(id), count, "20", "-1", "20", "11"};
}

int parcelCount(const core::Document& document) {
    int n = 0;
    for (const auto& entity : document.entities())
        if (dynamic_cast<const cadastre::ParcelEntity*>(entity.get())) ++n;
    return n;
}

} // namespace

int main() {
    core::Document document;
    auto* parcel = document.addEntity(std::make_unique<cadastre::ParcelEntity>(
        std::vector<geom::Point2>{{0, 0}, {40, 0}, {40, 10}, {0, 10}}));
    assert(parcel);
    const int id = parcel->id();

    // Saisies refusees par la fabrique : l'hote ne valide rien lui-meme.
    for (const char* bad : {"1", "0", "101", "abc", ""}) {
        auto rejected = cadastre::makeSubdivideParcel(argsFor(id, bad));
        assert(!rejected);
    }
    assert(!cadastre::makeSubdivideParcel({std::to_string(id), "4"}));

    auto command = cadastre::makeSubdivideParcel(argsFor(id, "4"));
    assert(command);
    command->execute(document);
    assert(parcelCount(document) == 4);
    double total = 0;
    for (const auto& entity : document.entities()) {
        const auto* lot = dynamic_cast<const cadastre::ParcelEntity*>(entity.get());
        assert(lot);
        const double area = cadastre::parcelArea(*lot);
        // 400 m2 en 4 lots egaux, au trait de coupe pres (meme tolerance que
        // ParcelOpsTest : chaque coupe retire 0,01 m2).
        assert(std::abs(area - 100.0) < kCutLoss);
        total += area;
    }
    assert(std::abs(total - (400.0 - 3 * kCutLoss)) < 1e-6);

    // Annuler rend la parcelle d'origine ; refaire redecoupe.
    command->undo(document);
    assert(parcelCount(document) == 1);
    assert(document.findEntity(id) != nullptr);
    command->execute(document);
    assert(parcelCount(document) == 4);
    command->undo(document);
    assert(parcelCount(document) == 1);

    // --- Scinder (BoxSplit : id + mediane) : faire, defaire, refaire ---
    {
        core::Document doc;
        const int pid = doc.addEntity(std::make_unique<cadastre::ParcelEntity>(
            std::vector<geom::Point2>{{0, 0}, {40, 0}, {40, 10}, {0, 10}}))->id();
        auto split = cadastre::makeSplitParcel({std::to_string(pid), "20", "-1", "20", "11"});
        assert(split);
        split->execute(doc);
        assert(parcelCount(doc) == 2);
        split->undo(doc);
        assert(parcelCount(doc) == 1 && doc.findEntity(pid));
        split->execute(doc);
        assert(parcelCount(doc) == 2);
    }

    // --- Fusionner (SelectionIds) : faire, defaire, refaire ---
    {
        core::Document doc;
        const int a = doc.addEntity(std::make_unique<cadastre::ParcelEntity>(
            std::vector<geom::Point2>{{0, 0}, {10, 0}, {10, 10}, {0, 10}}))->id();
        const int b = doc.addEntity(std::make_unique<cadastre::ParcelEntity>(
            std::vector<geom::Point2>{{10, 0}, {20, 0}, {20, 10}, {10, 10}}))->id();
        auto merge = cadastre::makeMergeParcels({std::to_string(a), std::to_string(b)});
        assert(merge);
        merge->execute(doc);
        assert(parcelCount(doc) == 1);
        merge->undo(doc);
        assert(parcelCount(doc) == 2 && doc.findEntity(a) && doc.findEntity(b));
        merge->execute(doc);
        assert(parcelCount(doc) == 1);
    }

    // --- Nouvelle parcelle depuis un contour dessine (PickPolygon) ---
    {
        // Sans contour, plus de rectangle fixe « A 001 » : refus.
        assert(!cadastre::makeCreateParcel({}));
        assert(!cadastre::makeCreateParcel({"0", "0", "10", "0"}));            // deux sommets
        assert(!cadastre::makeCreateParcel({"0", "0", "x", "0", "10", "10"})); // valeur illisible
        assert(!cadastre::makeCreateParcel({"0", "0", "10", "0", "10"}));      // nombre impair

        core::Document doc;
        auto create = cadastre::makeCreateParcel({"0", "0", "20", "0", "20", "10", "0", "10"});
        assert(create);
        create->execute(doc);
        assert(parcelCount(doc) == 1);
        const auto* parcel = dynamic_cast<const cadastre::ParcelEntity*>(doc.entities().front().get());
        assert(parcel && parcel->closed() && parcel->vertices().size() == 4);
        assert(std::abs(cadastre::parcelArea(*parcel) - 200.0) < 1e-9);
        assert(parcel->properties().getString("cadastre.section").empty());   // a saisir ensuite
        const int id = parcel->id();
        create->undo(doc);
        assert(parcelCount(doc) == 0);
        create->execute(doc);                                                  // retablir
        assert(parcelCount(doc) == 1 && doc.findEntity(id) != nullptr);        // meme id

        // La forme texte complete reste acceptee.
        auto full = cadastre::makeCreateParcel({"0,0;5,0;5,5|B|12"});
        assert(full);
        full->execute(doc);
        assert(parcelCount(doc) == 2);
    }

    std::printf("Commandes de parcelle : tests PASSED\n");
    return 0;
}
