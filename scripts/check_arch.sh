#!/bin/bash
# Architecture validation script - run in CI
# Exits with non-zero if architectural violations found

set -euo pipefail

VIOLATIONS=0

# Un grep sur un chemin absent ne trouve rien et « reussit » : les gardes
# ci-dessous mesureraient alors le vide sans le dire. Les racines scannees
# doivent donc exister avant toute verification.
for dir in include/bcad src tests; do
    if [ ! -d "$dir" ]; then
        echo "ERROR: repertoire attendu absent: $dir (garde architecturale invalide)"
        exit 1
    fi
done

echo "=== Architecture Checks ==="

# 1. Core must not include render/
echo "Checking Core -> Render dependency..."
if grep -r '#include.*render/' include/bcad/core/ src/core/ 2>/dev/null; then
    echo "ERROR: Core includes render/ headers"
    VIOLATIONS=$((VIOLATIONS + 1))
else
    echo "OK: No render/ includes in core"
fi

# 2. Core must not link render (check CMakeLists.txt)
echo "Checking Core CMakeLists.txt for render dependency..."
if grep -q 'bcad_render' src/core/CMakeLists.txt 2>/dev/null; then
    echo "ERROR: Core CMakeLists.txt links bcad_render"
    VIOLATIONS=$((VIOLATIONS + 1))
else
    echo "OK: Core does not link render"
fi

# 3. Public headers must not expose CGAL types (not just comments)
echo "Checking for CGAL types in public headers..."
# Look for actual CGAL type usage (not just comments)
# Exclude legacy_cgal.h which is explicitly for backward compatibility
if grep -r 'CGAL::' include/bcad/geometry/ --include="*.h" | grep -v 'detail/' | grep -v 'legacy_cgal.h' | grep -v '^\s*//' | grep -v 'implémentation basée sur' 2>/dev/null; then
    echo "ERROR: CGAL types exposed in public geometry headers"
    VIOLATIONS=$((VIOLATIONS + 1))
else
    echo "OK: No CGAL types in public headers"
fi

# 4. No Qt in public headers
# La regle portait sur include/bcad/core/ seulement, parce que include/bcad/app/
# (l'interface Qt de l'hote) etait installee avec le reste. Elle est desormais
# vide de sens comme exception : src/app/ est prive, tout ce qui reste sous
# include/bcad/ est une API publique et ne doit donc trainer aucun type Qt.
echo "Checking for Qt in public headers..."
if grep -rn '#include <Qt' include/bcad/ 2>/dev/null; then
    echo "ERROR: Qt includes in public headers (ADR-009)"
    VIOLATIONS=$((VIOLATIONS + 1))
else
    echo "OK: No Qt in public headers"
fi

# 5. Count switch(EntityType) occurrences (should be 0)
echo "Counting switch(EntityType) occurrences..."
# L'exception « un switch toléré dans Database.cpp pour la compatibilité SQLite »
# n'a plus d'objet depuis que la lecture v1 passe par les identifiants de type :
# la garder aurait rendu cette mesure aveugle à Database.cpp.
MATCHES=$(grep -r 'switch.*EntityType' src/ --include="*.cpp" --include="*.h" 2>/dev/null || true)
if [ -z "$MATCHES" ]; then
    COUNT=0
else
    COUNT=$(echo "$MATCHES" | grep -c '^')
fi
if [ "$COUNT" -gt 0 ]; then
    echo "ERROR: Found $COUNT switch(EntityType) occurrences (target: 0)"
    echo "$MATCHES"
    VIOLATIONS=$((VIOLATIONS + 1))
else
    echo "OK: No switch(EntityType) found"
fi

# 6. Count EntityType enum usage (should decrease over time)
ET_COUNT=$(grep -r 'EntityType::' src/ --include="*.cpp" --include="*.h" 2>/dev/null || true | grep -v 'type()' || true | grep -v 'TypeId' || true | wc -l)
echo "EntityType:: usage count (excl. type()/TypeId): $ET_COUNT"

# 7. ADR-003: Core must not know cadastre TypeIds or include cadastre headers
echo "Checking ADR-003: Core must not expose cadastre TypeIds..."
# Only flag TypeId declarations in public headers (inline constexpr TypeId TypeId_X{"cadastre.*"})
# Allow string literal comparisons in .cpp for backward compat with plugin detection
if grep -r 'inline.*TypeId.*cadastre\.' include/bcad/ 2>/dev/null | grep -v 'test' | grep -v '\.md'; then
    echo "ERROR: Cadastre TypeId declarations in public Core headers (ADR-003 violation)"
    VIOLATIONS=$((VIOLATIONS + 1))
