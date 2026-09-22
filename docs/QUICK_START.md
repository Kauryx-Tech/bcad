# Guide de démarrage rapide BCAD

> **Tu as 5 minutes ?** Ce guide te permet de compiler et lancer BCAD en moins de 5 minutes.

## Prérequis

Assure-toi d'avoir installé les dépendances :

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential cmake qt6-base-dev libqt6opengl6-dev \
    libcgal-dev libboost-dev libboost-all-dev libsqlite3-dev \
    libgl1-mesa-dev libglu1-mesa-dev
```

## Étape 1 : Cloner le projet (30 secondes)

```bash
git clone https://github.com/Kauryx-Tech/bcad.git
cd bcad
```

## Étape 2 : Configurer (1 minute)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

**Si ça échoue :** Vérifie les dépendances manquées. L'erreur indique quel package installer.

## Étape 3 : Compiler (2-3 minutes)

```bash
cmake --build build -j$(nproc)
```

## Étape 4 : Lancer les tests (30 secondes)

```bash
ctest --test-dir build --output-on-failure
```

Tu devrais voir :
```
Test project build
    Start 1: smoke_test
1/1 Test #1: smoke_test..................Passed
100% tests passed, 0 tests failed out of 1
```

## Étape 5 : Lancer l'application (30 secondes)

```bash
./build/src/app/bcad
```

**Félicitations !** 🎉 BCAD est maintenant lancé. Tu vois une fenêtre avec :
- Un ruban en haut (Home, Modify, View)
- Un canevas de dessin au centre
- Une barre de statut en bas

## Première action : dessiner une ligne

1. Clique sur l'outil **Ligne** dans le ruban (onglet Home)
2. Clique quelque part sur le canevas pour le point de départ
3. Clique ailleurs pour le point d'arrivée
4. Appuie sur **Entrée** ou **clic droit** pour terminer

**Ça y est !** Tu as dessiné ta première entité.

## Prochaines étapes

| Tu veux... | Lis... |
|------------|--------|
| Comprendre ce que tu viens de voir | [VISUAL_ARCHITECTURE.md](VISUAL_ARCHITECTURE.md) |
| Explorer le code | [WALKTHROUGH.md](WALKTHROUGH.md) |
| Contribuer | [FIRST_CONTRIBUTION.md](FIRST_CONTRIBUTION.md) |
| Résoudre un problème | [TROUBLESHOOTING.md](TROUBLESHOOTING.md) |

## Commandes clavier essentielles

| Raccourci | Action |
|-----------|--------|
| `F` | Zoom sur l'ensemble |
| `F3` | Activer/désactiver l'accrochage aux objets |
| `F7` | Afficher/masquer la grille |
| `F9` | Activer/désactiver l'accrochage à la grille |
| `Ctrl+Z` | Annuler (Undo) |
| `Ctrl+Y` | Rétablir (Redo) |
| `Molette` | Zoom |
| `Clic molette + glisser` | Pan |

## Structure des dossiers clés

```
bcad/
├── include/bcad/       # API publique (SDK)
│   ├── geometry/        # Types géométriques
│   ├── core/            # Document
│   └── app/             # Interface Qt
├── src/                 # Implémentation
│   ├── geometry/
│   ├── core/
│   └── app/
└── tests/
    └── smoke_test.cpp   # Tests de base
```

## Besoin d'aide ?

- **Problème de compilation ?** → [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
- **Questions sur l'architecture ?** → [ARCHITECTURE.md](ARCHITECTURE.md)
- **Questions sur la contribution ?** → [CONTRIBUTOR_GUIDE.md](CONTRIBUTOR_GUIDE.md)

---

*Pour un guide plus détaillé, voir [GETTING_STARTED.md](GETTING_STARTED.md).*