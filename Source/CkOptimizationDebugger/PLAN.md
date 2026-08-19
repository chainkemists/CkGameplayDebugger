# CkOptimizationDebugger — campaign plan

| | |
|---|---|
| **Last updated** | 2026-08-16 |
| **Owner** | optimization-toolkit campaign |
| **Status** | P0 done (98/98); P1 done (103/103); P2 done (107/107); P3 done (111/111); P4 done (gate deferred into P5's); P5 done; P6 done (123/123) — **all six phases implemented**. P7 (review fixes, 2026-08-11) done (126/126). P8 (external-reference correctness + name collisions, 2026-08-16) done. P9 (shared severity iconography, narrowing filters, four new fixes, 2026-08-16) done — remaining work is the [EDITOR-VERIFY] walkthroughs in CLAUDE.md and shipping. |
| **Feature spec (source of truth for the check list)** | `F:\optimization-toolkit-goal.txt` |
| **Pattern brief (naming, module type, deps, registration, widgets, styling)** | `C:\Users\sulfu\AppData\Local\Temp\claude\E--Repos-CkPlugins-Other\4e6182fb-2226-4d2b-b6dd-5716e1692c90\scratchpad\optimization-pattern-brief.md` |
| **Module authoring doc** | [CLAUDE.md](CLAUDE.md) |

**Freshness rule:** if this file and the code disagree, the code wins — fix this file in the same change. A phase
is only "done" when its gates below are green on the FINAL artifact, not on a run that predates the last edit.

---

## Phases

| # | Phase | Scope | Status |
|---|---|---|---|
| **P0** | Module skeleton + registration | `Source/CkOptimizationDebugger/` module (Build.cs, `_Module.{h,cpp}`, `.uplugin` DeveloperTool entry), Slate-free model (pages, severities, categories, targets, finding rows, filter state, pure projections), window shell (chrome + page bar + switcher + placeholder pages + disabled Scan + status strip), per-user threshold settings, registration + model specs, launcher census entry, module CLAUDE.md, this plan | **Done** — gate green **98/98, 2026-08-11** |
| **P1** | Analysis engine + findings page | Scan orchestrator over the persistent level and its loaded sub-levels behind a cancelable `FScopedSlowTask`; a plain copyable threshold struct populated from the settings object once per scan; **28 checks** across seven families (mesh / texture / material / lighting / actor / blueprint / project settings) in `Analysis/Checks/`; findings list UI — dual search (filter + highlight), severity toggles in the chrome icon row, category toggles above the list, flat list with per-check group headers, right-click copy menu, detail panel; session-invalidated clear; model additions (grouping projection, worst-visible severity, scan info, highlight predicate) | **Done** — gate green **103/103, 2026-08-11** |
| **P2** | Fixes + navigation | `Fixes/CkOptimizationDebugger_Fixes.{h,cpp}` — a registry keyed by check id (verb, transactional, destructive, needs-rescan) over the **ten** fix-bearing checks, pure projections (`Can_ApplyFix`, `Get_FixableFindings`, `Partition_ForBatch`, `Build_FixButtonLabel`), single apply inside its own `FScopedTransaction` and batch apply inside ONE outer transaction with the config write separated out after it. `Fixes/CkOptimizationDebugger_Navigation.{h,cpp}` — Content Browser sync for assets, select + frame for actors, `ISettingsModule::ShowViewer` for settings findings, with the container/category/section asked of the settings object rather than hard-coded. Window: Go To + Apply Fix buttons in the detail panel, both actions in the context menu, double-click to navigate, one shared `DoRun_Scan` that the post-fix refresh reuses. Fix catalog and the two downgrades (LOD group instead of the reduction interface, bounds box instead of a fitted hull) are in [CLAUDE.md](CLAUDE.md) | **Done** — gate green **107/107, 2026-08-11** |
| **P3** | Dashboard | `FCkOptimizationDebugger_ScanSummary` folded out of the SAME gather the checks read (actors, mesh usages, unique meshes / materials / textures, LOD0 triangle total over unique meshes, lights by mobility, findings by severity, per-level rows including the excluded and the unloaded); `_PreviousSummary` rotated by `Set_Summary` with a pure `Get_SummaryDelta` projection; `Analysis/CkOptimizationDebugger_DiskScan.{h,cpp}` — package-granular `/Game` disk breakdown by asset family through `IAssetRegistry::GetAssetsByPath` + `FAssetPackageData::DiskSize`, behind the scan's own slow task; per-level exclusion set persisted in `UCkOptimizationDebuggerSettings::ExcludedLevelNames` and honoured by `Run_Scan` through the pure `ShouldScanLevel` seam. Dashboard UI: six delta-bearing `SCkDebug_StatPair` tiles, a clickable severity strip that solos a severity on the Findings page, a `SCkDebug_MeterBar` disk breakdown, a plain-`SVerticalBox` level list with `SCkDebug_Switch` toggles, a reflection-driven `SCkDebug_NumericEditor` threshold panel behind `FCkInspectorEditGuard`, and an empty-state call to action before the first scan | **Done** — gate green **111/111, 2026-08-11** |
| **P4** | Memory analyzer | `Analysis/CkOptimizationDebugger_MemoryScan.{h,cpp}` — a `TObjectIterator` pass over RESIDENT content (independent of the level scan and of `WITH_EDITOR`), producing plain rows for textures, render targets and static meshes with `GetResourceSizeEx(Exclusive)` sizes, a separable video-memory figure where the engine reports one, and guarded texture-streaming metrics behind `IStreamingManager::Get_Concurrent()`. Model additions: memory rows + streaming availability + a page-local filter string, and pure projections (`Get_SortedMemoryRows` with a non-flipping path tie-break, `Compare_MemoryRows`, `Matches_MemoryFilter`, `Get_MemoryTotals`, the column/table id round-trips and the cell-text/sort-key helpers). Page: `SCkDebug_UnderlineTabs` sub-table selector with per-table counts, three `SCkDebug_StatPair` header tiles plus Refresh, an `SHeaderRow`-sorted `SListView` of `SMultiColumnTableRow`s with a per-row `SCkDebug_MeterBar`, a single debounced `SCkDebug_SearchBar`, a multi-select copy menu, double-click Content Browser sync through the new shared `Navigate_ToTarget`, and a dim streaming-unavailable note. Contracts in [CLAUDE.md](CLAUDE.md) §"The memory analyzer" | **Done** — gate deferred: the P4 run was blocked by an unrelated environmental flake, so the P5 gate covers both |
| **P5** | Profiling launcher | `Commands/CkOptimizationDebugger_ProfileCommands.{h,cpp}` — a plain-data catalog of **27 entries** across six shelves (8 timing stats, `profilegpu`, 6 view modes, 5 Nanite / 4 Lumen / 3 VSM visualizers), each carrying exactly ONE armed lever; an editor-only executor that drives `GCurrentLevelEditingViewportClient` through `ExecEngineStat` / `SetViewMode` / `Change*VisualizationMode` rather than console strings; pure rules (`Is_CommandActive`, `TryGet_ActiveViewportCommandId`, `Get_ActiveCommandCount`, `Push_RecentCommand`) over a plain viewport-state struct. Page: six `SCkDebug_SectionHeader` shelves of wrapping `SCkDebug_IconToggle`s in an `SScrollBox`, a live `SCkDebug_StatusPill` naming the active viewport mode, a per-visualizer-shelf **Lit** reset reusing the one catalog entry, and a pinned `SEditableTextBox` console box with a five-deep re-run rail. **Toggle state is READ off the viewport, never recorded** — see [CLAUDE.md](CLAUDE.md) §"The profiling launcher" | **Done** — covered by the P6 gate |
| **P6** | Project cleanup | `Analysis/CkOptimizationDebugger_CleanupScan.{h,cpp}` — an asset-registry pass over `/Game` behind its own four-step cancelable `FScopedSlowTask`, producing plain rows in four review categories: unreferenced (`GetReferencers` over the `Package` category with `NoRequirements`, i.e. hard AND soft, minus a five-entry always-rooted class list), possible duplicates (name + class + disk size, never content), redirectors (labelled with a referencer count rather than a loaded destination), and dirty packages (`FEditorFileUtils::GetDirtyPackages`, the one live-state category). `Commands/CkOptimizationDebugger_CleanupCommands.{h,cpp}` — a plain-data catalog of **three** actions with pure selection rules (`Get_ApplicableRows`, `Can_RunAction`, `Get_ActionDisabledReason`, `Build_ActionButtonLabel`) and one editor-only executor returning `{DidRun, Message}`: `ObjectTools::DeleteAssets` with `bShowConfirmation = true`, `IAssetTools::FixupReferencers`, and `FEditorFileUtils::SaveDirtyPackages` with its prompt. Model additions: cleanup rows, the duplicate grouping, per-category totals, a page-local filter, and a `Reset()` that deliberately KEEPS the census. Page: an explicit **Scan Project** button, five `SCkDebug_StatPair` tiles, four `SCkDebug_UnderlineTabs` categories over one `SListView` with duplicate group headers, one category-following action button with disabled-reason tooltips, a copy/navigate context menu, and an empty state. **Deletion always goes through the engine's own dialog** — see [CLAUDE.md](CLAUDE.md) §"The project cleanup pass" | **Done** — gate green **123/123, 2026-08-11** |
| **P7** | Review fixes | **2026-08-11.** Adversarial review of the whole module; 11 Major and 14 Minor findings applied. Correctness: `Run_CleanupScan` now reports `RequiresEditor` outside the editor (it printed a clean project it had never scanned); the texture-streaming fix uses the CHECKED `TryUpdateDefaultConfigFile` and reads the cvar back, rolling the CDO back on failure; `SaveDirtyPackages` counts the dirty set before and after instead of trusting a return value that treats "Don't Save" as success; `FixupReferencers` reports "handed to the engine" and suppresses the re-scan when it defers; `Lighting.LightmapResolution` aggregates per actor so its stable keys are unique; the sRGB and Nanite fixes re-validate their check's WHOLE condition. Safety: destructive/config-write batches ask first (`Build_BatchConfirmation`); both actor-destroying fixes respect `FLevelUtils::IsLevelLocked` and clean up `USelection`; fixes and cleanup actions are refused during PIE with a reason. Performance: every tab/sub-tab count reads a cached field rather than walking its census twice per frame on every page, the three filter predicates answer an empty query before allocating a haystack, and the profiling tab count uses allocation-free live variants. Honesty: the light-mobility verb is "Select Lights For Review", the instancing finding says "up to N", `HasSeparableGpuSize` is type-based, dirty-package rows are dropped at a PIE boundary, and `ApplicationCore`/`DesktopPlatform`/`Json` are gone until the export they were declared for actually lands. Dead code removed: `RequiresRescanOfAssets`, `Get_AllFixCheckIds`, `_SuppressSelectionEcho`. Full disposition in the review record | **Done** — gate green **126/126, 2026-08-11** (one spec-side regression, `Memory.Totals` vs the type-based GPU rule, reconciled in the same phase) |

| **P8** | External-reference correctness + name collisions | **2026-08-16.** Two changes, one a defect fix and one a new category. (1) `Run_CleanupScan`'s unreferenced walk consulted only the on-disk package graph, so any asset reached solely through an AngelScript generated `assets::` accessor — a text call with no dependency edge — was reported as unreferenced and offered to `ObjectTools::DeleteAssets`, with the engine's own delete dialog agreeing because it reads the same graph. New `FCk_AssetReferenceProviderRegistry` in `CkCore/Reference/` inverts the dependency: `CkAngelscriptGenerator`'s asset subsystem registers a query under `"AngelScript"` backed by the same two maps its `OnAssetsPreDelete` warning reads, and the walk asks the registry alongside the graph. A claimed asset is suppressed AND counted (`ExternallyReferencedCount`), never dropped silently; the three states — claimed / none claimed / **no provider registered** — are distinct on the status strip, and the third is Warn-toned because "nobody was there to ask" is not "asked and found none". (2) New `NameCollisions` cleanup category: assets sharing an exact NAME across folders whatever their class or size. A different question from `Duplicates`, not a relaxation of it — and load-bearing here because the AngelScript accessor generator is keyed on the asset NAME and renames the loser `<Name>_DUP1` in registry-iteration order, so `assets::Get_Foo()` can resolve differently per machine. Carries no action on purpose (the fix is a rename). Supporting renames: `DuplicateGroupKey` → `GroupKey`, `FCk_..._CleanupDuplicateGroup` → `..._CleanupGroup`, `Get_CleanupDuplicateGroups` → `Get_CleanupGroups(rows, category, filter)`, plus `Get_IsGroupedCleanupCategory` as the single grouping predicate. Catalog rule relaxed from "exactly one action per category" to "at most one", with the null asserted by name | **Done** — see the Gates note below |

| **P9** | Severity iconography + narrowing filters + four fixes | **2026-08-16.** Three of the reviewed proposals, in the order that avoided rework. (1) **Shared severity glyphs.** Four SVGs (`Severity_{Error,Warning,Info,Success}`) at `Resources/Icons/` ROOT — not `General/`, which is the pool a feature is assigned a glyph from at random — plus `ck::debug_axes::Get_ToneIconId(ECk_Tone)` beside the colour ramps, so tone → colour and tone → glyph cannot drift. `Get_SeverityTone` now feeds both. Swept the five `Skull` sites that all meant *failure* (this module's Critical, the gallery's "Failed", Eqs' failed candidates, Crowd's world trouble, Goap's pause-on-plan-failed) onto `Severity_Error`. `FAppStyle` was rejected: its brushes sit outside Style Lab and it is an editor style set in modules that ship packaged. (2) **Three narrowing filters** on `FCkOptimizationDebugger_FilterState`: a real `PathScope` prefix axis (typing a path into the free-text box also matched titles — one control, two questions), `ShowOnlyWithSuggestedFix` reading the check's `HasAutoFix` with the affordance worded as the narrower claim, and a `StableKey`-keyed mute set persisted per-user beside `ExcludedLevelNames`. Muting is kept honest by two things, both required: the count of muted findings **in the current scan** printed beside the toggle rather than in its tooltip, and a `MUTED` chip on any revealed row. (3) **Four new fixes**, taking coverage 10 → 14 of 28: `Texture.MissingMipmaps`, `Mesh.NaniteMaterialIncompatible` (re-asking the newly exported `Is_NaniteIncompatible`), `Lighting.LightmapResolution` (every over-budget component, level-lock respected, threshold read fresh) and `Blueprint.TickEnabled` — the last carrying a NEW `ChangesBehavior` flag rather than a widened `IsDestructive`, because one field meaning two risks leaves the confirmation unable to say which it is asking about. `Texture.MaxSize` was dropped after review: it would write a per-user threshold into a shared asset | **Done** — gate green: `Ck.OptimizationDebugger` **36/36**, `Ck.DebuggerCommon` **41/41**, `Ck.DebuggerLauncher` **3/3** |
| **P10** | Texture built-size accuracy + Open Asset + snapshots (capture half) | **2026-08-18.** Three items from the user's report, planned in `docs/plans/2026-08-18-optimization-accuracy-and-snapshots/`. (1) The three size-judging texture checks read `UTexture::GetBuiltTextureSize` — the engine's own PowerOfTwoMode / ResizeDuringBuild / MaxTextureSize / LOD-bias rule, and the number the texture editor prints as "Max In-Game" — instead of the imported source size, so a 4096 source capped to 2048 at build no longer reports as over a 2048 budget. It reads the serialized source header only, so the no-DDC-build fence the source path exists for still holds; `Analysis.EffectiveTextureDims` pins the four cases. (2) `Can_OpenAsset` / `Get_OpenAssetDescription` / `Open_TargetAsset` join the navigation namespace as the single implementation behind a third detail-panel button and an entry in both context menus, single-selection only. (3) A sixth **Snapshots** page: plain-data snapshot types with six spec-driven pure rules (RLE codec, bounds-checked pixel read, letterbox mapping, click semantics, LOD0 selection totals, the approximate draw-call label), model storage that survives `Reset()`, two per-user settings, and a capture engine + viewer that stores the camera's view as a compressed image plus a table of every static / instanced / skeletal mesh in frame | **Partial** — code Done (gates 42/42 → 43/43 → 50/50 → 50/50, all 2026-08-18); the **editor-verify sweep is outstanding**, and this phase's deliverable is visual, so it is not Done until a human has looked |

| **P11** | Per-pixel mesh identification + selection UI | **2026-08-18.** The stencil-batch ID map (≤255 primitives per CustomDepth-Stencil pass, every candidate rendering custom depth in EVERY pass so occlusion stays honest, all passes in one game-thread scope, a ledger that restores foreign custom-depth users and the `r.CustomDepth` cvar on every exit path); the debug-dump harness that proves silhouette-exactness to a human; and selection mode over it — delta-gated hover, an overlay rebuilt only on change and never per paint, an ID map decoded once per snapshot, and a detail panel rebuilt on selection change but never on hover. Identity renders through CkUsf's **`StencilId`** look rather than a hand-authored material: `.ush` + an AngelScript asset declaration, at a new `ReplacingTonemapper` blendable location added to CkUsf for it (CkFoundation `84d117c84`). Pure rules pinned by `Snapshot.{StencilBatching, PassResolution}` | **Code complete, visually unverified** — gates 52/52 throughout; needs `Ck_Usf_GenerateLooks StencilId` plus the harness comparison. See `docs/plans/2026-08-18-optimization-accuracy-and-snapshots/PROGRESS.md` |

| **P12** | Snapshot QA round: panel, stats, report, save/load | **2026-08-18.** Adam's feedback after verifying the core loop. Richer capture data (LOD vertices and authored screen size, the mesh asset's exclusive resource size, per-slot texture names, deduplicated unique-material / unique-texture counts and resident texture bytes) behind ONE `Get_SnapshotAggregates` projection the panel and the report both read. An `SSplitter` in place of the fixed-width facts panel and an `SScrollBox` over it, fixing a live overflow on a many-slot mesh. `Model/CkOptimizationDebugger_SnapshotReport.{h,cpp}` — a PURE self-contained-HTML builder (embedded base64 images, escaped names, worst-first table, timestamp handed in) landing the export the module doc had reserved for whatever change could arrive WITH a determinism spec. `Model/CkOptimizationDebugger_SnapshotCodec.{h,cpp}` — a versioned `.cksnap` file that round-trips every field including the selection, decoding whole-or-nothing, plus a bare-`.png` load path for any screenshot. `DesktopPlatform` returns to the MAIN dependency block because QA saves from packaged Development | **Code complete, UNGATED** — committed `f13e900` without a build at Adam's direction; the pattern gate is owed |
| **P13** | Optimization lenses, mesh list, A/B compare, auxiliary views | **2026-08-18.** The ID map says which mesh owns every pixel and the prim table says what that mesh costs, so **every per-mesh statistic is a heatmap a pure function can paint** — eleven lenses (triangle density, samplers, texture and mesh memory, instances, distance, Nanite / material-flag / unidentified masks, and a budget lens painting the analysis engine's own grading), each with a legend line saying what its colours mean. Screen coverage — pixels per prim, counted in the decode picking already pays for — turns "200k triangles" into "200k triangles across forty pixels" and feeds the panel, the report and the density lens. Codec **v2** (no v1 compatibility): the snapshot carries its POV, its capture context (`sg.*`, screen percentage, build version) and auxiliary images, which is what makes "Recapture From Here" and cross-machine comparison possible. A sortable mesh list whose selection IS the picture's selection, guarded against echo. `Build_SnapshotDelta` — a pure A/B comparison keyed by mesh ASSET, aggregating placements, worst regression first. Solo isolates the selection; the list's context menu copies paths. Auxiliary base-colour and world-normal captures ride the same component, POV and game-thread scope | **Code complete, UNGATED and visually unverified** — five commits (`a6e326a`, `b6283c3`, `854721c`, `a2ddc4f`, `16420b9`), no build run at Adam's direction |

---

## Gates

Run by the **orchestrator session**, never by a phase's own implementation agent (a machine-wide build lock exists;
concurrent builds and test runs poison each other):

1. Toolbox build — `CkAuto/UnrealToolbox.exe --build …`. Never `Build.bat`, UBT or `UnrealEditor-Cmd` directly.
2. `--test-pattern OptimizationDebugger` — this module's registration and model specs.
3. `Ck.DebuggerLauncher` — the catalog census spec, which asserts the exact tool list and that every
   category/order slot is unique. Slot **Tools/40** is this tool's; 10/20/30 are Insights, Style Lab and Save.

The verdict is the log's `=== Test summary ===` block, on a run that postdates the last source edit. A green run
from before an edit is stale-green, not green.

**Anything visual is `[EDITOR-VERIFY]` and deferred to the user** — page-bar switching, glyph rendering, launcher
placement, tooltip text, and every scan result over real project content. Say `[EDITOR-VERIFY]` rather than
claiming a PIE or editor outcome.

---

## Decisions already taken (do not re-litigate)

1. **Name is `CkOptimizationDebugger`**, not `CkOptimizationToolkit` — every Gen-2 tool is `Ck<Feature>Debugger`.
2. **`DeveloperTool`, not `UncookedOnly`** — the 2026-08-11 shipped policy for this plugin. No Editor companion
   module: one is only needed to host a reflected EdMode, and this tool has none.
3. **No `FCkDebug_EntityTargetRoute`** — an offline asset/level analyzer targets no ECS entity, and a route that
   only opens a tab is invalid.
4. **Page bar lives at the top of the BODY**, not in the chrome toolbar — matching `CkIntentDebugger` and
   `CkGoapDebugger`. The toolbar holds commands (Scan, and later Export).
5. **Thresholds are per-user** (`config=GameUserSettings`, Editor Preferences → Ck), not project config.
6. **The window does not override `Tick`** — see the no-live-handle invariant in `CLAUDE.md`.
7. **`Lighting.MovableLightCount`'s fix SELECTS the movable lights rather than changing their mobility** (settled in
   P2). A light that genuinely moves must stay Movable and no offline rule can tell which those are; the spec's own
   wording for this one is "Review and adjust light mobility", and reviewing is what it does. It keeps `HasAutoFix`
   and carries `RequiresRescanOfAssets = false`.
