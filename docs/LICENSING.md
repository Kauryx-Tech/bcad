# Analyse des licences BCAD

> **NE MODIFIE PAS LA LICENCE ACTUELLE.** Ce document analyse les options sans affirmer de compatibilité juridique sans vérification.

## 1. Licence actuelle

BCAD est sous **GPL-3.0-or-later**. Justification : CGAL utilise GPL pour `Boolean_set_operations_2` et `Triangulation_2`. Voir `LICENSE`.

## 2. Options

| Option | Copyleft | Plugins propriétaires | Compatibilité CGAL |
|--------|----------|---------------------|-------------------|
| GPL-3.0 | Fort | Oui (liaison dynamique) | ✓ |
| LGPL-3.0 | Faible | Oui | Partielle |
| MIT/BSD | Aucun | Oui | ✗ (CGAL non permissive) |
| Propriétaire | Aucun | Oui | ✗ |
| Dual (GPL + commerciale) | Variable | Oui | ✓ (si GPL) |

**Choix actuel :** GPL-3.0-or-later. Plugins propriétaires permis via liaison dynamique.

## 3. Dépendances

| Dépendance | Licence | Impact |
|------------|---------|--------|
| CGAL | GPL/LGPL mixte | Force GPL sur `geometry/` |
| Qt6 | LGPL v3 | Compatible GPL via liaison dynamique |
| SQLite3 | Domaine public | Aucune |
| OpenGL | SGI License | Aucune |

> Note : Boost n'est pas une dépendance directe de BCAD — il n'arrive que transitivement via CGAL.

## 4. Implications pour les plugins

Avec Core GPL :
- Plugins GPL : ✓
- Plugins propriétaires : ✓ (liaison dynamique)

## 5. Points à valider par un juriste

- [ ] Compatibilité exacte de chaque version de CGAL avec GPL
- [ ] Clauses de redistribution spécifiques
- [ ] Statut des contributions (CLA/DCO)
- [ ] Conditions de redistribution binaire
- [ ] Utilisation commerciale

## 6. Hypothèses vs Faits

**Faits :**
- BCAD = GPL-3.0-or-later (LICENSE)
- CGAL = GPL pour Boolean_set_operations_2, Triangulation_2
- Qt6 = LGPL v3
- Boost = BSL (transitif via CGAL, pas de dépendance directe)
- SQLite3 = domaine public

**Hypothèses (à valider) :**
- Liaison dynamique Qt + Core GPL = conforme
- Plugins propriétaires via liaison dynamique = autorisés
- Utilisation modules CGAL LGPL = suffisant pour LGPL BCAD

## 7. Recommandations

**Court terme (v1.x) :**
- Maintenir GPL-3.0-or-later
- Ajouter CLA/DCO pour contributions
- Documenter les licences tierces

**Moyen terme (v2.x) :**
- Évaluer migration vers LGPL
- Clarifier la politique commerciale

**Long terme :**
- Évaluer open-core si approprié
- Conseils juridiques

## 8. Conclusion

BCAD reste sous GPL-3.0-or-later. Plugins propriétaires permis via liaison dynamique. Aucune modification prévue.

## 9. Voir aussi

- `THIRD_PARTY_LICENSES.md`
- `LICENSE`
- `CONTRIBUTING.md`