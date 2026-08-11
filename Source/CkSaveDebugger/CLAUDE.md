# CkSaveDebugger

**Purpose:** UncookedOnly Slate debugger for CK `.sav` snapshot files. It is an **offline** inspector: it opens a
save off disk, hands the bytes to `CkSnapshot`'s inspection API, and renders the resulting document — census,
ownership tree, per-entity recipe, per-payload blobs, and diagnostics. Opened via the `ck.SaveDebugger` console
command or the shared debugger launcher (**Tools** category, slot 30).

**Depends on:** `CkCore`, `CkEcs`, `CkSnapshot`, `CkDebuggerCommon`, `CkEditorTools`, plus `DesktopPlatform` (file
dialogs) and `Json` (export) — neither of which is transitive — and `UnrealEd` behind `Target.bBuildEditor`.
Mirrors `CkAggroDebugger`'s module registration.

---

## Ownership boundary

`CkSnapshot` owns **parsing, validation, decoding and projection**; this module owns **presentation and export
commands**. Nothing here re-implements a read: every fact on screen comes from
`ck::snapshot::Inspect_*` / `TryDecode_*` and the plain-data document they return. CkSnapshot never depends on this
module, and this module never depends on a game module — there are no game-specific decoders and no adapter
registry (deferred out of v1 on purpose).

---

## The no-live-handle invariant

This debugger never instantiates a saved entity, opens the map, calls hydration, mutates a registry, or fabricates
a live `FCk_Handle`. **Every entity reference it shows is a raw saved id from the file.** Two consequences worth
stating because both are easy to break by copying another debugger:

- `SCkDebug_EntityRef` is **banned here.** Its click navigates the ECS Debugger to a LIVE entity; a saved id names
  no such thing. Owner and context-owner links are plain `SCkDebug_KeyValueRow` key buttons that select another
  ROW in this window.
- The module registers **no** `FCkDebug_EntityTargetRoute` and holds no handles, so it needs neither the
  `OnEnginePreExit` teardown hook nor the PIE `EndPIE` clear that handle-holding debuggers require.

It is also **fully offline in the refresh sense**: no world is ever polled and nothing is collected per tick. Every
rebuild is driven by an explicit event — open, reload, filter, provenance chip, ProblemsOnly, selection, decode.
The only per-tick work in the window is the base class's gated style-revision watch, which is why
`SCkSaveDebuggerWindow` does not override `Tick` at all: an override would forward to
`SCkDebugger_WindowBase::Tick` and do nothing else.

---

## Type fallback and the opaque UX

A save names types by path. Whether this editor can SEE a given type is a fact about the editor, not a defect in
the file, so the window never colours it as an error:

| Situation | Presentation |
|---|---|
| Type resolves (`FindObject` hit at document build) | `TYPE OK` pill, Ok tone; `Try Decode` projects a value tree |
| Type not loaded in this editor | `TYPE UNAVAILABLE` pill, **Neutral** tone; `Try Load Type` re-runs the decode with `AllowTypeLoad`, which is the only path permitted to load a package |
| Stream decodes but claims a different type than the save declared | `DeclaredTypeMismatch`, Err tone, BOTH paths reported and **no** value tree — a tree whose type claim is a lie is worse than none |
| `_ActorSaveFieldBytes` | `OPAQUE`, Neutral tone — see below |

An opaque blob stays fully searchable, selectable, diagnosable and exportable: byte count, SHA-256, a bounded hex
preview, `Copy Metadata` and `Export Raw Blob...` all work regardless of whether anything could be decoded.

### The `_ActorSaveFieldBytes` v1 limitation

`FCk_Snapshot_V3_EntityEntry::_ActorSaveFieldBytes` is a class-scoped `SerializeScriptProperties` capture of a
bridged actor's `UPROPERTY(SaveGame)` fields, taken with `ArIsSaveGame = true`. Replaying it needs a live object
instance of that actor class to serialize INTO — which is exactly what an offline inspector must not create. There
is therefore **no decode entry point for it in the inspection API**, and this module must never invent one: it
presents the blob as `UnsupportedBlob` (byte count + SHA-256 + hex preview + raw export) and says why on screen.