else
    echo "OK: No cadastre TypeId declarations in public Core headers"
fi

# 8. Core must not include cadastre headers
echo "Checking Core does not include cadastre headers..."
if grep -r '#include.*cadastre/' include/bcad/core/ include/bcad/geometry/ include/bcad/layers/ include/bcad/index/ include/bcad/io/ include/bcad/render/ include/bcad/registry/ include/bcad/serialization/ include/bcad/commands/ include/bcad/events/ include/bcad/plugin/ include/bcad/properties/ src/core/ src/geometry/ src/layers/ src/index/ src/io/ src/render/ src/registry/ src/serialization/ src/commands/ src/events/ src/plugin/ src/properties/ 2>/dev/null; then
    echo "ERROR: Core includes cadastre/ headers (ADR-003 violation)"
    VIOLATIONS=$((VIOLATIONS + 1))
else
    echo "OK: Core does not include cadastre headers"
fi

# 9. Core CMakeLists must not link bcad_cadastre (should be plugin)
echo "Checking Core CMakeLists does not link bcad_cadastre..."
if grep -q 'bcad_cadastre' src/core/CMakeLists.txt 2>/dev/null; then
    echo "ERROR: Core links bcad_cadastre (should be plugin, ADR-005)"
    VIOLATIONS=$((VIOLATIONS + 1))
else
    echo "OK: Core does not link bcad_cadastre"
fi

# 10. App must not contain domain literals: UI metier = workbench declare par
# le plugin (ADR-003/005/016). Verifie les patterns GENERIQUES, pas seulement
# le cas cadastral : un nouveau domaine ne doit pas pouvoir se faire un menu en dur.
echo "Checking App for hardcoded domain UI..."
APP_DOMAIN_HITS=$(grep -rn --include=*.cpp --include=*.h '"[a-z_]\+\.[a-z_]\+"' src/app/ 2>/dev/null \
    | grep -v 'QCoreApplication\|\.json\|\.bcad\|\.dxf\|\.pdf' \
    | grep -i 'cadastre\|arch\.\|topo\.\|network\.\|parcel' || true)
if grep -r 'ParcelTool\|ToolMode::Parcel' src/app/ 2>/dev/null | grep -v test || [ -n "$APP_DOMAIN_HITS" ]; then
    echo "ERROR: Hardcoded domain UI in app/ (should be declared by a plugin workbench)"
    [ -n "$APP_DOMAIN_HITS" ] && echo "$APP_DOMAIN_HITS"
    VIOLATIONS=$((VIOLATIONS + 1))
else
    echo "OK: No hardcoded domain UI in app/"
fi

# 10bis. App must not name a plugin module nor a domain in its UI literals
# (ADR-016 principe 4). La garde 10 ne prenait que les littéraux "xxx.yyy" :
# elle laissait passer un nom de module (`bcad_cadastre_plugin`) et les
# libellés métier en français.
echo "Checking App for plugin module names and domain literals..."
APP_MODULE_HITS=$(grep -rn --include=*.cpp --include=*.h '"[^"]*bcad_[a-z0-9_]*"' src/app/ 2>/dev/null || true)
APP_MEMBER_HITS=$(grep -rn --include=*.cpp --include=*.h 'PluginHandle\* *[a-z][A-Za-z]*Plugin_\|[a-z][A-Za-z]*Plugin_ *=' src/app/ 2>/dev/null || true)
APP_TR_HITS=$(grep -rn --include=*.cpp --include=*.h 'tr([^)]*\(cadastr\|parcelle\|servitude\|bornage\|contenance\|section cadastr\)' src/app/ 2>/dev/null || true)
# Les motifs ci-dessus rataient un commentaire nommant un domaine, ou une cle
# « cadastre.xxx » ecrite hors tr() : la regle d'ADR-016 porte sur le mot lui-meme,
# pas seulement sur sa forme de chaine affichee.
APP_WORD_HITS=$(grep -rni --include=*.cpp --include=*.h 'cadastr\|parcelle\|servitude\|bornage' src/app/ 2>/dev/null || true)
if [ -n "$APP_MODULE_HITS" ] || [ -n "$APP_MEMBER_HITS" ] || [ -n "$APP_TR_HITS" ] || [ -n "$APP_WORD_HITS" ]; then
    echo "ERROR: src/app nomme un module plugin ou un domaine (l'hote decouvre ses modules et construit son UI depuis les workbenches)"
    [ -n "$APP_MODULE_HITS" ] && echo "$APP_MODULE_HITS"
    [ -n "$APP_MEMBER_HITS" ] && echo "$APP_MEMBER_HITS"
    [ -n "$APP_TR_HITS" ] && echo "$APP_TR_HITS"
    [ -n "$APP_WORD_HITS" ] && echo "$APP_WORD_HITS"
    VIOLATIONS=$((VIOLATIONS + 1))
