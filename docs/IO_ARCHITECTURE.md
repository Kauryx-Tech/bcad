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

### État du plugin cadastral

Le plugin cadastral fournit un service GeoJSON pour les géométries `Point`,
`LineString` et `Polygon`, ainsi qu'un export CSV des coordonnées tessellées.
Ces services sont testés séparément et ne remplacent pas le
`SerializerRegistry` : celui-ci reste réservé aux sérialiseurs d'entités
enregistrés par `TypeId` pour la persistance native et le cycle de vie des
plugins.

Les formats GeoPackage et ArcGIS restent indisponibles tant qu'un backend
réel n'est pas configuré. Ils doivent retourner un échec explicite, jamais un
succès sans fichier produit.

Le GeoPackage cadastral est maintenant écrit dans SQLite avec les tables
`gpkg_contents`, `gpkg_geometry_columns` et `cadastre_parcels`. La colonne `geom` contient un
`GeoPackageBinary` avec un WKB `POLYGON` little-endian ; les attributs
cadastraux sont stockés dans des colonnes séparées. La lecture ne vide le
document qu'après ouverture et préparation réussies du fichier.

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
- LINE, CIRCLE, ARC, LWPOLYLINE, POLYLINE (+VERTEX/SEQEND), TEXT, MTEXT
- Layers (group code 8), couleur ACI (62)
- Propriétés : une seule XDATA générique, appid `BCAD_PROPS`, triplets
  `1000 clé / 1000 type / 1000 valeur` (types `double|int|string|bool|color`).
  L'écrivain ne connaît le nom d'aucune clé : il vide le `PropertyMap` de chaque
  entité, y compris les clés d'un module absent. L'import les restaure telles
  quelles. Le lecteur relit en outre l'ancien appid `BCAD_CADASTRE` (paires
  clé/valeur, préfixées `cadastre.` à la lecture) pour ne pas casser les fichiers
  écrits avant ce contrat ; rien n'écrit plus cet appid.

Ce que le format ne porte pas : le `TypeId` d'une entité de plugin. Une parcelle
cadastrale exportée est une `LWPOLYLINE` fermée enrichie de `BCAD_PROPS` ; la
re-conversion vers le type du module est une décision du module, pas de `src/io`.

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

L'écriture d'une entité est virtuelle (`Entity::writeDxf`) : l'écrivain n'a pas de
liste de types. Une entité de plugin qui n'implémente pas `writeDxf` ne contribue
aucune géométrie au fichier ; ses propriétés, elles, partent en `BCAD_PROPS` dès
l'instant où l'entité est dans le document. Le mapping bloc/proxy (INSERT) reste à
faire, côté import comme côté export.

### 4.3 Mapping DXF ↔ BCAD

| DXF | BCAD |
|-----|------|
| LINE | LineEntity |
| CIRCLE | CircleEntity |
| ARC | ArcEntity |
| LWPOLYLINE / POLYLINE | PolylineEntity |
| INSERT (block) | BlockEntity ou entité plugin (non implémenté) |
| DIMENSION | DimensionEntity (futur) |
| TEXT, MTEXT | TextEntity |
| XDATA `BCAD_PROPS` | `PropertyMap` de l'entité |

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