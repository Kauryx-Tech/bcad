#pragma once

// Fournisseur de styles déclaratifs (couche, tracé, texte) — point d'extension
// pour les modules métier (ADR-016/ADR-017). Un style est une donnée, pas du code :
// il ne contient aucune logique, uniquement des valeurs (couleurs, épaisseurs,
// polices, motifs de trait) que l'hôte applique sans les interpréter.
//
// Le format des données (JSON) est la source de vérité : un module déclare ses
// styles dans des fichiers installés (layers.json, plot_styles.json,
// text_styles.json) et les fournit via cette interface. L'hôte ne connaît pas
// le schéma des fichiers ; il ne fait que transmettre les valeurs au rendu.

#include "bcad/plugin/Api.h"
#include <memory>
#include <string>
#include <vector>

namespace bcad::plugin {

// Une définition de calque (identité visuelle, pas seulement nom).
struct BCAD_PLUGIN_API LayerStyle {
    std::string id;          // identifiant stable ("cadastre.parcels")
    std::string name;        // nom affiché ("Parcelles")
    int aci = 7;             // index couleur ACI (1-255, 0 = défaut)
    std::string line_type = "CONTINUOUS";
    double line_weight = 0.25; // mm
    bool visible = true;
    bool locked = false;
    bool printable = true;
};

// Un style de tracé (couleur, épaisseur, motif).
struct BCAD_PLUGIN_API PlotStyle {
    std::string id;          // identifiant stable ("cadastre.parcel")
    std::string color = "#000000"; // hex RRGGBB
    double width = 0.25;     // mm
    std::string fill = "none";   // "none" ou hex
    std::vector<double> dash;    // motif de trait (ex: [2, 1])
};

// Un style de texte (police, taille, couleur).
struct BCAD_PLUGIN_API TextStyle {
    std::string id;          // identifiant stable ("cadastre.parcel_label")
    std::string font = "Sans";   // nom de police
    double size = 2.5;         // mm
    std::string color = "#000000"; // hex
};

// Fournisseur de styles déclaratifs. Un module implémente cette interface
// pour fournir ses styles à l'hôte. L'hôte ne connaît pas le format des
// fichiers sources (JSON, XML, etc.) ; il interroge simplement le fournisseur.
class BCAD_PLUGIN_API IStyleProvider {
public:
    virtual ~IStyleProvider() = default;

    // Identifiant stable, préfixé par le plugin ("cadastre.styles").
    virtual std::string id() const = 0;

    // Libellé du lot de styles, dans la langue du module.
    virtual std::string label() const = 0;

    // Calques déclarés par le module. Vide = aucun calque spécifique.
    virtual std::vector<LayerStyle> layerStyles() const = 0;

    // Styles de tracé (trait, remplissage, motif).
    virtual std::vector<PlotStyle> plotStyles() const = 0;

    // Styles de texte (police, taille, couleur).
    virtual std::vector<TextStyle> textStyles() const = 0;
};

// Registre des fournisseurs de styles. Singleton porté par l'hôte.
class BCAD_PLUGIN_API StyleProviderRegistry {
public:
    static StyleProviderRegistry& instance();

    // Faux si l'identifiant est déjà pris.
    bool registerProvider(std::unique_ptr<IStyleProvider> provider);
    void unregisterProvider(const std::string& id);
    void clear();

    std::vector<const IStyleProvider*> providers() const;
    const IStyleProvider* find(std::string_view id) const;
    std::size_t size() const { return entries_.size(); }

    StyleProviderRegistry(const StyleProviderRegistry&) = delete;
    StyleProviderRegistry& operator=(StyleProviderRegistry&) = delete;

private:
    StyleProviderRegistry() = default;
    std::vector<std::unique_ptr<IStyleProvider>> entries_;
};

} // namespace bcad::plugin