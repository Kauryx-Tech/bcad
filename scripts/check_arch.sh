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

echo "=== Summary ==="
if [ "$VIOLATIONS" -eq 0 ]; then
    echo "All architecture checks PASSED"
    exit 0
else
    echo "Found $VIOLATIONS architectural violation(s)"
    exit 1
fi