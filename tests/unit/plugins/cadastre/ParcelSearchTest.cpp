// Recherche par reference cadastrale (F4). Elle existait dans le noyau
// (include/bcad/cadastre/ParcelSearch.h) et a ete supprimee lors de la
// migration vers le module sans successeur : l'utilisateur n'avait plus aucun
// moyen de retrouver « A 007 ». Ce test est la preuve que le chemin est refait,
// et qu'il est bien dans le module (la regle de saisie est metier).
//
// Les appels a effet de bord sont hors des assert() : en RelWithDebInfo NDEBUG
// est defini et assert() n'evalue pas son argument.

#include "commands/FindParcelCommand.h"
#include "entities/ParcelEntity.h"

#include "bcad/core/Document.h"
#include "bcad/geometry/Polyline.h"

#include <cassert>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace bcad;
using namespace bcad::cadastre;

namespace {

std::unique_ptr<ParcelEntity> makeParcel(const std::string& section, const std::string& numero,
                                         double x) {
    auto parcel = std::make_unique<ParcelEntity>(std::vector<geom::Point2>{
        {x, 0}, {x + 10, 0}, {x + 10, 5}, {x, 5}});
    parcel->properties().setString("cadastre.section", section);
    parcel->properties().setString("cadastre.numero", numero);
    return parcel;
}

// Parcelle venue d'un DXF : une polyligne fermee portant les proprietes
// cadastrales, sans etre une ParcelEntity. `isCadastreParcel` la reconnait, la
// recherche doit donc la retrouver aussi.
std::unique_ptr<geom::PolylineEntity> makeImportedParcel(const std::string& section,
                                                         const std::string& numero,
                                                         double x) {
    auto poly = std::make_unique<geom::PolylineEntity>(std::vector<geom::Point2>{
        {x, 0}, {x + 10, 0}, {x + 10, 5}, {x, 5}}, true);
    poly->properties().setString("cadastre.section", section);
    poly->properties().setString("cadastre.numero", numero);
    return poly;
}

// Troncon ordinaire : ne doit jamais etre selectionne par une recherche.
std::unique_ptr<geom::PolylineEntity> makeOpenLine(double x) {
    return std::make_unique<geom::PolylineEntity>(
        std::vector<geom::Point2>{{x, 0}, {x + 3, 4}}, false);
}

int selectedCount(const core::Document& doc) {
    int count = 0;
    for (const auto& entity : doc.entities())
        if (entity->selected) ++count;
    return count;
}

std::vector<int> selectedIds(const core::Document& doc) {
    std::vector<int> ids;
    for (const auto& entity : doc.entities())
        if (entity->selected) ids.push_back(entity->id());
    return ids;
}

// Les ecritures d'une meme reference doivent tomber sur le meme resultat :
// c'est la seule partie de la recherche que l'utilisateur maitrise.
void checkParsing() {
    struct Case { std::string_view raw; std::string section; std::string numero; };
    const std::vector<Case> cases = {
        {"A 007", "A", "7"}, {"a|7", "A", "7"},   {"A-007", "A", "7"},
        {"A7", "A", "7"},    {"  A / 7 ", "A", "7"},
        {"B", "B", ""},      {"007", "", "7"},    {"0", "", "0"},
        // Le parseur ne juge pas la forme, il la normalise : la regle metier
        // (une a trois lettres) reste chez le validateur.
        {"abcd", "ABCD", ""},
    };
    for (const auto& test : cases) {
        const ParcelReference parsed = parseParcelReference(test.raw);
        assert(parsed.section == test.section);
        assert(parsed.numero == test.numero);
    }
    assert(parseParcelReference("A 007B").isEmpty());   // suite non numerique
    assert(parseParcelReference("").isEmpty());
    assert(parseParcelReference("   |  ").isEmpty());
}

// L'usine refuse ce qui ne peut rien chercher : l'hote affiche alors son message
// generique au lieu d'executer une commande qui ne selectionnerait rien.
void checkFactoryRejectsUselessArgs() {
    const std::vector<std::vector<std::string>> rejected = {
        {}, {""}, {"   "}, {"§§"}, {"A", "7"},
    };
    for (const auto& args : rejected) assert(makeFindParcel(args) == nullptr);

    core::Document empty;
    auto command = makeFindParcel(std::vector<std::string>{"A 7"});
    assert(command != nullptr);
    command->execute(empty);
    assert(selectedCount(empty) == 0);
    command->undo(empty);
    assert(selectedCount(empty) == 0);
}

const FindParcelCommand& asFind(const std::unique_ptr<commands::Command>& command) {
    const auto* find = dynamic_cast<const FindParcelCommand*>(command.get());
    assert(find != nullptr);
    return *find;
}

void checkSelectionReplacesAndRestores(core::Document& doc) {
    auto command = makeFindParcel(std::vector<std::string>{"a|007"});
    assert(command != nullptr);

    command->execute(doc);
    // « A 007 » : la parcelle du module et sa copie importee de DXF, ni A/001
    // ni B/007, ni le troncon choisi a la main avant la recherche.
    const auto& results = asFind(command).matchedIds();
    assert(results.size() == 2);
    assert(selectedCount(doc) == 2);
    for (int id : results) assert(id > 0);

    command->undo(doc);
    const auto restored = selectedIds(doc);
    assert(restored.size() == 1);
    assert(selectedCount(doc) == 1);

    // Re-execution (redo) : meme resultat, et l'annulation revient toujours a
    // la selection d'origine, pas a celle du premier passage.
    command->execute(doc);
    assert(selectedCount(doc) == 2);
    command->undo(doc);
    assert(selectedCount(doc) == 1);
}

void checkSectionOnlyAndNumeroOnly(core::Document& doc) {
    auto inSectionA = makeFindParcel(std::vector<std::string>{"A"});
    assert(inSectionA != nullptr);
    inSectionA->execute(doc);
    assert(selectedCount(doc) == 3);   // A/001, A/007, A/007 importee

    auto number7 = makeFindParcel(std::vector<std::string>{"7"});
    assert(number7 != nullptr);
    number7->execute(doc);
    assert(selectedCount(doc) == 3);   // A/007, A/007 importee, B/007
}

} // namespace

int main() {
    checkParsing();
    checkFactoryRejectsUselessArgs();

    core::Document doc;
    auto* first = doc.addEntity(makeParcel("A", "001", 0));
    auto* second = doc.addEntity(makeParcel("A", "007", 20));
    auto* third = doc.addEntity(makeParcel("B", "007", 40));
    auto* imported = doc.addEntity(makeImportedParcel("A", "007", 60));
    auto* plain = doc.addEntity(makeOpenLine(80));
    assert(first && second && third && imported && plain);

    // L'utilisateur a selectionne une ligne a la main avant de chercher.
    plain->selected = true;
    checkSelectionReplacesAndRestores(doc);
    checkSectionOnlyAndNumeroOnly(doc);

    return 0;
}
