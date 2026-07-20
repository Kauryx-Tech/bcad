# bcad

A personal, from-scratch 2D CAD application in the spirit of AutoCAD: Qt GUI,
OpenGL rendering, a CGAL-backed geometry engine, DXF interoperability, and a
native SQLite project format.

See [`architecture bcad.txt`](architecture%20bcad.txt) for the original design
sketch, [`ARCHITECTURE.md`](ARCHITECTURE.md) for how the code maps onto it,
and [`DESIGN_NOTES.md`](DESIGN_NOTES.md) for the open-source CAD prior art
this design draws on.

## Status

Every layer in the architecture implemented, built, and run (verified in
this environment, including visually): geometry engine, layers, spatial
indexing, DXF/SQLite I/O, OpenGL rendering pipeline, and a ribbon-based Qt
GUI with interactive drawing tools, snapping, and undo/redo. See
[`CAHIER_DES_CHARGES.md`](CAHIER_DES_CHARGES.md) for the full, continuously
updated feature checklist and prioritized roadmap — the summary below is a
snapshot, that document is the source of truth.

## Features (MVP)

- Entities: line, circle, arc, polyline (open/closed).
- Layers: create/rename/remove, visibility, lock, per-layer color.
- Interactive tools: select, move, line, circle (center+radius), arc
  (center+start+end), polyline (multi-click, Enter/right-click to finish).
  Every placed point also accepts typed coordinate entry (`x,y`, `@dx,dy`,
  `@dist<angle`) via the status-bar command line.
- Object snap (endpoint/midpoint/center/intersection/perpendicular) and
  snap-to-grid, both with on-canvas indicators; independent toggles for
  object snap (`F3`), grid display (`F7`), grid snap (`F9`).
- Undo/redo (`Ctrl+Z`/`Ctrl+Y`) covering drawing, move, delete, and boolean
  operations.
- Pan (middle-drag), zoom-to-cursor (wheel), zoom-to-fit (`F`).
- Boolean ops (union/intersection/difference/symmetric difference), wired to
  a GUI command (Modify menu / ribbon): select two closed polylines, apply.
  Delaunay/constrained-Delaunay triangulation is implemented and tested at
  the geometry-engine level but not yet wired to a GUI command.
- Import/export DXF (R2000 ASCII subset: LINE/CIRCLE/ARC/LWPOLYLINE + layers).
- Native `.bcad` project files (SQLite).
- Background-threaded tessellation so panning/zooming large drawings doesn't
  stall the UI thread; quadtree-based viewport culling.
- Ribbon UI (`RibbonBar`: tabs of captioned button panels, à la recent
  AutoCAD) alongside a classic menu bar — both drive the same `QAction`s.

## Building

Requires a C++20 compiler, CMake ≥ 3.20, Qt6 (Widgets, OpenGLWidgets, OpenGL,
Gui), CGAL, Boost, SQLite3, and OpenGL dev headers:

```bash
sudo apt-get install build-essential cmake qt6-base-dev libqt6opengl6-dev \
    libcgal-dev libboost-dev libboost-all-dev libsqlite3-dev \
    libgl1-mesa-dev libglu1-mesa-dev
```

Then:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure   # smoke tests
./build/src/app/bcad                          # run the app
```

### Alternative: vcpkg-managed dependencies

Instead of installing Qt6/CGAL/SQLite3 system-wide, [`vcpkg.json`](vcpkg.json)
pins reproducible versions via [vcpkg](https://vcpkg.io):

```bash
git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT="$PWD/vcpkg"

cmake --preset vcpkg
cmake --build --preset vcpkg
ctest --preset vcpkg --output-on-failure
```

The first configure builds Qt6 and CGAL from source and takes a while; CI
uses the system-package path above instead for turnaround time.

## Layout

```
include/bcad/<module>/   public headers, one directory per module
src/<module>/            implementation + that module's CMakeLists.txt
tests/                   smoke_test.cpp — one check per module, no framework
```

Modules: `geometry` (CGAL entities/ops) → `layers` → `render` (quadtree,
camera, GL renderer) → `core` (Document, ties layers+entities+index together)
→ `io` (DXF, SQLite) → `app` (Qt GUI). Each is its own CMake static library
target so the dependency graph stays explicit and one-directional.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for the build/test loop and PR
checklist, and [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) for community
standards.

## License

GPL-3.0-or-later — see [LICENSE](LICENSE). This follows from `geometry`'s
use of CGAL's `Boolean_set_operations_2` and `Triangulation_2` packages,
which CGAL itself licenses GPL (not LGPL); the rest of the project matches
so the whole binary stays distributable.

## Roadmap

See [`CAHIER_DES_CHARGES.md`](CAHIER_DES_CHARGES.md) §3 for the prioritized,
up-to-date list (P0–P3). Highlights: interactive rotate/scale/mirror,
multi-select, editable entity properties, then text/dimensions/hatches/
blocks as new Entity subtypes, then DWG support (proprietary format — likely
via an external converter rather than a from-scratch reader).