else
    echo "OK: No plugin module name nor domain literal in app/"
fi

# 11. PropertiesPanel must remain generic (no hardcoded cadastre fields).
# Bloquant depuis que le panneau ne depend plus d'aucun domaine : un WARNING
# laissait la regression passer.
echo "Checking PropertiesPanel for hardcoded domain fields..."
if grep -rq 'sectionEdit_\|numeroEdit_\|contenanceEdit_\|communeEdit_\|proprietaireEdit_\|natureEdit_\|cadastreWidget_' src/app/PropertiesPanel.cpp 2>/dev/null; then
    echo "ERROR: champs metiers codés en dur dans PropertiesPanel (doit rester generique)"
    VIOLATIONS=$((VIOLATIONS + 1))
else
    echo "OK: PropertiesPanel reste generique"
fi

# 11bis. MainWindow et Viewport sont reparties sur plusieurs unites de
# traduction par responsabilite ; la garde empeche qu'une seule ne recomble.
# Le seuil ne vise que ces deux classes : LayerPanel.cpp (422 lignes) est un
# autre chantier, et une regle globale sur src/app/ echouerait des aujourd'hui.
echo "Checking split class translation units stay small..."
OVERLONG=""
for f in src/app/MainWindow*.cpp src/app/Viewport*.cpp; do
    LINES=$(wc -l < "$f")
    if [ "$LINES" -gt 350 ]; then OVERLONG="$OVERLONG$f ($LINES lignes)\n"; fi
done
if [ -n "$OVERLONG" ]; then
    echo "ERROR: unite de traduction trop longue (decouper par responsabilite)"
    echo -e "$OVERLONG"
    VIOLATIONS=$((VIOLATIONS + 1))
else
    echo "OK: MainWindow et Viewport restent reparties sur des unites de taille lisible"
fi

# 12. src/io must stay free of business modules (ADR-016, ADR-003/004/005).
# Deux niveaux, parce qu'ils ne verrouillent pas la meme chose :
#   12a. les dépendances : src/io et include/bcad/io ne peuvent inclure ni
#        l'en-tête d'un module, ni un en-tête privé de plugin ;
#   12b. les identifiants de contrat : un littéral de clé, un nom de type ou
#        une table d'un domaine prouve que l'écrivain a pris une décision métier.
# La deuxième liste reste courte volontairement : une énumération de mots
# français produirait des faux positifs sur les commentaires et finirait ignorée.
# Une ligne marquée NOLINT(arch-legacy-v1) est exemptée : c'est le format v1
# dépassé qu'elle nomme, pas un module.
echo "Checking IO for business module includes..."
IO_INCLUDE_HITS=$(grep -rn --include=*.cpp --include=*.h \
    -E '#[[:space:]]*include.*(plugins/|bcad/(cadastre|topography|network|architecture)/)' \
    src/io/ include/bcad/io/ 2>/dev/null | grep -v 'NOLINT(arch-legacy-v1)' || true)
if [ -n "$IO_INCLUDE_HITS" ]; then
    echo "ERROR: src/io ou include/bcad/io inclut un module metier (ADR-003/005)"
    echo "$IO_INCLUDE_HITS"
    VIOLATIONS=$((VIOLATIONS + 1))
else
    echo "OK: No business module header included by io"
fi

echo "Checking IO for business contract identifiers..."
IO_IDENT_HITS=$(grep -rn --include=*.cpp --include=*.h \
    -E 'cadastre\.parcel|CADASTRE_|cadastre\.(section|numero|contenance|commune|proprietaire|nature)|isCadastreParcel|ParcelEntity|kCadastre' \
    src/io/ include/bcad/io/ 2>/dev/null | grep -v 'NOLINT(arch-legacy-v1)' || true)
if [ -n "$IO_IDENT_HITS" ]; then
    echo "ERROR: src/io nomme un contrat metier (l'ecrivain et le chargeur doivent rester generiques, ADR-016)"
    echo "$IO_IDENT_HITS"
    VIOLATIONS=$((VIOLATIONS + 1))
else
    echo "OK: No business contract identifier in io"
fi

# 14. Le modele de mise en page ne regarde pas vers le core (ADR-017 decision 1).
# `Document` tient des feuilles : les en-tetes du modele sont donc vus par
# `bcad_core`, alors que `bcad_layout` — le peintre — lie `bcad_core`. Le cycle
# n'existe que si un en-tete de `include/bcad/layout/` fait le chemin en sens
# inverse. Cette regle est ce qui l'empeche mecaniquement, et non la bonne
# volonte de celui qui ecrit l'en-tete suivant.
echo "Checking layout model headers for core dependencies..."
LAYOUT_CORE_HITS=$(grep -rn --include=*.h -E '#[[:space:]]*include.*bcad/core/' \
    include/bcad/layout/ 2>/dev/null || true)