---

## Model first

`FCkSaveDebugger_Model` (`Public/CkSaveDebugger/Model/`) is Slate-free and header-visible: the document, the decode
results the user has asked for, the filter/highlight/provenance/ProblemsOnly state, the selection keys, and the
ownership tree built from them. Every predicate and projection is a pure function in the
`ck_save_debugger_model` namespace, so the specs test the behaviour without constructing a widget. The window is a
consumer — if a rule is worth testing, it belongs in the model.

Two shapes are load-bearing:

- **Ownership layout** (`Build_OwnershipLayout`) is computed as plain data before any `TSharedPtr` row exists.
  Rows whose owner id is absent from the table group under one synthetic **Owner [id] — Not Persisted** root per
  absent owner id (the loader-sanctioned encoding of "owned by the transient/world root" — severity of the paired
  `OwnerNotPersisted` diagnostic is per-provenance, NOT a blanket error); every member of an
  owner cycle lands under a synthetic **Owner Cycle** root. The owner-chain walk is coloured
  (unvisited/visiting/settled) exactly like `CkSnapshot`'s own cycle analyzer — **never recursive**, because a save
  is untrusted input and a cyclic one must not be able to hang or overflow the inspector. Both synthetic roots
  correspond to diagnostics the inspection API already emitted.
- **Row identity is stable.** `SListView`/`STreeView` track selection by pointer identity, so rows are reused by
  key (entity `SavedId` + node kind; payload/diagnostic table index) and updated in place. `RequestTreeRefresh` is
  called only when the row set or the ownership SHAPE changed — the shape is hashed into a layout signature
  precisely because a pure reparent adds and removes no keys.

---

## Presentation contracts

The window owns no bespoke look. Every surface is a `CkDebuggerCommon` widget over `FCkDebuggerStyle` brushes and
`CkStyle::` tokens — see [CkDebuggerCommon/CLAUDE.md](../CkDebuggerCommon/CLAUDE.md) for the binding rules. What is
specific to this module:

- **Icon vocabulary** (all ids resolve through `FCkDebuggerStyle::Get_IconBrush`, all rendered through
  `SCkDebug_Icon` — never a bare `SImage`). `Cassette` is this tool's identity (launcher descriptor, toolbar file
  slot, File & Header panel); `Chest`/`Disc`/`Scroll`/`Clipboard` are open/reload/export/copy; `Scale` the save
  diff (toolbar command, panel header, group detail) with `Door` closing it and `Anchor` the show-unchanged
  toggle; `Key` identity,
  `Web` ownership, `Book` recipe, `Compass` transforms, `Package` payloads, `Lock` raw/opaque bytes, `Gear` decode,
  `Bulb` load-the-type, `Bug` diagnostics, `TreasureMap` the empty ownership selection.
- **Provenance and node-kind glyphs are model rules, not widget rules.**
  `ck_save_debugger_model::Get_ProvenanceIconId` maps the four provenances to four DISTINCT ids and
  `Get_NodeKindIconId` gives the synthetic roots their own (`Ghost` non-persisted, `Trap` cycle). Rows carry the
  resolved id in `FCkSaveDebugger_TreeNode::IconId`, so a reused row never re-derives it.
  `Ck.SaveDebugger.Model.RowPresentation` pins the distinctness.
- **`Get_NodeKindTone` is the tone authority for a row's own kind**: `NonPersistedOwnerRoot` is NEUTRAL (normal
  top-level shape), `CycleRoot` is Err. Same reasoning as the opaque-UX table above — never colour an observation
  about the environment as a defect in the file.
- **Name depth.** The toolbar hosts an `SCkDebug_NameDepthCycler` bound to a window-owned `_NameDepth`; every type
  path renders through `SCkDebug_NameLabel::Get_ShortName` with the full path in the tooltip. `_MaxNameDepth` is
  recomputed in `DoRebuild_All`, never on a paint pass — this window does no per-tick work.
