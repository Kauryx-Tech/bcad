# Système de propriétés BCAD

> [!IMPORTANT]
>
> ## Statut : IMPLEMENTE — coeur `bcad::properties`
>
> `bcad::properties` existe (`PropertyMap` copiable, `PropertyTypes`, **événements `PropertyChanged`**, **accès variant `getPropertyValue`/`set`**) dans `include/bcad/properties/`. L'enregistrement de propriétés « dynamiques » par plugins fonctionne ; le panneau applicatif (`src/app/PropertiesPanel`) génère ses éditeurs dynamiquement depuis `PropertyMap`.
>
> Système de propriétés générique. Le Core expose le mécanisme ; les plugins déclarent les propriétés.

## 1. Principe

Toute entité (Core ou plugin) peut exposer des propriétés dynamiques typées. Le Core ne connaît pas les propriétés métier. Le mécanisme est générique.

**Exemple :**

```
Wall
 ├── Height      (double, m, contraint > 0)
 ├── Thickness   (double, m)
 ├── Material    (string, contraint à liste)
 └── FireRating  (enum: A1, A2, B, C, D)
```

## 2. Architecture

```cpp
namespace bcad::properties {

enum class PropertyType {
    Double, Int, String, Bool, Color, Enum, Vector3, Point3, DateTime
};

class Property {
public:
    Property(const std::string& name, PropertyType type);
    virtual ~Property() = default;

    const std::string& name() const;
    PropertyType type() const;

    // Métadonnées
    void setUnit(const std::string& u);
    const std::string& unit() const;
    void setDescription(const std::string& d);
    const std::string& description() const;

    // Contraintes
    void setRange(double min, double max);    // pour Double
    void setEnumValues(const std::vector<std::string>& values);  // pour Enum
    void setReadOnly(bool ro);
    bool isReadOnly() const;

    // Accès typé
    virtual double asDouble() const = 0;
    virtual int asInt() const = 0;
    virtual std::string asString() const = 0;
    virtual bool asBool() const = 0;
    virtual geom::Color asColor() const = 0;

    // Mutation
    virtual void setFromDouble(double) = 0;
    virtual void setFromInt(int) = 0;
    virtual void setFromString(const std::string&) = 0;
    virtual void setFromBool(bool) = 0;
    virtual void setFromColor(const geom::Color&) = 0;
};

}
```

## 3. PropertyMap

```cpp
namespace bcad::properties {

class PropertyMap {
public:
    // Création
    template<typename T>
    T* add(const std::string& name);

    Property* addDouble(const std::string& name, double initial = 0.0);
    Property* addInt(const std::string& name, int initial = 0);
    Property* addString(const std::string& name, const std::string& initial = "");
    Property* addBool(const std::string& name, bool initial = false);
    Property* addColor(const std::string& name, const geom::Color& initial = {});
    Property* addEnum(const std::string& name, int initial = 0,
                      const std::vector<std::string>& values = {});

    // Accès
    Property* get(const std::string& name) const;
    const Property* find(const std::string& name) const;
    bool has(const std::string& name) const;
    void remove(const std::string& name);

    // Helpers
    double getDouble(const std::string& name, double def = 0.0) const;
    int getInt(const std::string& name, int def = 0) const;
    std::string getString(const std::string& name) const;
    bool getBool(const std::string& name, bool def = false) const;

    void setDouble(const std::string& name, double v);
    void setInt(const std::string& name, int v);
    void setString(const std::string& name, const std::string& v);
    void setBool(const std::string& name, bool v);

    std::vector<std::string> listNames() const;

    // Events
    events::EventBus& onChanged();
};

}
```

## 4. Exemple d'entité plugin

```cpp
class WallEntity : public document::Entity {
    // Implémentation...
    PropertyMap props_;

    void init() {
        props_.addDouble("height", 2.5)->setUnit("m");
        props_.addDouble("thickness", 0.2)->setUnit("m");
        props_.addString("material", "concrete");
        props_.addEnum("fireRating", 0, {"A1", "A2", "B", "C", "D"});
    }
};

void useWall(WallEntity& wall) {
    double h = wall.properties().getDouble("height");   // 2.5
    wall.properties().setDouble("height", 3.0);
    std::string mat = wall.properties().getString("material");
}
```

### Règle : `set*` crée la propriété absente

Un `setX("cle", v)` sur une clef non déclarée **crée** la propriété (type `X`,
valeur de départ vide) puis lui affecte `v` — il n'avale plus l'écriture en
silence. Les entités natives (`Point`, `Polyline`, …) n'ont aucun schéma déclaré,
et les imports (XDATA DXF, GeoPackage) y posent des champs métier : avec une
sémantique « ne touche qu'une propriété existante », ces valeurs étaient perdues
sans erreur.

