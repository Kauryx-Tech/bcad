# Schémas d'architecture BCAD

> Fichiers sources Draw.io pour les schémas d'architecture.

## Liste

| Fichier | Description |
|---------|-------------|
| `architecture_overview.drawio` | Vue d'ensemble : Core, Services, App, Plugins |
| `core_dependencies.drawio` | Dépendances inversées Core → Render |
| `plugin_lifecycle.drawio` | Cycle de vie d'un plugin |
| `document_entity.drawio` | Modèle Document/Entity/Command |
| `event_bus.drawio` | Flux EventBus typé |

## Visualisation

### VS Code
Installer l'extension **Draw.io Integration** (hediet.vscode-drawio).
Ouvrir un fichier `.drawio` et clic droit → "Open with Draw.io".

### Web
Importer sur https://app.diagrams.net

### Ligne de commande
Utiliser `drawio-batch` ou `puppeteer` (Headless) pour générer PNG/SVG.

## Format

Draw.io utilise un format XML. Les fichiers sont versionnables et diffables.

## Contribution

Pour modifier un schéma :
1. Ouvrir dans Draw.io
2. Modifier
3. Vérifier le XML (clic droit → Exporter)
4. Commit le `.drawio`

## Voir aussi

- `../ARCHITECTURE.md` — documentation textuelle
- `../ARCHITECTURE_DECISIONS.md` — ADR
- `../GLOSSARY.md` — termes