#!/usr/bin/env bash
# Preuve end-to-end du module métier cadastre (Phase 13) :
#   1. installe BCAD dans <build>/_sdk_install ;
#   2. configure et compile le projet externe examples/cadastre_proof avec
#      find_package(BCAD CONFIG REQUIRED) (consommateur + plugin + chargeur) ;
#   3. execute le consommateur puis charge/décharge le plugin.
#
# Usage: prove_cadastre.sh <CMAKE_COMMAND> <SOURCE_DIR> <BUILD_DIR>
set -euo pipefail

CMAKE="$1"
SOURCE_DIR="$2"
BUILD_DIR="$3"

PREFIX="${BUILD_DIR}/_sdk_install"
PROOF_BUILD="${BUILD_DIR}/_cadastre_proof_build"
PROOF_SRC="${SOURCE_DIR}/examples/cadastre_proof"

echo "==> 1. Installation du SDK dans ${PREFIX}"
"${CMAKE}" --install "${BUILD_DIR}" --prefix "${PREFIX}"

echo "==> 2. Configuration du projet externe cadastre (find_package BCAD)"
"${CMAKE}" -S "${PROOF_SRC}" -B "${PROOF_BUILD}" \
    -DCMAKE_PREFIX_PATH="${PREFIX}" \
    -DCMAKE_BUILD_TYPE=Release

echo "==> 3. Compilation du projet externe cadastre"
"${CMAKE}" --build "${PROOF_BUILD}" -j

echo "==> 4. Execution du consommateur SDK (Phase 9)"
"${PROOF_BUILD}/bin/bcad_cadastre_consumer"

echo "==> 5. Charge/décharge du module métier cadastre (Phase 10)"
"${PROOF_BUILD}/loader/bcad_cadastre_loader" \
    "${PROOF_BUILD}/plugin/libbcad_cadastre_plugin.so"

echo "==> Module métier cadastre : OK"