Conséquences assumées :

1. Une propriété peut apparaître sans `add*` explicite, donc sans type choisi par
   le métier : son type est celui du `setX` employé (`setDouble` → `Double`, …).
2. `readOnly` reste respecté : seule la création est immédiate, la modification
   d'une propriété existante protégée est toujours ignorée.
3. `has()` cesse d'être un test d'appartenance au schéma d'un type : il teste la
   présence effective de la valeur, ce qui est ce que l'import DXF veut dire par
   « cette polyligne porte une section cadastrale ».

## 5. Persistance

Les propriétés sont sérialisées dans le document :

```json
{
  "wall:001": {
    "typeId": "architecture:wall",
    "properties": {
      "height": { "type": "double", "value": 2.5, "unit": "m" },
      "thickness": { "type": "double", "value": 0.2, "unit": "m" },
      "material": { "type": "string", "value": "concrete" },
      "fireRating": { "type": "enum", "value": 0, "values": ["A1", ...] }
    }
  }
}
```

Le SerializerRegistry convertit ce JSON en bytes dans le format `.bcad` (SQLite binaire).

### Le contrat d'export DXF (`BCAD_PROPS`)

Le DXF, lui, est typé explicitement : chaque propriété part en triplet XDATA
`1000 clé / 1000 type / 1000 valeur`, et l'import recrée la propriété avec le
`setX` du type annoncé. Une énumération est exportée sous son **libellé** et avec
le tag `string`, pas sous son index.

Ce n'est pas une approximation paresseuse : `Property::setFromEnum(index)` lève
`std::out_of_range` hors du domaine, et le domaine (`enumValues`) n'est pas porté
par la valeur — il appartient au schéma du module. Un fichier qui rendrait un
index sans domaine rendrait donc soit une valeur illégale, soit une valeur
réinterprétée dès que le module fait évoluer son énumération. Le libellé, lui,
est relisible et redevient un `Enum` quand le module redéclare sa propriété.

## 6. UI dynamique

Le `PropertyPanel` Qt utilise la PropertyMap pour générer dynamiquement les widgets d'édition :

```cpp
// src/app/PropertiesPanel.cpp
for (const auto& name : entity.properties().listNames()) {
    auto* prop = entity.properties().get(name);
    QWidget* editor = createEditor(prop);  // QDoubleSpinBox, QLineEdit, QComboBox, ...
    layout->addRow(QString::fromStdString(name), editor);
}
```

**Avantage :** ajouter une nouvelle entité/plugin n'oblige pas à modifier le `PropertiesPanel`. Il s'adapte automatiquement.

## 7. Événements

```cpp
// Modification d'une propriété → PropertyChangedEvent
bus.subscribe<PropertyChangedEvent>([](const PropertyChangedEvent& e) {
    // e.entityId, e.propertyName, e.oldValue, e.newValue
});
```

## 8. Accès variant générique (`PropertyValue`)

Pour les outils génériques (UI, sérialisation, commandes), `PropertyMap` expose un accès basé sur `std::variant` :

```cpp
namespace bcad::properties {

using PropertyValue = std::variant<double, int, std::string, bool, geom::Color, EnumIndex>;

class PropertyMap {
public:
    // Récupère la valeur brute (variant) — retourne variant vide si absent
    PropertyValue getPropertyValue(const std::string& name) const;

    // Définit la valeur via variant — dispatch interne selon l'index du variant
    void set(const std::string& name, const PropertyValue& v);

    // ... setDouble, setString, etc. restent disponibles pour le code métier typé
};

}
```

Cela permet à l'UI, aux commandes génériques et à la sérialisation de manipuler n'importe quel type sans connaître le type à la compilation.

## 9. Commande Core : `SetEntityPropertyCommand`

Nouvelle commande pure C++ dans `bcad::commands` pour modifier toute propriété typée via `PropertyMap` :

```cpp
// include/bcad/commands/ConcreteCommands.h
class SetEntityPropertyCommand : public Command {
public:
    // Constructeur métier : capture oldValue au moment de l'exécution
    SetEntityPropertyCommand(int entityId, std::string propertyName,
                             properties::PropertyValue newValue, std::string text = "Change Property");

    void execute(core::Document& doc) override;
    void undo(core::Document& doc) override;
    bool mergeWith(const Command& other) override;
    std::unique_ptr<Command> clone() const override;

private:
    int entityId_;
    std::string propertyName_;
    std::optional<properties::PropertyValue> oldValue_;  // capturé à l'exécution
    properties::PropertyValue newValue_;
    std::string text_;
};
```