- **Row-safety.** All five views (`ownership tree`, `payloads`, `diagnostics`, `value tree`, `diff groups`) build rows from
  `STextBlock` / `SBox` / `SBorder` / `SCkDebug_Icon` only, use the translucent `CkDebugger.TableView.Row` style,
  and keep their copy actions on `OnContextMenuOpening`. Badge chrome uses the registered
  `CkDebugger.Badge.Rounded` / `CkDebugger.Panel.Border` brushes — a row generator must never allocate one.

---

## Save diff

**The OPEN save is the CURRENT side.** `Compare Against...` (toolbar, `Scale` glyph) picks the **baseline** — the
older file — inspects it with `ck::snapshot::Inspect_SaveFile`, and runs
`ck::snapshot::Diff_Documents(picked, open)`. Get that direction backwards and every tone in the panel inverts.

The baseline **document is dropped the moment the diff returns**; only its `Get_SourceDescription()` is kept. A
`FCk_SnapshotInspection_Diff` is self-contained by contract, so a second whole document on the model would be state
with no projection behind it — and twice as much for the no-live-handle invariant to keep honest.

- **The diff dies with the document.** `Set_Document` (so also `Reset`, and so also a plain **Reload**) calls
  `Clear_Diff`: a comparison whose CURRENT side was swapped out underneath it describes nothing on screen.
  `Ck.SaveDebugger.Diff.DiffLifecycle` pins that, plus the invalid-diff case — an invalid diff is still HELD (the
  panel prints its `_InvalidReason`) but lists no groups.
- **Groups, not rows.** Saved ids are not stable across captures, so the diff matches by identity path and the
  window keys its list rows by `IdentityPath` — the only key a group survives a re-diff under. The list is never
  re-sorted: `Diff_Documents` already ordered it biggest-population-change first, and a second sort site is a
  second place for that contract to drift.
- **Show unchanged** (`SCkDebug_IconToggle`, default off) is the only thing that filters the list; the diff itself
  always carries the Unchanged groups so the report can document what did NOT move.
- Selecting a group with a current-side member routes through the existing `DoSelect_Entity` path, so the ownership
  tree lands on the group's first current-side saved id. A `Removed` group names nothing the open save contains and
  therefore selects nothing — the detail panel is the whole answer there.

### Tone table — growth is the suspect direction

`ck_save_debugger_model::Get_DiffGroupTone(kind, countDelta)`. A leak reads as a population that only ever went up,
so growth earns the warning and disappearance is merely an observation — the same "never colour an environment fact
as a defect" reasoning as `Get_NodeKindTone` and the opaque-UX table above.

| Group kind | Tone |
|---|---|
| `Added` | Warn |
| `Removed` | Info |
| `CountChanged`, count grew | Warn |
| `CountChanged`, count shrank | Info |
| `PayloadsChanged` | Accent (no population moved at all — not a severity) |
| `Unchanged` | Neutral |

`Get_DiffDeltaTone(delta)` applies the same reading to a bare census delta (`> 0` Warn, `< 0` Info, `0` Neutral).
`Ck.SaveDebugger.Diff.KindTones` pins both tables and the three text formatters.

### Diff report determinism contract

`Build_DiffReportText` holds to the same rules as the JSON export: every array is walked in the order the diff
already sorted it (never re-sorted here), the census is emitted in the fixed provenance order, and nothing time-,
pointer- or environment-derived enters it — the only inputs are the diff's own fields and the two source
descriptions. It is a **Copy action only**: `Build_JsonExport` is untouched and carries no diff in v1.
`Ck.SaveDebugger.Diff.DiffReportDeterminism` builds it twice off one model and once off an independently rebuilt
model fed an independently computed diff.

### Layout