8. **Config-write fixes are never inside the batch transaction** (settled in P2). Undo does not reach
   `DefaultEngine.ini`; `Partition_ForBatch` is where that separation lives.
9. **No spec applies a fix** (settled in P2). Every apply path mutates a real asset, level or config file. The
   registry, the partition and the selection projection are the testable parts; the mutations are `[EDITOR-VERIFY]`.
10. **The dashboard's counts are the WHOLE scan, the Findings tab's are the filtered ones** (settled in P3). The
    Dashboard tab's `CountText` and warn dot read `Get_CountsBySeverity`, never `Get_VisibleCountsBySeverity`: a
    filter typed on another page must not make the headline drop and read as a level that got better.
11. **A level toggle never re-scans** (settled in P3). It is a statement about the next scan; re-walking a large
    world because somebody flicked a switch is not what the switch says it does.
12. **The threshold rows come from reflection over `UCkOptimizationDebuggerSettings`, not a hand-written table**
    (settled in P3). A second list is a second place to forget when a threshold is added, and it would let this
    panel and Editor Preferences disagree about what a threshold is called. Rows sort by label with the property
    name as the tie-break; `TFieldIterator`'s own order is an implementation detail nothing should render from.
13. **The deferred threshold rebuild runs on a one-shot `RegisterActiveTimer`, not inline** (settled in P3).
    `OnEditStateChanged` fires inside the text box's own commit handler, and rebuilding there would destroy the
    widget still on the stack. The window still overrides no `Tick`.
