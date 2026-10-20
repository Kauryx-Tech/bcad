// Proces-verbal de bornage (K-07) : pre-rempli depuis le dossier, les bornes
// et les riverains, sans rien inventer ; les saisies sont echappees ; le PDF
// est ecrit. Sans ecran : QT_QPA_PLATFORM=offscreen.
//
// Les appels a effet de bord sont hors des assert().

#include "commands/LimitesCommand.h"
#include "entities/ParcelEntity.h"
#include "io/ProcesVerbalExporter.h"
#include "layout/CadastreSheet.h"
#include "layout/ProcesVerbal.h"

#include "bcad/core/Document.h"
#include "bcad/properties/PropertyMap.h"

#include <QFile>
#include <QGuiApplication>
#include <QTemporaryDir>

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace bcad;
using namespace bcad::cadastre;

namespace {

int addParcel(core::Document& doc, double x0, const std::string& numero, const std::string& proprietaire) {
    auto parcel = std::make_unique<ParcelEntity>(
        std::vector<geom::Point2>{{x0, 0}, {x0 + 10, 0}, {x0 + 10, 10}, {x0, 10}});
    parcel->properties().setString("cadastre.section", "A");
    parcel->properties().setString("cadastre.numero", numero);
    parcel->properties().setString("cadastre.proprietaire", proprietaire);
    return doc.addEntity(std::move(parcel))->id();
}

bool contient(const std::string& texte, const std::string& motif) {
    return texte.find(motif) != std::string::npos;
}

} // namespace

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);

    assert(htmlEscape("<b>&'\"") == "&lt;b&gt;&amp;&#39;&quot;");

    core::Document doc;
    doc.properties().addString("cadastre.dossier.geometre", "Kossi A.");
    doc.properties().addString("cadastre.dossier.agrement_geometre", "G-042");
    const int a1 = addParcel(doc, 0, "1", "Ama <B>");
    addParcel(doc, 10, "2", "");

    EditLimitesCommand prepare({a1});
    prepare.execute(doc);
    auto& props = doc.findEntity(a1)->properties();
    props.setString(limiteKey(0, "riverain"), "Voie publique");
    props.setString(limiteKey(2, "riverain"), "Voie publique");   // une seule signature
    props.setString(limiteKey(1, "riverain"), "Ignoré");          // A 2 le borde
    props.setEnum(limiteKey(1, "presence"), 1);

    const ProcesVerbal pv = buildProcesVerbal(doc);
    assert(pv.geometre == "Kossi A." && pv.agrement == "G-042" && pv.date.empty());
    assert(pv.parcelles.size() == 2);
    const auto& p1 = pv.parcelles[0];
    assert(p1.designation == "A 1" && p1.proprietaire == "Ama <B>");
    assert(p1.bornes.size() == 4 && p1.bornes[1].numero == "B2" && p1.bornes[1].x == 10.0);
    assert(p1.limites.size() == 4);
    assert(p1.limites[0].riverain == "Voie publique" && p1.limites[0].presence.empty());
    assert(p1.limites[1].riverain == "A 2" && p1.limites[1].presence == "Présent");
    assert(p1.surface > 99.99 && p1.surface < 100.01);
    assert(pv.parcelles[1].limites[3].riverain == "Ama <B> (A 1)");

    // Geometre a part ; proprietaires de A 1 et A 2 ; un seul riverain non leve.
    assert(pv.signataires.size() == 3);
    assert(pv.signataires[0].qualite == "Propriétaire de A 1" && pv.signataires[0].nom == "Ama <B>");
    assert(pv.signataires[1].qualite == "Propriétaire de A 2" && pv.signataires[1].nom.empty());
    assert(pv.signataires[2].qualite == "Riverain" && pv.signataires[2].nom == "Voie publique");

    const std::string html = procesVerbalHtml(pv);
    assert(contient(html, "PROCÈS-VERBAL DE BORNAGE CONTRADICTOIRE"));
    assert(contient(html, "Ama &lt;B&gt;") && !contient(html, "Ama <B>"));
    assert(contient(html, "G-042") && contient(html, "100,00 m²"));
    assert(contient(html, "☐ posée"));                      // etat des bornes a constater
    assert(contient(html, "☐ présent &nbsp; ☐ absent"));    // presence non renseignee
    assert(contient(html, "<td>Présent</td>"));             // presence saisie
    assert(!contient(html, "Ignoré"));

    QTemporaryDir dir;
    assert(dir.isValid());
    const std::string chemin = dir.filePath("pv.pdf").toStdString();
    ProcesVerbalExporter exporter;
    assert(exporter.extension() == "pdf");
    std::string erreur;
    const bool ecrit = exporter.writeDocument(doc, chemin, &erreur);
    assert(ecrit && erreur.empty());
    QFile pdf(QString::fromStdString(chemin));
    const bool ouvert = pdf.open(QIODevice::ReadOnly);
    assert(ouvert);
    assert(pdf.read(5) == "%PDF-" && pdf.size() > 2000);

    const bool refuse = exporter.writeDocument(doc, dir.filePath("absent/pv.pdf").toStdString(), &erreur);
    assert(!refuse && !erreur.empty());

    std::cout << "cadastre_pv_test OK\n";
    return 0;
}
