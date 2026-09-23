# Spécification — Module métier Cadastre

> Statut : squelette en place (`examples/cadastre_proof/`), à enrichir.
> ADR concernées : ADR-005 (plugins), ADR-009 (commandes), ADR-002 (CGAL masqué)

## 1. Objectif

Gérer des parcelles cadastrales dans BCAD : création, validation géométrique, opérations métier (division/fusion), persistance, affichage.

## 2. Entité `cadastre.parcel`

| Champ | Type | Obligatoire | Description |
|-------|------|-------------|-------------|
| `vertices` | `Point2[]` | oui | ≥3 sommets, polygone fermé |
| `section` | `string` | oui | Lettre(s) cadastrale(s), ex: "A", "AB" |
| `numero` | `string` | oui | Numéro parcellaire, ex: "42" |
| `contenance` | `string` | oui | Surface déclarée, ex: "500m²" |
| `commune` | `string` | non | Code/nom commune |
| `proprietaire` | `string` | non | Nom propriétaire |
| `nature` | `enum` | non | Nature culture (terre, pré, bois...) |

**TypeId** : `cadastre.parcel` (inconnu du Core, enregistré par le plugin)

**Validation** (`isValid`) :
- ≥3 sommets distincts
- Aire > 1e-9 (non colinéaire)
- Polygone simple (pas d'auto-intersection)
- `section` et `numero` non vides

**Format sérialisé** : `x,y;x,y;...|section|numero|contenance|commune`

## 3. Tâches

### Phase A — Géométrie & validation (FAIT)

- [x] `ParcelEntity : PolylineEntity` fermée + champs métier
- [x] Validation géométrique + métier
- [x] Factory `makeParcel(string_view)` + enregistrement `cadastre.parcel`
- [x] Serializer CSV

### Phase B — Commande création (FAIT)

- [x] `cadastre.create_parcel` : `execute` ajoute au Document, `undo` retire

### Phase C — Opérations métier

| # | Tâche | Description | Dépend |
|---|-------|-------------|--------|
| C1 | `split_parcel` | Diviser une parcelle par une polyligne de coupe | B |
| C2 | `merge_parcels` | Fusionner 2+ parcelles adjacentes | B |
| C3 | `subdivide` | Lotissement : découpe régulière en N lots | C1 |
| C4 | Calcul contenance | Aire géométrique → m²/ha, comparaison déclarée | A |

### Phase D — Persistance

| # | Tâche | Description | Dépend |
|---|-------|-------------|--------|
| D1 | SQLite | Table `cadastre_parcels` + requêtes | A |
| D2 | DXF | Calque `CADASTRE` + étiquettes | A |
| D3 | Import DXF | Lecture parcelles existantes (LWPOLYLINE → ParcelEntity) | D2 |

### Phase E — Données & référentiel

| # | Tâche | Description |
|---|-------|-------------|
| E1 | Unicité section+numéro | Contrainte + message si doublon |
| E2 | Propriétaire / indivision | Champ + UI édition |
| E3 | Nature / zonage | Enum + couleur par nature |

### Phase F — UI

| # | Tâche | Description | Dépend |
|---|-------|-------------|--------|
| F1 | Outil dessin | Clic → polygone parcelle, validation en live | A |
| F2 | Panneau propriétés | Édition section/numéro/contenance/commune | A |
| F3 | Étiquettes | Numéro + surface affichés sur carte | A |
| F4 | Recherche | Par `section+numéro` → zoom sur parcelle | A |
| F5 | Détection recouvrement | Highlight zones en conflit | C1,C2 |

## 4. Chaîne complète : plan parcellaire → mise en page → impression

> Objectif : produire un plan parcellaire imprimable (comme un géomètre).

### État actuel du socle

| Étape | Statut | Notes |
|-------|--------|-------|
| Dessin géométrique | ✅ | Points, lignes, polylignes ; `cadastre.parcel` via plugin |
| Calques | ✅ | LayerManager |
| Rendu OpenGL | ✅ | GlRenderer |
| DXF lecture/écriture | ✅ | DxfReader/DxfWriter (géométrie, pas cadastre) |
| SQLite | ✅ | Sauvegarde document |
| Mise en page | ❌ | Pas de cartouche/échelle/format |
| Cotation/étiquetage | ❌ | Pas de cotations cadastrales auto |
| Impression / PDF | ❌ | Pas de QPrinter/QPrintDialog |

### Phase G — Mise en page (Layout)

| # | Tâche | Description | Dépend |
|---|-------|-------------|--------|
| G1 | Modèle de feuille | Formats A4/A3/A2/A1/A0, orientation portrait/paysage, marges | — |
| G2 | Cartouche | Bloc titre : commune, section, échelle, date, géomètre, légende | G1 |
| G3 | Échelle & carroyage | Échelles cadastrales (1:500, 1:1000, 1:2000...) + grille Lambert | G1 |
| G4 | Viewport plan | Fenêtre du dessin dans la feuille, zoom/pan indépendant | G1 |
| G5 | Tableau parcellaire | Liste parcelles (section, numéro, contenance, propriétaire) auto | A |

### Phase H — Cotation & annotation

| # | Tâche | Description | Dépend |
|---|-------|-------------|--------|
| H1 | Cotations linéaires | Distances entre bornes (côtés parcelle) | A |
| H2 | Cotations angulaires | Angles aux sommets | A |
| H3 | Étiquettes parcelle | Numéro + contenance au centre, échelle-dépendant | F3 |
| H4 | Bornes | Symboles + numérotation | A |
| H5 | Flèche Nord | Orientation | G3 |

### Phase I — Impression & export

| # | Tâche | Description | Dépend |
|---|-------|-------------|--------|
| I1 | Aperçu impression | QPrintPreviewDialog | G1 |
| I2 | Export PDF | QPrinter → PDF vectoriel (via rendu) | G1 |
| I3 | Export DXF complet | Géométrie + calques `CADASTRE`/`COTATION`/`CARTOUCHE` | D2,G2 |
| I4 | Export image | PNG/JPG haute résolution | G1 |

### Ordre d'implémentation (chaîne minimale viable)

```
G1 (feuille) → G4 (viewport) → H3 (étiquettes) → G2 (cartouche) → I2 (PDF)
     ↓
H1 (cotations) → G3 (échelle) → I1 (aperçu)
```

## 5. Critères d'acceptation

- Chaque opération métier est une `Command` annulable (`execute`/`undo`/`clone`)
- Aucune modif du Core : tout passe par `PluginRegistry` (ADR-005)
- Tests : `cadastre_external_test` couvre entité + commande + serializer + loader
- `check_arch.sh` reste vert
- Plan imprimable : DXF ou PDF généré contient parcelles + étiquettes + cartouche à l'échelle

## 6. Prochaine étape

**Chaîne minimale viable** : **G1 (modèle de feuille A3/A4) → G4 (viewport) → H3 (étiquettes)**.
Alternative : **C1 `split_parcel`** si priorité opérations métier avant mise en page.