14. **The disk breakdown is package-granular and loads nothing** (settled in P3). `FAssetPackageData::DiskSize` is a
    package's size, so exactly one family may claim it — the primary asset's. Classification uses
    `FAssetData::IsInstanceOf` with `EResolveClass::No`; an audit tool must not load a class, or trigger a DDC
    build, to describe an asset.
15. **The memory analyzer walks the object graph, not the world** (settled in P4). "What does this level cost" and
    "what is loaded right now" are different questions with different answers — the editor holds thumbnails,
    previews and PIE leftovers that no level places and that really cost memory. The two scans never trigger each
    other, and the memory page is NOT behind `WITH_EDITOR`: `TObjectIterator` and the streaming manager are runtime
    facilities, so it answers in a packaged Development/DebugGame build too.
16. **`EResourceSizeMode::Exclusive`, not `EstimatedTotal`** (settled in P4). Exclusive is what an object costs right
    now — resident mips for a texture; `EstimatedTotal` is the editor's "maximum required memory" estimate over
    every possible mip plus the serialized `UObject` graph. This page's claim is "what is resident".
17. **The GPU column reports an em dash where the engine reports no separable figure** (settled in P4). Only
    `UTexture2D` adds through `AddDedicatedVideoMemoryBytes`; `UTextureRenderTarget2D` and `FStaticMeshRenderData`
    add through `AddUnknownMemoryBytes`, and so do virtual textures. Printing `0 B` for those would be a claim they
    never made.
