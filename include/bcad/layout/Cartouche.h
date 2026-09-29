#pragma once

#include <initializer_list>
#include <stdexcept>
#include <string>
#include <optional>
#include <vector>
#include <utility>

namespace bcad::layout {

struct Cartouche {
    // Identification projet
    std::string projectName;      // Nom du projet
    std::string projectNumber;    // Numéro de projet
    std::string phase;            // Phase (EXE, APD, PRO, etc.)
    std::string lotNumber;        // Numéro de lot
    
    // Identification parcelle
    std::string commune;
    std::string section;          // Section cadastrale
    std::string numero;           // Numéro de parcelle
    std::string contenance;       // Contenance (ex: "500 m²", "1.2 ha")
    std::string communeCode;      // Code INSEE commune
    
    // Informations techniques
    std::string echelle;          // ex: "1:500"
    std::string date;             // ex: "23/09/2026"
    std::string geometre;         // Géomètre-expert
    std::string dossier;          // Numéro de dossier
    std::string proprietaire;     // Propriétaire
    std::string nature;           // Nature de culture / zonage
    std::string referencePlan;    // Référence du plan
    std::string revision;         // Révision
    
    // Métadonnées
    std::string auteur;           // Auteur du plan
    std::string verifiePar;       // Vérifié par
    std::string approuvePar;      // Approuvé par
    std::string dateCreation;     // Date création
    std::string dateModification; // Date modification

    // Signatures
    struct Signature {
        std::string nom;           // Nom du signataire
        std::string role;          // Rôle (ex: "Géomètre", "Propriétaire", "Maire")
        std::string date;          // Date de signature
        std::string signaturePath; // Chemin vers image de signature (optionnel)
    };

    std::vector<Signature> signatures; // Table des signatures

    // Apparence
    double heightMm = 25.0;       // hauteur du cartouche en bas de feuille
    double borderWidth = 0.5;     // épaisseur bordure
    std::string fontName = "Standard";
    double fontSizeMm = 2.5;
    
    // Un cartouche a quelque chose à montrer dès qu'un champ d'attribut est
    // renseigné : réserver sa bande et la peindre ne doit pas dépendre de
    // *lequel*. Auparavant seuls commune, section et projectName comptaient, si
    // bien qu'un cartouche rempli de dix attributs du dossier n'était ni réservé
    // ni peint — disparaissait-il de la feuille sans un mot.
    //
    // `echelle` est tenu hors de la liste : la composition le remplit elle-même
    // (applySuggestedScale), donc le compter ferait apparaître un cartouche sur
    // une feuille où l'opérateur n'a saisi aucun attribut. Les trois champs
    // d'apparence ont toujours une valeur par défaut, et ne sont pas des
    // attributs.
    bool isValid() const {
        const std::initializer_list<const std::string*> champs = {
            &projectName, &projectNumber, &phase, &lotNumber,
            &commune, &section, &numero, &contenance, &communeCode,
            &date, &geometre, &dossier, &proprietaire,
            &nature, &referencePlan, &revision, &auteur,
            &verifiePar, &approuvePar, &dateCreation, &dateModification};
        for (const std::string* champ : champs)
            if (!champ->empty()) return true;
        // Une signature est un contenu comme un autre : un cartouche qui ne
        // porte qu'un tableau de signatures a quelque chose à montrer, et doit
        // donc être réservé et peint comme les autres.
        if (!signatures.empty()) return true;
        return false;
    }
    
std::string title() const {
        std::string t;
        if (!projectName.empty()) t += projectName;
        if (!commune.empty()) {
            if (!t.empty()) t += " - ";
            t += commune;
        }
        if (!section.empty()) t += (t.empty() ? "" : " - ") + std::string("Section ") + section;
        if (!echelle.empty()) t += "  (" + echelle + ")";
        return t;
    }

    // Sérialisation des signatures en JSON simple
    static std::string serializeSignatures(const std::vector<Signature>& signatures);
    static std::vector<Signature> deserializeSignatures(const std::string& json);
    
