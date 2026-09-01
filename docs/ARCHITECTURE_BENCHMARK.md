# Benchmark architectural BCAD

> Comparaison des concepts architecturaux avec AutoCAD, FreeCAD, QCAD/LibreCAD, BRL-CAD et Open CASCADE. Seuls les concepts publics sont comparés. Implémentations propriétaires non détaillées.

## Décisions

| Décision | Signification |
|----------|---------------|
| **KEEP** | Conserver le concept dans BCAD |
| **ADAPT** | Adapter le concept au contexte BCAD |
| **REJECT** | Rejeter le concept pour BCAD |
| **FUTURE** | Considérer pour une version ultérieure |

## Comparaison par concept

| Concept | AutoCAD | FreeCAD | QCAD/LibreCAD | BRL-CAD | Open CASCADE | BCAD actuel | BCAD cible | Décision |
|---------|---------|---------|----------------|---------|---------------|-------------|------------|----------|
| **Document** | DWG database | App::Document | RS_Graphic | db_non_union | TDocStd_Document | core::Document | core::Document | KEEP |
| **Entity** | AcDbEntity | App::DocumentObject | RS_Entity | db_*_object | TDF_LabelTree | Entity→enum | Entity+Registry | ADAPT |
| **Geometry kernel** | BREP privé | OpenCASCADE | Interne | BREP | OCCT | CGAL exposé | CGAL privé | KEEP (privé) |
| **Commands** | ObjectARX | Python+C++ | ECMAScript | C | CASCADE | C++ Qt | C++ pur | KEEP |
| **Undo/Redo** | Undo Recorder | Oui | Oui | Limité | TDocStd | QUndoCommand | Transaction | KEEP |
| **Transactions** | Oui | openTransaction | Non | Limité | Oui | Non | Oui | KEEP |
| **Selection** | AcDbSelectionSet | Gui::Selection | RS_Selection | Limité | AIS_Selection | vector<int> | SelectionSet | ADAPT |
| **Snapping** | Osnap markers | Oui | Oui | Limité | AIS | Implémenté | Extensible | KEEP |
| **Layers** | Group code 8 | App::Property | Oui | Limité | TDataStd | Layer | Layer | KEEP |
| **Properties** | XData/XRecord | App::Property | Limité | Limité | TDataStd | Non | Property System | KEEP |
| **Registry** | RxObject system | Oui | Limité | Limité | Oui | Non | Oui | KEEP |
| **Plugins** | ObjectARX (.arx) | Workbench | Script | Limité | TKL | Non | .so/.dll | KEEP |
| **Workbench** | CUI | Oui | Non | Non | Oui | Non | WorkbenchRegistry | KEEP |
| **Rendering** | BREP + HLR | Coin3D | Qt | OpenGL | OpenCASCADE | OpenGL 3.3 | IRenderBackend | KEEP |
| **Persistence** | DWG | FCStd (ZIP+XML) | DXF | .g | .cbf | SQLite | SQLite+Registries | KEEP |
| **Events** | AcRx reactor | Signal/Slot | Limité | Limité | TFunction | std::function | EventBus | KEEP |
| **Coordinates** | UCS | Placement | Limité | Oui | TFunction | Implicite | CoordinateSystem | KEEP |
| **Spatial index** | Quadtree | AbstractForest | RTree | Oui | Oui | Quadtree | ISpatialIndex | KEEP |
| **SDK** | ObjectARX, .NET | Python | ECMAScript | libgdip | C++ | Aucun | BCAD SDK | KEEP |

## Concepts KEEP (à conserver)

- Modèle de document unique
- Hiérarchie d'entités avec registre dynamique
- Kernel géométrique privé (encapsulation CGAL)
- Système de transactions et undo/redo
- Property system dynamique
- Registry de types, commandes, formats
- Plugin system dynamique (bibliothèques partagées)
- Workbench model
- Abstraction de rendering (IRenderBackend)
- Bus d'événements typé
- Abstraction de spatial index (ISpatialIndex)
- Format SQLite inspectable

## Concepts ADAPT (à adapter)

- **EntityType enum → string + GUID** : plus flexible pour plugins
- **Selection vector<int> → SelectionSet interface** : plus structuré
- **QUndoCommand → Transaction explicite** : plus puissant pour plugins
- **std::function onChanged → EventBus** : découplage fort

## Concepts REJECT (à rejeter pour v1)

- **DWG binaire propriétaire** : trop complexe, format fermé
- **Macro/scripting natif** : pas MVP
- **Persistance de l'historique** : complexité élevée

## Concepts FUTURE (futur)

- **3D natif** : après la plateforme
- **Multi-document (xref)** : après plugins
- **Sandbox de plugins** : complexité
- **Python SDK** : optionnel
- **R-tree, Octree, BVH** : selon les besoins