18. **The streaming manager is asked through `Get_Concurrent()`, never `Get()`** (settled in P4). `Get()` lazily
    CONSTRUCTS a manager collection; a tool that summoned the subsystem it was asked to describe would be reporting
    on its own side effect. Three states are on the record — Available / StreamingDisabled / ManagerUnavailable —
    and the last two print an em dash plus a reason rather than a zero.
19. **The memory page has its own filter string** (settled in P4). `_MemoryFilterString`, not `_Filter.FilterString`:
    a query typed on the findings page silently narrowing the memory tables would be a list the reader cannot
    explain. The matching semantics (`Passes_TextFilter`) are shared; the state is not.
20. **The memory sort's path tie-break does not reverse with the arrow** (settled in P4). The header toggle reverses
    the COLUMN only, so two rows equal on the sorted column keep their relative order in both directions —
    otherwise a direction toggle would swap two rows the reader cannot tell apart.
21. **Every viewport-affecting profiling entry drives `FEditorViewportClient` directly, never a console string**
    (settled in P5). `viewmode <name>` is wired into `UGameViewportClient::Exec` only
    (`GameViewportClient.cpp:3406`) and does nothing at an editor viewport; `r.Lumen.Visualize.ViewMode` is
    registered but never read back by any renderer code; `r.Shadow.Virtual.Visualize` only takes effect once the
    show flag it would have set is already on. Three console strings that all look right and all do nothing —
    which is why the catalog stores an `EViewModeIndex` and the spec asserts the console string is EMPTY on those
    entries.
