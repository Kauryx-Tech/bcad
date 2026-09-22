# Système de propriétés BCAD

> [!IMPORTANT]
>
> ## Statut : ARCHITECTURE CIBLE — non implémentée
>
> Le module `bcad::properties` (`PropertyMap`, `PropertyChangedEvent`), le panneau dynamique et
> l'enregistrement de propriétés par plugins **n'existent pas**. Le panneau réel
> (`include/bcad/app/PropertiesPanel.h`) calcule des propriétés génériques via un
> `switch(e.type())` codé en dur dans `src/app/PropertiesPanel.cpp`.

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

## 8. Règles

1. Le Core expose le PropertyMap
2. Les plugins déclarent leurs propriétés via `PropertyMap::add*`
3. Les contraintes sont déclaratives
4. Le PropertyPanel Qt est générique, pas d'IHM spécifique par entité
5. La persistance est déléguée au SerializerRegistry
6. Aucune valeur par défaut codée en dur dans l'UI