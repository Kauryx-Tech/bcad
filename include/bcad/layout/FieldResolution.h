#pragma once

// La resolution : le pont entre le gabarit d'un module et ce que le document
// porte (ADR-017 decision 7, troisieme et quatrieme pieces).
//
// L'hote repond a une demande de valeur sous une cle opaque et ne sait rien de
// ce que la cle veut dire. Il ne decide pas non plus que faire quand plusieurs
// valeurs differentes se presentent : il les rend distinctes et comptees, et la
// regle — unanimite, defaut, erreur — reste a celui qui la connait, le module.
// C'est ce qui empeche une regle cadastrale de glisser dans l'interface installee.
//
// Rien ici ne recoit un `core::Document` : cet en-tete ne regarde pas vers le
// core, et n'en a pas besoin.

#include "bcad/layout/Furniture.h"
#include "bcad/layout/FurnitureTemplate.h"
#include "bcad/properties/PropertyMap.h"
#include "bcad/validation/Diagnostics.h"

#include <string>
#include <vector>

namespace bcad::layout {

// Les deux lieux ou l'hote sait chercher. `entites` est la liste deja filtree par
// l'appelant de ce qu'il juge pertinent : l'hote ne connait aucun type d'entite,
// il ne peut donc rien trier, ni rien exiger.
struct FieldScope {
    const properties::PropertyMap* dossier = nullptr;
    std::vector<const properties::PropertyMap*> entites;
};

// Ce qu'une cle a donne. Une entree par valeur DISTINCTE rencontree, les vides
// écartées : zero = rien trouve, une = sans ambiguite, plusieurs = a l'appelant
// de trancher. L'hote n'applique aucune precedence.
struct FieldAnswer {
    std::vector<TypedValue> values;
    bool fromDossier = false;

    bool isEmpty() const { return values.empty(); }
    bool isUnambiguous() const { return values.size() == 1; }
};

inline bool sameTypedValue(const TypedValue& a, const TypedValue& b) {
    return a.type == b.type && a.value == b.value && a.enumValues == b.enumValues;
}

inline TypedValue typedValueOf(const properties::Property& prop) {
    TypedValue out;
    out.type = prop.type();
    out.value = prop.value();
    out.enumValues = prop.enumValues();
    return out;
}

inline bool isBlank(const TypedValue& valeur) {
    const auto* texte = std::get_if<std::string>(&valeur.value);
    return texte && texte->empty();
}

inline FieldAnswer resolveField(const std::string& key, const FieldScope& scope) {
    FieldAnswer answer;
    if (key.empty()) return answer;

    if (scope.dossier) {
        if (const properties::Property* prop = scope.dossier->find(key)) {
            answer.fromDossier = true;
            answer.values.push_back(typedValueOf(*prop));
            return answer;
        }
    }

    for (const properties::PropertyMap* props : scope.entites) {
        if (!props) continue;
        const properties::Property* prop = props->find(key);
        if (!prop) continue;
        TypedValue valeur = typedValueOf(*prop);
        if (isBlank(valeur)) continue;
        bool deja_rencontree = false;
        for (const TypedValue& connue : answer.values) {
            if (sameTypedValue(connue, valeur)) {
                deja_rencontree = true;
                break;
            }
        }
        if (!deja_rencontree) answer.values.push_back(std::move(valeur));
    }
    return answer;
}

// Les champs d'un meuble, resolus contre un gabarit. Ce que le gabarit attend et
// ne trouve pas ressort sans valeur ET avec un diagnostic nommant la cle : la
// valeur manquante est un constat, jamais une disparition silencieuse. Ce que le
// meuble porte et que le gabarit ne nomme pas est conserve, et le dit aussi.
//
// Rien ne leve ici : la validite d'une feuille est une validation (decision 5).
inline std::vector<Field> resolveFields(const Furniture& meuble,
                                        const FurnitureTemplate& gabarit,
                                        const FieldScope& scope,
                                        std::vector<validation::Diagnostic>& diagnostics) {
    std::vector<Field> resolves;
    for (const TemplateField& attendu : gabarit.fields) {
        Field champ;
        champ.role = attendu.role;
        champ.label = attendu.label;
        champ.key = attendu.key;
        champ.format = attendu.format;
        champ.slot = attendu.slot;

        if (attendu.key.empty()) {
            champ.value.value = attendu.literal;   // un litteral porte sa valeur, il n'a rien a resoudre
            resolves.push_back(std::move(champ));
            continue;
        }
        const FieldAnswer reponse = resolveField(attendu.key, scope);
        if (reponse.isUnambiguous()) {
            champ.value = reponse.values.front();
        } else if (reponse.values.size() > 1) {
            diagnostics.push_back({validation::Severity::Warning,
                                   "champ « " + attendu.label + " » : " +
                                       std::to_string(reponse.values.size()) +
                                       " valeurs distinctes sous la cle « " + attendu.key +
                                       " » — a l'appelant de trancher",
                                   {}});
        } else {
            diagnostics.push_back({validation::Severity::Warning,
                                   "champ « " + attendu.label +
                                       " » : aucune valeur sous la cle « " + attendu.key + " »",
                                   {}});
        }
        resolves.push_back(std::move(champ));
    }

    for (const Field& porte : meuble.fields()) {
        if (porte.key.empty()) continue;
        bool nomme = false;
        for (const TemplateField& attendu : gabarit.fields) {
            if (attendu.key == porte.key) {
                nomme = true;
                break;
            }
        }
        if (nomme) continue;
        diagnostics.push_back({validation::Severity::Info,
                               "meuble « " + meuble.nature() + " » : champ « " + porte.label +
                                   " » conserve sans etre rendu, absent du gabarit « " + gabarit.id +
                                   " »",
                               {}});
        resolves.push_back(porte);
    }
    return resolves;
}

} // namespace bcad::layout