22. **Toggle state is read off the viewport, not recorded on the window** (settled in P5). `IsStatEnabled`,
    `GetViewMode` and `Is*VisualizationModeSelected` are live queries, and stat GROUPS reach `EnabledStats` through
    `FCoreDelegates::StatEnabled`. The prescribed `TSet<FName> _ActiveToggleIds` was rejected because it would make
    this page disagree with the console and with the viewport's own Show menu — the exact limitation it would have
    had to document.
23. **A toggle press ignores the requested state** (settled in P5). A stat command TOGGLES and a view mode SETS;
    neither lever offers "off", so both directions run the same action and the next paint reads back what actually
    happened.
24. **The VSM `casters` mode is omitted** (settled in P5). It is the one registered VSM mode that does not take
    `AddVisualizationMode`'s default view-mode index, and the path by which `VMI_ShadowCasters` reaches the
    renderer was not traced end-to-end. Shipping it would mean guessing its "is it on" answer.
25. **The cleanup census survives a session invalidation** (settled in P6). `Model::Reset` drops the findings and the
    resident memory rows and deliberately KEEPS the cleanup rows: a finding names an actor inside a world and a memory
    row names an object the play session may have freed, but a cleanup row names an ASSET, and an asset nothing
    references is unreferenced whether or not somebody pressed Play. The dirty-package rows inside it are the live-state
    exception; they go stale, the category's hint says so, and Refresh is the answer.