if [ -n "$LAYOUT_CORE_HITS" ]; then
    echo "ERROR: un en-tete de include/bcad/layout/ inclut bcad/core/ — Document ne peut pas porter un objet dont le modele depend de lui (ADR-017)"
    echo "$LAYOUT_CORE_HITS"
    VIOLATIONS=$((VIOLATIONS + 1))
else
    echo "OK: le modele de mise en page ignore le core"
fi

# 15. Les six en-tetes du modele sont des donnees, sans Qt et sans rendu
# (ADR-001, ADR-009). La regle 4 interdit deja `#include <Qt...>` dans tout
# `include/bcad/` ; celle-ci interdit en plus de tirer le peintre, et ne
# s'applique qu'a la liste blanche du modele — pas a `PdfExport.h`, qui est le
# peintre et qui a le droit de nommer QPainter.
echo "Checking layout model headers for painter dependencies..."
LAYOUT_MODEL_FILES="include/bcad/layout/GeometryMm.h include/bcad/layout/Sheet.h include/bcad/layout/Viewport.h include/bcad/layout/Furniture.h include/bcad/layout/FurnitureTemplate.h include/bcad/layout/FieldResolution.h"
LAYOUT_RENDER_HITS=$(grep -n --include=*.h -E '#[[:space:]]*include.*(bcad/render/|bcad/layout/PdfExport|bcad/layout/Composition|Q[A-Z])' \
    $LAYOUT_MODEL_FILES 2>/dev/null || true)
if [ -n "$LAYOUT_RENDER_HITS" ]; then
    echo "ERROR: un en-tete du modele de feuille depend du rendu — la donnee du document ne peut pas peindre (ADR-001)"
    echo "$LAYOUT_RENDER_HITS"
    VIOLATIONS=$((VIOLATIONS + 1))
else
    echo "OK: le modele de feuille reste de la donnee, sans rendu"
fi

# 16. Le peintre de mise en page ne nomme aucun domaine (ADR-017 consequence,
#    point d'arrivee de la decision). Meme forme que la garde 12 sur src/io :
#   16a. les dépendances : src/layout et include/bcad/layout ne peuvent inclure
#        ni l'en-tête d'un module, ni un en-tête privé de plugin ;
#   16b. les identifiants de contrat : un littéral de clé, un nom de type ou un
#        nom de module prouve que le peintre a pris une décision métier — les
#        libellés qu'il peint viennent des gabarits, jamais de son code.
echo "Checking layout for business module includes..."
LAYOUT_INCLUDE_HITS=$(grep -rn --include=*.cpp --include=*.h \
    -E '#[[:space:]]*include.*(plugins/|bcad/(cadastre|topography|network|architecture)/)' \
    src/layout/ include/bcad/layout/ 2>/dev/null || true)
if [ -n "$LAYOUT_INCLUDE_HITS" ]; then
    echo "ERROR: src/layout ou include/bcad/layout inclut un module metier (ADR-016/017)"
    echo "$LAYOUT_INCLUDE_HITS"
    VIOLATIONS=$((VIOLATIONS + 1))
else
    echo "OK: No business module header included by layout"
fi

echo "Checking layout for business contract identifiers..."
LAYOUT_IDENT_HITS=$(grep -rn --include=*.cpp --include=*.h \
    -E 'cadastre\.parcel|CADASTRE_|cadastre\.(section|numero|contenance|commune|proprietaire|nature|dossier)|isCadastreParcel|ParcelEntity|kCadastre' \
    src/layout/ include/bcad/layout/ 2>/dev/null || true)
if [ -n "$LAYOUT_IDENT_HITS" ]; then
    echo "ERROR: src/layout nomme un contrat metier (le peintre doit rester generique, ADR-016/017)"
    echo "$LAYOUT_IDENT_HITS"
    VIOLATIONS=$((VIOLATIONS + 1))
else
    echo "OK: No business contract identifier in layout"
fi

# 13. Plugin architecture: verify bcad_plugin_init exists in plugins
echo "Checking plugin entry points..."
# (Informational - actual plugin loading tested in cadastre_external_test)

echo "=== Summary ==="
if [ "$VIOLATIONS" -eq 0 ]; then
    echo "All architecture checks PASSED"
    exit 0
else
    echo "Found $VIOLATIONS architectural violation(s)"
    exit 1
fi