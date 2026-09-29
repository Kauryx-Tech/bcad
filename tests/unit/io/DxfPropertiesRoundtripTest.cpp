// Le DXF de BCAD est le seul format d'échange ou le noyau transporte des
// proprietes. Ce test porte la regle qui a motive le retrait du metier de
// src/io : le noyau doit restituer ce qu'il ne comprend pas.
//
// Aucune cible ici ne charge le module Cadastre : ce binaire ne lie que le noyau.
// Les cles « cadastre.* » et « network.* » y sont donc toutes deux inconnues de
// l'hote, et leur conservation est la mesure du resultat.

#include "bcad/core/Document.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/TextEntity.h"
#include "bcad/io/DxfReader.h"
#include "bcad/io/DxfWriter.h"
#include "bcad/properties/PropertyMap.h"
#include "bcad/properties/PropertyTypes.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace bcad;

#ifndef BCAD_FIXTURES_DIR
#define BCAD_FIXTURES_DIR ""
#endif

namespace {

std::string tempPath(const std::string& name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

// Document n'est ni copiable ni deplacable (verrou interne) : le aller-retour
// se fait dans un document que l'appelant possede.
void writeThenRead(const core::Document& doc, const std::string& name, core::Document& back) {
    const std::string path = tempPath(name);
    assert(io::writeDxf(path, doc));
    assert(io::readDxf(path, back));
    std::filesystem::remove(path);
}

std::string readFile(const std::string& path) {
    std::ifstream f(path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// La table APPID declare BCAD_PROPS dans tout fichier ecrit par BCAD : seule la
// XDATA portee par une entite prouve que des proprietes sont parties.
bool carriesPropertiesXData(const std::string& dxf) {
    return dxf.find("1001\nBCAD_PROPS\n") != std::string::npos;
}

// 1. Tous les types du PropertyMap traversent le DXF, sur une entite qui n'est
//    pas une parcelle et sous des cles qu'aucun module enregistre n'a declarees.
void testTypedPropertiesSurvive() {
    core::Document doc;
    geom::LineEntity line(geom::Point2(0, 0), geom::Point2(5, 5));
    auto& props = line.properties();
    props.setString("network.material", "PVC");
    props.setDouble("network.diameter_m", 0.125);
    props.setInt("network.joints", 7);
    props.setBool("network.pressurized", true);
    props.addEnum("network.status", 1, { "prevu", "pose", "supprime" });
    props.setColor("network.flag", geom::Color{ 0.25f, 0.5f, 0.75f, 1.0f });
    doc.addEntity(std::make_unique<geom::LineEntity>(line));

    core::Document back;
    writeThenRead(doc, "bcad_dxf_props.dxf", back);
    assert(back.entities().size() == 1);
    auto& found = back.entities().front()->properties();

    assert(found.getString("network.material") == "PVC");
    assert(found.getDouble("network.diameter_m", -1.0) == 0.125);
    assert(found.getInt("network.joints", -1) == 7);
    assert(found.getBool("network.pressurized", false) == true);
    // Un enum s'exporte en libelle : la liste des valeurs est du schema du
    // module, pas une donnee du fichier. Ecrire l'index 2 sans ce domaine
    // produirait une valeur hors domaine que Property::setFromEnum rejetterait.
    assert(found.getString("network.status") == "pose");
    const geom::Color flag = found.getColor("network.flag");
    assert(flag.r == 0.25f && flag.g == 0.5f && flag.b == 0.75f);

    // Le type est une donnee du fichier, pas une supposition du lecteur : sans
    // lui, 0.125 reviendrait en chaine et un champ vide en zero.
    assert(found.find("network.diameter_m")->type() == properties::PropertyType::Double);
    assert(found.find("network.joints")->type() == properties::PropertyType::Int);
    assert(found.find("network.pressurized")->type() == properties::PropertyType::Bool);
    assert(found.find("network.status")->type() == properties::PropertyType::String);
    assert(found.find("network.flag")->type() == properties::PropertyType::Color);
    assert(found.find("network.material")->type() == properties::PropertyType::String);
}

// 2. L'hote n'invente pas d'appid metier : celui qu'il ecrit est le sien, et
//    l'ancien nom cadastral a disparu de l'ecriture.
void testExportUsesOneGenericAppId() {
    core::Document doc;
    geom::PolylineEntity parcel({ { 0, 0 }, { 10, 0 }, { 10, 5 }, { 0, 5 } }, true);
    parcel.properties().setString("cadastre.section", "AB");
    parcel.properties().setString("cadastre.numero", "152");
    doc.addEntity(std::make_unique<geom::PolylineEntity>(parcel));

    const std::string path = tempPath("bcad_dxf_appid.dxf");
    assert(io::writeDxf(path, doc));
    const std::string dxf = readFile(path);
    std::filesystem::remove(path);

    assert(carriesPropertiesXData(dxf));
    assert(dxf.find("BCAD_CADASTRE") == std::string::npos);
    assert(dxf.find("CADASTRE_") == std::string::npos);
}

// 3. Un texte s'exporte et se relit comme un texte. Avant la generisation, TEXT
//    n'etait pas analyse a la lecture et MTEXT devenait un point vide en (0,0)
//    porteur de cles « cadastre.label_* » que personne ne lisait.
void testTextRoundTripsAsText() {
    core::Document doc;
    doc.addEntity(std::make_unique<geom::TextEntity>(
        geom::Point2(1.5, 2.5), "Registre 1932", 3.5, 0.0));

    core::Document back;
    writeThenRead(doc, "bcad_dxf_text.dxf", back);
    assert(back.entities().size() == 1);
    auto& e = *back.entities().front();
    assert(e.typeId() == "bcad.Text");
    auto* text = dynamic_cast<geom::TextEntity*>(&e);
    assert(text != nullptr);
    assert(text->text() == "Registre 1932");
    assert(text->height() == 3.5);
    assert(text->position().x() == 1.5 && text->position().y() == 2.5);
    // Rien de cadastral n'est apparu au passage.
    assert(text->properties().listNames().empty());
}

// 4. Les XDATA d'un appid inconnu sont ignorees, pas converties en proprietes :
//    le noyau ne peut pas deviner un contrat qu'il ne connait pas.
void testUnknownAppIdIsIgnored() {
    const std::string path = tempPath("bcad_dxf_unknown_appid.dxf");
    {
        std::ofstream f(path);
        f << "0\nSECTION\n2\nENTITIES\n"
          << "0\nLWPOLYLINE\n8\n0\n90\n2\n70\n1\n"
          << "10\n0.0\n20\n0.0\n10\n4.0\n20\n3.0\n"
          << "1001\nAUTRE_LOGICIEL\n1002\n{\n1000\ncle\n1000\nvaleur\n1002\n}\n"
          << "0\nENDSEC\n0\nEOF\n";
    }
    core::Document doc;
    assert(io::readDxf(path, doc));
    std::filesystem::remove(path);

    assert(doc.entities().size() == 1);
    assert(doc.entities().front()->properties().listNames().empty());
}

// 5. Un DXF publie avant la forme generique se recharge : l'ancienne XDATA
//    BCAD_CADASTRE allait par paires et le lecteur reconstruisait les cles.
//    Lire n'est pas ecrire — plus rien ne produit ce fichier dans le depot.
void testLegacyCadastreXDataStillReads() {
    const std::string path = std::string(BCAD_FIXTURES_DIR) + "/dxf/legacy_cadastre_xdata.dxf";
    core::Document doc;
    assert(io::readDxf(path, doc));

    assert(doc.entities().size() == 1);
    auto* poly = dynamic_cast<geom::PolylineEntity*>(doc.entities().front().get());
    assert(poly != nullptr);
    assert(poly->closed());
    assert(poly->vertices().size() == 4);
    assert(poly->properties().getString("cadastre.section") == "AB");
    assert(poly->properties().getString("cadastre.numero") == "152");
    // Une valeur vide n'etait pas une propriete a l'ecriture : elle ne le
    // devient pas a la lecture.
    assert(!poly->properties().has("cadastre.contenance"));
}

// 6. Une valeur invalide dans un champ typé ne doit pas faire remonter
//    d'exception : un DXF vient de l'exterieur.
void testMalformedTypedValueIsSkipped() {
    const std::string path = tempPath("bcad_dxf_malformed.dxf");
    {
        std::ofstream f(path);
        f << "0\nSECTION\n2\nENTITIES\n"
          << "0\nLWPOLYLINE\n8\n0\n90\n2\n70\n1\n"
          << "10\n0.0\n20\n0.0\n10\n4.0\n20\n3.0\n"
          << "1001\nBCAD_PROPS\n1002\n{\n"
          << "1000\nmauvais_reel\n1000\ndouble\n1000\npas_un_nombre\n"
          << "1000\nbon_texte\n1000\nstring\n1000\nok\n"
          << "1002\n}\n"
          << "0\nENDSEC\n0\nEOF\n";
    }
    core::Document doc;
    assert(io::readDxf(path, doc));
    std::filesystem::remove(path);

    assert(doc.entities().size() == 1);
    auto& props = doc.entities().front()->properties();
    assert(!props.has("mauvais_reel"));
    assert(props.getString("bon_texte") == "ok");
}

// 7. Une entite sans representation DXF (un point, dans notre sous-ensemble)
//    n'ecrit aucun groupe : ses proprietes ne doivent pas se poser en XDATA sur
//    l'enregistrement du voisin, qui les recupererait a la relecture.
void testUnwrittenEntityCarriesNoProperties() {
    core::Document doc;
    doc.addEntity(std::make_unique<geom::LineEntity>(geom::Point2(0, 0), geom::Point2(1, 1)));
    auto point = std::make_unique<geom::PointEntity>(geom::Point2(5, 5));
    point->properties().setString("cadastre.section", "AB");
    doc.addEntity(std::move(point));

    const std::string path = tempPath("bcad_dxf_orphan_xdata.dxf");
    assert(io::writeDxf(path, doc));
    const std::string dxf = readFile(path);

    assert(!carriesPropertiesXData(dxf));

    core::Document back;
    assert(io::readDxf(path, back));
    std::filesystem::remove(path);

    // Une ligne relit, le point na pas de representation DXF : rien ne doit
    // avoir ete invente pour lui.
    assert(back.entities().size() == 1);
    assert(back.entities().front()->properties().listNames().empty());
}

} // namespace

int main() {
    testTypedPropertiesSurvive();
    testExportUsesOneGenericAppId();
    testTextRoundTripsAsText();
    testUnknownAppIdIsIgnored();
    testLegacyCadastreXDataStillReads();
    testMalformedTypedValueIsSkipped();
    testUnwrittenEntityCarriesNoProperties();
    return 0;
}
