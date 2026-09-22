# TypeId Stability Policy

## Overview

`TypeId` is the primary extensibility mechanism for entity types in BCAD. It replaces the closed `EntityType` enum and allows plugins to register new entity types without modifying the Core.

## TypeId Format

- **Native types**: `bcad.<typename>` (e.g., `bcad.Point`, `bcad.Line`, `bcad.Circle`, `bcad.Arc`, `bcad.Polyline`)
- **Plugin types**: `<reverse-domain>.<plugin>.<typename>` (e.g., `com.example.civil.Alignment`, `org.mycad.mechanical.Gear`)

## Stability Guarantees

| Version Change | Native TypeId Stability |
|----------------|------------------------|
| Patch (x.y.z → x.y.z+1) | **Guaranteed stable** - no changes to native TypeIds |
| Minor (x.y → x.y+1) | **Guaranteed stable** - native TypeIds preserved; new types may be added |
| Major (x → x+1) | **May change** - breaking changes allowed with migration path |

### Native TypeId Registry

The following native TypeIds are guaranteed stable within a major version:

| TypeId | Since | Notes |
|--------|-------|-------|
| `bcad.Point` | 1.0 | |
| `bcad.Line` | 1.0 | |
| `bcad.Circle` | 1.0 | |
| `bcad.Arc` | 1.0 | |
| `bcad.Polyline` | 1.0 | |

## Plugin TypeId Requirements

- Must use reverse-domain notation to avoid collisions
- Plugin authors own their namespace
- BCAD reserves `bcad.*` namespace

## Serialization Impact

TypeIds are used as primary keys in:
- SQLite database (`entities.type` column stores TypeId string)
- DXF export (maps to DXF entity type names)
- Plugin registration

**Changing a native TypeId is a major breaking change** requiring:
1. Major version bump
2. Migration tool for existing files
3. Deprecation period in prior minor version

## Testing

The test `tests/typeid_stability_test.cpp` verifies:
1. All native types have unique, non-empty TypeIds
2. TypeIds follow the `bcad.<typename>` pattern
2. No duplicate TypeIds exist