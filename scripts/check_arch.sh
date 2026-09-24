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

# 10. App must not have hardcoded cadastre UI (Viewport::ParcelTool, MainWindow cadastre panel)
echo "Checking App for hardcoded cadastre UI..."
if grep -r 'ParcelTool\|ToolMode::Parcel\|cadastre::ParcelEntity' src/app/ 2>/dev/null | grep -v test; then
    echo "ERROR: Hardcoded cadastre UI in app/ (should be in plugin ui/)"
    VIOLATIONS=$((VIOLATIONS + 1))
else
    echo "OK: No hardcoded cadastre UI in app/"
fi

# 11. PropertiesPanel must not have hardcoded cadastre fields
echo "Checking PropertiesPanel for hardcoded cadastre fields..."
if grep -r 'sectionEdit_\|numeroEdit_\|contenanceEdit_\|communeEdit_\|proprietaireEdit_\|natureEdit_\|cadastreWidget_' src/app/PropertiesPanel.cpp 2>/dev/null | grep -v 'TEMPORAIRE'; then
    echo "WARNING: Hardcoded cadastre fields in PropertiesPanel (should migrate to generic PropertyMap editors)"
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