    // Champs standardisés pour échange DXF/EDIGEO
    std::vector<std::pair<std::string, std::string>> toKeyValuePairs() const {
        std::vector<std::pair<std::string, std::string>> v;
        auto add = [&v](const std::string& key, const std::string& val) {
            if (!val.empty()) v.emplace_back(key, val);
        };
        add("PROJECT_NAME", projectName);
        add("PROJECT_NUMBER", projectNumber);
        add("PHASE", phase);
        add("LOT_NUMBER", lotNumber);
        add("COMMUNE", commune);
        add("SECTION", section);
        add("NUMERO", numero);
        add("CONTENANCE", contenance);
        add("COMMUNE_CODE", communeCode);
        add("ECHELLE", echelle);
        add("DATE", date);
        add("GEOMETRE", geometre);
        add("DOSSIER", dossier);
        add("PROPRIETAIRE", proprietaire);
        add("NATURE", nature);
        add("REFERENCE_PLAN", referencePlan);
        add("REVISION", revision);
        add("AUTEUR", auteur);
        add("VERIFIE_PAR", verifiePar);
        add("APPROUVE_PAR", approuvePar);
        add("DATE_CREATION", dateCreation);
        add("DATE_MODIFICATION", dateModification);
        add("SIGNATURES", serializeSignatures(signatures));
        add("ECHELLE_NUM", echelle);
        add("BORDER_WIDTH", std::to_string(borderWidth));
        add("FONT_NAME", fontName);
        add("FONT_SIZE_MM", std::to_string(fontSizeMm));
        return v;
    }
    
private:
    // Un fichier venía de l'extérieur : une valeur illisible se remplace par le
    // defaut, elle ne doit pas faire echouer la lecture entiere.
    static double parseDouble(const std::string& text, double fallback) {
        try {
            std::size_t lu = 0;
            const double value = std::stod(text, &lu);
            return lu == text.size() ? value : fallback;
        } catch (const std::exception&) {
            return fallback;
        }
    }

public:
    static Cartouche fromKeyValuePairs(const std::vector<std::pair<std::string, std::string>>& pairs) {
        Cartouche c;
        for (const auto& [k, v] : pairs) {
            if (k == "PROJECT_NAME") c.projectName = v;
            else if (k == "PROJECT_NUMBER") c.projectNumber = v;
            else if (k == "PHASE") c.phase = v;
            else if (k == "LOT_NUMBER") c.lotNumber = v;
            else if (k == "COMMUNE") c.commune = v;
            else if (k == "SECTION") c.section = v;
            else if (k == "NUMERO") c.numero = v;
            else if (k == "CONTENANCE") c.contenance = v;
            else if (k == "COMMUNE_CODE") c.communeCode = v;
            else if (k == "ECHELLE") c.echelle = v;
            else if (k == "DATE") c.date = v;
            else if (k == "GEOMETRE") c.geometre = v;
            else if (k == "DOSSIER") c.dossier = v;
            else if (k == "PROPRIETAIRE") c.proprietaire = v;
            else if (k == "NATURE") c.nature = v;
            else if (k == "REFERENCE_PLAN") c.referencePlan = v;
            else if (k == "REVISION") c.revision = v;
            else if (k == "AUTEUR") c.auteur = v;
            else if (k == "VERIFIE_PAR") c.verifiePar = v;
            else if (k == "APPROUVE_PAR") c.approuvePar = v;
            else if (k == "DATE_CREATION") c.dateCreation = v;
            else if (k == "DATE_MODIFICATION") c.dateModification = v;
            else if (k == "SIGNATURES") c.signatures = deserializeSignatures(v);
            else if (k == "ECHELLE_NUM") c.echelle = v;
            else if (k == "BORDER_WIDTH") c.borderWidth = parseDouble(v, c.borderWidth);
            else if (k == "FONT_NAME") c.fontName = v;
            else if (k == "FONT_SIZE_MM") c.fontSizeMm = parseDouble(v, c.fontSizeMm);
        }
        return c;
    }
};

} // namespace bcad::layout