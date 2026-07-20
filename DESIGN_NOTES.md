# Design notes: prior art

Before/while writing this, I compared bcad's module boundaries against three
established open-source CAD codebases and Autodesk's own current UI
direction, to sanity-check the split in `architecture bcad.txt` rather than
inventing module boundaries in a vacuum.

## LibreCAD

Qt-based, C++17, the closest architectural sibling to bcad (2D, Qt, DXF-first).
Its layering maps almost one-to-one onto ours:

| LibreCAD | bcad | Notes |
|---|---|---|
| `RS_Entity` / `RS_EntityContainer` | `geom::Entity` / `core::Document` | LibreCAD nests containers (blocks, polylines *and* the document all derive from the same container base); bcad keeps `PolylineEntity` as a flat vertex list and `Document` as a separate non-`Entity` owner. Simpler, at the cost of not being able to nest a `Document` inside another entity (blocks, when added, will need their own small container type). |
| `RS_LayerList` / `RS_BlockList` (owned by `RS_Graphic`) | `layers::LayerManager` (owned by `core::Document`) | Same idea: layers are a manager object hanging off the document, not a first-class entity type. |
| `RS_GraphicView` | `app::Viewport` + `render::GlRenderer` | LibreCAD combines view/interaction/paint in one class; bcad splits GL drawing (`GlRenderer`) from event handling (`Viewport`) and pushes tessellation to a worker thread — LibreCAD predates the expectation of that being necessary at LibreCAD's target drawing sizes. |
| `RS_ActionInterface` | `ToolMode` enum + switch in `Viewport` | LibreCAD's is a proper polymorphic action-object stack (undo-friendly, one class per tool). bcad's enum+switch is the deliberately smaller MVP version — called out in the roadmap as needing to become polymorphic once undo/redo lands, since a stack of `Action` objects is what makes undo tractable. |
| `RS_FilterDXFRW` | `io::DxfReader` / `io::DxfWriter` | LibreCAD delegates to the external `libdxfrw` library for full DXF/DWG coverage. bcad hand-rolls a DXF R2000 ASCII subset (LINE/CIRCLE/ARC/LWPOLYLINE + layers) — much smaller surface, but no DWG and no spline/hatch/dimension entities yet. If DXF fidelity becomes a priority, vendoring `libdxfrw` is the LibreCAD-proven path rather than growing the hand-rolled parser indefinitely. |

Source: [LibreCAD/LibreCAD on DeepWiki](https://deepwiki.com/LibreCAD/LibreCAD).

## QCAD

Also Qt+C++ at the core, but pushes almost all *tool* logic out into an
ECMAScript layer (`scripts/`, entry point `autostart.js`) — the C++ core
stays small and generic, tools/menus/dialogs are scripted. bcad doesn't do
this (tools are compiled C++ in `Viewport`), which is the right call for a
personal project at this scale — a scripting layer is a lot of infrastructure
to buy back extensibility bcad doesn't need yet — but it's worth remembering
as the answer if/when "let users write custom tools without recompiling"
becomes a real requirement.

Source: [qcad/qcad on GitHub](https://github.com/qcad/qcad), [QCAD ECMAScript developer docs](https://qcad.org/doc/qcad/3.0/developer/group__ecma__scripts.html).

## FreeCAD

3D parametric, much bigger scope than bcad, but its top-level layering
principle is the one worth borrowing regardless of dimensionality: strict
separation between a document-object data model and the geometry
kernel/constraint solver underneath it (`Sketcher::SketchObject` stores
geometry + constraints; `planegcs` solves them; the GUI only ever talks to
the document object, never the solver directly). bcad's `core::Document` /
`geometry::` split follows the same principle at 2D scale — the GUI
(`app::Viewport`) never touches CGAL types directly, only `geom::Entity`.
Where bcad currently has *no* equivalent is FreeCAD's constraint solver:
there's no parametric/constraint layer in bcad today (entities are drawn at
fixed coordinates, not solved from constraints) — worth keeping in mind as
the ceiling on how "parametric" bcad can become without a much bigger
addition.

Source: [FreeCAD/FreeCAD on DeepWiki](https://deepwiki.com/FreeCAD/FreeCAD).

## UI: dark theme

AutoCAD has shipped with a dark ribbon theme by default since the
`COLORTHEME` system variable was introduced in AutoCAD 2016, on the
rationale that a dark canvas is easier on the eyes over long sessions —
full-saturation entity colors read clearly against low-chroma dark chrome.
`app::MainWindow::applyDarkTheme()` follows the same logic (dark neutral
`#2b2d31`/`#202124` chrome, no theme switcher yet). AutoCAD's ribbon
(tabs → panels → tools) was initially estimated as a materially bigger UI
investment than a flat toolbar justified, and deferred. Revisited once more
tools existed to organize (drawing tools, boolean ops, view/snap toggles):
`app::RibbonBar` (tabs of captioned panels, built on stock
`QTabWidget`/`QToolButton`, no external ribbon library) turned out cheap
enough to build directly rather than staying deferred — see
`CAHIER_DES_CHARGES.md` §2.13. It reuses the same `QAction` objects as the
classic menu bar (kept alongside it, not replaced, for discoverability and
keyboard shortcuts), so there's no logic duplicated between the two.

Source: [AutoCAD 2025 Help — About the Ribbon](https://help.autodesk.com/view/ACD/2025/ENU/?guid=GUID-D20EF1D7-4135-48A7-B68E-65BF3BFF3D70), [Color Theme (COLORTHEME) in AutoCAD](https://knowledge.autodesk.com/support/autocad/getting-started/caas/simplecontent/content/color-theme-autocad.html).