26. **Deletion is never a silent unlink, and there is no flag that could make it one** (settled in P6).
    `ObjectTools::DeleteAssets` is called with `bShowConfirmation = true` at the one call site, and nothing in this
    module's public surface takes a boolean that reaches it. The engine's dialog is the reference check, and a reader
    cancelling it is reported as an ordinary Warn-toned outcome rather than a failure.
27. **The save action ignores the selection, and the catalog says so** (settled in P6).
    `FEditorFileUtils::SaveDirtyPackages` prompts with the editor's own checklist over every dirty package and offers
    no supported narrowing; `OperatesOnSelection == false` is how the catalog records that, and it is why that button
    never grows a count. `FEditorFileUtils::PromptForCheckoutAndSave` would take a package list, but it is a different
    dialog with different semantics and was not adopted for one button.
28. **Hard AND soft referencers means the DEFAULT query, not `Hard | Soft`** (settled in P6).
    `UE::AssetRegistry::EDependencyQuery::Soft` is defined as `NotHard`, so OR-ing the two sets Required and Excluded
    to the same bit and filters out everything. `NoRequirements` over the `Package` category is the spelling.
29. **The unreferenced pass asks an external-reference REGISTRY, never the AngelScript module directly** (settled
    in P8). `CkOptimizationDebugger` is a `DeveloperTool` module that ships in packaged Development/DebugGame builds;
    `CkAngelscriptGenerator` is an `Editor` module. A direct dependency — even gated on `Target.bBuildEditor` — would
    put a codegen module in a debugger's link line and teach this tool that AngelScript exists. The registry in
    `CkCore/Reference/` inverts it: the module that CREATES invisible references declares them, and any tool
    reasoning about reachability asks. Copying `ScanScriptFilesForUsage`'s logic into the scan was rejected outright
    — it is the same defect as a fix re-implementing its check's predicate, which this module already forbids.
30. **A name collision is NOT a relaxed duplicate match** (settled in P8). Duplicates ask "is one of these
    redundant" and need class and size to answer; collisions ask "does this name resolve to what the author meant",
    which class and size are irrelevant to. Relaxing the duplicate key to just the name would stop reporting
    duplicates; requiring same-class-same-size would miss the mesh-vs-texture collision that is the common case. Two
    passes over one table, sharing only the grouping projection.
