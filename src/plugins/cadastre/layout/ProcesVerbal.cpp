#include "layout/ProcesVerbal.h"

#include "layout/CadastreSheet.h"

#include "bcad/core/Document.h"
#include "bcad/properties/PropertyMap.h"
#include "entities/ParcelEntity.h"

#include <algorithm>

namespace bcad::cadastre {

namespace {

std::string attribut(const core::Document& document, const char* cle) {
    return document.properties().getString(std::string("cadastre.dossier.") + cle);
}

// Une valeur saisie, ou une ligne a remplir a la main.
std::string ouLigne(const std::string& valeur) {
    return valeur.empty() ? std::string("……………………………………") : htmlEscape(valeur);
}

const char* kCases = "☐ posée &nbsp; ☐ reconnue &nbsp; ☐ manquante";

} // namespace

std::string htmlEscape(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c;
        }
    }
    return out;
}

ProcesVerbal buildProcesVerbal(const core::Document& document) {
    ProcesVerbal pv;
    pv.date = attribut(document, "date");
    pv.geometre = attribut(document, "geometre");
    pv.agrement = attribut(document, "agrement_geometre");
    pv.dossier = attribut(document, "dossier");
    pv.commune = attribut(document, "commune");
    pv.systeme = attribut(document, "systeme_coordonnees");

    // Les trois calculs parcourent les parcelles dans le meme ordre et donnent
    // une ligne par sommet : on les decoupe parcelle par parcelle.
    const auto surfaces = surfaceComputations(document);
    const auto coordonnees = coordinateRows(document);
    const auto riverains = riverainRows(document);
    std::vector<const geom::Entity*> parcelles;
    for (const auto& entity : document.entities()) {
        if (!isCadastreParcel(entity.get())) continue;
        if (static_cast<const geom::PolylineEntity&>(*entity).vertices().size() < 3) continue;
        parcelles.push_back(entity.get());
    }

    std::vector<std::string> nonLeves;
    std::size_t debut = 0;
    for (std::size_t k = 0; k < surfaces.size() && k < parcelles.size(); ++k) {
        const auto& props = parcelles[k]->properties();
        PvParcelle p;
        p.designation = surfaces[k].parcelle;
        p.proprietaire = props.getString("cadastre.proprietaire");
        p.commune = props.getString("cadastre.commune");
        p.contenance = props.getString("cadastre.contenance");
        p.surface = surfaces[k].netArea;
        const std::size_t n = surfaces[k].steps.size();
        for (std::size_t i = debut; i < debut + n && i < coordonnees.size(); ++i)
            p.bornes.push_back({coordonnees[i].borne, coordonnees[i].x, coordonnees[i].y});
        for (std::size_t i = debut; i < debut + n && i < riverains.size(); ++i) {
            const auto& r = riverains[i];
            PvLimite limite{r.de, r.vers, r.riverain, r.presence};
            if (limite.presence == presenceValues().front()) limite.presence.clear();
            // Le nom saisi n'est repris que faute de voisin leve (riverainRows).
            const std::string saisi = props.getString(limiteKey(i - debut, "riverain"));
            if (!saisi.empty() && r.riverain == saisi &&
                std::find(nonLeves.begin(), nonLeves.end(), saisi) == nonLeves.end())
                nonLeves.push_back(saisi);
            p.limites.push_back(std::move(limite));
        }
        debut += n;
        pv.signataires.push_back(
            {"Propriétaire de " + p.designation,
             p.proprietaire.empty() ? std::string() : p.proprietaire});
        pv.parcelles.push_back(std::move(p));
    }
    for (const auto& nom : nonLeves) pv.signataires.push_back({"Riverain", nom});
    return pv;
}

std::string procesVerbalHtml(const ProcesVerbal& pv) {
    std::string h;
    h += "<html><head><meta charset=\"utf-8\"><style>"
         "body{font-family:sans-serif;font-size:9pt;}"
         "h1{font-size:14pt;text-align:center;margin:0;}"
         "h2{font-size:11pt;margin-top:12pt;}"
         "table{border-collapse:collapse;}"
         "td,th{border:1px solid black;padding:3px;}"
         "th{background:#e8e8e8;}"
         "</style></head><body>";
    h += "<h1>PROCÈS-VERBAL DE BORNAGE CONTRADICTOIRE</h1>";

    h += "<p>Le " + ouLigne(pv.date) + ", nous soussigné " + ouLigne(pv.geometre) + ", géomètre, agrément n° " +
         ouLigne(pv.agrement) +
         ", avons procédé, en présence des parties désignées ci-après, à la reconnaissance "
         "et au bornage contradictoire des limites de la (des) parcelle(s) suivante(s).</p>";
    h += "<p>Dossier : " + ouLigne(pv.dossier) + " &nbsp;&nbsp; Commune : " + ouLigne(pv.commune) +
         "<br>Système de coordonnées : " + ouLigne(pv.systeme) + "</p>";

    for (const auto& p : pv.parcelles) {
        h += "<br><h2>Parcelle " + htmlEscape(p.designation) + "</h2>";
        h += "<p>Propriétaire : " + ouLigne(p.proprietaire) +
             "<br>Contenance de l'acte : " + ouLigne(p.contenance) +
             " &nbsp;&nbsp; Surface calculée : " + formatDecimal(p.surface, 2) + " m²</p>";

        h += "<p><b>Bornes</b></p><table width=\"100%\" cellspacing=\"0\">"
             "<tr><th>Borne</th><th>X (m)</th><th>Y (m)</th><th>État constaté</th></tr>";
        for (const auto& b : p.bornes)
            h += "<tr><td>" + htmlEscape(b.numero) + "</td><td align=\"right\">" +
                 formatDecimal(b.x, 3) + "</td><td align=\"right\">" + formatDecimal(b.y, 3) +
                 "</td><td>" + kCases + "</td></tr>";
        h += "</table>";

        h += "<p><b>Limites et riverains</b></p><table width=\"100%\" cellspacing=\"0\">"
             "<tr><th>Côté</th><th>Riverain</th><th>Présence au bornage</th>"
             "<th>Accord sur la limite</th></tr>";
        for (const auto& l : p.limites) {
            const std::string presence =
                l.presence.empty() ? std::string("☐ présent &nbsp; ☐ absent &nbsp; ☐ représenté")
                                   : htmlEscape(l.presence);
            h += "<tr><td>" + htmlEscape(l.de) + " - " + htmlEscape(l.vers) + "</td><td>" +
                 htmlEscape(l.riverain) + "</td><td>" + presence +
                 "</td><td>☐ oui &nbsp; ☐ non</td></tr>";
        }
        h += "</table>";
    }

    // Un cadre vide : les observations s'ecrivent a la main, sur place.
    h += "<h2>Observations</h2><table width=\"100%\" cellspacing=\"0\">"
         "<tr><td><br><br><br><br><br><br></td></tr></table>";

    h += "<br><h2>Signatures</h2><table width=\"100%\" cellspacing=\"0\">"
         "<tr><th>Qualité</th><th>Nom</th><th width=\"35%\">Signature</th></tr>";
    h += "<tr><td>Géomètre</td><td>" + ouLigne(pv.geometre) + "</td><td><br><br></td></tr>";
    for (const auto& signataire : pv.signataires)
        h += "<tr><td>" + htmlEscape(signataire.qualite) + "</td><td>" + ouLigne(signataire.nom) +
             "</td><td><br><br></td></tr>";
    h += "</table>";
    h += "<p>Fait le " + ouLigne(pv.date) + ", en autant d'exemplaires que de parties.</p>";
    h += "</body></html>";
    return h;
}

} // namespace bcad::cadastre