While a diff is live the right-hand column switches (`SWidgetSwitcher`) from blob inspection to the diff panel — a
diff session is a different task, and the ownership tree and payload list stay beside it either way. `Close Diff`
gives the column back. The panel's chrome above the list is attribute-bound rather than rebuilt (the diff is
immutable once computed); only the selected group's payload-type table is a rebuild.

### `[EDITOR-VERIFY]`

Open a save, then **Compare Against...** an older copy of it. Confirm: the group list orders the biggest change
first and every kind badge's tone matches the table above; **Show unchanged** adds the flat groups and removes
them again; clicking a group selects its row in the ownership tree (and a `Removed` group selects nothing without
erroring); right-click → Copy Group on a multi-selection joins with newlines; **Copy Diff Report** pastes the census
deltas, the group table and the payload-type table; **Close Diff** restores the blob column with the previously
selected payload still shown; **Reload** with a diff open drops it (the column returns to blobs by itself). Point
it at an unreadable file and confirm the panel shows the `DIFF UNAVAILABLE` pill and the reason, with no list.

---

## JSON export determinism contract

`Build_JsonExport` must produce **byte-identical output for identical input, every time**. The rules it holds to:

1. Every array is emitted in an explicitly sorted order — entities by saved id, payloads by (owner id, type path),
   diagnostics by (severity desc, code, message) — with the table index as the final tie-break so equal keys never
   depend on container order.
2. Value-tree children are the one deliberate exception: their ORDER is the data (property order, container order)
   and is never sorted.
3. Field names are camelCase and the field set is fixed — nothing is emitted conditionally except a decode block
   the user actually requested.
4. **Hashes are always present** (source, snapshot bytes, per-payload) even when the read failed.
5. **No raw blob bytes, ever** — blobs appear as a bounded hex preview (~256 bytes, `Format_HexPreview`). Getting
   the real bytes is the separate per-blob `Export Raw Blob...` action.
6. Nothing time-, pointer- or environment-derived enters the document: no wall clock, no addresses, no iteration
   artefacts.

`Ck.SaveDebugger.Export.JsonDeterminism` pins 1-6 by exporting twice from one model and once from an
independently built twin.

---

## Tests

`Private/Tests/*.spec.cpp`, whole file inside `#if WITH_DEV_AUTOMATION_TESTS`, `IMPLEMENT_SIMPLE_AUTOMATION_TEST`,
`EditorContext | EngineFilter`, names `Ck.SaveDebugger.<Group>.<Case>`. Fixtures build a real document — hand-built
`FCk_Snapshot_V3_Tables` → `SerializeItem` → `UCk_Snapshot_SaveGame` → `ck::snapshot::Inspect_SaveGameObject` — so
the diagnostics and `HasProblems` flags under test are the ones the shipping analyzer produces.

**`BB_QuickSave.sav` is never an automated fixture.** Real saves belong to manual `[EDITOR-VERIFY]` passes only.

---

## Verification

- Run `Ck.SaveDebugger` for the model, export and registration coverage.
- Run `Ck.DebuggerLauncher`; its catalog spec asserts the exact tool census, so adding or renaming this tab id
  requires editing `CkDebuggerLauncher/Private/Tests/CkDebuggerLauncherCatalog.spec.cpp` in the same change.
- `[EDITOR-VERIFY]` Open **Tools > Debug > CK Save Debugger** (or `ck.SaveDebugger 1`). Open a real `.sav`;
  confirm the summary strip, the compatibility pill's tone, and the ownership tree. Filter and highlight; toggle
  each provenance chip and Problems Only. Select an entity, follow its owner link, select a payload, and click
  `Try Decode` then `Try Load Type` on a type the editor does not have loaded — confirm the pill stays neutral,
  not red. Export JSON and re-export; confirm the two files are identical. Export a raw blob. Click a diagnostic
  and confirm it selects its entity and payload rows. Cycle the toolbar's Name control and confirm the payload
  list and the recipe rows re-shorten together; collapse and expand each inspector panel; confirm a
  **Owner [id] — Not Persisted** group renders muted, not red, while an **Owner Cycle** root renders red.