31. **The name-collision category has no action** (settled in P8). Resolving one means renaming an asset, which is a
    content decision no batch action should make for the reader. This is what relaxed the catalog rule to "at MOST
    one action per category"; the spec asserts the null explicitly so "deliberately actionless" stays
    distinguishable from "somebody forgot to add it".
32. **Severity glyphs are shared, shape-distinct, and live outside the decorative pool** (settled in P9). They come
    off `ck::debug_axes::Get_ToneIconId` so a tool cannot hold its own opinion about what severity looks like; they
    are authored rather than taken from `FAppStyle` because an `FAppStyle` brush cannot be restyled by Style Lab and
    is an editor style set; and they sit at `Resources/Icons/` root because only `General/` feeds the random
    assignment pool, and a severity picture handed out as decoration would make the one thing that must mean exactly
    one thing mean anything.
33. **The suggested-fix filter reads the CHECK's claim, not the registry's** (settled in P9). `HasAutoFix` alone,
    with the affordance worded "has a suggested fix". Whether the button can RUN one additionally needs the registry,
    an editor session and no PIE — three session facts a filter has no business consulting, and which the disabled
    tooltip already answers in place.
34. **Muting hides and is always visible as such** (settled in P9). Keyed by `StableKey` so a re-scan keeps it and a
    new finding on the same asset does not inherit it; the muted count for the CURRENT scan prints beside the toggle
    (never only in a tooltip); revealed rows carry a `MUTED` chip. A filter that can silently suppress findings makes
    the whole tool unreliable, so none of those three is optional.
35. **`ChangesBehavior` is a SECOND flag, never a wider `IsDestructive`** (settled in P9). "Removes or replaces
    actors" and "changes what the game does" are different risks needing different sentences in the dialog. Undo
    reverses the behaviour-change fix like any property edit — what the flag buys is the prompt.
36. **`Texture.MaxSize` gets no fix** (settled in P9), despite being mechanically trivial. The value it would write
    is a per-user calibration and the asset is shared; that is worse than the committed-config case the module
    already refuses, and moving the threshold to project config to justify it would duplicate the number.
37. **A redirector row is labelled with its referencer COUNT, not its destination** (settled in P6). Reading a
    destination means loading the redirector, and a scan that loaded every redirector in the project to label a row
    would be doing exactly what this page refuses to do. The count is registry data and is the number that decides how
    much work a fix-up is. The ACTION does load — that is a press, not a scan.
38. **A texture budget judges the BUILT size, not the imported one** (settled in P10). `GetBuiltTextureSize` IS the
    engine's resize/clamp/bias rule; re-implementing it would be a second copy guaranteed to drift, and reading
    `UTexture2D::GetSizeX()` instead would take the platform-data path this module refuses because it can trigger a DDC
    build for a texture nobody has touched. Consequences accepted with it: `Texture.NonPowerOfTwo` tests the BUILT
    dimensions, so a non-POT source the build pads or resizes is compliant and silent; `Texture.MissingMipmaps` grades
    by the built largest side; and severities legitimately DROP on the next scan of an unchanged project.
39. **Open Asset is single-selection only** (settled in P10). One click opening N asset editors is not what the reader
    asked for and closing one window does not undo it. It is also NARROWER than Go To — only an Asset-kind target has an
    editor behind it — so an actor or settings finding disables the button with the reason in its tooltip rather than
    hiding it, per the module's vanishing-buttons rule.
40. **Snapshots survive `Reset()`** (settled in P10), which is the strongest form of the cleanup rows' argument. A PIE
    boundary invalidates answers about the world NOW; a snapshot is a picture of a moment that has already passed and
    every number in it was captured with the image, so dropping it at the boundary would delete exactly the evidence
    the reader took it to compare against.
41. **Per-pixel identity comes from batched CustomDepth-Stencil passes stored WITH the image** (settled in P10's
    planning, implemented in P11). Hover and selection must work on a stored snapshot after the world has
    changed and in a packaged Development build, which kills every live-world and editor-only mechanism at once: hit
    proxies have no storage in game viewports (`GameViewportClient.h`), a hover-time ray cast asks the wrong world and
    misses collisionless meshes, and per-pixel bounds containment is far too loose at contact regions. Two fences the
    implementation must hold: batch membership controls the stencil VALUE only — every candidate renders custom depth
    in EVERY pass, or an out-of-batch occluder lets a hidden primitive stamp its id where the occluder is visible — and
    all passes run inside one game-thread scope, or pose and WPO time move between them and the silhouettes disagree.
42. **The estimated draw-call figure names what it counted** (settled in P10). A true per-pass count depends on what
    the renderer batched that frame and is not attributable to one primitive offline; the label is "≈ N draw calls
    (LOD0 sections)" so the number cannot be read as a profiler measurement. Same family of decision as the sampler
    proxy standing in for shader instruction counts.
43. **A look may replace the tonemapper, and the identity pass does** (settled in P11, implemented in CkFoundation).
    CkUsf exposed four of the engine's seven blendable locations and not `BL_ReplacingTonemapper`; for a LOOK that
    is usually right, since replacing the tonemapper means inheriting responsibility for the whole tonemap. It is
    wrong for a pass whose output is DATA, where a tone curve corrupts the values it carries. The enum value was
    APPENDED last: these serialize into look assets, so inserting one renumbers every look declared after it.
