# Switch(EntityType) Removal Patterns

This document documents the pattern used to replace each `switch(EntityType)` occurrence in the codebase.

## Summary

| File | Original Pattern | Replacement Pattern | Status |
|------|------------------|---------------------|--------|
| `src/io/Database.cpp` | `switch(e.type())` for serialize/deserialize | Virtual `serializeParams()` + `SerializerRegistry` | ✅ Complete |
| `src/io/DxfWriter.cpp` | `switch(e->type())` for DXF export | Virtual `writeDxf()` on each entity | ✅ Complete |
| `src/app/PropertiesPanel.cpp` | `switch(e.type())` for geometry info | Virtual `geometryInfo()` on each entity | ✅ Complete |
| `src/app/SnapEngine.cpp` | `switch(e.type())` for snap candidates | Virtual `addSnapCandidates()` with `SnapPointType` enum | ✅ Complete |
| `src/app/ViewportDrawTools.cpp` | `switch(e.type())` for tool placement | Virtual `writeDxf()` / `addSnapCandidates()` | ✅ Complete |
| `src/io/Database.cpp` (load) | `switch(static_cast<EntityType>(typeInt))` for legacy DB compat | TypeId mapping + `SerializerRegistry` | ✅ Complete |

## Pattern Details

### 1. Database.cpp - Serialization/Deserialization

**Before:**
```cpp
std::string serializeParams(const Entity& e) {
    switch (e.type()) {
        case EntityType::Line: { ... }
        case EntityType::Circle: { ... }
        // ...
    }
}
```

**After:**
```cpp
// Entity.h - Virtual method
virtual std::string serializeParams() const = 0;

// Database.cpp - Uses virtual method
std::string serializeParams(const Entity& e) {
    return e.serializeParams();
}

// Each entity implements:
std::string LineEntity::serializeParams() const {
    std::ostringstream ss;
    ss.precision(17);
    ss << start_.x_ << ',' << start_.y_ << ',' << end_.x_ << ',' << end_.y_;
    return ss.str();
}
```

### 2. DxfWriter.cpp - DXF Export

**Before:**
```cpp
switch (e->type()) {
    case EntityType::Line:
        writeLine(f, static_cast<const LineEntity&>(*e));
        break;
    // ...
}
```

**After:**
```cpp
// Entity.h
virtual void writeDxf(std::ostream& f, const std::string& layer, 
                      const std::optional<Color>& colorOverride) const = 0;

// DxfWriter.cpp - Polymorphic call
for (const auto& e : doc.entities()) {
    e->writeDxf(f, e->layer(), e->colorOverride());
}
```

### 3. PropertiesPanel.cpp - Geometry Info Display

**Before:**
```cpp
switch (e.type()) {
    case EntityType::Line: {
        const auto& l = static_cast<const LineEntity&>(e);
        return tr("Line\nLength: %1").arg(l.length());
    }
    // ...
}
```

**After:**
```cpp
// Entity.h
virtual std::string geometryInfo() const = 0;

// PropertiesPanel.cpp
QString PropertiesPanel::geometryInfoFor(const Entity& e) const {
    return QString::fromStdString(e.geometryInfo());
}
```

### 4. SnapEngine.cpp - Snap Candidate Generation

**Before:**
```cpp
switch (e.type()) {
    case EntityType::Line: {
        add(l.start(), SnapType::Endpoint);
        // ...
    }
    // ...
}
```

**After:**
```cpp
// Entity.h - NVI pattern
enum class SnapPointType { Endpoint, Midpoint, Center, Quadrant, ... };
using SnapCallback = std::function<void(const Point2&, SnapPointType)>;
virtual void addSnapCandidates(const Point2& cursor, SnapCallback add) const = 0;

// SnapEngine.cpp
e.addSnapCandidates(cursor, [&](const Point2& p, SnapPointType t) {
    out.push_back({p, mapSnapType(t), geom::distance(cursor, p)});
});
```

### 5. ViewportDrawTools.cpp - Tool Placement

(known at the time as `Viewport.cpp`; the tool `switch` now lives in the
drawing-tools unit of the split — see `VISUAL_ARCHITECTURE.md` §6.3)

**Before:**
```cpp
switch (tool_) {
    case ToolMode::Move: {
        if (moveTarget_ && moveAnchor_) {
            double dx = CGAL::to_double(world.x() - moveAnchor_->x());
            // ...
        }
    }
}
```

**After:**
```cpp
// Uses polymorphic methods on entities directly
// No switch on EntityType needed
```

### 6. Database.cpp (Load) - Legacy Compatibility

**Before:**
```cpp
auto type = static_cast<EntityType>(sqlite3_column_int(st.stmt, 1));
auto entity = deserializeEntity(type, params);
```

**After:**
```cpp
// Legacy DB stores integer EntityType, map to TypeId
int typeInt = sqlite3_column_int(st.stmt, 1);
TypeId typeId;
switch (static_cast<EntityType>(typeInt)) {
    case EntityType::Point: typeId = TypeId_Point; break;
    case EntityType::Line: typeId = TypeId_Line; break;
    // ...
}
auto entity = deserializeEntity(typeId, params);
```

**Note:** This single switch is kept for backward compatibility with existing SQLite databases. It converts the stored integer enum to the new TypeId system.

## Pattern Guidelines

1. **Prefer virtual methods** on Entity for behavior that varies by type
2. **Use Registry/Factory** for creation/serialization that needs type lookup
3. **Use NVI (Non-Virtual Interface)** pattern for callbacks (e.g., `addSnapCandidates`)
4. **Keep TypeId as the primary key** - it's stable and extensible
5. **Single backward-compat switch allowed** in Database.cpp for legacy DB loading