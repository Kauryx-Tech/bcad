#!/bin/bash
# Architecture validation script - run in CI
# Exits with non-zero if architectural violations found

set -euo pipefail

VIOLATIONS=0

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

# 4. No Qt in core headers
echo "Checking for Qt in core headers..."
if grep -r '#include <Qt' include/bcad/core/ 2>/dev/null; then
    echo "ERROR: Qt includes in core headers"
    VIOLATIONS=$((VIOLATIONS + 1))
else
    echo "OK: No Qt in core headers"
fi

# 5. Count switch(EntityType) occurrences (should be 0, except backward compat)
echo "Counting switch(EntityType) occurrences..."
# Allow one in Database.cpp for backward compat with old SQLite files
MATCHES=$(grep -r 'switch.*EntityType' src/ --include="*.cpp" --include="*.h" 2>/dev/null | grep -v 'Database.cpp' || true)
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
    echo "OK: No switch(EntityType) found (except allowed backward compat in Database.cpp)"
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
APP_DOMAIN_HITS=$(grep -rn '"[a-z_]\+\.[a-z_]\+"' src/app/*.cpp 2>/dev/null \
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
APP_MODULE_HITS=$(grep -rn '"[^"]*bcad_[a-z0-9_]*"' src/app/ include/bcad/app/ 2>/dev/null || true)
APP_MEMBER_HITS=$(grep -rn 'PluginHandle\* *[a-z][A-Za-z]*Plugin_\|[a-z][A-Za-z]*Plugin_ *=' src/app/ include/bcad/app/ 2>/dev/null || true)
APP_TR_HITS=$(grep -rn 'tr([^)]*\(cadastr\|parcelle\|servitude\|bornage\|contenance\|section cadastr\)' src/app/ include/bcad/app/ 2>/dev/null || true)
# Les motifs ci-dessus rataient un commentaire nommant un domaine, ou une cle
# « cadastre.xxx » ecrite hors tr() : la regle d'ADR-016 porte sur le mot lui-meme,
# pas seulement sur sa forme de chaine affichee.
APP_WORD_HITS=$(grep -rni 'cadastr\|parcelle\|servitude\|bornage' src/app/ include/bcad/app/ 2>/dev/null || true)
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

# 11bis. MainWindow est repartie sur cinq unites de traduction par
# responsabilite ; la garde empeche qu'une seule ne recomble. Le seuil vise
# MainWindow* seulement : Viewport.cpp (1318 lignes) est un autre chantier et
# une regle globale echouerait des aujourd'hui.
echo "Checking MainWindow translation units stay small..."
OVERLONG=""
for f in src/app/MainWindow*.cpp; do
    LINES=$(wc -l < "$f")
    if [ "$LINES" -gt 350 ]; then OVERLONG="$OVERLONG$f ($LINES lignes)\n"; fi
done
if [ -n "$OVERLONG" ]; then
    echo "ERROR: unite de traduction MainWindow trop longue (decouper par responsabilite)"
    echo -e "$OVERLONG"
    VIOLATIONS=$((VIOLATIONS + 1))
else
    echo "OK: MainWindow reste repartie sur des unites de taille lisible"
fi

# 12. Plugin architecture: verify bcad_plugin_init exists in plugins
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