44. **The identity material is CODE, not a hand-authored asset** (settled in P11). The plan called for authoring
    `M_CkOptimizationStencilVis` by hand; a CkUsf look is a `.ush` plus an AngelScript declaration, so the graph is
    reviewable in a diff and cannot drift from the shader. The residual manual step is one console command
    (`Ck_Usf_GenerateLooks StencilId`) rather than wiring four nodes and verifying them by hand. The look is named
    and worded generically because it is the "show me the stencil" pass, not a debugger feature — capability in
    CkFoundation, consumption here.

45. **The snapshot file format is v2 with NO v1 compatibility** (settled in P13). Nothing had been published, so a
    compatibility path would have been a reader for a format with no files in the wild. Both unknown-version
    directions — older and newer — decode to nothing rather than to a guess, which is the same whole-or-nothing rule
    the RLE decoder follows and for the same reason: half a prim table under a real picture reads as the truth about
    that picture.
46. **Optimization views are COMPUTED from the ID map, not captured** (settled in P13). A lens needs no GPU pass, so
    it works on a snapshot loaded from a file on a machine that never saw the level, in a packaged Development build
    where the engine's debug viewmodes are gone, and inside a spec. Captured views are reserved for the questions a
    real buffer answers and statistics cannot (base colour, world normal). Every lens ships with a legend, because a
    heatmap whose units the reader has to guess is worse than no measurement — and the scalar ramps normalize across
    THIS view rather than an absolute project-wide scale, which the legend also says.
47. **The mesh list and the picture are ONE selection** (settled in P13). Two selection models over one snapshot
    would disagree the moment either is filtered or sorted. The window owns the single mutation path
    (`Apply_SnapshotClick` for a pixel, `Apply_SnapshotSelection` for a set, both dropping indices the prim table
    cannot back) and an apply-guard around the window-to-list push is what keeps one click from echoing into an
    endless refresh — the `DebugSelectionSync` receive-side idiom, applied locally.
48. **A/B compare is a pure delta keyed by mesh ASSET** (settled in P13). Prim indices are per capture — the same
    shelf is index 4 in one snapshot and index 11 in the next — so an index-keyed comparison would report every mesh
    as both added and removed. Placements aggregate into one row because what changed between two captures of a level
    is "there are three more of these"; mesh memory is the ASSET's so re-use never reads as new memory; and coverage
    deltas are UNSET rather than zero when either side lacks an ID map, because zero would claim the mesh occupies the
    same room and neither capture supports that.
49. **The report's mesh table stays sorted by LOD0 triangles, with triangles-per-pixel as a COLUMN only** (settled in
    P13). Density is the better question but it is only answerable when the snapshot carries an ID map, and a sort key
    that changes with the presence of identification would make two reports of the same meshes disagree about their
    order. The index tie-break stays, so re-generating a report is byte-identical.
50. **The fragile half of the captured views was not built** (settled in P13). Depth needs a float target plus a
    normalization heuristic, and shader complexity needs a debug viewmode that may not apply inside a scene capture in
    5.7; both are only knowable by a human looking at a readback. PHASE_8 pre-authorized shipping without them rather
    than shipping fragility, so base colour and world normal — plain GBuffer reads that need no viewmode — are what
    landed.

---

## Open questions for later phases

- ~~**Sub-level scope**~~ — **settled in P1**: an unloaded sub-level is REPORTED, not skipped silently.
  `FCkOptimizationDebugger_ScanResult::SkippedUnloadedLevelNames` carries the names and the status strip prints
  them, because saying nothing about a level nobody opened is indistinguishable from finding it clean. The
  per-sub-level include toggles that let a user narrow the scope are still P3; P1 records the owning level name on
  every actor-targeted finding and every asset's referencing-level list so those toggles have something to filter.
- **Shader instruction budgets are not implemented and cannot be** without compiling shaders in 5.7 — the reasons,
  with the exact deprecated/unexported symbols, are in [CLAUDE.md](CLAUDE.md) §"Deliberate omissions". If someone
  wants the number, the honest route is an explicit, opt-in, per-material action that admits it compiles shaders,
  not a check that runs during a scan.
- ~~**Duplicate detection**~~ — **settled in P6**: name + class + disk size, lower-cased on the two text halves, and
  nothing else. Content-hash equality is a different, much larger claim and stays out of scope until someone asks for
  it; the label, the hint and every row's own detail line all say "possible" so the claim cannot be over-read.
- **Report export is unscheduled, and no longer pre-declares its dependencies.** `Json`, `DesktopPlatform` and
  `ApplicationCore` were carried in `.Build.cs` from P0 with comments describing an export that was never written;
  P7 removed all three, because a dependency whose comment describes a feature the module does not have is one the
  next reader trusts. Add them back in the change that lands the export. When it lands it inherits
  `CkSaveDebugger`'s determinism contract: explicit sorts everywhere with a final tie-break, a fixed camelCase field
  set, and nothing time-, pointer- or environment-derived in the document — pinned by a spec that exports twice from
  one model and once from an independently built twin.
