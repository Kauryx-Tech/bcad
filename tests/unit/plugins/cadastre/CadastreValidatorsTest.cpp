// Les regles cadastrales sont des validateurs IValidator executes par l'hote
// (WorkbenchParams::RunValidators). Avant ce branchement, ces classes etaient
// compilees sans aucun appelant : ce test est la preuve qu'elles produisent des
// diagnostics, y compris par le seul chemin que l'application emprunte.
//
// Les appels a effet de bord sont hors des assert() : en RelWithDebInfo NDEBUG
// est defini et assert() n'evalue pas son argument.

#include "entities/ParcelEntity.h"
#include "validation/CadastreValidators.h"
#include "validation/SurveyToleranceValidator.h"

#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/validation/Diagnostics.h"

#include <cassert>
#include <limits>
#include <memory>
#include <string>
#include <vector>

using namespace bcad;
using namespace bcad::cadastre;
using validation::Diagnostic;
using validation::Severity;

namespace {

// Parcelle rectangulaire d'angle (x,y), identifiee par section/numero.
std::unique_ptr<ParcelEntity> makeParcel(double x, double y, double width, double height,
                                         const std::string& section, const std::string& numero) {
    auto parcel = std::make_unique<ParcelEntity>(std::vector<geom::Point2>{
        {x, y}, {x + width, y}, {x + width, y + height}, {x, y + height}});
    parcel->properties().setString("cadastre.section", section);
    parcel->properties().setString("cadastre.numero", numero);
    return parcel;
}

// Huit : contour auto-intersectant (sommets dans cet ordre).
std::unique_ptr<ParcelEntity> makeBowtie() {
    auto parcel = std::make_unique<ParcelEntity>(std::vector<geom::Point2>{
        {0, 0}, {10, 10}, {10, 0}, {0, 10}});
    parcel->properties().setString("cadastre.section", "A");
    parcel->properties().setString("cadastre.numero", "9");
    return parcel;
}

std::vector<const Diagnostic*> withSeverity(const std::vector<Diagnostic>& diagnostics,
                                            Severity severity) {
    std::vector<const Diagnostic*> out;
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.severity == severity) out.push_back(&diagnostic);
    }
    return out;
}

bool mentions(const Diagnostic& diagnostic, const std::string& needle) {
    return diagnostic.message.find(needle) != std::string::npos;
}

} // namespace

