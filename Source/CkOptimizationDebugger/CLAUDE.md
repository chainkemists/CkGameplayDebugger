# CkOptimizationDebugger

**Purpose:** DeveloperTool Slate toolkit for **offline / editor-time optimization work**. It analyzes the open
levels and the project's assets, reports what it found, and offers the fixes it can apply safely. It is available in
editor and packaged Development/DebugGame targets; Test and Shipping exclude it. Opened via the
`ck.OptimizationDebugger` console command, **Tools > Debug > CK Optimization Debugger**, or the shared debugger
launcher (**Tools** category, slot 40, `Stopwatch` glyph).

**Depends on:** `CkCore`, `CkEcs`, `CkDebuggerCommon`, `CkEditorTools`, `DeveloperSettings`, `PhysicsCore`, and
`UnrealEd` / `AssetRegistry` / `AssetTools` / `Settings` / `WorkspaceMenuStructure` behind `Target.bBuildEditor`.
`AssetTools` is there for exactly one symbol: `IAssetTools::FixupReferencers`, whose implementation is Private to
that module, so the interface is the only supported route. Module registration mirrors `CkSaveDebugger`.

**There is no report export.** It is unscheduled (PLAN.md), and `ApplicationCore` / `DesktopPlatform` / `Json` are
deliberately NOT declared — they were carried for a while with comments describing an export that had never been
written. Clipboard copy goes through `CkDebuggerCommon`'s `ck::DebugCopyMenu`, not `FPlatformApplicationMisc`. Add
the three back in the change that actually lands the export, together with its determinism spec.

---

## Page map

| Page | What it answers | Phase |
|---|---|---|
| **Dashboard** | "How is this level doing overall, and is it worse than last time?" — level stats, findings by severity, disk-size breakdown, deltas vs the previous scan, per-sub-level include toggles, inline threshold editing | P3 |
| **Level analysis** (Findings) | "What is wrong, where, and what do I do about it?" — the finding list with dual search, **path scope, suggested-fix and mute filters**, severity/category filters, grouping, copy menu, detail panel | P1 (P9 added the three narrowing filters) |
| **Memory** | "What is actually resident?" — textures, render targets and static meshes in three sortable tables with resource and GPU totals, guarded texture-streaming metrics, search, copy and Content Browser sync | P4 |
| **Profiling** | "Let me look at it running" — eight stat overlays, a GPU capture, six view modes, the Nanite / Lumen / virtual-shadow-map visualizers, and a console box, all applied to the active level editor viewport | P5 |
| **Cleanup** | "What can I delete, and what is wrong with my project's shape?" — unreferenced assets, possible duplicates, **name collisions**, redirectors and dirty packages under `/Game`, in five sub-tabs over one list, with deletion routed through the editor's own confirmation dialog | P6 (P8 added name collisions + the external-reference gate) |

The page bar is `SCkDebug_UnderlineTabs` over an `SWidgetSwitcher`. **The switcher's slots are added in
`ECkOptimizationDebugger_Page` declaration order** and `ck_optimization_debugger_model::Get_PageIndex` is the enum's
own value — reorder one without the other and clicking a tab silently shows a different page.
`Ck.OptimizationDebugger.Model.PageIdentity` pins that agreement.

---

## The no-live-handle invariant

This module holds **no `FCk_Handle`**, opens no PIE session, mutates no registry and spawns nothing. Everything it
shows is derived from assets, editor levels and project settings by an explicit, user-driven scan, and stored as
plain data (`FString` / `FName` / `FSoftObjectPath` / enums / ints).

Two consequences, stated because both are easy to break by copying another debugger:

- It needs **neither** the `FEditorDelegates::EndPIE` handle clear **nor** the `FCoreDelegates::OnEnginePreExit`
  Slate teardown that handle-holding debuggers require.
- It DOES subscribe to `ck::DebugSessionLifecycle::Get_OnSessionInvalidated()`, for a different reason: an
  actor-targeted finding names an object path INSIDE a world, and entering or leaving PIE swaps the world those
  paths resolve against. The findings are dropped at that boundary — they would otherwise still LOOK right and
  navigate nowhere. That subscription is the window destructor's only job.
- `SCkDebug_EntityRef` is **banned here.** Its click navigates to a LIVE entity; an asset path names no such thing.
  A finding navigates to its target through Content Browser sync / actor focus / a Project Settings page instead.

It is also fully offline in the refresh sense: **no world is polled and nothing is collected per tick.** Every
rebuild is driven by an explicit event — scan, filter, page switch — which is why `SCkOptimizationDebuggerWindow`
does not override `Tick` at all. The base's gated style-revision watch is the only per-tick work in the window, and
`OnStyleRevisionChanged` routes into the same `DoRebuild_*` entry points a user action would.

**Known gap — style revision does not restyle everything.** `OnStyleRevisionChanged` reaches the dashboard (which is
torn down and rebuilt) and the recent-command chips, and nothing else: the five page bodies are built once in
`Construct`, and list ROWS are cached by `SListView` against their item pointer, so `RequestListRefresh` on an
unchanged set does not regenerate them. Re-styling those needs `RebuildList()`, which the suite uses elsewhere
(`SCkCrowdDebugger_AgentListPanel`, `SCkEqsDebugger_QueryList`, `SCkGoapDebugger_Sidebar`). This matches
`SCkSaveDebuggerWindow`, so it is a suite-level gap rather than something specific to this module — recorded here so
the claim above is not read as more than it is. It costs a stale palette on a Style Lab revision until the tab is
reopened, and nothing else. `OnStyleRevisionChanged` deliberately does NOT touch the status strip
(`DoRebuild_All(false)`): a palette change has nothing to say, and letting the findings line through reset the strip
to "No scan yet." after a cleanup or memory scan.

A finding names an ACTOR by soft path plus the owning level's name. It never retains an `AActor*` — a debugger that
outlives a map change is exactly the one that must not hold one.

---

## Model first

`FCkOptimizationDebugger_Model` (`Public/CkOptimizationDebugger/Model/`) is Slate-free and header-visible: the
active page, the findings the last scan produced, the filter/highlight/severity/category state, and the
scanned-or-not flag. Every predicate and projection is a pure free function in the
`ck_optimization_debugger_model` namespace, so the specs test the behaviour without constructing a widget.
**If a rule is worth testing, it belongs in the model.**

Load-bearing shapes:

- **Findings are sorted on store, never on read.** `Set_Findings` runs `Get_SortedFindings` once; a second sort
  site would be a second place for the ordering contract to drift, and the order is what row identity sits on.
- **The sort is total.** Severity (most severe first) → category → title → **stable key**. `TArray::Sort` is
  unstable, so without the final tie-break two findings equal on every visible key would swap places between two
  otherwise identical scans and the list would appear to churn on its own.
- **`StableKey` is `CheckId|<target path>`** — the identity a row is reused BY across re-scans, so a re-scan that
  reproduces a finding does not move the user's selection. A `ProjectSettings` target carries no object path, so
  its section name stands in.
- **Filter and highlight are different things.** `Matches_Filter` consults the text filter, the severity mask, the
  category mask, the path scope, the suggested-fix flag and the mute set — never the highlight query. Highlight dims;
  it does not hide.
- **The narrowing axes are SEPARATE controls, not spellings of the text query.** Typing `/Game/Characters` into the
  filter box also matched any finding whose title or explanation contained the word, so one control silently answered
  two questions; `PathScope` is a real prefix test over the target path. A `ProjectSettings` finding carries no path
  and is therefore excluded by any non-empty scope — deliberate, and pinned, because a reader narrowed to a content
  folder is not asking about the renderer and keeping those rows would make the scope look like it had failed.
- **The suggested-fix filter reads the CHECK's claim, and its label says so.** `ShowOnlyWithSuggestedFix` tests
  `FindingRow::HasAutoFix` alone. Whether the button can RUN a fix additionally needs the registry to hold the check
  id, an editor session and no PIE — three session facts a filter has no business consulting. The affordance is
  therefore worded "has a suggested fix", never "fixable", so it cannot promise an enabled button.
- **Muting hides; it never deletes, fixes or excludes.** Keyed by `StableKey` — already the identity a row is REUSED
  by across re-scans — so the same problem stays muted when it is found again while a genuinely new finding on the
  same asset gets its own key and appears. Two things keep it honest, and both are required: the count of muted
  findings **in the current scan** is printed beside the toggle (not in its tooltip — the reader must not have to
  hover to learn the list is hiding something), and a revealed row carries an inert `MUTED` chip so a mixed list can
  never read as all-live. `Get_MutedFindingCount` counts the CURRENT findings, never the persisted set: that set
  accumulates keys from other scans and branches, and printing its size would claim this level hides more than it
  does. The set is per-user `config` and grows unbounded by design — pruning it against the current scan would
  silently un-mute every level nobody has opened this session.
- **Two count projections, deliberately different numbers.** `Get_CountsBySeverity` is the whole scan (the
  dashboard headline); `Get_VisibleCountsBySeverity` is what survived the filter (the page-bar count). Confusing
  them would report a filtered list as a clean level.
- **Grouping does not re-sort.** `Get_FindingsGroupedByCheck` walks the already-sorted list and groups by check id
  in first-appearance order, so groups come out worst-first for free. A second sort here would be a second place
  for the ordering contract to drift.
- **`_LastScanTime` is handed IN, never read off the clock.** The model has no `FDateTime::Now()` call, which is
  what lets a spec pin a status line. It is display state only: nothing exported, sorted or compared reads it.
- **`Set_Summary` ROTATES.** It moves the census it is replacing into `_PreviousSummary` before storing the new one,
  gated on `_HasSummary` rather than `_HasScanned` so the rotation does not depend on the order it is called in
  relative to `Set_Findings` — a call-order dependency between two setters is a bug waiting for the third caller.
  `Get_SummaryDelta()` returns an unset delta until a second scan lands.
- **`Set_SeveritySolo` assigns, it does not toggle.** A dashboard badge names the one severity the reader wants; a
  second click on the same badge must leave that answer standing rather than emptying the list, which is what
  toggling the same bit would do.

---

## The analysis engine

`Public/CkOptimizationDebugger/Analysis/` — three layers, and the split is what makes the checks testable.

| Piece | What it owns |
|---|---|
| `CkOptimizationDebugger_Thresholds.{h,cpp}` | The plain, copyable `FCkOptimizationDebugger_Thresholds` and the graduated-severity rule. Copied out of the settings CDO ONCE per scan. |
| `CkOptimizationDebugger_ScanContext.{h,cpp}` | What one pass over the open levels found — levels, actors, mesh usages, lights, and the unique mesh / material / texture / Blueprint tables with their referencing levels. Plus the three target builders and `Build_Finding`, the one place a stable key is derived. |
| `CkOptimizationDebugger_LevelScan.{h,cpp}` | `Run_Scan(UWorld*, Thresholds, ExcludedLevelNames) -> FCkOptimizationDebugger_ScanResult`: gathers the context behind a cancelable `FScopedSlowTask`, runs the seven check families over it, and folds the SAME context into the dashboard's `FCkOptimizationDebugger_ScanSummary`. `TryGet_EditorWorld()` is the only `GEditor` touch. |
| `CkOptimizationDebugger_DiskScan.{h,cpp}` | `Build_ProjectDiskBreakdown()` — the `/Game` disk-size census, asset-registry driven, run as the scan's last progress step. |
| `Analysis/Checks/CkOptimizationDebugger_Checks_*.{h,cpp}` | One family each. Every one is a single `Run_Checks(Context, Thresholds, OutFindings)`. |

Rules the engine holds to:

- **A check never reads `UCkOptimizationDebuggerSettings::Get()`.** It takes the plain threshold struct. A check
  that read the CDO would be a check no spec can drive, and a threshold edited half way through a ten-thousand-actor
  walk would judge the second half by a different rule than the first.
- **Gathered once, not re-walked per family.** Two checks that each walk the world can disagree about what was in
  it, and two findings that disagree about one level are worse than one finding fewer.
- **Every check-family header and body is inside `#if WITH_EDITOR`** — declarations included. The module ships in
  packaged Development/DebugGame, where `Run_Scan` returns `RequiresEditor` and the window says so on the status
  strip rather than reporting a clean level.
- **An unloaded sub-level is NAMED, never skipped silently** (`SkippedUnloadedLevelNames`, surfaced on the status
  strip). Saying nothing about a level nobody opened is indistinguishable from finding it clean.
- **A cancelled scan still keeps what it gathered.** The checks run over the partial context and the status strip
  says the answer is partial — a partial answer the reader was told about beats discarding work they waited for.
- **Determinism.** Every asset table is sorted by path and every referencing-level list is sorted before the checks
  see them, because world iteration order depends on the order sub-levels happened to load.
- **An excluded level is skipped WHOLE.** `ck_optimization_debugger_model::ShouldScanLevel(name, excluded)` is asked
  once, before the level joins the scan list — never gathered and then filtered out of the findings. A half-excluded
  level would still pay for its own walk and would still fold its actors into the census, which is the opposite of
  what excluding it means. It is still RECORDED in `Summary.Levels` with `WasIncluded == false` so the dashboard can
  grey it rather than drop it.

### Severity is graduated, not binary

`ck_optimization_debugger_thresholds::Get_GraduatedSeverity(Value, Budget, AtBudgetSeverity)`: at or past **2×**
the budget the finding escalates one step past `AtBudgetSeverity` (saturating at Critical); merely past the budget
it IS `AtBudgetSeverity`. "3,000 triangles over" and "300,000 triangles over" are not the same finding, and a list
that paints them identically is a list the reader stops sorting by. A non-positive budget grades to the base
severity — a ratio cannot exceed zero. `Ck.OptimizationDebugger.Analysis.GraduatedSeverity` pins all of it.

### Check catalog

Twenty-eight checks across seven families. `Grad(base)` below means `Get_GraduatedSeverity` with that base severity.
The auto-fix column marks findings whose `HasAutoFix` is already `true` and whose `FixDescription` names the
action **P2** is expected to implement — P1 applies nothing.

