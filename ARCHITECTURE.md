# Architecture

This maps the original design sketch in [`architecture bcad.txt`](architecture%20bcad.txt)
onto the actual module layout. See [`DESIGN_NOTES.md`](DESIGN_NOTES.md) for
how this compares to LibreCAD/QCAD/FreeCAD.

```
┌──────────────────────────────────────────────────────────────┐
│ app/        MainWindow, Viewport, LayerPanel, TessellationWorker │
│             Qt GUI, interactive tools, threading glue           │
├──────────────────────────────────────────────────────────────┤
│ render/     Camera2D, LevelOfDetail, Quadtree, GlRenderer,      │
│             TessellationTypes                                   │
│             OpenGL 3.3 core pipeline, spatial indexing, LOD      │
├──────────────────────────────────────────────────────────────┤
│ core/       Document                                            │
│             Owns entities + LayerManager + Quadtree, thread-safe │
│             via a shared_mutex (readers: render/pick, writers:   │
│             add/remove/transform)                                │
├──────────────────────────────────────────────────────────────┤
│ io/         DxfReader, DxfWriter, Database                      │
│             DXF R2000 ASCII subset; native SQLite .bcad format   │
├──────────────────────────────────────────────────────────────┤
│ layers/     Layer, LayerManager                                 │
│             Name-based layer references (DXF convention)         │
├──────────────────────────────────────────────────────────────┤
│ geometry/   Entity, Line/Circle/Arc/Polyline/Point, Transform2D,│
│             BooleanOps, Triangulation, GeometryUtils (CGAL)      │
│             The only module with no dependency on anything else │
└──────────────────────────────────────────────────────────────┘
```

Dependency direction is strictly downward — `geometry` depends on nothing in
this project, `layers` depends only on `geometry`, `render` depends on
`geometry` (+ Qt/GL), `core` depends on `layers` + `render`, `io` depends on
`core`, `app` depends on everything. This was a deliberate fix during
implementation: an earlier draft had `render`'s `TessellationWorker` include
`core::Document`, which is backwards (`core` already depends on `render` for
`Quadtree`/`TessellationTypes`) and would have created a circular CMake
target dependency. `TessellationWorker` now lives in `app`, since it's really
Qt-threading glue between a `core::Document` and `render::TessellationResult`,
not a rendering primitive itself.

## Key design decisions

**Kernel choice.** `geometry::Kernel` is CGAL's
`Exact_predicates_inexact_constructions_kernel` — exact enough for robust
boolean ops and triangulation, fast enough (double-precision constructions)
for interactive dragging. `Simple_cartesian` would be faster but unsafe for
booleans; a fully exact kernel would be safe but too slow for live dragging.

**Entities own their exact representation.** `ArcEntity` stores
center/radius/angles, not a pre-tessellated polyline — `tessellate(double
maxDeviation)` is called fresh at render/export time with a deviation
tolerance driven by current zoom (`render::worldToleranceForZoom`). This is
what makes LOD work: circles are cheap line loops when zoomed out, dense
when zoomed in, and DXF export writes a real `ARC`/`CIRCLE` entity instead of
a fixed-resolution polyline.

**Document is intentionally non-copyable.** It holds a `std::shared_mutex`
guarding the entity list + quadtree, so a background `TessellationWorker`
thread can safely query it (`buildTessellation`, shared lock) while the GUI
thread edits it (`addEntity`/`removeEntity`/`notifyEntityChanged`, exclusive
lock). Because of that mutex, `Document` can't be moved or copied — `io::`
load functions take `Document&` (populate-in-place) rather than returning a
`Document` by value.

**Rendering is two-stage.** `TessellationWorker` (worker thread) turns
visible entities into flat `ColorBatch` vertex arrays — no GL calls, just
`Entity::tessellate()` grouped by resolved color. `GlRenderer` (GL/main
thread) only uploads that data and issues `glMultiDrawArrays` — one draw call
per distinct color in view, not per entity. This is the "multi-threading
rendering" and "frustum culling" boxes from the original sketch: culling is
the quadtree region query inside `buildTessellation`, threading is running
that query + tessellation off the GL thread.

**Layers referenced by name, not pointer.** Matches the DXF `8` group code
convention directly, so import/export doesn't need an indirection table, and
layers can be renamed without walking every entity.

## Known gaps vs. the original sketch

- Vulkan: not implemented (OpenGL 3.3 core only). Revisit if/when multi-GPU
  or compute-shader tessellation becomes worth the complexity.
- DWG: not implemented — it's a closed, actively-changing binary format;
  realistic paths are linking a DWG library (e.g. the LGPL `libdxfrw` reads
  DXF only, not DWG) or shelling out to an external converter.
- Boolean ops / triangulation are implemented and unit-tested
  (`tests/smoke_test.cpp`) but not yet exposed as a GUI command.