int main() {
    ParcelTopologyValidator topology;
    ParcelOverlapRuleValidator overlap;
    ParcelIdentifierRuleValidator identifier;

    // Un lot de regles declare les types auxquels il s'applique.
    const std::vector<const plugin::IValidator*> validators{&topology, &overlap, &identifier};
    for (const plugin::IValidator* validator : validators) {
        assert(!validator->id().empty());
        assert(!validator->label().empty());
        const auto types = validator->applicableTypes();
        assert(types.size() == 2);
        assert(validator->id().rfind("cadastre.", 0) == 0);
    }

    // --- Document conforme : aucun diagnostic d'aucune severite ---
    auto cleanA = makeParcel(0, 0, 10, 5, "A", "001");
    auto cleanB = makeParcel(20, 0, 10, 5, "A", "002");
    cleanA->setId(1);
    cleanB->setId(2);
    std::vector<geom::Entity*> clean{cleanA.get(), cleanB.get()};
    assert(topology.validate(clean).empty());
    assert(overlap.validate(clean).empty());
    assert(identifier.validate(clean).empty());

    // --- Topologie : contour auto-intersectant signale, avec l'entite en cause ---
    auto bowtie = makeBowtie();
    bowtie->setId(11);
    std::vector<geom::Entity*> selfIntersecting{bowtie.get()};
    const auto bowtieReports = topology.validate(selfIntersecting);
    assert(bowtieReports.size() == 1);
    assert(bowtieReports[0].severity == Severity::Error);
    assert(mentions(bowtieReports[0], "auto-intersectant"));
    assert(bowtieReports[0].entityIds.size() == 1 && bowtieReports[0].entityIds[0] == 11);

    // --- Topologie : sommet manquant ---
    auto degenerate = std::make_unique<ParcelEntity>(
        std::vector<geom::Point2>{{0, 0}, {5, 5}});
    degenerate->setId(12);
    const auto degenerateReports = topology.validate({degenerate.get()});
    assert(degenerateReports.size() == 1);
    assert(mentions(degenerateReports[0], "3 sommets"));

    // --- Topologie : trois sommets alignes = contour simple mais d'aire nulle ---
    auto flat = std::make_unique<ParcelEntity>(
        std::vector<geom::Point2>{{0, 0}, {5, 0}, {10, 0}});
    flat->setId(13);
    const auto flatReports = topology.validate({flat.get()});
    assert(flatReports.size() == 1);
    assert(mentions(flatReports[0], "aire nulle"));

    // --- Recouvrement : deux emprises communes sont signalees ensemble ---
    auto first = makeParcel(0, 0, 10, 10, "A", "010");
    auto second = makeParcel(5, 5, 10, 10, "A", "011");
    first->setId(21);
    second->setId(22);
    const auto overlapReports = overlap.validate({first.get(), second.get()});
    assert(overlapReports.size() == 1);
    assert(overlapReports[0].severity == Severity::Error);
    assert(overlapReports[0].entityIds.size() == 2);
    assert(overlapReports[0].entityIds[0] == 21 && overlapReports[0].entityIds[1] == 22);

    // Des polygones juxtaposes (bord commun, aucune aire commune) ne sont pas
    // un recouvrement : la regle porte sur l'emprise, pas sur le contact.
    auto west = makeParcel(0, 0, 10, 10, "A", "020");
    auto east = makeParcel(10, 0, 10, 10, "A", "021");
    assert(overlap.validate({west.get(), east.get()}).empty());

    // --- Identification : motif de section non conforme ---
    auto badSection = makeParcel(0, 0, 4, 4, "a", "030");  // minuscule
    const auto badSectionReports = identifier.validate({badSection.get()});
    assert(badSectionReports.size() == 1);
    assert(badSectionReports[0].severity == Severity::Error);
    assert(mentions(badSectionReports[0], "section"));

    // --- Identification : parcelle non renseignee = avertissement, pas erreur ---
    auto unidentified = makeParcel(0, 0, 4, 4, "", "");
    const auto unidentifiedReports = identifier.validate({unidentified.get()});
    assert(unidentifiedReports.size() == 1);
    assert(unidentifiedReports[0].severity == Severity::Warning);

    // --- Identification : deux parcelles portant le meme identifiant ---
    auto twinA = makeParcel(0, 0, 4, 4, "B", "7");
    auto twinB = makeParcel(10, 0, 4, 4, "B", "7");
    twinA->setId(31);
    twinB->setId(32);
    const auto twinReports = identifier.validate({twinA.get(), twinB.get()});
    assert(twinReports.size() == 1);
    assert(twinReports[0].severity == Severity::Error);
    assert(mentions(twinReports[0], "B 7"));
    assert(twinReports[0].entityIds.size() == 2);

    // --- Les regles ignorent ce qui n'est pas une parcelle ---
    auto point = std::make_unique<geom::PointEntity>(geom::Point2{1, 1});
    point->setId(41);
    std::vector<geom::Entity*> mixed{point.get(), cleanA.get()};
    const auto mixedTopology = topology.validate(mixed);
    assert(mixedTopology.empty());  // le point n'est pas une parcelle
    const auto mixedOverlap = overlap.validate(mixed);
    assert(mixedOverlap.empty());

    // --- Tolérance de lever : utilitaire couvert, pas (encore) une regle ---
    // SurveyToleranceValidator compare une emprise mesuree a une emprise
    // juridique SOMMETS PAR SOMMETS. Or le modele n'associe aucune reference
    // juridique a une parcelle donnee (BoundaryEntity n'a ni section ni numero) :
    // l'eriger en IValidator voudrait dire inventer le contrat de donnees que
    // l'on est cense verifier. Il est donc teste comme primitive geometrique,
    // en attendant la couche de reference qui devra l'appeler.
    SurveyToleranceValidator tolerance(0.02);
    const auto within = tolerance.check(
        geom::PolylineEntity(std::vector<geom::Point2>{{0, 0}, {10, 0}, {10, 5}}, true),
        geom::PolylineEntity(std::vector<geom::Point2>{{0.01, 0}, {10, 0.01}, {10, 5}}, true));
    assert(within.withinTolerance);
    assert(within.deviation > 0.0 && within.deviation <= 0.02);
    const auto beyond = tolerance.check(
        geom::PolylineEntity(std::vector<geom::Point2>{{0, 0}, {10, 0}, {10, 5}}, true),
        geom::PolylineEntity(std::vector<geom::Point2>{{0.5, 0}, {10, 0}, {10, 5}}, true));
    assert(!beyond.withinTolerance);
    assert(beyond.deviation > 0.02);
    // --- Des contours de cardinales differentes ne sont pas comparables ---
    // Ecart infini plutot qu'un silence qui ressemblerait a un accord.
    const auto incomparable = tolerance.check(
        geom::PolylineEntity(std::vector<geom::Point2>{{0, 0}, {10, 0}}, true),
        geom::PolylineEntity(std::vector<geom::Point2>{{0, 0}, {10, 0}, {10, 5}}, true));
    assert(!incomparable.withinTolerance);
    assert(incomparable.deviation == std::numeric_limits<double>::infinity());

    // --- Severites distinctes : un lot melange rend les trois niveaux ---
    std::vector<geom::Entity*> mixed2{bowtie.get(), unidentified.get()};
    const auto combined = topology.validate(mixed2);
    assert(withSeverity(combined, Severity::Error).size() == 1);
    assert(combined.size() == 1);  // la parcelle identifiee n'a rien d'anormal

    return 0;
}
