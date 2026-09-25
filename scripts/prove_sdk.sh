#!/usr/bin/env bash
# Preuve end-to-end du SDK installable (Phase 9) et du plugin externe minimal
# (Phase 10) :
#   1. installe BCAD dans <build>/_sdk_install ;
#   2. configure et compile le projet externe examples/sdk_proof avec
#      find_package(BCAD CONFIG REQUIRED) (consommateur + plugin + chargeur) ;
#   3. execute le consommateur puis charge/decharge le plugin.
#
# Usage: prove_sdk.sh <CMAKE_COMMAND> <SOURCE_DIR> <BUILD_DIR>
set -euo pipefail

CMAKE="$1"
SOURCE_DIR="$2"
BUILD_DIR="$3"

PREFIX="${BUILD_DIR}/_sdk_install"
PROOF_BUILD="${BUILD_DIR}/_sdk_proof_build"
PROOF_SRC="${SOURCE_DIR}/examples/sdk_proof"

# Garde-fou : la suppression ci-dessous ne doit jamais porter sur autre chose
# que le repertoire tampon de cette preuve.
case "${PREFIX}" in
    */_sdk_install) ;;
    *) echo "ERREUR: PREFIX inattendu: ${PREFIX}" >&2; exit 2 ;;
esac

echo "==> 1. Installation du SDK dans ${PREFIX}"
# `cmake --install` n'efface pas ce qui n'est plus produit : un SDK installe une
# fois avec des en-tetes retires laisserait trainer les anciens. On repart d'un
# repertoire vide pour que la verification du contenu mesure les regles
# d'installation, pas les restes d'une installation precedente.
rm -rf "${PREFIX}"
"${CMAKE}" --install "${BUILD_DIR}" --prefix "${PREFIX}"

# L'installation definit la surface publique : include/bcad/ n'est installe que
# parce qu'il est public, et l'hote (src/app/) n'y a pas sa place. Un dossier
# app/ revenu ici serait une fausse API — linkable par personne, exporte quand
# meme — qu'aucun consommateur ne verrait cassée avant de l'utiliser.
if [ -e "${PREFIX}/include/bcad/app" ]; then
    echo "ERREUR: l'installation expose include/bcad/app (interface privee de l'hote)" >&2
    exit 1
fi
echo "OK: aucun en-tete prive d'hote n'est installe"

echo "==> 2. Configuration du projet externe (find_package BCAD)"
"${CMAKE}" -S "${PROOF_SRC}" -B "${PROOF_BUILD}" \
    -DCMAKE_PREFIX_PATH="${PREFIX}" \
    -DCMAKE_BUILD_TYPE=Release

echo "==> 3. Compilation du projet externe"
"${CMAKE}" --build "${PROOF_BUILD}" -j

echo "==> 4. Execution du consommateur SDK (Phase 9)"
"${PROOF_BUILD}/bin/bcad_sdk_consumer"

echo "==> 5. Charge/decharge du plugin externe (Phase 10)"
"${PROOF_BUILD}/loader/bcad_plugin_loader" \
    "${PROOF_BUILD}/plugin/libbcad_hello_plugin.so"

echo "==> SDK et plugin externe : OK"