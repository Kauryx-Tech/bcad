# Architecture I/O BCAD

> [!IMPORTANT]
>
> ## Statut : MIXTE — état actuel + architecture cible
>
> Le **.bcad** (SQLite) et le **DXF** existent réellement (voir section 3, 4). Les sections **2, 5, 6,
> 7** décrivent l'**architecture cible** (`SerializerRegistry`, `IDocumentSerializer`, `FileService`,
> adaptateurs DWG/SVG) **non implémentée**.

> Formats de fichiers. Le modèle interne est séparé des formats externes.

## 1. Principe

```
Document model (Core)
       │
       ├── Native persistence (.bcad)
       ├── DXF adapter
       ├── Future DWG adapter
       └── Other formats
```

Les formats externes **ne contaminent pas** le modèle interne.

## 2. Architecture

```
┌─────────────────────────────────────────┐
│           Document (Core)                │
│  entities, layers, properties, ...       │
└─────────────────┬───────────────────────┘
                  │ serialize/deserialize
                  ▼
┌─────────────────────────────────────────┐
│        SerializerRegistry                │
│  (WallSerializer, LineSerializer, ...)   │
└─────────────────┬───────────────────────┘
                  │
       ┌──────────┼──────────┐
       ▼          ▼          ▼
  ┌─────────┐ ┌──────┐ ┌──────────┐
  │ .bcad   │ │ DXF  │ │ Future   │
  │ (SQLite)│ │      │ │ DWG, SVG │
  └─────────┘ └──────┘ └──────────┘
```

## 3. Format natif .bcad

SQLite, géométrie sérialisée en paramètres compacts (pas de JSON). Voir `PERSISTENCE_ARCHITECTURE.md` et `src/io/Database.cpp`.

## 4. DXF

### 4.1 État actuel

Implémentation manuelle d'un sous-ensemble DXF ASCII R2000 :
- LINE, CIRCLE, ARC, LWPOLYLINE
- Layers (group code 8)

### 4.2 Cible

Le DXF devient un `IDocumentSerializer` :

```cpp
class DxfSerializer : public IDocumentSerializer {
    std::string format() const override { return "dxf"; }
    void save(const Document& doc, const std::string& path,
              const SerializerRegistry& registry) const override;
    void load(Document& doc, const std::string& path,
              const SerializerRegistry& registry) const override;
};
```

Les entités Core sont mappées. Les entités plugin sont :
- Ignorées en export
- Mappées en bloc/proxy en export
- Importées depuis bloc/proxy vers l'entité la plus proche

### 4.3 Mapping DXF ↔ BCAD

| DXF | BCAD |
|-----|------|
| LINE | LineEntity |
| CIRCLE | CircleEntity |
| ARC | ArcEntity |
| LWPOLYLINE | PolylineEntity |
| INSERT (block) | BlockEntity ou entité plugin |
| DIMENSION | DimensionEntity (futur) |
| TEXT, MTEXT | TextEntity (futur) |

## 5. DWG (futur)

DWG est un format binaire propriétaire, fermé, en évolution constante.

**Options :**
1. **LibreCAD/libdxfrw** : ne lit que le DXF, pas le DWG
2. **ODA Files** : SDK commercial (licence restrictive)
3. **Teigha** : SDK propriétaire d'Open Design Alliance
4. **Convertisseur externe** : exporter en DXF depuis AutoCAD/BricsCAD, puis importer dans BCAD

**Recommandation :** Ne pas implémenter DWG directement. Prioriser l'interopérabilité via DXF et supporter les workflows avec convertisseur externe.

## 6. SVG (futur)

SVG 2D peut être utilisé pour l'import/export de dessins 2D simples.

## 7. API publique

```cpp
namespace bcad::io {

class FileService {
public:
    // Formats disponibles
    std::vector<const IDocumentSerializer*> availableFormats() const;

    // Sauvegarde
    void save(const Document& doc, const std::string& path) const;
    void save(const Document& doc, const std::string& path, const std::string& format) const;

    // Chargement
    std::unique_ptr<Document> load(const std::string& path) const;
    std::unique_ptr<Document> load(const std::string& path, const std::string& format) const;

    // Auto-détection
    bool canOpen(const std::string& path) const;
    std::optional<std::string> detectFormat(const std::string& path) const;
};

}
```

## 8. Règles

1. Document ne connaît pas les formats
2. Chaque format est un IDocumentSerializer
3. Les plugins peuvent ajouter des formats
4. DXF est le format d'échange principal
5. DWG direct n'est pas prioritaire
6. L'API publique utilise FileService