| CheckId | Family | Fires when | Severity | Threshold | Target | Auto-fix |
|---|---|---|---|---|---|---|
| `Mesh.TriangleBudget` | Mesh | LOD0 triangles > budget | Grad(Major) | `MaxTriangleCountLOD0` | mesh asset | — |
| `Mesh.MissingLods` | Mesh | one LOD, Nanite off, tris ≥ Nanite floor | Grad(Major) vs `MaxTriangleCountLOD0` | `MinTrianglesForNanite` + `MaxTriangleCountLOD0` | mesh asset | Generate LODs |
| `Mesh.NaniteCandidate` | Mesh | Nanite off and tris ≥ floor | Grad(Minor) | `MinTrianglesForNanite` | mesh asset | Enable Nanite |
| `Mesh.NaniteOnLowPoly` | Mesh | Nanite on and tris < low-poly floor | Minor | `MaxTrianglesForNaniteWarning` | mesh asset | Disable Nanite |
| `Mesh.NaniteMaterialIncompatible` | Mesh | Nanite on, some slot material has `bUsedWithNanite` off | Major | — | mesh asset | — |
| `Mesh.ComplexCollision` | Mesh | `GetCollisionTraceFlag() == CTF_UseComplexAsSimple` | Major | — | mesh asset | Generate simple collision |
| `Mesh.CollisionPrimitiveCount` | Mesh | `AggGeom.GetElementCount()` > budget | Grad(Minor) | `MaxCollisionPrimitives` | mesh asset | — |
| `Texture.MaxSize` | Texture | largest side > budget | Grad(Major) | `MaxTextureSize` | texture asset | — |
| `Texture.NonPowerOfTwo` | Texture | either axis is not a power of two | Minor | — | texture asset | — |
| `Texture.MissingMipmaps` | Texture | `MipGenSettings == TMGS_NoMipmaps` and `LODGroup != TEXTUREGROUP_UI` | Grad(Major) vs size | `MaxTextureSize` | texture asset | — |
| `Texture.NormalMapCompression` | Texture | name reads as a normal map, compression is not `TC_Normalmap` | Major | — | texture asset | Set `TC_Normalmap` |
| `Texture.DataTextureSrgb` | Texture | data texture (`TC_Masks`/`TC_Grayscale`/`TC_Alpha` or packed-channel name) with `SRGB` on | Major | — | texture asset | Disable sRGB |
| `Material.SlotCount` | Material | mesh slot count > budget | Grad(Major) | `MaxMaterialSlots` | mesh asset | — |
| `Material.EmptySlot` | Material | a slot has no material | Major | — | mesh asset | — |
| `Material.DuplicateSlots` | Material | one material fills ≥ 2 slots | Minor | — | mesh asset | — |
| `Material.TranslucentTwoSided` | Material | translucent blend mode OR two-sided | Major when both, else Minor | — | material asset | — |
| `Material.SamplerBudget` | Material | distinct `GetUsedTextures` count > budget | Grad(Major) | `MaxTextureSamplers` | material asset | — |
| `Lighting.MovableLightCount` | Lighting | movable lights in one level > budget | Grad(Major) | `MaxMovableLights` | level asset | Review light mobility |
| `Lighting.LightmapResolution` | Lighting | `bOverrideLightMapRes` and `OverriddenLightMapRes` > budget, **aggregated per actor** | Grad(Major) | `MaxLightmapResolution` | actor | — |
| `Actor.EmptyStaticMesh` | Actor | plain `AStaticMeshActor` with no mesh | Minor | — | actor | Delete the actor |
| `Actor.InstancingCandidate` | Actor | ≥ N static-mobility plain SMActors sharing mesh **and** materials | Grad(Major) | `MinRepeatedActorsForInstancing` | mesh asset | Convert to HISM |
| `Blueprint.TickEnabled` | Blueprint | CDO has `bCanEverTick && bStartWithTickEnabled` | Grad(Minor) vs placement count | `MinRepeatedActorsForInstancing` | Blueprint asset | — |
| `Blueprint.DependencyChain` | Blueprint | hard package dependencies > budget | Grad(Major) | `MaxBlueprintDependencies` | Blueprint asset | — |
| `ProjectSettings.RayTracing` | Project settings | `bEnableRayTracing` | Minor | — | Rendering | — |
| `ProjectSettings.PathTracing` | Project settings | `bEnablePathTracing` | Minor | — | Rendering | — |
| `ProjectSettings.ForwardShading` | Project settings | `bForwardShading` | Minor | — | Rendering | — |
| `ProjectSettings.ExpensiveLightingFeatures` | Project settings | Lumen GI / Lumen reflections / virtual shadow maps enabled | Minor | — | Rendering | — |
| `ProjectSettings.TextureStreamingDisabled` | Project settings | `bTextureStreaming == 0` and scanned texture count ≥ threshold | Grad(Major) | `MinTexturesForStreamingWarning` | Rendering | Enable texture streaming |

### Deliberate omissions and the reasons

- **Shader instruction budgets are NOT implemented.** They do not exist offline in 5.7:
  `UMaterialInterface::GetMaterialResource(ERHIFeatureLevel::Type, …)` is deprecated to an unconditional `NULL`
  return, `FMaterialStatsUtils::GetRepresentativeInstructionCounts` carries no `MATERIALEDITOR_API`, and the
  exported `ExtractMatertialStatsInfo`'s out-parameter type lives in a **Private** header. Instruction counts come
  from a compiled shader map, so getting one means compiling shaders — which an audit tool must not do to describe
  an asset. `Material.SamplerBudget` is the complexity proxy that IS cheaply readable.
- **Nanite material compatibility reads `UMaterial::bUsedWithNanite`**, not `CheckMaterialUsage[_Concurrent]`.
  Both usage-check calls can stall on a shader compile; the flag is the authored answer to the same question.
- **Texture dimensions prefer `FTextureSource`** over `UTexture2D::GetSizeX()`. The platform-data path can trigger
  a DDC build for a texture nobody has touched this session. `Source.IsValid()` is a serialized-header read;
  dynamic textures have no source, which is why the platform-data path stays as the fallback.
- **Normal-map and data-texture detection is name-based** (plus the compression setting for data textures). The
  compression setting is the thing under test, so it cannot also be the evidence. The suffix lists are kept narrow
  on purpose: a false positive here tells an artist their correctly-authored texture is wrong.
- **`ProjectSettings.ExpensiveLightingFeatures` is aggregated**, not one finding per feature. Lumen and virtual
  shadow maps are the UE5 defaults; three rows telling a reader their defaults are on would be noise.
- **`Actor.InstancingCandidate` reports one finding per MESH**, choosing the largest matching material group. The
  stable key is `check id | target path`, so two groups sharing a mesh would produce two rows the list could not
  tell apart. Its wording says "convert **up to** N": the scan matches on mesh + materials + static mobility, while
  the FIX additionally refuses any placement with an attach parent, attached children or a second scene component,
  and converts one level at a time. The apply message reports the number it actually took.
- **`Lighting.LightmapResolution` is aggregated PER ACTOR**, for the same reason and it is the same hazard. The
  override lives on a static mesh COMPONENT but the finding targets the actor, so an actor carrying two over-budget
  components emitted two findings with an identical stable key. The window reuses list rows by that key: a duplicate
  made every rebuild allocate a fresh row for the second one, firing `RequestListRefresh` on every keystroke in the
  filter box and leaving that row unable to hold a selection. One actor, one row, graded and worded by the worst
  override and saying how many components are over budget.
  `Ck.OptimizationDebugger.Analysis.LightmapFindingIdentity` pins it over a synthetic context.

**Stable-key uniqueness is a CHECK-AUTHORING obligation, not something the window can repair.** A check that emits
per-sub-object findings against a parent target must aggregate them. There is no de-duplication downstream, and the
symptom is a list that churns rather than an error.

---

## Fixes and navigation

`Public/CkOptimizationDebugger/Fixes/` — two files, and the split is deliberate: navigation takes the reader
somewhere, a fix changes something. They share a target but not a risk profile.

| Piece | What it owns |
|---|---|
| `CkOptimizationDebugger_Fixes.{h,cpp}` | The registry (`FCkOptimizationDebugger_FixInfo` per check id), the pure projections a spec can drive (`Can_ApplyFix`, `Get_FixableFindings`, `Partition_ForBatch`, `Build_FixButtonLabel`), and `Apply_Fix` / `Apply_Fixes`. Every engine mutation is inside `#if WITH_EDITOR`; outside it every fix fails with a message saying an editor session is needed. |
| `CkOptimizationDebugger_Navigation.{h,cpp}` | `Get_NavigationDescription` / `Can_Navigate` (pure) and `Navigate_ToFinding` (editor-only), dispatching on `Target.Kind`. |

### The fix catalog

Every entry carries an `ECkOptimizationDebugger_FixExecution`, and the three values are what the batch partition and
its run order are built on:

| Execution | Means | Batch behaviour |
|---|---|---|
| `Transactional` | mutates an asset or a level inside an `FScopedTransaction` | one record for the whole transactional part |
| `ConfigWrite` | writes a config file Undo cannot reach | applied AFTER the record and outside it |
| `Review` | writes nothing; it SHOWS the reader something | applied LAST, so the selection it leaves is the one still highlighted |

It replaced a `bool IsTransactional`, which was covering two genuinely different things and therefore filed the
light-selection action into a bucket named `ConfigWrite`.

**Two independent risk flags, and they are deliberately not one field.** `IsDestructive` means the fix removes or
replaces actors rather than editing a property. `ChangesBehavior` means it alters how the game BEHAVES rather than only
what it costs. Both are read by `Build_BatchConfirmation`, each producing its own line, because the two are genuinely
different risks and a single field could not say which one the dialog is asking about: a deleted actor is visibly gone
and Undo restores it, while a Blueprint that no longer ticks from frame zero looks identical until something it was
driving quietly stops happening. `Blueprint.TickEnabled` is the only entry carrying `ChangesBehavior` today, and Undo
still reverses it — what the flag buys is the prompt.

| CheckId | What the fix actually does | Undo | Destructive |
|---|---|---|---|
| `Mesh.NaniteCandidate` | `SetNaniteSettings` with `bEnabled = true`, then `NotifyNaniteSettingsChanged()` — the same `PostEditChangeProperty` the mesh editor's own checkbox raises, which is what builds the Nanite data | yes | no |
| `Mesh.NaniteOnLowPoly` | the same, with `bEnabled = false` | yes | no |
| `Mesh.MissingLods` | `UStaticMesh::SetLODGroup` with the first available of `LargeProp` / `SmallProp` / `Deco`, else the first non-`None` configured group. The engine's own path applies the group's default LOD count and per-LOD reduction settings and rebuilds | yes | no |
| `Mesh.ComplexCollision` | flips `UBodySetup::CollisionTraceFlag` to `CTF_UseSimpleAndComplex`, and when the body setup has NO simple primitives adds one `FKBoxElem` sized from the mesh bounds, then `InvalidatePhysicsData` + `CreatePhysicsMeshes` + `PostEditChange` | yes | no |
| `Texture.NormalMapCompression` | `CompressionSettings = TC_Normalmap` **and** `SRGB = 0`, then `PostEditChange()` | yes | no |
| `Texture.DataTextureSrgb` | `SRGB = 0`, then `PostEditChange()` | yes | no |
| `Texture.MissingMipmaps` | `MipGenSettings = TMGS_FromTextureGroup`, then `PostEditChange()`. `FromTextureGroup` rather than a specific setting: the group is where a project states its mip policy, so this hands the decision back to that policy instead of inventing one per texture. Re-validates BOTH halves — a texture moved into `TEXTUREGROUP_UI` since the scan is legitimately mipless and is refused | yes | no |
| `Mesh.NaniteMaterialIncompatible` | `bUsedWithNanite = 1` on each offending BASE material, then `PostEditChange()`. Re-asks the check's own exported `Is_NaniteIncompatible` per slot, and refuses outright if Nanite has been turned off on the mesh since the scan. Fixing the base material means a mesh using several instances of one parent is fixed once. **Queues a shader compile**, which the result message says because nothing else on screen would | yes | no |
| `Lighting.LightmapResolution` | clamps `OverriddenLightMapRes` to the CURRENT budget on **every** over-budget component of the actor — not the first, because the check aggregates per actor precisely because one actor can carry several. Clamps rather than clearing the override: clearing falls back to the mesh's own default, a different number nobody chose that may also be over budget. Reads the threshold fresh, and checks `FLevelUtils::IsLevelLocked` before touching anything | yes | no |
| `Blueprint.TickEnabled` | `bStartWithTickEnabled = false` on the generated class's CDO, then `PostEditChange()`. **`bCanEverTick` is deliberately left alone** — the class keeps the ability to tick, so anything that enables it at runtime still works; clearing it would break `SetActorTickEnabled` and turn a cost fix into a broken actor. Re-validates both halves of the check | yes | no, but **`ChangesBehavior`** |
| `Actor.EmptyStaticMesh` | re-validates that the actor is still a plain `AStaticMeshActor` with no mesh, then `UWorld::EditorDestroyActor` | yes | **yes** |
| `Actor.InstancingCandidate` | re-derives the convertible group from the live world, spawns one actor with a `UHierarchicalInstancedStaticMeshComponent`, copies the template's materials and every matching actor's world transform as an instance, then deletes the originals | yes | **yes** |
| `Lighting.MovableLightCount` | **selects** every movable-light actor in that level for review. Not a mobility change — see below. Its verb is **"Select Lights For Review"**, because "Review Light Mobility" read as though it changed mobility | n/a (changes nothing) | no |
| `ProjectSettings.TextureStreamingDisabled` | sets `URendererSettings::bTextureStreaming`, writes it with **`TryUpdateDefaultConfigFile`** (checked), pushes `r.TextureStreaming` at the console variable and **reads it back** | **no — config write** | no |

### Rules the fix engine holds to

- **Every fix re-validates the WHOLE condition its check tested, not just the property it is about to flip.** A scan
  can be minutes old. Re-checking only "is sRGB still on" let a texture re-authored as an albedo since the scan get
  its sRGB stripped and report plain success — so `Texture.DataTextureSrgb` re-asks the check's own exported
  `Is_DataTexture`, and the Nanite fixes re-read the triangle count against the current thresholds. Re-validating
  half a two-clause condition is not re-validating.
