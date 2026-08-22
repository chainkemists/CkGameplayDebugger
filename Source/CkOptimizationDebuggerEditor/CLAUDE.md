# CkOptimizationDebuggerEditor

**Purpose:** the one thing the optimization debugger needs that a runtime module cannot provide — a
reflected `UEdMode` that draws measured performance positions over the level-editor viewport.

**Type:** `Editor` — never loaded in a packaged build, never loaded in the `-game` measurement child.
**Depends on:** `UnrealEd`, `EditorFramework`, `CkCore`, `CkEcs`, `CkEditorTools`, `CkDebuggerCommon`,
`CkPerfLab`.
**Used by:** nothing. It is discovered, not called.

---

## Why this module exists at all

`UEdMode` is a `UCLASS` that `UAssetEditorSubsystem` discovers by scanning CDOs after modules load.
That requires `UnrealEd`, which `CkOptimizationDebugger` must not link — it is a runtime module that
ships in Development builds. So the mode lives here, in the second Editor module in this plugin, and
`CkSaveDebuggerEditor` is the precedent it was built from.

Because discovery is CDO-based and nothing references the class, the module's `StartupModule` holds a
deliberate `(void)UCk_PerfLab_HeatmapEdMode::StaticClass();`. It is not dead code — without it the
linker is free to strip the reflected class and the mode silently stops existing.

---

## What's here

```
CkOptimizationDebuggerEditor/
├── CkOptimizationDebuggerEditor_Module.cpp   – the StaticClass() reference that preserves discovery
└── Heatmap/
    └── CkPerfLab_HeatmapEdMode.h/.cpp        – the mode, and its hit proxy
```

---

## The mode holds no state

Every `Render` pulls the immutable snapshot from `ck::perf_lab::heatmap`, and every click pushes a
position id back through the same slot. The mode owns nothing that could go stale and nothing that
could outlive the debugger window — which matters because the window can be closed, reopened, or
never opened at all while the mode is active.

It spawns no actors, adds no components, and opens no transaction. **The level is never modified in
order to display a measurement of it.** A heatmap that dirtied the map would be a tool nobody dares
leave switched on.

The hit proxy carries an `FString` position id, never a handle or a pointer. It crosses from a
viewport click into a Slate page through a global slot, and a live reference on that path is exactly
the lifetime bug the debugger's no-live-handle invariant exists to prevent.

---

## Two refusals, both load-bearing

**It will not draw a session measured against a different map.** Positions are world coordinates; a
session from another level would place markers somewhere arbitrary in this one, and they would look
exactly as authoritative as real ones. `Render` compares the editor world's package path against the
snapshot's and returns early on a mismatch.

**It will not draw a position that was never measured.** `Build_Snapshot` emits no marker for one, so
the cool end of the ramp always means "measured, and fast" rather than "no reading". This is the same
zero-as-data mistake `CkPerfLab` refuses one layer down, expressed visually.

---

## Encoding is redundant on purpose

Colour (`ck::debug_axes::Get_HeatColor`), shape (sides by severity band), and size (over-budget
multiple) each carry the result independently. Colour alone would exclude a deuteranopic reader from
the entire feature; shape carries the same message without it. Size is clamped so one catastrophic
position cannot paint over the rest of the level.

The ramp comes from `CkDebuggerCommon` rather than being chosen here, so this tool cannot drift from
every other debugger surface that means the same thing by the same colour.

---

## Anti-patterns

- **Do not give the mode member state that mirrors the slot.** It would be the copy that goes stale.
- **Do not put a handle, pointer, or `UObject*` in the hit proxy.** Object paths and ids only.
- **Do not spawn anything, or mark the level dirty.** Not for a preview, not temporarily.
- **Do not skip the streamer-mode check.** A performance overlay is exactly what
  `ck::debug_draw::Is_SuppressedForStreamerMode()` exists to hide.
- **Do not add gameplay or analysis logic here.** This module draws; `CkPerfLab` decides.

---

## See also

- `Source/CkPerfLab/Claude.md` — where the snapshot comes from, and the availability contract
- `Source/CkOptimizationDebugger/CLAUDE.md` — the page that publishes the snapshot
- `Source/CkSaveDebuggerEditor/` — the precedent this module copies