- `execute()` : capture `oldValue_` via `PropertyMap::getPropertyValue()`, puis `PropertyMap::set()`.
- `undo()` : restaure via `PropertyMap::set(oldValue_)`.
- `mergeWith()` : fusionne si même `entityId` + même `propertyName` (garde la dernière valeur).
- `clone()` : utilise le constructeur de clone complet (avec `oldValue_` capturé).

Compatible `QtCommandAdapter` pour l'usage depuis `app/`.

## 10. Panneau de propriétés générique (`PropertiesPanel`)

`src/app/PropertiesPanel.cpp` reconstruit ses éditeurs à chaque sélection via `rebuildPropertyEditors(entity)` :

```cpp
void PropertiesPanel::rebuildPropertyEditors(geom::Entity* entity) {
    clearLayout(propertyForm_);
    auto names = entity->properties().listNames();
    std::sort(names.begin(), names.end());

    for (const auto& name : names) {
        auto* prop = entity->properties().find(name);
        if (!prop || prop->isReadOnly()) continue;

        switch (prop->type()) {
            case PropertyType::String:
                makeLineEdit(entity, name, prop->asString());
                break;
            case PropertyType::Enum:
                makeComboBox(entity, name, prop->enumValues(), prop->asEnum());
                break;
            case PropertyType::Double:
                makeDoubleSpinBox(entity, name, prop->asDouble(), prop->min_, prop->max_, prop->hasRange_, prop->unit());
                break;
            case PropertyType::Int:
                makeSpinBox(entity, name, prop->asInt(), prop->min_, prop->max_, prop->hasRange_, prop->unit());
                break;
            case PropertyType::Bool:
                makeCheckBox(entity, name, prop->asBool());
                break;
            // Color: à venir (QColorDialog button)
        }
    }
    propertyWidget_->setVisible(propertyForm_->rowCount() > 0);
}
```

Chaque factory crée le widget Qt approprié, connecte son signal de fin d'édition (`editingFinished`, `valueChanged`, `toggled`) à une lambda qui pousse un `SetEntityPropertyCommand` dans l'`undoStack` (ou applique directement si pas d'undo). Le widget est créé avec `QSignalBlocker` implicite via la logique de `refresh()` → `rebuildPropertyEditors()`.

**Types supportés** : `String` (`QLineEdit`), `Enum` (`QComboBox`), `Double` (`QDoubleSpinBox` avec `unit`, `range`), `Int` (`QSpinBox` avec `unit`, `range`), `Bool` (`QCheckBox`). `Color` prévu.

## 11. Événements `PropertyChanged`

`PropertyMap::set*()` et `remove()` publient maintenant `events::PropertyChanged` via l'`EventBus` multi-abonnés :

```cpp
struct PropertyChanged : Event {
    core::Document* document;
    geom::Entity* entity;
    std::string propertyName;
    properties::PropertyValue oldValue;
    properties::PropertyValue newValue;
};
```

- Publié **seulement si la valeur change vraiment** (comparaison variant index + valeur).
- `oldValue` / `newValue` sont des `PropertyValue` (variant) pour inspection générique.
- Multi-abonnés : UI (rafraîchissement), validation, persistance, plugins.
- Pas de dépendance Qt dans le Core.

## 11 bis. Hauteur de texte des cotations

`dimension.text_height` (Double, unités du dessin) porte la hauteur du texte
d'une cotation ; flèches, écart à l'objet et dépassement des lignes d'attache en
dérivent (proportions ISO). Absente, elle vaut 2,5 ; lue d'un fichier, elle est
bornée à [1e-6 ; 1e6]. L'outil de cote la règle au premier point : hauteur de
la dernière cote du dessin, sinon valeur ronde lisible au zoom. Le dessin d'une
cote (canevas, PDF, sélection, emprise, DXF) est calculé à un seul endroit,
`geom::dimensionGraphics`.

La cotation linéaire mesure désormais le long de sa direction (`rotation`) :
un fichier ancien portant une rotation non nulle affiche la valeur projetée.

## 12. Règles

1. Le Core expose le PropertyMap
2. Les plugins déclarent leurs propriétés via `PropertyMap::add*`
3. Les contraintes sont déclaratives (`unit`, `range`, `enumValues`, `readOnly`, `description`)
4. Le PropertyPanel Qt est générique, pas d'IHM spécifique par entité
5. La persistance est déléguée au SerializerRegistry
6. Aucune valeur par défaut codée en dur dans l'UI
7. Les modifications passent par `SetEntityPropertyCommand` (Core) ou `SetEntityPropertyCommand` (Qt wrapper) → undo/redo garanti
8. `PropertyChanged` via EventBus pour notification multi-abonnés