- **A check's predicate is EXPORTED rather than copied** when a fix needs it (`ck_optimization_debugger_checks_texture::Is_DataTexture`).
  A second copy of the rule in the fix is a second place for it to drift, which is the defect the re-validation
  exists to prevent wearing a different hat.
- **Actor mutations respect the level lock and clean up the selection.** `UWorld::EditorDestroyActor` and
  `SpawnActor` with an `OverrideLevel` both bypass the lock that `UEditorEngine::edactDeleteSelected` and `AddActor`
  respect, and neither touches `USelection` — so a locked sub-level would have been rewritten anyway and a deleted
  actor left a stale selection entry the outliner reads. Both destructive fixes check `FLevelUtils::IsLevelLocked`
  first and deselect before destroying. The instancing conversion checks the lock BEFORE it spawns, because it
  spawns before it deletes and a half-done conversion leaves the level holding both.
- **`Lighting.MovableLightCount` selects, it does not change mobility.** A light that genuinely moves must stay
  Movable, and no offline rule can tell which those are. The fix hands the reader the exact set to judge. It keeps
  `HasAutoFix` because it IS an action the button can take, and it is `Execution == Review` — which is what puts it
  outside the transaction, outside the config-write bucket, and LAST in a batch so its selection survives.
- **What stops a re-scan is `FCkOptimizationDebugger_FixResult::ChangedState`**, the per-invocation truth, and
  nothing else. There was a registry-level `RequiresRescanOfAssets` beside it saying the same thing a level earlier;
  it was never read, and two flags that can disagree about the same question is one flag too many.
- **The four fixes added in P9 are all one-property transactional edits**, and each re-validates the WHOLE condition
  its check tested rather than the property it is about to flip — the rule that already governs the sRGB and Nanite
  fixes. `Mesh.NaniteMaterialIncompatible` re-asks the check's own `Is_NaniteIncompatible`, exported for the purpose,
  because a second spelling of "is this incompatible" in the fix is a second place for it to drift from what the list
  reported.
- **`Texture.MaxSize` is mechanically fixable and deliberately has NO fix.** `UTexture::MaxTextureSize` would clamp the
  built texture without touching source art, undoably. It is refused because the value it would write is a **per-user**
  calibration (`config=GameUserSettings`) and the asset it writes into is **shared** — strictly worse than the
  committed-config case this module already refuses, since one QA person's 2048 would be baked into an artist's asset.
  Moving the threshold to project config to justify it would put the same number in two places.
  `Ck.OptimizationDebugger.Fixes.RegistryCoverage` lists it among the non-fix checks with that reason attached.
- **A batch that cannot be undone asks first.** `Build_BatchConfirmation` is pure over the selection and returns the
  prompt, or an unset confirmation when there is nothing to ask about. It fires on exactly two things: a fix flagged
  `IsDestructive`, and a `ConfigWrite`. A property edit inside a transaction is one Ctrl+Z away and deliberately
  does NOT prompt — a dialog in front of every fix is a dialog the reader learns to dismiss unread.
  `Ck.OptimizationDebugger.Fixes.BatchConfirmation` pins which selections ask.
- **A selection of ONE goes through `Apply_Fix`, not `Apply_Fixes`.** That is the entry point that labels the undo
  record with the fix's own verb — "Enable Nanite (SM_Foo)" rather than "Apply 1 optimization fix(es)". Routing
  everything through the batch made that label unreachable and the single-fix path dead code.
- **Fixes are refused during PIE.** `Get_CanApplyFixes()` / `Get_FixesUnavailableReason()` gate the button, the
  context-menu entry and both apply entry points. Every transactional fix edits an asset or destroys an actor, and
  doing either to a world a play session has running is a change nobody asked for at a moment nothing undoes
  cleanly. The findings stay readable; only the button waits, and the disabled tooltip says which of the two
  reasons applies.
- **The config write is separated out of the batch's transaction.** `Partition_ForBatch` splits a selection into the
  transactional part (one `FScopedTransaction` for the whole batch — a reader who applied six fixes with one click
  expects one Ctrl+Z) and the config-write part, applied after it and outside it. Holding an ini edit inside an undo
  record would promise a Ctrl+Z that silently does nothing.
- **`Can_ApplyFix` needs BOTH halves.** The check must have set `HasAutoFix` **and** the registry must hold the check
  id. A finding built before a check learned to flag itself must not sprout a button because the registry grew one.
  `Ck.OptimizationDebugger.Fixes.SelectionProjection` pins that.
- **Asset targets may load; actor targets may not.** A fix or a navigation on an asset path loads the package the
  reader asked about. An actor path names something inside a world, so it is only ever `ResolveObject`d — dragging a
  map back in behind the reader's back is not navigation.
- **Deterministic where a choice exists.** The instancing conversion groups by level AND material set (one instanced
  component lives in exactly one level), picks the largest group with the group key as tie-break, and sorts the
  actors by path before adding instances — so two runs over the same level produce the same instance order.

### What was NOT built, and why

- **LOD generation does not drive the reduction interface directly.** `GenerateKDopAsSimpleCollision` and the rest of
  the static-mesh editor's geometry utilities live in `Editor/UnrealEd/**Private**/GeomFitUtils.h` — exported symbols
  behind a private header, which a plugin module cannot include. `SetLODGroup` is the engine's own supported path to
  the same outcome, and the `FixDescription` says which group it assigned.
- **Simple collision is a bounds box, not a fitted hull**, for the same reason. The fix says so in its result message
  and tells the reader to replace it. A mesh left with the flag flipped and no primitives at all would collide with
  nothing — a silent behaviour change far worse than the cost the finding was about — so the box is the floor, not
  the goal.

---

## The dashboard

Five sections in one scroll box, rebuilt only by `DoRebuild_Dashboard` — which runs on a scan, on a level toggle, on
a session invalidation and on a style revision, and on nothing else. Every number inside is a `TAttribute` over the
model, so the sections track the next scan without being re-created.

| Section | Reads | Notes |
|---|---|---|
| **Empty state** | `Get_HasSummary()` | Replaces the whole page before the first scan and carries a second Scan button — the same `DoRun_Scan` the toolbar's runs. A page of zeroes is a worse answer than "press this". The thresholds panel stays visible under it: those are worth setting BEFORE a scan. |
| **Stat tiles** | `Summary.{ActorCount, UniqueStaticMeshCount, Lod0TriangleTotal, UniqueMaterialCount, LightCount, FindingCounts}` + `Get_SummaryDelta()` | `SCkDebug_StatPair` (`Stacked_ValueOnTop`) with the delta underneath. Values print through `Format_AbbreviatedCount` (1.2M), deltas through `Format_Delta` (+12 / -3 / —). |
| **Severity strip** | `Get_CountsBySeverity()` — the UNFILTERED scan | Three `SCkDebug_CountBadge`s inside `HoverHintOnly` buttons; clicking one calls `Set_SeveritySolo` and switches to Level analysis. |
| **Disk size** | `Summary.Disk` | One `SCkDebug_KeyValueRow` + `SCkDebug_MeterBar` per non-empty family, in the order the scan sorted them. |
| **Levels** | `Summary.Levels` + `Get_ExcludedLevelNames()` | One row per level the world listed — included, excluded or unloaded — with an `SCkDebug_Switch`. |
| **Thresholds** | `UCkOptimizationDebuggerSettings` reflection | Collapsed `SCkDebug_InspectorPanel` of `SCkDebug_NumericEditor` rows. |

Rules this page holds to:

- **Counts on the dashboard are the WHOLE scan; counts on the Findings tab are what survived the filter.** The
  dashboard tab's own `CountText` and warn dot read `Get_CountsBySeverity`, deliberately not
  `Get_VisibleCountsBySeverity`. A filter somebody typed on another page must not make the headline drop and read as
  a level that got better.
- **The delta is captured in ONE place**: `_Model.Set_Summary` rotates the census it replaces into
  `_PreviousSummary`, and `DoRun_Scan` is the only caller. Because the post-fix refresh re-enters `DoRun_Scan`, a fix
  produces a delta too — a second overwrite path would silently make every delta read zero, which is exactly what
  "nothing changed" looks like.
- **`HasPrevious` is not "no change".** With one scan in the session every delta is genuinely zero, and a row of
  `+0`s would claim a comparison that was never made. The tiles print an em dash and tone the caption muted.
- **Only a stat with a better DIRECTION is coloured.** `Get_DeltaTone(delta, FewerIsBetter)` returns `Neutral` for
  every count and `Ok`/`Err` only for findings. Painting a level that grew by ten actors red would be this tool
  asserting an opinion it has no basis for.
- **The levels list is a plain `SVerticalBox`, never an `SListView`.** The set is small and fixed between scans, so
  there is no virtualization to buy — and a plain box is what makes an interactive `SCkDebug_Switch` legal in a row
  at all. Inside an `STableRow` the switch would consume the click that selects the row (click-trap #1). The severity
  badges are buttons for the same reason: they are toolbar-level chrome, not list rows.
- **Excluded levels are greyed, never removed**, and their hint text is an ATTRIBUTE — the toggle changes what the
  row says and nothing rebuilds the section when it does. A row that vanished would leave nothing to switch back on.
- **A level toggle never re-scans.** It is a statement about the NEXT scan; silently re-walking a large world because
  somebody flicked a switch is not what the switch says it does. It persists immediately (below) and updates the
  status strip.

### Threshold editing — the two things that make it safe

1. **The row list comes from reflection, not from a second list here.** `Get_ThresholdEntries()` walks
   `TFieldIterator<FIntProperty>` over `UCkOptimizationDebuggerSettings`, keeps the `CPF_Config` ones, and reads
   `DisplayName` / `ToolTip` / `ClampMin` from the property's own metadata (behind `WITH_EDITORONLY_DATA`, falling
   back to the property name). Add a `UPROPERTY` to the settings object and this panel grows the row for free, with
   the wording Editor Preferences already shows. Rows are sorted by label with the property name as the tie-break:
   `TFieldIterator`'s own order is a property-link-list detail, and a settings panel whose rows reordered between
   sessions is one the reader stops navigating by position.
2. **`FCkInspectorEditGuard` is per-WINDOW and defers the rebuild, never drops it.** `DoRebuild_Dashboard` bails to
   `Request_Rebuild()` while any field has focus. The consume happens on a **one-shot `RegisterActiveTimer`**, not
   inline in `OnEditStateChanged`: that callback runs inside the text box's own commit handler, and rebuilding there
   would destroy the widget still on the stack. That timer is not a Tick — it fires once, from a user action, and
   returns `EActiveTimerReturnType::Stop`. The window still overrides no `Tick`.

Writes go to `GetMutableDefault<UCkOptimizationDebuggerSettings>()` + `SaveConfig()` — the same per-user
`config=GameUserSettings` store Editor Preferences → Ck writes, and the same idiom as `SCkCrowdDebuggerWindow` and
`SCkDebugger_RefreshControls`. The **next** scan reads them; nothing re-scans on a commit.

### Exclusion persistence

`UCkOptimizationDebuggerSettings::ExcludedLevelNames` is a `UPROPERTY(config) TArray<FName>` with no `EditAnywhere`:
a level name typed into a preferences page would not match anything the open world lists, and a field that silently
does nothing is worse than no field. `Save_ExcludedLevelNames` writes it **sorted** — a `TSet`'s iteration order
follows its hash layout, so persisting it raw would rewrite the same ini line differently on different machines.
`SCkOptimizationDebuggerWindow::Construct` loads it back before the body is built, so the toggles come up showing
what the next scan will actually do.

`FCkOptimizationDebugger_Model::Reset()` drops the summary and its previous, and **keeps** the exclusion set — the
set is the user's narrowing of the QUESTION, in exactly the sense the filter state is. A PIE boundary invalidates
the answers, never the question.

### The project disk breakdown

`ck_optimization_debugger_disk::Build_ProjectDiskBreakdown()`, run as the scan's last progress step because it is
the most expensive one and everything about the LEVEL is already computed by the time it starts.

- **Package-granular, because that is the granularity the number exists at.** `FAssetPackageData::DiskSize` is the
  size of a package on disk, not of one object inside it, so each package is attributed to exactly one family — the
  family of its PRIMARY asset (the one whose name matches the package's). No byte is counted twice and the buckets
  add up to the total printed beside them.
- **Nothing is loaded.** `GetAssetsByPath("/Game", recursive, onDiskOnly)` + `TryGetAssetPackageData` read serialized
  registry data only, and classification goes through `FAssetData::IsInstanceOf<T>()` with the default
  `EResolveClass::No` — a class nobody has loaded lands in `Other` rather than being loaded to find out. An audit
  tool that triggered a DDC build to describe an asset would be the defect it exists to report.
- **Redirectors are skipped**: they are a cleanup question, and their bytes belong on the Cleanup page next to the
  action that removes them.
- **`IsAvailable` and `WasStillIndexing` are both on the record.** Zero bytes, "nobody asked" (packaged builds, no
  registry) and "the registry was still indexing so this is a floor" are three different statements, and a bare 0
  would be indistinguishable from all three.
- **Only non-empty buckets are rendered**, largest first with the enum value as the tie-break — `TArray::Sort` is
  unstable, so two families that weigh the same would otherwise swap rows between identical scans.

---

## The memory analyzer

`Public/CkOptimizationDebugger/Analysis/CkOptimizationDebugger_MemoryScan.{h,cpp}` +
`ck_optimization_debugger_model`'s memory projections + `DoCreate_MemoryPage`. Three tables — Textures, Render
Targets, Static Meshes — behind one `SCkDebug_UnderlineTabs` selector, over a sortable `SListView` with an
`SHeaderRow`.

**It is not the level scan, and it deliberately answers a different question.** `Run_MemoryScan` walks the live
object graph with `TObjectIterator`, not the world: the editor holds thumbnails, Content Browser previews and
whatever the last PIE session dragged in, none of which any level places and all of which is really costing memory.
The two scans are independent in both directions — a level scan does not refresh the tables, and Refresh does not
touch the findings.

Consequences worth stating:

- **The memory page is NOT editor-only.** `TObjectIterator` and the streaming manager are runtime facilities, so
  unlike `Run_Scan` this answers in a packaged Development/DebugGame build too. Nothing here sits behind
  `#if WITH_EDITOR`. (Navigation still does — double-clicking a row needs a Content Browser.)
- **Nothing is loaded and nothing is built.** Only already-resident objects are visited, and every accessor reads
  state the object already has — see the texture-accessor note below.
- **Refresh is an explicit user action.** The scan is cheap next to the level scan; it is still never ticked, never
  polled and never re-run on a page switch.

### Which size API, and why `Exclusive`

`UObject::GetResourceSizeEx(FResourceSizeEx{EResourceSizeMode::Exclusive})`, and the row keeps two numbers off the
same accumulator:

| Row field | Source | Notes |
|---|---|---|
| `ResourceSizeBytes` | `FResourceSizeEx::GetTotalMemoryBytes()` | System + video + untagged, i.e. everything the object reported |
| `GpuSizeBytes` / `HasSeparableGpuSize` | `FResourceSizeEx::GetDedicatedVideoMemoryBytes()` | The flag follows the row's TABLE, never the byte count |

- **`Exclusive`, not `EstimatedTotal`.** Exclusive counts what the object costs *right now* — for a texture,
  `CalcTextureMemorySize(GetNumResidentMips())`. `EstimatedTotal` counts every possible mip plus the serialized
  `UObject` graph, which is the editor's "maximum required memory" estimate. This page's whole claim is "what is
  resident", so the estimate would be the wrong number in the one place a reader is most likely to trust it.
- **The GPU split comes free with that one call**, so there is no second `CalcTextureMemorySizeEnum` pass. Reading
  the engine's own implementations is what settles which types can report one: `UTexture2D::GetResourceSizeEx` adds
  its mip bytes through `AddDedicatedVideoMemoryBytes`, while `UTextureRenderTarget2D` and `FStaticMeshRenderData`
  both add theirs through `AddUnknownMemoryBytes`. So a mesh and a render target report ONE figure, and
  `HasSeparableGpuSize` is what makes the GPU column print an em dash for them instead of `0 B` — a claim neither of
  them made.
- **`HasSeparableGpuSize` is a property of the TABLE, never of the number that came back.**
  `ck_optimization_debugger_model::Has_SeparableGpuSize(Table)` is the one rule, and both the scan and the specs'
  fixtures ask it. Deriving it from `GpuSizeBytes > 0` conflated two different statements — "this type reports no
  separable figure" and "this instance currently has none resident" — so a fully streamed-out texture, zero resident
  mips and therefore zero dedicated bytes, printed the em dash that means "this could not be measured" over a
  measurement that had been taken and was genuinely zero. On a page whose whole claim is "what is resident", `0 B`
  is the true answer there. `Ck.OptimizationDebugger.Memory.SeparableGpuSize` pins both halves.

### The two texture accessors that were chosen for what they do NOT do

`GetSizeX` / `GetSizeY` / `GetNumMips` / `GetPixelFormat` read `PrivatePlatformData` directly. `GetPlatformData()` —
the accessor they superficially resemble — **blocks on an in-flight texture build** (`UTexture2D::GetPlatformData`
logs "forcing a wait on data that is not yet ready" and shows a progress dialog). An audit tool must never make the
editor finish compiling a texture in order to describe it, which is the same rule the level scan's
`FTextureSource`-first policy follows.

Dimensions come through the generic `UTexture::GetSurfaceWidth/Height`, which every texture type implements — a cube
map and a volume texture answer without this code branching on which it got. Mip count falls back to the streamer's
`FStreamableRenderResourceState::MaxNumLODs` for the types that carry no `GetNumMips` on that path.

### The streaming guard — three states, never a crash

`ck_optimization_debugger_memory::Get_StreamingAvailability()` is asked ONCE, before the walk (asking per row would
let a manager that shut down half way through produce a table whose first half claims a measurement its second half
says was impossible):

| Session state | How it is detected | What the table does |
|---|---|---|
| **Available** | `IStreamingManager::Get_Concurrent() != nullptr` **and** `FStreamingManagerCollection::IsTextureStreamingEnabled()` | Streamable rows print `resident / wanted`; non-streamable rows print "not streamable" |
| **StreamingDisabled** | manager present, `IsTextureStreamingEnabled()` false | Column prints an em dash on every row; a dim note above the table says streaming is off and that the SIZES are still real |
| **ManagerUnavailable** | `Get_Concurrent()` returned null — never allocated, or `IStreamingManager::Shutdown` poisoned it | Same, with the other reason |

**`Get_Concurrent()`, never `Get()`.** `Get()` lazily *constructs* a `FStreamingManagerCollection` on first call; a
tool that summoned the subsystem it was asked to describe would be reporting on its own side effect.
`Get_Concurrent()` is the accessor documented to fail, and it returns null in both the not-yet-allocated and the
post-shutdown cases (`Shutdown` leaves a `(FStreamingManagerCollection*)-1` sentinel that it filters).

The numbers themselves come off the ASSET (`UStreamableRenderAsset::GetStreamableResourceState()`), not off the
manager — but they only mean anything while a streamer is maintaining them, which is what the availability gate is
for. A state that `IsValid()` reports false is the ordinary answer for an asset whose render resources have not been
created; `HasStreamingMetrics` stays false there rather than reporting its zeroes as "nothing is resident".

### What the scan skips, and the one rule it uses

Templates (CDOs and archetypes — excluded by the iterator itself and re-checked), and anything whose **package** is
the transient package or lives under `/Engine/Transient`. That one package-scoped test is what removes editor
thumbnails, asset-preview render targets and every other piece of editor scratch — they are all parented to a
transient package, which IS the `RF_Transient` outer.

Deliberately **not** a test of the object's own `RF_Transient` flag: an object flagged transient inside a real
package still has a path the reader can navigate to, and dropping it would make the row count disagree with the
Content Browser for no reason the reader could see. The consequence is the property the page relies on — every row
names a real asset path, which is what makes the double-click action meaningful.

### Model shapes

- **Rows are NOT sorted on store** — the opposite of findings. The reader picks the column, so a stored order would
  be a fourth ordering nothing renders from. `Run_MemoryScan` does sort by path before returning, because object
  iteration order follows the UObject array's allocation order and two identical sessions do not share it.
- **The sort is total, and the tie-break does not flip.** `Get_SortedMemoryRows` orders by the chosen column through
  `Compare_MemoryRows`, then by asset path **ascending in both directions**. Reversing the tie-break with the arrow
  would make two rows the reader cannot tell apart swap on a direction toggle — the same churn `TArray::Sort`'s
  instability causes, wearing a different hat.
- **Dimensions sort on a KEY, not on the printed string** — pixel count for a texture, LOD0 triangles for a mesh.
  Sorting `"2048 × 2048"` as text puts 1024 after 2048 and before 512, which is alphabetical order pretending to be
  a size. Each table is homogeneous, so one key per row kind is total within the table it appears in.
- **An unmeasured streaming row sorts BELOW every measured one** (`-1`), rather than mixing in among the
  zero-resident ones. "Nothing was measured" is not "nothing is resident".
- **`_MemoryFilterString` is its own field, not `_Filter.FilterString`.** The two pages are searched independently —
  a query typed on the findings page silently narrowing the memory tables would be a list the reader cannot explain.
  The matching SEMANTICS are shared: `Matches_MemoryFilter` runs the same `Passes_TextFilter` over a memory-shaped
  haystack (name + path + class + format). Dimensions and sizes are deliberately absent from that haystack — `"64"`
  would match a 64-wide texture, a 640-wide one and one whose path contains the number, which is three questions.
- **Totals are the CENSUS, the selector counts are the VIEW.** `Get_MemoryTotals` is unfiltered — the page header
  answers "what is resident", and a filter somebody typed must not make it drop and read as a session that got
  lighter. `Get_VisibleMemoryRowCount` is filtered, and it is what the sub-table selector and the Memory page tab
  bind to. Same split as Dashboard-vs-Findings.
- **`Reset()` drops the census and keeps the filter**, exactly as it does for findings. A play session loads and
  unloads content wholesale, so a table taken before the boundary describes objects that may not be there any more —
  but the question the reader asked survives the answer.

### Page contracts

- **`SCkDebug_UnderlineTabs` for the sub-table selector, not `SSegmentedControl`.** The segmented control carries no
  per-segment count, and a selector that could not say how many render targets there are would make the reader click
  each one to find out. The underline bar has `CountText` per tab and is already the page bar one level up.
- **The row is an `SMultiColumnTableRow`.** The columns are sortable and resizable and `SHeaderRow` owns both; a row
  that laid its own cells out would drift from the header the moment somebody dragged a divider. Its tooltip is set
  with `SetToolTipText` AFTER the super's `Construct` — `SMultiColumnTableRow::Construct` forwards only style,
  padding, selection and the drag-drop events to `STableRow`, so a tooltip passed through its arguments is silently
  dropped.
- **Row identity is keyed by asset path**, reused between filter and sort passes and dropped by a Refresh — the same
  rule the findings list holds to. The refresh trigger compares the row **order**, not just the membership: a sort
  reverses the list without adding or removing a row, and `SListView` renders in item-source order.
- **The size cell carries a thin `SCkDebug_MeterBar`** whose fraction is of the LARGEST row in the active table, not
  of the total — a share of the total leaves every bar invisible on a thousand-row table. Its colour is
  `ck::debug_axes::Get_HeatColor`, never a hand-written hex.

  The denominator is a `TAttribute`, not a construction-time read. A filter pass REUSES the row widgets whose asset
  path survived it, so a denominator captured when the row was built outlives the filter that moved it: filter out
  the biggest texture and every remaining bar keeps being drawn against a row that is no longer in the table, and
  reads near-empty. The colour binds off the same attribute so length and heat cannot drift apart.
- **The Memory tab shows a count and never a warn dot.** This page has no severity concept: a texture being large is
  a fact, not a finding, and a dot would be the tool asserting a judgement no threshold here backs up.
- **Double-click syncs the Content Browser through `Navigate_ToTarget`** — the same routine an asset-targeted
  finding uses, split out of `Navigate_ToFinding` in P4 so a second caller could not invent a second way of doing
  it. The context menu carries the same action above the copy entries, exactly as the findings menu does.
- **The empty state and the table are pre-allocated and toggled by `Visibility`**, never slotted in on a data
  change — swapping a page's children during a rebuild is the one-frame-scrunch defect.

---

## The profiling launcher

`Public/CkOptimizationDebugger/Commands/CkOptimizationDebugger_ProfileCommands.{h,cpp}` +
`DoCreate_ProfilingPage`. A plain-data catalog of 27 entries across six shelves, an executor that drives the active
level editor viewport, and a one-line console box under it.

**Every command string, view-mode index and visualization mode name below was read out of this engine's source, not
remembered.** The two entry families that were dropped are named at the end of this section with the reason. A
profiling panel's whole value is that its buttons do what they say; a guessed string is worse than a missing button,
because the reader concludes the FEATURE is broken rather than the launcher.

### The lever table — and why `viewmode` is not one

| Lever | Engine call | Used by |
|---|---|---|
| `EngineStat` | `UEngine::ExecEngineStat(World, ViewportClient, Name)` (`Engine.h:3829`, impl `UnrealEngine.cpp:18654`) | the eight timing stats |
| `ConsoleCommand` | `GEngine->Exec(World, Cmd, *GLog)` → exec handlers, then `IConsoleManager::ProcessUserConsoleInput` (`UnrealEngine.cpp:5415`) | `profilegpu`, and the custom box |
| `ViewMode` | `FEditorViewportClient::SetViewMode(EViewModeIndex)` (`EditorViewportClient.h:991`) | the six view modes |
| `NaniteVisualization` | `FEditorViewportClient::ChangeNaniteVisualizationMode(FName)` (`EditorViewportClient.cpp:3122`) | the five Nanite modes |
| `LumenVisualization` | `FEditorViewportClient::ChangeLumenVisualizationMode(FName)` (`:3139`) | the four Lumen modes |
| `VirtualShadowMapVisualization` | `FEditorViewportClient::ChangeVirtualShadowMapVisualizationMode(FName)` (`:3156`) | the three VSM modes |

Three findings from the engine that this design is built on, each of which would have produced dead buttons:

- **`viewmode <name>` does not work in an editor viewport.** It is wired into `UGameViewportClient::Exec` only
  (`GameViewportClient.cpp:3406-3409` → `HandleViewModeCommand`, `:3838`). `UUnrealEdEngine::Exec`
  (`UnrealEdSrv.cpp:633`) does not handle the token, and `UEditorEngine` does not override `Exec` at all. So every
  viewport entry here carries an `EViewModeIndex` and no console string, and the catalog spec asserts that the
  console string is EMPTY on those entries.
- **`r.Lumen.Visualize.ViewMode` is inert.** The string cvar is registered (`LumenVisualizationData.cpp:99-103`) but
  nothing in Runtime or Renderer reads it back — `FLumenVisualizationData` has no `Update()` at all. The lever that
  does work from a console string is the unrelated integer `r.Lumen.Visualize` (`LumenVisualize.cpp:25-35`), which
  bypasses the show flag entirely. We drive the viewport client instead, which is what the engine's own menu does.
- **`r.Shadow.Virtual.Visualize` alone is inert too.** Its `Update()` computes a `bForceShowFlag`, but the only
  caller discards the return value and is itself reached only once `EngineShowFlags.VisualizeVirtualShadowMap` is
  already true (`VirtualShadowMapArray.cpp:937, 2148-2154`). No `SetVisualizeVirtualShadowMap(true)` auto-force
  exists anywhere. Nanite is the one family where the cvar IS self-sufficient
  (`DeferredShadingRenderer.cpp:1984-1990`) — and it is still driven through the viewport client, for uniformity
  and because that is what selects the view mode too.

### The catalog

| Id | Kind | What it pulls |
|---|---|---|
| `Timing.Fps` | Toggle | `stat fps` |
| `Timing.Unit` | Toggle | `stat unit` |
| `Timing.UnitGraph` | Toggle | `stat unitgraph` |
| `Timing.Game` | Toggle | `stat game` |
| `Timing.SceneRendering` | Toggle | `stat scenerendering` |
| `Timing.Rhi` | Toggle | `stat rhi` |
| `Timing.InitViews` | Toggle | `stat initviews` |
| `Timing.Streaming` | Toggle | `stat streaming` |
| `Gpu.ProfileGpu` | OneShot | `profilegpu` |
| `ViewMode.Lit` | ViewMode | `VMI_Lit` |
| `ViewMode.LightComplexity` | ViewMode | `VMI_LightComplexity` |
| `ViewMode.ShaderComplexity` | ViewMode | `VMI_ShaderComplexity` |
| `ViewMode.QuadOverdraw` | ViewMode | `VMI_QuadOverdraw` |
| `ViewMode.LightmapDensity` | ViewMode | `VMI_LightmapDensity` |
| `ViewMode.StationaryLightOverlap` | ViewMode | `VMI_StationaryLightOverlap` |
| `Nanite.{Overview, Mask, Triangles, Clusters, Overdraw}` | ViewMode | `VMI_VisualizeNanite` + that mode name |
| `Lumen.{Overview, PerformanceOverview, LumenScene, ReflectionView}` | ViewMode | `VMI_VisualizeLumen` + that mode name |
| `Vsm.{mask, mip, cache}` | ViewMode | `VMI_VisualizeVirtualShadowMap` + that mode name |

`fps` / `unit` / `unitgraph` are engine stats (`UnrealEngine.cpp:2331,2333,2345`); the other five are stat GROUPS
(`GlobalStats.inl:25,28,61,67,77`). `ExecEngineStat` serves both, which is why one entry kind covers them.
Visualization mode names are exactly the first argument of the engine's own `AddVisualizationMode` calls
(`NaniteVisualizationData.cpp:31-71`, `LumenVisualizationData.cpp:24-78`,
`VirtualShadowMapVisualizationData.cpp:16-100`) — Nanite and Lumen are `PascalCase`, VSM is lower-case, and that
inconsistency is the ENGINE's, so it is reproduced rather than tidied.

### Deliberate omissions

- **The VSM `casters` mode.** Every other registered VSM mode takes `AddVisualizationMode`'s default view-mode index
  (`VirtualShadowMapVisualizationData.h:92`); `casters` explicitly passes `VMI_ShadowCasters`
  (`VirtualShadowMapVisualizationData.cpp:57`), and `ApplyViewMode` only sets the VSM show flag for
  `VMI_VisualizeVirtualShadowMap` (`ShowFlags.cpp:402`). It reaches the renderer through a different path
  (`DVSM_ShadowCasters`) that was not traced end-to-end, so it is left out rather than shipped as an entry whose
  "is it on" answer would be a guess. Adding it means recording its own view-mode index on the entry.
- **Nothing that needs a shader-complexity variant beyond `VMI_ShaderComplexity` and `VMI_QuadOverdraw`.**
  `VMI_ShaderComplexityWithQuadOverdraw` exists and works; it was left off because two entries already answer the
  question and a third would be a third thing to explain.

### State is READ, never recorded

There is no `TSet<FName>` of active toggles on the window, and that is a deliberate departure from the obvious
design. Every "is this on" answer comes off the viewport client:

| Entry kind | Read |
|---|---|
| stat overlay | `FEditorViewportClient::IsStatEnabled(Name)` — populated by `SetStatEnabled` for engine stats AND, through `FCoreDelegates::StatEnabled` (`StatsCommand.cpp:773` → `EditorViewportClient.cpp:2603`), for stat groups |
| view mode | `GetViewMode() == entry's index` |
| visualizer | `Is{Nanite,Lumen,VirtualShadowMap}VisualizationModeSelected(Mode)`, which checks the view mode AND the mode name |

The consequence is the one a recorded set could not buy: **a stat somebody toggled from the console, or a view mode
they picked from the viewport's own Show menu, moves these controls too.** The two surfaces cannot disagree, because
there is only one source. `FString` comparison is case-insensitive, which is what lets the engine menu's `"FPS"` and
this catalog's `"fps"` name one overlay.

`ck_optimization_debugger_profile_commands::Is_CommandActive(command, state)` is the pure rule over a plain
`FCkOptimizationDebugger_ProfileViewportState`; `Get_IsCommandActive(command)` is the live wrapper. The wrapper
short-circuits the stat case to `IsStatEnabled` rather than building a state struct — it is the SAME rule (a
`Contains` over the same array) and it avoids copying that array once per button, per paint.

### Page contracts

- **The requested toggle state is deliberately ignored.** `OnStateChanged` runs the entry's lever in both
  directions, because a stat command TOGGLES and a view mode SETS — neither engine lever offers "off". The next
  paint reads back what actually happened, so a press that did nothing shows as a control that did not move.
- **Exclusivity is the viewport's property, not this bar's.** Every `ViewMode`-kinded entry — the plain view modes
  AND all three visualizer families — competes for the one view mode a viewport has, so setting any of them clears
  the rest with nothing here enforcing it. `TryGet_ActiveViewportCommandId` is the projection that states it, and
  the header pill prints its answer.
- **`ViewMode.Lit` is the only way back, and it exists once.** Each visualizer shelf's section header carries a
  `Lit` button that runs that ONE catalog entry, rather than three more entries that would all do the same thing.
- **The page tab's count excludes Lit.** Lit is the resting state; a badge reading `1` on a freshly opened editor
  would be a badge nobody could ever clear. No warn dot either: leaving Quad Overdraw on is worth noticing but it
  is the reader's own doing, and this tool's dot means "something is wrong with your level".
- **Enabling a stat also turns on `SetShowStats` and `SetRealtime`, and says so — and the LAST stat going off puts
  real time back.** A viewport only draws stats while it is showing them and running in real time
  (`SEditorViewport::IsStatCommandVisible`), so an overlay enabled without those is indistinguishable from one that
  did not turn on. Both are viewport state the reader did not ask for directly, which is why the status line names
  them.

  `SetRealtime` writes a value the editor PERSISTS BETWEEN SESSIONS, so raising it and never lowering it left the
  reader's viewport running in real time forever with nothing on screen explaining why. The launcher therefore keeps
  exactly one piece of state — which viewport client it raised real time on — and restores it once
  `Has_AnyCatalogStatEnabled` reports the last overlay has gone off. It is raised only when `IsRealtime()` was
  already false, so a reader who was in real time before they opened this page keeps it.

  **That is not a contradiction of "state is READ, never recorded" below.** That rule is about which entries are
  ACTIVE, and it still holds — every toggle reads the viewport. This is an undo record for a side effect, which is a
  different thing, and the absence of one was the defect.
- **A command nobody claimed is a FAILURE.** `GEngine->Exec` returning false means no handler took the string, and
  reporting that as a success toned the status strip Ok and pushed a typo'd cvar into the RECENT rail as though it
  had worked. The rail is a list of things worth running again. The editor world is null-checked before every
  `Exec` too — it is null between map transitions and several exec handlers dereference it.
- **The page tab's count reads `Get_ActiveCommandCountLive()`, never `Get_ActiveCommandCount(Get_ViewportState())`.**
  That tab bar sits outside the page switcher, so it paints on every page, every frame, twice per tab — and the
  state struct copies the enabled-stat array to answer a question about view modes that never reads it. The live
  variants (`Get_ActiveCommandCountLive`, `TryGet_ActiveViewportCommandIdLive`) are the same rules with no
  allocation. `Get_ViewportState()` remains the form a SPEC drives, because a spec needs to hand in a state.
- **The custom box is a plain `SEditableTextBox`, and commits on Enter ONLY.** `SCkDebug_SearchBar` was the
  alternative and is the wrong shape: it debounces and reports every keystroke, and half a console command must
  never run. Commit-on-focus-loss is likewise refused — the opposite of `SCkDebug_NumericEditor`'s rule, because a
  threshold that commits when the reader clicks away is a value they meant, and a command that runs when they click
  away is a command they never pressed anything to run.
- **The recent rail is the page's only derived state**, which is why `DoRebuild_Profiling` exists at all and why it
  is the only profiling call in `DoRebuild_All`. Chips are CLICKABLE, which is legal because this is a fixed panel
  and not an `SListView` row; a click re-fills the box and runs it through the same path a typed command takes.
  `Push_RecentCommand` de-duplicates case-insensitively (re-running MOVES a command to the front) and caps at five.
- **Outside an editor session every control is disabled and a line says why.** `Get_CanExecute()` is
  `GEditor != nullptr && GCurrentLevelEditingViewportClient != nullptr`; the module ships in packaged
  Development/DebugGame builds where neither exists.
- **`GCurrentLevelEditingViewportClient`, not `GEditor->GetActiveViewport()`.** The latter hands back an `FViewport`
  whose client would have to be downcast without RTTI; the former is already typed
  (`FLevelEditorViewportClient*`, `Editor.h:907`) and is the same "last focused level viewport" discriminator the
  shared viewport picker uses.

---

## The project cleanup pass

`Public/CkOptimizationDebugger/Analysis/CkOptimizationDebugger_CleanupScan.{h,cpp}` +
`Public/CkOptimizationDebugger/Commands/CkOptimizationDebugger_CleanupCommands.{h,cpp}` +
`ck_optimization_debugger_model`'s cleanup projections + `DoCreate_CleanupPage`. Four review categories behind one
`SCkDebug_UnderlineTabs` selector over one `SListView`, plus one action button whose verb follows the active category.

**Everything on this page is presented for REVIEW.** A scan deletes nothing, renames nothing and saves nothing. The
actions that can remove content are the reader's own presses, and each one ends in a dialog the engine owns.

It is its own explicit pass, independent of the level scan and the memory scan in every direction: **Scan Project**
walks `/Game`, and nothing else on this window triggers it.

### The five categories, as implemented

| Category | Fires for | How it is decided |
|---|---|---|
| **Unreferenced** | an asset under `/Game` that no other package references on disk **and that no registered external-reference provider claims** | `IAssetRegistry::GetReferencers(PackageName, Out, EDependencyCategory::Package, EDependencyQuery::NoRequirements)` — hard AND soft — with self-references discarded, over the package's PRIMARY asset, minus the always-rooted class list below, **then `FCk_AssetReferenceProviderRegistry`** for the references that graph structurally cannot see |
| **Possible duplicates** | assets sharing a **name**, a **class** and a **disk size** across different folders | `Build_DuplicateGroupKey(name, class, size)`, lower-cased on the two text halves. **Content is never hashed and never compared.** Groups of one are dropped |
| **Name collisions** | assets sharing an exact **name** across different folders, whatever their class or size | `Build_NameCollisionGroupKey(name)` — the lower-cased name and NOTHING else. Grouped like duplicates; groups of one are dropped. **Carries no action** (see below) |
| **Redirectors** | `FAssetData::IsRedirector()` under `/Game` | the row's detail is the REFERENCER COUNT, not the redirector's destination — reading a destination means loading the redirector, and a scan that loaded every redirector in the project to label a row would be doing the thing this page refuses to do |
| **Dirty packages** | `FEditorFileUtils::GetDirtyPackages`, narrowed to `/Game` | the one category that reads LIVE editor state rather than the registry, and the only one that can go stale between scans |

### Name collisions are a different question from duplicates, and the split is deliberate

A duplicate match asks **"is one of these redundant?"** and needs class and size to say so. A name collision asks
**"does this name resolve to what the author meant?"**, which class and size have nothing to do with. An `SM_Rock` and
a `T_Rock` are not duplicates and never will be — but every short-name lookup, every codegen accessor keyed on the
name, and every Content Browser search still has to pick one of them, and which one it picks is not something the
project decides.

Concretely, in this codebase: `UCkAssetRegistrySubsystem` generates one `assets::` accessor per asset NAME, and on a
collision the loser is renamed `<Name>_DUP1` (`CkAssetRegistrySubsystem.cpp:676-687`). Which asset wins depends on
asset-registry iteration order, so `assets::Get_Foo()` can resolve to a different asset on a different machine.

Consequences that fall out of it being a different question:

- **Folding the two passes together would split exactly the pairs worth reporting.** Relaxing the duplicate key to
  just the name would ALSO stop reporting duplicates, and matching only same-class-same-size names would miss the
  mesh-vs-texture collision that is the common case.
- **A package the registry could not size still takes part**, unlike the duplicate pass. A missing size is no evidence
  either way about a NAME, and dropping it would hide a collision for a reason unrelated to the question.
- **A collision group carries no class and no size**, and its rows carry no bytes. Members share only the name, so
  copying the first member's class and size onto the group would print one member's facts as the group's. The header
  says "N assets share this name"; nothing here is reclaimable, and the sort therefore falls through to member count —
  the group most worth looking at is the one the most assets are fighting over.
- **The category has NO action, on purpose.** Resolving a collision means renaming an asset, which is a content
  decision no batch action should make for the reader. `TryGet_ActionForCategory` returns null and the button says why
  in those words — "at most one action per category" is now the catalog rule, and
  `Ck.OptimizationDebugger.Cleanup.ActionCatalog` asserts the null explicitly so "deliberately actionless" stays
  distinguishable from "somebody forgot".
- **The label is "Name collisions", never "Duplicate names".** The second would send the reader looking for something
  to delete.
- **Scope limit, stated rather than assumed: it walks the same package table the other passes do**, which holds one
  entry per package — the PRIMARY asset. A secondary object inside a multi-object package therefore does not take
  part, even though the AngelScript generator's own discovery is per-asset rather than per-package. In `/Game` content
  that is one asset per package almost without exception, so the two agree in practice; if a project ever grows
  multi-object packages under `/Game`, this pass narrows silently and would need its own asset-granular walk.

### Unreferenced consults the external-reference registry, because the package graph is not the whole truth

`IAssetRegistry::GetReferencers` answers only from serialized package edges. An asset reached from AngelScript goes
through a generated `assets::` accessor — a text call resolved at runtime — which creates **no edge at all**. Asking
the graph alone therefore does not merely miss it; it reports a script-critical asset as unreferenced and offers it to
`ObjectTools::DeleteAssets`, and the engine's own delete dialog derives its referencer list from the SAME graph, so
the reader's safety net agrees with the mistake.

The walk asks `FCk_AssetReferenceProviderRegistry` (`CkCore/Reference/`) in the same breath as the graph.
`CkAngelscriptGenerator`'s asset subsystem registers under `"AngelScript"`, backed by the same two maps its
`OnAssetsPreDelete` warning reads — so the dialog and this scan agree by construction rather than by two copies of one
rule. Neither module links the other, which is what keeps this DeveloperTool module packageable.

Three states, and the page must never collapse them:

| State | How | What the status strip says |
|---|---|---|
| Provider(s) registered, some assets claimed | `Get_HasAnyProvider()` true, count > 0 | "N asset(s) kept off the unreferenced list because AngelScript references them", Ok tone |
| Provider(s) registered, none claimed | true, count 0 | nothing extra — the count is simply correct |
| **No provider registered** | false | "no external-reference provider was registered, so references made only from script or config were NOT considered", **Warn tone** |

The third is the load-bearing one. "Nobody was there to ask" is not "asked and found none", and reporting the
unreferenced count bare in that state presents a project this pass could not fully consider as one it did — the same
defect `RequiresEditor` prevents one level up. It is Warn-toned for the same reason a cancelled or still-indexing scan
is: all three are partial answers, and the tone is what tells the reader that before they read the counts.

A hit **suppresses the row and increments a count** rather than dropping it silently: a row that vanished would make
this tab disagree with the Reference Viewer with nothing on screen explaining why, which is why an excluded level is
greyed rather than removed.

**The registry is resolved ONCE, before the walk.** Asking per row would let a provider that unregistered half way
through produce a list whose first half considered script references and whose second half did not — the hazard the
memory page's streaming guard is asked once for.

**Still not the whole truth.** Soft paths assembled at runtime in a Blueprint or a DataTable, and any reference no
provider covers, remain invisible. The Unreferenced hint says so.

**The conservative duplicate rule, verbatim, because it is the one claim on this page that could be over-read:**
*possible duplicates are identified conservatively using matching asset name, class and disk size; they are presented
for review and are not claimed to be byte-identical.* That sentence is in the tab LABEL ("Possible duplicates"), in
`Get_CleanupCategoryHint`, and in every row's own detail line ("Same name, class and size as: …"), because the reader
who only ever sees one of the three is the one most likely to act on it.

### The action-safety contract

**Deletion always goes through the engine's own dialog.** `ObjectTools::DeleteAssets(Assets, bShowConfirmation)` is
called with `true`, and there is no parameter anywhere in this module that can turn it off. That dialog is the one
that lists what still references an asset and offers Force Delete — it *is* the safety, and this module's whole job
is to put the right set in front of it. A cancelled dialog is reported as a Warn-toned "nothing was deleted", never
as an error: the reader exercising that veto is the feature working.

| Action | Engine call | Applies to | Notes |
|---|---|---|---|
| **Delete Selected** | `ObjectTools::DeleteAssets(TArray<FAssetData>, /*bShowConfirmation*/ true)` (`UnrealEd`) | Unreferenced + Duplicates | rows are re-resolved through `IAssetRegistry::GetAssetByObjectPath` first, so a scan minutes old cannot hand a vanished asset to the dialog. `DeleteAssets` may delete MORE than it was given — it expands localized variants when confirmation is on — which is why the result reports "deleted N of the M offered" |
| **Fix Up Redirectors** | `IAssetTools::Get().FixupReferencers(TArray<UObjectRedirector*>)` (`AssetTools`) | Redirectors | the engine defaults are kept: prompt for checkout, then delete the redirectors it fixed up. This is the one path that LOADS — the fix-up API takes objects, and it is an action the reader pressed, not a scan. It returns `void` and is **not necessarily synchronous** — see below |
| **Save Dirty Packages** | `FEditorFileUtils::SaveDirtyPackages(/*bPromptUserToSave*/ true, true, true)` (`UnrealEd`) | Dirty packages | **the one action that ignores the selection**, and its `OperatesOnSelection == false` says so in the catalog. The editor's save prompt is a checklist over every dirty package and offers no supported narrowing; a button that implied one would save more than it said |

**Neither of the two non-deleting actions is trusted to report its own outcome, and both used to be.**

- `FixupReferencers` returns `void`, and while the asset registry is still loading it puts up a discovering-assets
  dialog and hands the work to a completion delegate. Reporting "referencing packages were rewritten and re-saved"
  before anything had been read was the window claiming an outcome it had no way to know — on the exact state this
  page already records as `WasStillIndexing`. The result now says "handed to the engine's fix-up" in that case and
  sets `ShouldRefresh = false`, because re-scanning on top of an operation that has not started describes a state
  nothing has reached.
- `SaveDirtyPackages` returns `true` when the prompt was DECLINED as well as when it saved — `bCanBeDeclined`
  defaults to true and makes "Don't Save" a successful outcome of the CALL. The action therefore counts
  `GetDirtyPackages` before and after and reports the difference, which is ground truth. Declining now reports
  "nothing was saved", which is what the module's own verify step has always asked for.

After any action that ran **and asked to be refreshed**, the window re-enters `DoRun_CleanupScan` — the same path
the button takes — because an action that changed what is on disk changed the answer the list is showing.

### Rules this page holds to

- **The catalog is plain data and the executor is one function.** `Get_AllActions()` is an immutable table of
  `FCkOptimizationDebugger_CleanupActionInfo`; `Can_RunAction` / `Get_ApplicableRows` / `Get_ActionDisabledReason` /
  `Build_ActionButtonLabel` are pure over a selection; `Run_Action` is the only thing that touches the engine and it
  returns `{DidRun, Message}`. Same split as the profiling launcher, and for the same reason: the rules are testable
  and the mutations are `[EDITOR-VERIFY]`.
- **Editor availability is NOT part of `Can_RunAction`.** It is a session fact, not a property of the selection, and
  folding it in would make a spec need an editor to assert a selection rule. The window ANDs the two through
  `Get_ActionsUnavailableReason()`, which returns the SENTENCE rather than a bool — "not here" and "not while you
  are playing" are different answers and a disabled tooltip has to give the right one.
- **Actions are refused during PIE.** Deleting an asset out from under a live world, or writing packages while the
  game holds them, is a decision nobody asked for — and the delete dialog's referencer list does not know about a
  play session's hard references either. The rows stay readable; only the button waits. The fixes page holds the
  same gate for the same reason.
- **`Run_CleanupScan` reports `RequiresEditor` outside the editor**, exactly as `Run_Scan` does. Without it the
  packaged build printed "0 unreferenced / 0 duplicates / 0 redirectors / 0 dirty — 0 B reclaimable" in Ok tone and
  reported a project it had never looked at as clean.
- **Cancel cancels all four passes.** It used to be polled only inside the unreferenced walk, so pressing it left
  the duplicate match and the redirector walk to run to completion; the dirty-package pass is now skipped outright
  once cancelled, because a partial answer that still filled one category in full is one the reader would trust.
- **At MOST one action claims each category**, which is what lets the page show ONE button instead of three that
  mostly disable. Not "exactly one" — `NameCollisions` deliberately has none, and the spec asserts that null by name.
  `Ck.OptimizationDebugger.Cleanup.ActionCatalog` pins it.
- **A mixed selection is narrowed, never refused.** A reader who rubber-banded a list is asking about the rows the
  button understands; `Get_ApplicableRows` is that narrowing and the button's count is what it found.
- **The always-rooted exclusion list is short and reasoned.** `World`, `Level`, `DataAsset`, `PrimaryDataAsset`,
  `PrimaryAssetLabel` — every one a class the on-disk reference graph structurally cannot see a reference to. Levels
  are named by project settings, data assets by config or code, a primary asset label exists to be discovered. It is
  matched on the class NAME with no class loaded, exactly as the disk breakdown classifies. It is deliberately not a
  place to add anything that merely feels risky: a list that grew would be a tab that reports nothing.
- **`/Game/Developers` is fair game and says so.** A personal sandbox is exactly where unreferenced content
  accumulates. The row's detail names the folder, because deleting somebody else's scratch is a different decision.
- **A package the registry cannot size does not take part in a duplicate match.** Grouping the unmeasured ones
  together would produce a "duplicate" set whose whole evidence is a missing number.
- **The cleanup census SURVIVES a session invalidation — MINUS the dirty-package rows.**
  `FCkOptimizationDebugger_Model::Reset` deliberately leaves `_CleanupRows` alone: a finding names an actor inside a
  world and a memory row names an object the play session may have freed, but a cleanup row names an ASSET, and an
  asset nothing references is unreferenced whether or not somebody pressed Play. Dropping it at a PIE boundary would
  throw away a scan that is still true.

  That argument covers unreferenced assets, duplicates and redirectors — all registry facts. It does NOT cover dirty
  packages, which are live editor state a play session changes, so `Drop_DirtyPackageRows()` removes exactly those
  at the boundary and the status strip says how many it dropped and that the rest still stands. Keeping them while
  the same strip said "results cleared" had the page contradicting itself in two places at once. `_HasCleanupScan`
  stays true: the other three categories are still a real answer, and returning the page to its empty state to
  remove one of them would throw away the scan the invariant above exists to keep.
- **Row identity is keyed `<category id>|<asset path>`.** The category is in the key because one asset can
  legitimately appear under two categories — a redirector nothing points at is both a redirector and unreferenced —
  and a key that dropped it would make the second appearance replace the first.
- **The list is hand-laid, not an `SHeaderRow`.** The two GROUPED categories carry group header LINES, and a header
  row has nowhere to put one. `Get_IsGroupedCleanupCategory` is the one predicate that decides grouping, indentation
  and which projection the rebuild calls — asked once rather than compared against a category literal in three places,
  which is how the third one gets forgotten. A group header carries the active category on its otherwise-empty row so
  the row generator can word itself: a header reading "N copies · X reclaimable" over a set of name collisions would
  be telling the reader to go delete one of them. The order is therefore the projection's and not the reader's: biggest first with the
  asset path breaking every tie, and duplicate groups heaviest-first by what keeping ONE copy would free.
- **The page tab counts the WHOLE census, not the active category.** The reader's question there is "is there
  anything to look at"; a number that changed when they clicked a sub-tab would answer a different one. No warn dot:
  an unreferenced asset is not a defect, and this tool's dot means "something is wrong with your level".
- **Reclaimable is the unreferenced bytes and only those.** A duplicate's bytes are reclaimable only once somebody
  decides which copy is redundant, a redirector weighs almost nothing, and a dirty package is not on disk yet —
  adding those together would put a number on a decision nobody has made.
- **A never-written package prints an em dash, not `0 B`** — in the rendered cell AND in the clipboard summary
  `Build_CleanupRowSummary` produces. Zero bytes on disk and nothing on disk are different statements, exactly as
  they are on the memory page's GPU column, and a copied line that disagreed with the row it came from was worse
  than either.

---

## Presentation contracts

The window owns no bespoke look: every surface is a `CkDebuggerCommon` widget over `FCkDebuggerStyle` brushes and
`CkStyle::` tokens — see [CkDebuggerCommon/CLAUDE.md](../CkDebuggerCommon/CLAUDE.md) for the binding rules. What is
specific to this module:

- **Severity tone is a model rule, not a widget rule.** `Get_SeverityTone` maps Critical → `Err`, Major → `Warn`,
  Minor → `Info`. Nothing here is ever `Ok`-toned: a finding is by definition something the reader may want to act
  on, and painting one green says the opposite. `Ck.OptimizationDebugger.Model.SeverityTones` pins the mapping and
  its distinctness.
- **The severity GLYPH comes off that same tone**, through `ck::debug_axes::Get_ToneIconId` — so colour and picture
  cannot drift, and this tool no longer owns an opinion about what severity looks like. It used to name `Skull` /
  `Flame` / `Note` out of the decorative `Icons/General/**` set, which had two problems: those are not the pictures
  UE uses for severity anywhere else, so a reader had to learn them here; and `Skull` simultaneously meant "Critical"
  here, "Failed" in the gallery, and "world trouble" in the crowd debugger — one picture, three meanings, in sibling
  tools sharing one launcher. See [CkDebuggerCommon/CLAUDE.md](../CkDebuggerCommon/CLAUDE.md) for the axis.
- **`Stopwatch` is this tool's identity** (launcher descriptor); `Target` is the Scan command. Severity has its own
  glyphs on the chrome's icon-action row — `Skull` Critical, `Flame` Major, `Note` Minor — and each category has
  one, used in the filter row AND on the group header so a category never means two pictures: `Cube` mesh,
  `Palette` texture, `Brush` material, `Bulb` lighting, `Person` actor, `Puzzle` blueprint, `Gear` project
  settings. Every id resolves through `FCkDebuggerStyle::Get_IconBrush` and renders through `SCkDebug_Icon` —
  never a bare `SImage`. (The `SCkDebug_IconToggle`/`SCkDebug_IconToolbar` path resolves the same ids through
  `FCkDebuggerCommonStyle`, which scans the same `Resources/Icons/**` tree under a different prefix.)
- **Category colour comes from the shared categorical ramp** (`ck::debug_axes::Get_CategoricalColor`), indexed by
  the enum value — never a hand-written hex.
- **The findings list is FLAT and carries its own group headers.** A finding group never nests, so a tree would buy
  an expander and nothing else; a flat list keeps one stable row-identity map instead of a map plus a shape
  signature. Header lines are keyed `group|<check id>` and set `ShowSelection(false)`; a finding line is keyed by
  its stable key. Nothing collides, because a stable key always contains a check id followed by `|`.
- **Row identity is reused between FILTER passes, dropped between SCANS.** A re-scan can reproduce a key with a
  different severity behind it, and reused row widgets are only rebuilt when the set changes — so `DoRun_Scan`
  clears the map, which is what a fresh answer is. **There is exactly one scan path**: the Scan button and the
  post-fix refresh both call `DoRun_Scan`, so a fix cannot refresh the list by a second route that forgets to drop
  the map.
- **Action buttons bind to cached selection state, never to a live projection.** `DoRefresh_SelectionCommands`
  recomputes `_HasSelectedFinding` / `_SelectedFixableCount` / `_FixButtonLabel` / `_FixButtonEnabled` /
  `_FixButtonTooltip` on every selection change and every findings rebuild; the buttons' `TAttribute`s read those
  fields. An attribute that re-derived the selection would do it on the paint path, every frame.
- **EVERY tab and sub-tab count reads a cached field too, and this one is not a nicety.** The page tab bar lives
  OUTSIDE the `SWidgetSwitcher`, so it paints on every page on every frame — and `SCkDebug_UnderlineTabs` evaluates
  `CountText` twice per tab, once for the badge's visibility and once for its text. Binding those straight to
  `Get_VisibleFindingCount` / `Get_VisibleMemoryRowCount` / `Get_VisibleCleanupRowCount` therefore put a full walk of
  that census on the paint path, twice, whether or not the page it described was even open. On a memory census of
  tens of thousands of resident objects that was the most expensive thing this window did.

  `_VisibleFindingCount`, `_VisibleSeverityCounts`, `_TotalSeverityCounts`, `_VisibleMemoryCountByTable` and
  `_VisibleCleanupCountByCategory` are refreshed by the same `DoRebuild_*` that refills the list they describe —
  which is exactly when they can change. The per-table and per-category arrays are indexed by the enum VALUE and
  sized from the largest one, and read through bounds-checked helpers.
- **The filter predicates answer the empty query BEFORE building their haystack.** `Passes_TextFilter` early-outs on
  an empty filter, but its argument is evaluated first — so `Build_MemorySearchText` allocated an `FString` per row
  on a query that matches everything, which is the state the lists spend most of their life in. All three
  `Matches_*Filter` functions now check the query first.
- **Both action buttons stay visible on every finding** and merely disable when they do not apply. A button that
  vanishes teaches nobody the feature exists, and the disabled tooltip answers "why is this one not fixable" in
  place. Double-click on a row is the discoverable twin of **Go To**; the context menu carries both actions above
  the copy entries, multi-select aware.
- **Row-safety.** Every list row (findings, memory tables, cleanup tables) is built from `STextBlock` / `SBox` /
  `SBorder` / `SCkDebug_Icon` / `SCkDebug_CategoryDot` / `SCkDebug_StatusPill` and fully INERT `SCkDebug_Chip`s
  only, uses the translucent `CkDebugger.TableView.Row` style, and keeps its copy actions on
  `OnContextMenuOpening`. The `FIX` chip carries no `OnClicked` **and no `CopyText`** — the first would eat the
  left-click that selects the row, the second would eat the right-click that opens the list's copy menu. A row
  generator must never allocate a brush.
- **Named file-local namespace** (`ck_optimization_debugger_<file>`) — this module compiles unity and same-named
  anonymous-namespace helpers collide.
- **Weak captures are named `WeakPanel`, never `WeakThis`** — `WeakThis` shadows `TSharedFromThis::WeakThis` and
  C4458 is an error here.

---

## Editor-only code in a DeveloperTool module

This module is included in packaged Development/DebugGame builds, so **`UnrealEd`, `GEditor`, the asset registry
walk and level iteration all sit behind `#if WITH_EDITOR`**, with a stated non-editor fallback rather than a
silent no-op: outside the editor the analysis pages report that scanning requires an editor session. Editor
dependencies stay behind `Target.bBuildEditor` in the `.Build.cs`; packaged windows use runtime Slate's global tab
manager.

---

## Thresholds

`UCkOptimizationDebuggerSettings` (`Public/CkOptimizationDebugger/Settings/`) is `config=GameUserSettings` with
`GetContainerName() == "Editor"`, i.e. **per-user, under Editor Preferences → Ck → Optimization Debugger**. An
analysis threshold is one QA person's calibration of what they want flagged, not shared project policy — tightening
one must never dirty a committed config file for the whole team.

**Checks never read the settings object.** `ck_optimization_debugger_thresholds::Build_FromSettings()` copies it
into `FCkOptimizationDebugger_Thresholds` once, when the scan starts, and every check takes that struct — see the
analysis-engine section above for why. A check that hard-codes a number is a check with no threshold at all; add
a `UPROPERTY` here and a field there, and `Ck.OptimizationDebugger.Analysis.ThresholdDefaults` will fail until the
two defaults agree.

---

## Tests

`Private/Tests/*.spec.cpp`, whole file inside `#if WITH_DEV_AUTOMATION_TESTS`, `IMPLEMENT_SIMPLE_AUTOMATION_TEST`,
`EditorContext | EngineFilter`, names `Ck.OptimizationDebugger.<Group>.<Case>`. Model fixtures build findings
through the real `Build_StableKey`, so the identity the sort tie-breaks on is the same string the window would
reuse a row by.

**Never make a real project asset an automated fixture.** Scans over real content belong to manual
`[EDITOR-VERIFY]` passes only — a spec whose result depends on what is in `/Game` is a spec that fails on someone
else's branch.

---

## Verification

- Run `Ck.OptimizationDebugger` for the model, threshold, finding-construction, **lightmap-finding identity**,
  fix-registry, batch-partition, **batch-confirmation**, selection-projection, navigation-routing, summary-delta,
  level-exclusion, size-formatting, severity-solo, memory-sort/filter/totals, **separable-GPU-size**, profiling
  catalog-integrity / group-projection / viewport-state-transitions / recent-commands, cleanup duplicate-grouping /
  exclusion-list / per-category totals / filter-narrowing / action-catalog, and registration coverage. **No spec runs a cleanup action**: every one of them deletes an asset,
  rewrites a package or writes to disk, and a spec that ran one would change the branch it runs on. **No spec walks
  `/Game`** either — the census depends on what is on somebody's branch. **No spec runs a profiling command**: every lever mutates a live
  editor viewport, and a spec that flipped one would leave the branch's viewport in Quad Overdraw. The catalog's
  structure, the group projection, the active-state rules and the recent-command history are the testable parts,
  and they are all pure. **No spec walks a world, the asset registry or the resident
  object graph**: the scan census, the disk breakdown and the memory tables are all derived from whatever happens to
  be loaded on somebody's branch, so a spec asserting any of them would assert the branch. The projections over them
  — the delta, the exclusion seam, the formatters, the solo state, the memory sort/filter/totals — are the testable
  parts, and they are all pure over hand-built rows. **No spec applies a fix**: every apply path
  mutates a real asset, a real level or a config file, and a spec that did that would be a spec that changes the
  branch it runs on. The mutation paths are `[EDITOR-VERIFY]` below.
- Run `Ck.DebuggerLauncher`; its catalog spec asserts the exact tool census, so adding or renaming this tab id
  requires editing `CkDebuggerLauncher/Private/Tests/CkDebuggerLauncherCatalog.spec.cpp` in the same change.
- `[EDITOR-VERIFY]` Open **Tools > Debug > CK Optimization Debugger** (or `ck.OptimizationDebugger 1`). Confirm the
  five page tabs switch the body and the status strip reads "No scan yet." with a neutral dot. Open the launcher
  and confirm the tool appears in the Tools group with the stopwatch glyph, and that clicking it twice focuses the
  existing tab rather than duplicating it.
- `[EDITOR-VERIFY]` With a real level open, press **Scan**. Confirm the progress dialog appears and **Cancel**
  yields a partial answer with a Warn-toned status saying so. On a completed scan: findings group under one header
  per check, worst group first; the Findings tab shows the visible count and a warn dot only while a Critical is
  visible; the severity toggles in the chrome's icon row and the category toggles above the list both prune the
  list and move the count; typing in **Filter** narrows and typing in **Highlight** dims non-matches without
  removing them; selecting a finding fills the detail panel and selecting a group header does not; right-click with
  several rows selected copies titles / paths / summaries joined by newlines. Open a level with an UNLOADED
  sub-level and confirm the status strip names it as not scanned. Enter and leave PIE and confirm the findings
  clear with the "re-scan" status rather than pointing at a dead world.
- `[EDITOR-VERIFY]` **The three narrowing filters.** On a scanned level: type `/Game/` into the scope box and
  confirm every `ProjectSettings.*` finding disappears while asset and actor findings stay; narrow to a real content
  subfolder and confirm only findings under it survive and the Findings tab count drops with the list while the
  Dashboard count does NOT. Clear it. Press **Has a suggested fix** and confirm every remaining row shows a `FIX`
  chip; press it again to restore. Set both at once and confirm you get the intersection rather than one of them.
- `[EDITOR-VERIFY]` **Muting, end to end.** Right-click a finding → **Mute (1)**; it leaves the list, the status
  strip reports the new muted count, and `1 muted` appears beside the **Show muted** toggle. Turn **Show muted** on:
  the finding returns carrying a `MUTED` chip. Multi-select two muted rows plus one unmuted and confirm the menu
  offers **Mute (3)** rather than Unmute — the verb follows the whole selection. Select only muted rows and confirm
  it reads **Unmute**. Re-scan and confirm the mute survives, then restart the editor and confirm it survives that
  too. Finally confirm a group HEADER cannot be muted (right-click one with rows selected; only the rows change).
- `[EDITOR-VERIFY]` **Severity glyphs.** Confirm the chrome's severity toggles and the finding rows show a red
  circle-with-x for Critical, an amber triangle-with-! for Major and a blue circle-with-i for Minor — UE's own
  shapes — and that each is legible at 16px. Open the Style Lab and change the palette: the glyphs must re-tint with
  everything else (this is what an `FAppStyle` brush could not have done). Then open the Goap, Eqs and Crowd
  debuggers and confirm their failure toggles now show the SAME error glyph rather than a skull.
- `[EDITOR-VERIFY]` **Navigation.** Select a mesh/texture finding and press **Go To** — the Content Browser jumps to
  the ASSET, not to an actor placing it. Select an `Actor.EmptyStaticMesh` or `Lighting.LightmapResolution` finding —
  the actor is selected alone and the viewport frames it. Select any `ProjectSettings.*` finding — Project Settings
  opens on **Rendering**. Double-click any row and confirm it does the same thing the button does. Unload the
  sub-level an actor finding names, press Go To, and confirm the status strip says the level is not loaded rather
  than the button appearing to do nothing.
- `[EDITOR-VERIFY]` **Fixes, one at a time.** On a scanned level, apply each of the ten fixes and confirm the button
  label reads the action's own verb. Then press **Ctrl+Z** and confirm the change reverses for all of them EXCEPT
  `ProjectSettings.TextureStreamingDisabled`, whose status line says it was written to `DefaultEngine.ini` and which
  Undo must not appear to reverse. Confirm the findings list re-scans itself after every fix except
  `Lighting.MovableLightCount`, which instead leaves the movable-light actors selected in the outliner.
- `[EDITOR-VERIFY]` **The four fixes added in P9, one at a time.** `Texture.MissingMipmaps`: confirm Mip Gen
  Settings reads FromTextureGroup afterwards and the texture rebuilds with mips; move a flagged texture into the UI
  group first and confirm the fix REFUSES rather than generating mips. `Mesh.NaniteMaterialIncompatible`: confirm
  Used With Nanite is ticked on each named material, that a shader compile is queued, and that a mesh whose Nanite you
  turned off since the scan is refused. `Lighting.LightmapResolution`: on an actor with TWO over-budget components,
  confirm BOTH are clamped to the budget (not just the first) and that a locked level refuses with nothing changed.
  `Blueprint.TickEnabled`: confirm Start With Tick Enabled is off afterwards and **Can Ever Tick is still ON** — if
  that second one is off, the fix is wrong and would break `SetActorTickEnabled`. Ctrl+Z each.
- `[EDITOR-VERIFY]` **The behaviour-change prompt.** Select only Nanite/sRGB fixes and confirm applying asks nothing.
  Add a `Blueprint.TickEnabled` finding and confirm a dialog appears whose text names a BEHAVIOUR change (not a
  deletion and not a config write) and says Undo reverses it. Decline and confirm nothing was applied.
- `[EDITOR-VERIFY]` **Fixes, batched.** Multi-select several fixable findings of different checks (include the
  texture-streaming one), confirm the button reads "Fix N Findings", apply, and confirm ONE Ctrl+Z reverses the whole
  transactional part in one step. Confirm the status strip reads "N fixed, M failed — rescanned: K finding(s)".
- `[EDITOR-VERIFY]` **The confirmation gate.** Select only property-edit fixes (Nanite, sRGB) and confirm applying
  them asks NOTHING. Add an `Actor.EmptyStaticMesh` or `Actor.InstancingCandidate` finding to the selection and
  confirm a Yes/No dialog appears naming how many fixes remove actors; decline it and confirm the status strip says
  nothing was applied and the level is untouched. Repeat with the texture-streaming finding and confirm the dialog
  names the config write separately and says Undo cannot reverse it.
- `[EDITOR-VERIFY]` **The PIE gate.** Enter PIE with a scanned level. Confirm the Apply Fix button and the cleanup
  action button are both disabled with a tooltip reading "Not while a play session is running", that the findings
  and cleanup lists are still readable, and that the fix entry has vanished from the findings context menu. Stop PIE
  and confirm both come back.
- `[EDITOR-VERIFY]` **The config write's failure path.** Make `DefaultEngine.ini` read-only, apply
  `ProjectSettings.TextureStreamingDisabled`, and confirm the fix REFUSES with a message naming the file rather than
  reporting success — and that Project Settings still shows Texture Streaming off (the CDO was rolled back). Clear
  the flag, set `r.TextureStreaming 0` at the console, apply again, and confirm the message says the config was
  written but a higher-priority console setting still has it off in this session.
- `[EDITOR-VERIFY]` **Level locks.** Lock a sub-level, select an `Actor.EmptyStaticMesh` finding inside it, and
  confirm the fix refuses with "its level is locked" and deletes nothing. Same for `Actor.InstancingCandidate`, and
  confirm no HISM actor was spawned before it refused.
- `[EDITOR-VERIFY]` **Real time is put back.** With the viewport NOT in real time, press **FPS** and confirm the
  status says the viewport was put into real time. Press it again and confirm the status says it was put back out of
  real time and the viewport stops updating. Repeat with two stats on: turning the first off must NOT restore real
  time; turning the second off must. Then put the viewport into real time yourself, toggle FPS on and off, and
  confirm real time is LEFT ON — it was not this tool's to lower.
- `[EDITOR-VERIFY]` **The dashboard.** Open the tool with no scan run and confirm the Dashboard page shows the
  empty-state card with a working Scan button, and the collapsed **Analysis thresholds** panel below it — and nothing
  else. Press Scan. Confirm six tiles appear (Actors / Static meshes / LOD0 tris / Materials / Lights / Findings),
  each with an em dash under it because there is nothing to compare against yet, and that LOD0 tris is abbreviated
  (e.g. `1.2M`). Delete a few actors and Scan again: the deltas appear, the count tiles' deltas are muted grey in
  both directions, and only the **Findings** delta is coloured — green when it went down, red when it went up.
  Confirm the Dashboard tab's own count is the UNFILTERED total: type a filter on the Level analysis page and watch
  the Findings tab count drop while the Dashboard tab count does not.
- `[EDITOR-VERIFY]` **Severity solo.** Click the Critical badge on the dashboard. The window switches to Level
  analysis showing only Critical findings, the chrome's Major/Minor toggles read off, and the status strip says so.
  Click it a second time and confirm the list does NOT go empty. Re-enable the two toggles in the title bar and
  confirm the whole list comes back.
- `[EDITOR-VERIFY]` **Levels + persistence.** Open a map with at least one loaded sub-level. Confirm one row per
  level with its actor count, the persistent level in bold, and an unloaded sub-level listed with an em dash and a
  DISABLED switch. Switch a sub-level off: the row greys, the hint reads "excluded from the next scan", the status
  strip names it, and **nothing re-scans**. Scan again and confirm that level contributes no findings and no actors
  to the tiles, while still being listed greyed. Restart the editor, re-open the tool, and confirm the switch is
  still off before any scan has run.
- `[EDITOR-VERIFY]` **Disk breakdown.** Confirm the section header prints a total and a package count, that the
  per-family rows are largest-first with meters proportional to the total, and that the sum of the rows looks like
  the header total. Compare the header against the size of the project's `Content` folder in Explorer — it should be
  the same order of magnitude, allowing for unsaved and non-`/Game` content.
- `[EDITOR-VERIFY]` **Threshold editing.** Expand **Analysis thresholds**. Confirm one row per threshold with the
  same label and hover text Editor Preferences → Ck → Optimization Debugger shows. Type a new value, press Enter,
  re-open Editor Preferences and confirm it changed there too. Type a value and click away without pressing Enter —
  it must still commit. Type a value and press **Escape** — it must not. Start typing into a field and, without
  committing, trigger a style revision (Style Lab); confirm the field keeps your half-typed text and the panel
  refreshes only once the edit ends. Then Scan and confirm the new threshold changed which findings appear.
- `[EDITOR-VERIFY]` **The memory analyzer.** Open the **Memory** page with nothing measured and confirm it shows the
  empty-state card with a working **Refresh**, three stat tiles reading `0` / `0 B` / `—`, and no table. Press
  Refresh. Confirm the three sub-table tabs appear with counts, the tiles fill in, and the status strip reports the
  resident asset count with a resource total and a GPU total. Confirm the **Textures** table lists real `/Game` and
  `/Engine` textures and that no thumbnail or preview render target appears in it. Compare the resource total's
  order of magnitude against `stat memory` / the texture streaming HUD — it should be the same ballpark, allowing
  for everything this page does not count.
- `[EDITOR-VERIFY]` **Memory sorting and columns.** Click each column header and confirm the arrow moves, the list
  reorders, and clicking the SAME header again reverses it. Confirm **Size** is right-aligned monospace with a thin
  bar per row whose longest bar is the biggest row, that **Dimensions** sorts by area rather than alphabetically
  (a `2048 × 2048` sorts above a `512 × 512`, never between `1024` and `4096` as text), and that **GPU** prints a
  byte figure on textures and an em dash on static meshes and render targets. Switch tables and confirm the sort
  column and direction are kept.
- `[EDITOR-VERIFY]` **Memory search, copy and navigation.** Type into the filter box: the table narrows, the
  sub-table counts and the Memory page-tab count drop with it, and the header tiles do NOT. Confirm typing a filter
  on the **Level analysis** page leaves the memory tables untouched, and vice versa. Right-click with several rows
  selected and confirm Copy Name / Copy Path / Copy Summary produce one line per row. Double-click a row and confirm
  the Content Browser jumps to that asset; confirm the context menu's **Show in Content Browser** does the same.
- `[EDITOR-VERIFY]` **The streaming guard, all three states.** With streaming on, confirm the Streaming column reads
  `resident / wanted` on streamable textures and "not streamable" on the rest, and that NO note appears above the
  table. Set `r.TextureStreaming 0`, press Refresh, and confirm every streaming cell becomes an em dash, a dim note
  above the table says streaming is disabled and that the sizes are still real, and the status strip is Warn-toned
  rather than Ok — and that nothing crashes or empties. Switch to the **Static Meshes** table and confirm the note
  is hidden there. Set `r.TextureStreaming 1`, Refresh, and confirm the numbers come back.
- `[EDITOR-VERIFY]` **Memory across a PIE boundary.** Refresh the memory page, enter PIE, and confirm the tables
  clear back to the empty state with the "results cleared" status rather than listing objects the play session
  freed. Refresh inside PIE and confirm the count is larger than it was in the editor.
- `[EDITOR-VERIFY]` **The profiling launcher, shelf by shelf.** Open the **Profiling** page with a level open.
  Confirm six sections — Timing stats, GPU, Viewport visualizers, Nanite, Lumen, Virtual shadow maps — each with a
  section header and a row of controls that WRAPS rather than clips when the tab is narrowed to about 400px.
  Confirm the header pill reads `viewport: Lit`. Press **FPS**: the overlay appears in the level viewport, the
  toggle lights, the status strip says the viewport was put into real time, and the page tab's count goes to 1.
  Press it again and confirm both the overlay and the toggle go off. Repeat for **Unit** and **RHI** — a group stat
  and an engine stat behave the same. Then toggle `stat fps` from the editor's own console and confirm THIS page's
  toggle follows it without anything being clicked here.
- `[EDITOR-VERIFY]` **View modes and the exclusivity rule.** Press **Light complexity**: the viewport recolours, the
  toggle lights, the header pill reads `viewport: Light complexity`. Press **Quad overdraw** and confirm Light
  complexity goes off by itself — nothing switched it off, a viewport has one view mode. Press **Lit** and confirm
  everything in all four viewport shelves reads off and the tab count drops back to the number of enabled stats.
  Then switch the view mode from the level viewport's own View Mode menu and confirm this page's toggles follow.
- `[EDITOR-VERIFY]` **Each visualizer family.** On a level with Nanite meshes press **Nanite → Overdraw**: the
  viewport enters Nanite visualization, the pill names it, and the other four Nanite toggles read off. Press the
  **Lit** button in the Nanite section's own header and confirm it returns to normal. Repeat for **Lumen → Lumen
  scene** and **Virtual shadow maps → Cached page** — on a level with Lumen and VSM enabled, each must visibly
  change the viewport and not merely light its button. A visualizer that lights its toggle but leaves the viewport
  unchanged is the exact failure this phase's engine verification exists to prevent; report it rather than
  re-pressing.
- `[EDITOR-VERIFY]` **GPU capture.** Press **Profile GPU** and confirm the GPU profiler window opens for one
  captured frame and the status strip reads `Ran: profilegpu`. It is a button, never a toggle — confirm it does not
  latch.
- `[EDITOR-VERIFY]` **The custom command box.** Type `r.ScreenPercentage 50` and press Enter: the viewport
  resolution visibly drops, the status strip echoes the command, and a `RECENT` rail appears with one chip. Type
  `stat unit` and press Enter, then click the `r.ScreenPercentage 50` chip and confirm it re-fills the box AND runs.
  Run `r.ScreenPercentage 50` a third time and confirm the rail still holds two chips with it at the front, not
  three. Run six different commands and confirm the rail caps at five. Type a nonsense command and confirm the
  status says nothing claimed it rather than reporting success. Type half a command and click away — nothing must
  run; press Escape with text in the box — nothing must run.
- `[EDITOR-VERIFY]` **No editor viewport.** Close every level viewport (or open the tool in a packaged Development
  build). Confirm every profiling control is disabled, the explanation line appears above the shelves, and the
  header pill reads `no editor viewport`.
- `[EDITOR-VERIFY]` **The cleanup pass, first look.** Open the **Cleanup** page with nothing scanned and confirm it
  shows the empty-state card with a working **Scan Project** button, four sub-tabs with no counts, and no list. Press
  **Scan Project**. Confirm a progress dialog appears with four steps and that **Cancel** yields a partial answer
  whose status line says the scan was cancelled. On a completed pass: the five header tiles fill in (four counts plus
  Reclaimable), the sub-tabs carry counts, the page tab shows the WHOLE census count, and the status strip reads
  `N unreferenced / M possible duplicate(s) / R redirector(s) / D dirty — X reclaimable, scanned at <time>`.
- `[EDITOR-VERIFY]` **Each category.** **Unreferenced**: spot-check three rows in the Content Browser's Reference
  Viewer and confirm nothing points at them; confirm no level, no data asset and no primary asset label ever appears
  in the list; confirm a `/Game/Developers` asset appears with its detail saying so. **Possible duplicates**: confirm
  each group has a bold header line naming the asset with "N copies · X reclaimable", that members are indented under
  it, and that the header line cannot be selected. Confirm the tab reads **Possible duplicates** and the line under it
  says the match is name + class + size and is not a claim of byte-identity. **Redirectors**: confirm each row's
  detail is a referencer count. **Dirty packages**: modify an asset without saving, re-scan, and confirm it appears
  with an em dash rather than `0 B` if it has never been written.
- `[EDITOR-VERIFY]` **The external-reference gate — the defect this closes.** Find (or author) a `/Game` asset that
  NOTHING references by package but that an `.as` file reaches through its generated `assets::` accessor. Scan the
  project. Confirm it does **not** appear under Unreferenced, that the status strip says "N asset(s) kept off the
  unreferenced list because AngelScript references them", and that the tone is Ok. Then confirm the Reclaimable tile
  does not include its bytes. Remove the `.as` call, let AngelScript recompile (the usage map rebuilds on PostCompile),
  re-scan, and confirm the asset now DOES appear — the gate must track the script, not a snapshot of it.
- `[EDITOR-VERIFY]` **The provider-absent state.** This is the one that must never read as a clean project. Launch with
  `-NoCkAsRegen`, or otherwise reach a session where the AngelScript asset subsystem did not initialize, and scan.
  Confirm the status strip says "no external-reference provider was registered, so references made only from script or
  config were NOT considered", **and that the strip is Warn-toned rather than Ok**. A bare unreferenced count in this
  state is the tool presenting a project it could not fully consider as one it did.
- `[EDITOR-VERIFY]` **Name collisions.** Create three assets that all carry the SAME name in three different folders,
  and make at least one of them a different CLASS from the others (a `Rock` static mesh in two folders plus a `Rock`
  texture in a third). Scan. Confirm all three appear under **Name collisions** in ONE group whose header reads
  "3 assets share this name" with **no** reclaimable figure, that the header is not selectable, and that each row's
  detail names the other two by path. Then confirm the texture does NOT appear under Possible duplicates — that
  category still needs class and size to match, which is the whole reason this is a separate question. Confirm the
  action button reads **No action** and its tooltip says the fix is a rename, rather than "this category has nothing
  to act on".
- `[EDITOR-VERIFY]` **The collision consequence, end to end.** With two same-named assets in place, open the generated
  `*Assets.as` and confirm one accessor carries the plain name and the other `<Name>_DUP1`. Rename one asset, let
  AngelScript recompile, re-scan, and confirm the collision group is gone and both accessors carry plain names.
- `[EDITOR-VERIFY]` **Cleanup search, copy and navigation.** Type into the filter box: the list narrows, the sub-tab
  counts drop with it, and the header tiles and page-tab count do NOT. Confirm a filter typed on the **Level
  analysis** or **Memory** page leaves this list untouched, and vice versa. Right-click with several rows selected and
  confirm Copy Name / Copy Path / Copy Summary produce one line per row. Double-click a row and confirm the Content
  Browser jumps to that asset; confirm a dirty-package row says honestly that it cannot resolve rather than doing
  nothing.
- `[EDITOR-VERIFY]` **Deletion goes through the engine's dialog.** Select two unreferenced assets and press **Delete
  Selected (2)**. Confirm the editor's OWN delete dialog appears — the one listing references with a Force Delete
  option — and press **Cancel**: nothing must be deleted, the status strip must say so in Warn tone, and the list must
  be unchanged. Repeat and confirm: the assets are gone, the list re-scans itself, and the counts drop. Then select a
  duplicate row and confirm the same dialog appears for it.
- `[EDITOR-VERIFY]` **The other two actions.** With a redirector row selected, press **Fix Up Redirectors** and
  confirm the referencing packages are checked out and re-saved and the redirector disappears from a re-scan. On the
  **Dirty packages** tab, confirm the button reads **Save Dirty Packages** with no count even with rows selected,
  press it, and confirm the editor's own save checklist appears — declining it must report "nothing was saved".
- `[EDITOR-VERIFY]` **Cleanup button states.** Select nothing: the action button is disabled and its tooltip says to
  select a row (except on Dirty packages, where it stays live). Select rows on one tab, switch to another: the button
  re-labels to the new category's verb. Confirm the action button on **Possible duplicates** and **Unreferenced** is
  the same Delete action with the same wording.
- `[EDITOR-VERIFY]` **Cleanup across a PIE boundary.** Scan the project, enter PIE, and confirm the cleanup list is
  still there while the findings and memory tables clear — this is the one page whose answer a play session does not
  invalidate. Then re-scan and confirm the dirty-package rows are the ones that changed.
- `[EDITOR-VERIFY]` **The destructive two.** `Actor.EmptyStaticMesh` on an actor somebody has since assigned a mesh
  to must refuse and say "re-scan", not delete. `Actor.InstancingCandidate` must leave the level looking identical:
  compare the viewport before and after, confirm one HISM actor replaced the group, and confirm Ctrl+Z restores every
  original actor.
