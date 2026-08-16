# CkOptimizationDebugger — capability review, market gaps, and proposed work

| | |
|---|---|
| **Date** | 2026-08-15 |
| **Reviewed at** | P7 complete (126/126), campaign closed per [PLAN.md](../PLAN.md) |
| **Scope** | Market position · filtering · fix coverage · AngelScript reference blindness · shared severity iconography |
| **Status** | §2 (gaps 1–3), §3, §4 and §5 are **implemented and gated** (2026-08-16, PLAN.md P8–P9), as is the name-collision check. §1 remains proposals. §7's questions are **resolved** — see the resolutions table there. |

**Reading rule.** Claims are marked **[confirmed]** when they name a file:line I read, or **[inferred]** when they are
reasoned from what I read. Market claims name their source URL. Nothing here was verified in a running editor.

---

## 0. Where the tool stands

Twenty-eight checks across seven families, ten auto-fixes, five pages (dashboard / findings / memory / profiling /
cleanup), a per-user threshold panel driven by reflection, and a documented no-live-handle invariant. Against the Fab
field it is already **broader than every listing surveyed** on analysis depth and **the only one** with a
memory-residency page and a profiling launcher in the same window. The gaps below are not "it is behind"; they are
"these are the things a mature tool has that this one has not been asked for yet".

---

## 1. Market position and the gaps that follow

Surveyed (2026-08-15):

| Product | What it is | What it has that we do not |
|---|---|---|
| [Fab Package Builder – Submission Toolkit](https://www.fab.com/listings/3e90f27b-6da9-4be8-b10c-b0a09e8ca02f) | Marketplace-submission auditor | Hard/soft/**direct and transitive** dependency audit · a **readiness score** · Critical / Warning / Info / **Passed** severity (note the fourth) · **generated reports** |
| [Project Optimise](https://www.cbgamedev.com/blog/project-optimise-tool-released-onto-fab-unreal-marketplace) | UE5 project auditor | 19 optimisation issues over 8 asset types · **10 bug checks** over 7 asset types · **naming-convention checks against customisable prefixes** |
| [Unreferenced Assets Manager](https://www.fab.com/listings/67e1df48-252c-4663-8780-78813982ed7d) | Focused unreferenced sweep | Nothing we lack — our Cleanup page supersedes it (four categories vs one) |
| [Optimization Tools](https://www.fab.com/listings/575c45d1-4990-4038-b21f-bfc8ed51bdbf) | Blueprint/C++ perf toolset | Depth on **Blueprint hygiene** — we ship two BP checks |
| UE built-in (Data Validation, Asset Audit, Size Map, Reference Viewer) | Engine | A **commandlet/CI path** (`DataValidation`) · **Passed** as a reportable outcome |

### Gap 1 — no report export *(highest value, already the tool's own known gap)*

[PLAN.md](../PLAN.md) records the export as unscheduled and P7 removed the three dependencies that were declared for
it. Every competitor surveyed ships one, and Package Builder makes it the headline feature. **[confirmed]**

Why it matters more here than there: this tool already computes a *delta* against the previous scan, and that delta
dies with the session — it cannot survive a machine, a branch, or a code review. The export is what turns "I scanned
it" into "here is the diff since the last milestone" for someone who was not sitting at the machine.

The determinism contract is already written (inherit `CkSaveDebugger`'s: explicit sorts with a final tie-break, fixed
camelCase field set, nothing time-/pointer-/environment-derived). The change re-adds `Json` / `DesktopPlatform` /
`ApplicationCore` **in the same commit that lands the export**, per the existing note.

### Gap 2 — no naming-convention / project-structure family

Project Optimise's third pillar, absent here. It fits this module's constraints better than anything else on the list:

- Asset-registry only — **loads nothing**, which is the disk-scan rule already in force.
- Prefix table belongs in `UCkOptimizationDebuggerSettings`, so the reflection-driven threshold panel grows the rows
  for free (a `TMap<FName, FString>` needs a different editor than `SCkDebug_NumericEditor`, so this is the one place
  the panel's `TFieldIterator<FIntProperty>` walk needs widening). **[inferred]**
- It is **trivially auto-fixable**: rename + `IAssetTools::FixupReferencers` — and `AssetTools` is *already* a declared
  dependency for exactly that symbol. **[confirmed]**

BusterBlock has a documented convention (`_BB` prefix, `_BP` / `_MAP` / `_WBP` / `_IA` / `_IMC` suffixes) that nothing
currently enforces.

### Gap 3 — no `Passed` outcome, no score, no trend

Two competitors report **Passed** as a first-class result. We report only what is wrong, which means a clean level and
a level nobody scanned look identical on the Findings page — the same defect the module already fixed for unloaded
sub-levels and for `RequiresEditor`. **[confirmed as a consistency argument]**

`_PreviousSummary` holds exactly one prior scan. A score plus an N-deep history is what a lead reads; a raw finding
count is what an engineer reads. Both are cheap once the export exists (the export IS the history).

### Gap 4 — no headless / CI entry point

Everything requires a human pressing Scan. UE's own `DataValidation` commandlet is the reference shape, and
CkFoundation already ships a commandlet precedent (`CkAngelscriptGenerator_DriftCommandlet`). A
`-run=CkOptimizationAudit` that emits the export and exits non-zero past a threshold is the whole feature. Depends on
Gap 1.

### Gap 5 — Blueprint checks are thin (2 of 28)

`Blueprint.TickEnabled` and `Blueprint.DependencyChain`. Candidates that are readable **off the CDO or the asset
registry with no graph compilation**, i.e. within the "never compile to describe" rule: BP inheritance depth,
construction-script cost proxies, unbounded timeline/timer counts, `bReplicates` on a BP with no replicated
properties. Needs its own scoping pass — listed for completeness, not proposed.

### Gap 6 — no link-out to the engine's own audit surfaces

We compute a `/Game` disk breakdown; UE ships **Size Map** and **Asset Audit** (Tools > Audit) which answer the
per-asset follow-up question. A context-menu "Open in Size Map" / "Open in Reference Viewer" on any asset-targeted
row is a small change with a large payoff, and it costs no new claim — those windows are the authority, and pointing
at them is honest.

---

## 2. Filtering — what exists, and the seven gaps

**Today** (`ck_optimization_debugger_model`): a text filter, a highlight query (dims, never hides), a 3-bit severity
mask, and a 7-bit category mask, plus per-level exclusion which is a *pre-scan* narrowing rather than a view filter.
`Matches_Filter` consults the first three and deliberately never the highlight. **[confirmed]**

That is a good foundation with a specific shape of hole: **every axis is a coarse enum, and none of them is the
axis a reader actually narrows by** — which is "where" and "what can I act on".

Ranked by value-per-line:

| # | Gap | Why it is the gap it is | Cost |
|---|---|---|---|
| 1 | **Fixable-only filter** | `Get_FixableFindings` already exists and already drives the batch button. There is no way to *see* only them. This one predicate turns the findings list from a report into a work queue — the single highest-leverage addition on this page. | One mask bit + one predicate clause |
| 2 | **Path / folder scope** | The most-narrowed axis on any asset audit ("just `/Game/Characters`"). Today the reader types it into the free-text box and hopes no *title* contains that word — a filter that silently means two things. | A real path field + `FString::StartsWith` on the target path |
| 3 | **Mute / triage list** | Nothing lets a reader say "I have judged this one, stop showing it". Without it, a project with 400 Minor findings is a list nobody re-scans, and the tool's own delta becomes unreadable. `StableKey` is already the right identity and `ExcludedLevelNames` is the persistence precedent (`UPROPERTY(config)`, no `EditAnywhere`, written **sorted**). Needs a "Show muted (N)" toggle so it can never hide silently. | A `TSet<FString>` + a toggle + one predicate clause |
| 4 | **Per-check (CheckId) solo** | Category is 7 buckets over 28 checks. Group headers already exist and are already inert; making one solo-able mirrors `Set_SeveritySolo` exactly — including its "assigns, does not toggle" rule. | Reuse the solo pattern |
| 5 | **Over-budget ratio filter** | `Get_GraduatedSeverity` already computes the 2× ratio and then **collapses it into the severity enum**. "Show me only findings ≥2× budget" is data we have and throw away. Requires carrying the ratio onto the finding row. | A float on the row + a threshold control |
| 6 | **Saved filter presets** | Named severity+category+path+fixable masks. Cheap, and it is what makes #1–#4 reachable a second time. | Settings array + a combo |
| 7 | **Cross-page pivot** | A memory row cannot ask "show me the findings about this asset", and a finding cannot ask "how much is it resident". The per-page filter *state* separation is correct and must stay; a one-shot **navigation** that seeds the other page's filter is a different thing and does not violate it. | A shared seeding entry point |

**One thing to preserve:** the filter/highlight split is right and unusual, and the per-page filter-string isolation
(`_MemoryFilterString` ≠ `_Filter.FilterString`) is right. None of the above should be implemented by merging them.

---

## 3. Fix coverage — 10 of 28, and which of the remaining 18 are honestly trivial

I walked all eighteen. **Five are one-property transactional edits of exactly the shape the existing texture and
Nanite fixes already have.** The other thirteen should stay unfixable, and the *reasons* should be written into
CLAUDE.md so this question is not re-asked.

### Recommend adding — 4 fixes, taking coverage to 14/28

| CheckId | Proposed fix | Execution | Notes |
|---|---|---|---|
| `Mesh.NaniteMaterialIncompatible` | `bUsedWithNanite = true` on each offending `UMaterial` + `PostEditChange()` | `Transactional` | Same shape as the two texture fixes. **It is the only way to resolve the finding** — the alternative is turning Nanite off, which is a different finding. It queues a shader compile, so the result message must say so. |
| `Texture.MissingMipmaps` | `MipGenSettings = TMGS_FromTextureGroup` + `PostEditChange()` | `Transactional` | The check already excludes `TEXTUREGROUP_UI`, which is the one group where this would be wrong. **[confirmed]** |
| ~~`Texture.MaxSize`~~ | ~~`UTexture::MaxTextureSize = <threshold>`~~ | — | **DROPPED, 2026-08-16.** Mechanically fine, but the value it writes is a *per-user* calibration (`config=GameUserSettings`) and the asset it writes into is *shared*. That is strictly worse than the committed-config case this module already refuses — one QA person's 2048 would be baked into an artist's asset. Moving the threshold to project config to justify it would put the same number in two places. Four new fixes, not five (coverage 10 → 14). |
| `Lighting.LightmapResolution` | Clamp `OverriddenLightMapRes` to budget on the over-budget components, **or** clear `bOverrideLightMapRes` entirely | `Transactional` | An actor-component property edit — same target class as `Actor.EmptyStaticMesh` minus the destruction, so it inherits that fix's `FLevelUtils::IsLevelLocked` guard. The finding is already aggregated per actor, so the fix must walk **every** over-budget component on it. |
| `Blueprint.TickEnabled` | `bStartWithTickEnabled = false` on the BP CDO + dirty the package | `Transactional`, **and a NEW `ChangesBehavior` flag** | The most common UE optimization edit in existence — and it **can change behaviour** (a BP that relies on tick from frame 0). It must prompt, but **not** by widening `IsDestructive`: that flag means "removes or replaces actors", and a field meaning two things is the split-brain defect tenet 6 names. Add a second flag; both feed `Build_BatchConfirmation`, each with its own sentence, so the dialog can say which risk it is asking about. |

Every one of these must obey the existing rule: **re-validate the check's WHOLE condition, not just the property it
flips**, and export the check's own predicate rather than copying it. **[confirmed as existing doctrine]**

### Recommend NOT adding — and the reasons, for the record

- `Mesh.TriangleBudget`, `Mesh.CollisionPrimitiveCount`, `Material.SlotCount`, `Material.DuplicateSlots`,
  `Blueprint.DependencyChain` — all require re-authoring the asset. There is no property to flip.
- `Texture.NonPowerOfTwo` — resampling source art is not an audit tool's job.
- `Material.EmptySlot` — the fix is "assign the right material", and no offline rule knows which. Assigning
  `WorldGridMaterial` would look like a fix and be a content regression.
- `Material.TranslucentTwoSided` — the setting IS the visual intent.
- `Material.SamplerBudget` — same as the slot-count family.
- **`ProjectSettings.RayTracing` / `PathTracing` / `ForwardShading` / `ExpensiveLightingFeatures` — mechanically
  trivial** (they are `URendererSettings` bools with exactly the `TryUpdateDefaultConfigFile` shape
  `TextureStreamingDisabled`'s fix already has) **and should still not get one.** Turning Lumen off is not an
  optimization, it is a change of art direction. The finding's job is to say the cost exists; the navigation to
  Project Settings → Rendering is the right terminal action and already works. This is the one row where "we could"
  and "we should" genuinely diverge, which is why it is written down.

### One fix-engine gap, independent of the catalog

**A group header cannot "Fix all of this check".** The batch machinery (`Partition_ForBatch`, one outer transaction,
the config-write separation, `Build_BatchConfirmation`) is all built and the group header is a rendered, inert row.
The reader's actual verb is "enable Nanite on all 40 of these", and today that means rubber-banding 40 rows. The
header is not an `SListView`-row click-trap risk in the way a switch would be, but it *is* inside the list — so the
action belongs in the header's **context menu**, not on a button in it, matching the row-safety rule.

---

## 4. AngelScript asset references — a correctness defect, not a feature gap

**This is the sharpest finding in the review, and it can delete a shipping asset.**

### The defect

`Run_CleanupScan` decides "unreferenced" purely from the on-disk package reference graph:

```
Registry->GetReferencers(Entry.Key, Referencers,
    UE::AssetRegistry::EDependencyCategory::Package,
    UE::AssetRegistry::EDependencyQuery::NoRequirements);
```

— `CkOptimizationDebugger_CleanupScan.cpp:221-223` **[confirmed]**

An asset reached from AngelScript goes through a **generated accessor** (`assets::Get_Foo()` in a
`<X>Assets.as` file emitted by `UCkAssetRegistrySubsystem`). That is a text call resolved at runtime — it creates
**no package dependency edge whatsoever**.

Therefore: **every AS-only-referenced asset in the project lands in Cleanup → Unreferenced and is offered to
`ObjectTools::DeleteAssets`.** And the safety net agrees with the wrong answer — the engine's own delete dialog
derives its referencer list from the same graph, so it will also show zero referencers and offer a clean delete.

In a project whose gameplay layer is an AngelScript rewrite, that is not an edge case; it is most of the content.

### CkFoundation already solved this exact problem — for a different delete path

`UCkAssetRegistrySubsystem` maintains two maps and keeps them fresh: **[confirmed]**

| Map | Contents | Refreshed by |
|---|---|---|
| `AssetPathToFunctionName` | asset object path → generated accessor name | registry regen / `SeedMapsFromGeneratedFiles` |
| `FunctionUsageMap` | accessor name → the `.as` files that call it | `ScanScriptFilesForUsage`, on `FAngelscriptCodeModule::GetPostCompile()` (`CkAssetRegistrySubsystem.cpp:1141-1146`) and after every registry generation (`:608`, `:630`) |

…and hooks `FEditorDelegates::OnAssetsPreDelete` (`:273-274`) to raise a modal
**"AngelScript Asset Reference Warning"** naming the accessor and every `.as` file that calls it
(`HandleAssetsPreDelete`, `:1359-1407`).

So the data exists, is already maintained, is already correct, and the warning dialog the user described is already
built. It is simply not consulted by this scan.

### What the existing dialog does and does not buy us

`ObjectTools::DeleteAssets` does broadcast `OnAssetsPreDelete`, so the warning **should** fire from the Cleanup
page's delete path too — meaning today's worst case is probably "warned at the last second" rather than "silently
deleted". **[inferred — not verified in an editor; this is the single claim in this document I would most expect to
be wrong, and it is worth an `[EDITOR-VERIFY]` before anything is built.]**

Either way it is the wrong layer. The reader has by then selected the row, read a tab that told them the asset was
unreferenced, pressed a button, and reached a confirmation — three surfaces asserting a falsehood, corrected by a
fourth. And `Get_ApplicableRows` will still have counted it, and Reclaimable will still have included its bytes.

### Proposal — three layers, in order

1. **Consult the map at scan time (the correctness fix).** Export a query —
   `TryGet_ScriptReferencers(const FSoftObjectPath&) -> TArray<FString>` — from `CkAngelscriptGenerator`, and add
   its result to `ExternalReferencerCount` in the unreferenced walk. An asset with script referencers is not
   unreferenced, and Reclaimable stops counting its bytes.
2. **Show it, never hide it.** A row silently vanishing would make this tab disagree with the Reference Viewer with
   no explanation the reader could see — the same reasoning that keeps excluded levels greyed rather than removed.
   Two surfaces: an **"AngelScript-referenced" count tile**, and on any row that has script referencers, a detail
   line "Referenced from N AngelScript file(s)" with the file list in the copy summary. The reader learns the rule
   from the tool instead of from a deletion.
3. **Keep the dialog as the backstop** and add the `[EDITOR-VERIFY]` step that proves it fires from this path.

### The dependency fork this opens — needs a decision

`CkOptimizationDebugger` is a **DeveloperTool** module that ships in packaged Development/DebugGame builds.
`CkAngelscriptGenerator` is an **Editor** module. A hard dependency is therefore illegal — the tool would fail to
package. **[confirmed from the module's own CLAUDE.md and the uplugin type policy]**

| Option | Shape | Trade |
|---|---|---|
| **(a) Soft subsystem lookup** *(recommended)* | `GEditor->GetEditorSubsystem<UCkAssetRegistrySubsystem>()` behind `#if WITH_EDITOR`, with the module declared as a dynamically-loaded dependency | Matches how every other editor-only path in this module is already gated. The whole cleanup scan is already `RequiresEditor` outside the editor, so there is no non-editor case to serve. |
| (b) Duplicate the text scan here | Re-run `ScanScriptFilesForUsage`'s logic in the cleanup scan | **Rejected.** Directly violates the module's own rule — *"a check's predicate is EXPORTED rather than copied; a second copy of the rule is a second place for it to drift"*. |
| (c) Move the usage map down a tier | Relocate the maps into a Runtime/UncookedOnly module | Cleanest long-term, largest blast radius, and it touches the AS self-heal machinery. Not worth it for one consumer. |

### One line worth adding while we are here

The same blindness applies to **any** reference the package graph cannot see: soft string paths built at runtime in
Blueprints or DataTables, and `asset ... of ...` AngelScript asset definitions. The always-rooted class list is the
existing acknowledgement that this class of hole exists; the AS case is the one where we hold the data to close it.

---

## 5. Severity iconography — replace the bespoke glyphs with UE's language, in the shared layer

### What is there now

`Skull` = Critical, `Flame` = Major, `Note` = Minor, resolved through `FCkDebuggerStyle::Get_IconBrush` out of
`Resources/Icons/General/` — a **~140-glyph decorative set** (Apple, Barrel, Pizza, Popcorn, Umbrella…).
**[confirmed]**

Four problems:

1. **It is not UE's visual language.** UE uses an amber warning triangle, a red error circle and a blue info circle
   *everywhere* — Message Log, Output Log, Data Validation, compiler results, asset tooltips. A reader with a decade
   in the editor decodes those with no legend. Skull / Flame / Note has to be learned, once per tool.
2. **`Skull` is overloaded across the suite.** It appears in five places meaning at least three different things:
   `CkOptimizationDebugger` (Critical severity), `CkGoapDebugger`, `CkEqsDebugger`, `CkCrowdDebugger`, and the
   Gallery (where it labels **"Failed"**). One picture, several meanings, in sibling tools sharing one launcher.
   **[confirmed by grep across `Source/`]**
3. **There is no warning / error / info glyph in the icon set at all.** Nothing under `Resources/Icons/**` matches
   warn, error, info, alert or caution. So this is not "pick the right existing icon" — the glyphs do not exist.
   **[confirmed]**
4. **The colour axis is already shared and already correct; only the glyph axis is ad-hoc.** `ECk_Tone`
   (`Neutral / Ok / Info / Warn / Err`) lives with the style tokens and drives `SCkDebug_StatusPill`,
   `ck::debug_axes::Get_HeatColor` and the rest. `Get_SeverityTone` (Critical→`Err`, Major→`Warn`, Minor→`Info`) is
   already a *model* rule in this module. **[confirmed]** So the plumbing is built; the glyph just never got hung
   off it.

### Proposal

1. **Author four semantic SVGs** — `Severity_Error`, `Severity_Warning`, `Severity_Info`, `Severity_Success` —
   shaped like UE's (filled triangle-with-`!`, circle-with-`x`, circle-with-`i`, circle-with-check).
   They go in `Resources/Icons/` **root**, not `General/`: root is the semantic/tool-identity tier, `General/` is the
   decorative tier that also feeds a deterministic picker, and a severity glyph must never be handed out as a random
   decoration.
2. **Add `ck::debug_axes::Get_ToneIconId(ECk_Tone) -> FName`** in
   `CkDebuggerCommon/Public/CkDebuggerCommon/Styles/CkDebuggerAxes.h`, beside `Get_HeatColor` and
   `Get_CategoricalColor`. That file is already the home of "the one rule mapping a semantic to a look", and putting
   the glyph there means **colour and icon come off the same axis and cannot drift**.
3. **This module then changes almost nothing**: `Get_SeverityTone` already produces the tone; the chrome's icon row
   asks the axis for the glyph instead of naming `Skull` / `Flame` / `Note`.
4. **Sweep the other four `Skull` sites** onto whatever they each actually mean (the Gallery's "Failed" is
   `Severity_Error`; the Goap / Eqs / Crowd ones need a per-tool read), and **add a severity row to the Gallery** so
   the three glyphs are discoverable in the one place the suite documents its own vocabulary.

### The alternative, and why it loses

`FAppStyle::Get().GetBrush("Icons.Warning")` is free, is literally the engine's own art, and is used **nowhere** in
this plugin today. **[confirmed]** It loses on two counts: it is an editor style set, and these modules ship in
packaged Development/DebugGame builds where it may not be initialised; and an `FAppStyle` brush sits **outside Style
Lab**, so it is the one icon in the window that a style revision cannot restyle — in a suite whose entire look is
revision-driven. Authoring the SVGs costs one afternoon and keeps every brush inside one system.

---

## 6. Proposed sequencing

Nothing here is started. If all of it is wanted, this is the order that minimises rework:

| # | Item | Why here | Blast radius |
|---|---|---|---|
| 1 | **AS reference blindness** (§4) | It is a correctness defect that can delete content. Everything else is an improvement. | Low — one predicate, one dependency decision, two UI surfaces |
| 2 | **Shared severity iconography** (§5) | Lands in `CkDebuggerCommon`, so every later change inherits it. Doing it after the new filters means restyling them twice. | Suite-wide but shallow |
| 3 | **Fixable-only + path scope + mute filters** (§2, gaps 1–3) | Turns the findings list into a work queue, which is what makes the five new fixes worth having | Low — model-only, spec-testable |
| 4 | **Five new fixes** (§3) | Each is independent; ship them one at a time with their own `[EDITOR-VERIFY]` | Medium — every one mutates an asset |
| 5 | **Report export** (§1 gap 1) | Re-adds three dependencies; needs its own determinism spec | Medium |
| 6 | **Naming-convention family** (§1 gap 2) | New check family + a settings editor the reflection panel does not yet handle | Medium |
| 7 | **CI commandlet** (§1 gap 4) | Depends on 5 | Low once 5 exists |

---

## 7. Open questions — RESOLVED 2026-08-16

| # | Question | Resolution | Reasoning |
|---|---|---|---|
| 1 | The dependency fork in §4 | **Neither (a) nor (c) as written — a provider registry in `CkCore`** | (a) still puts a codegen module in a debugger's link line and teaches this tool that AngelScript exists; (c) is a large blast radius for one consumer. `FCk_AssetReferenceProviderRegistry` (`CkCore/Reference/`) inverts it: the module that CREATES invisible references declares them, the auditor asks. Neither links the other, and the seam also covers the config/soft-path cases named at the end of §4. **Implemented.** |
| 2 | `Blueprint.TickEnabled` as `IsDestructive` | **Fourth flag: `ChangesBehavior`** | Widening `IsDestructive` to cover "changes behaviour" makes one field mean two things, so the confirmation dialog could no longer say which risk it is asking about. Two flags, both feeding `Build_BatchConfirmation`. *Not yet implemented — lands with the four fixes.* |
| 3 | `Texture.MaxSize`'s fix is opinionated | **Drop the fix** | See the struck row in §3. A per-user calibration written into a shared asset is worse than the committed-config case the module already refuses, and the obvious escape (move the threshold to project config) duplicates the number. |
| 4 | `Passed` as a fourth severity | **No** | Declaration order IS sort order, the mask is three bits, and `Get_SeverityTone` guarantees nothing in the findings list is ever `Ok`-toned. "Scanned and clean" is a status-strip statement, not a findings row. Revisit only alongside the report export (§1 gap 1), where a machine-readable `passed` set genuinely earns its place. |
| 5 | Does the AS pre-delete dialog fire from this page? | **Still `[EDITOR-VERIFY]`, and now moot for correctness** | §4's fix means the asset never reaches the delete path in the first place. The dialog remains the backstop; the verify step is in CLAUDE.md. |

---

## 8. What actually landed, 2026-08-16

Implemented, built, and gated (`Ck.OptimizationDebugger` + `Ck.DebuggerLauncher`):

- **§4, the correctness defect.** `FCk_AssetReferenceProviderRegistry` in `CkCore/Reference/`;
  `UCkAssetRegistrySubsystem::Get_ScriptReferencersOfAsset` + registration under `"AngelScript"`; the unreferenced
  walk consults it, counts what it suppresses, and distinguishes all three provider states on the status strip with
  the no-provider case Warn-toned.
- **A fifth cleanup category, `NameCollisions`** (asked for mid-review): assets sharing an exact name across folders,
  whatever their class or size. Load-bearing here because the AngelScript accessor generator is keyed on the asset
  NAME and renames the loser `<Name>_DUP1` in registry-iteration order. No action, because the fix is a rename.
- Supporting: `DuplicateGroupKey` → `GroupKey`, `..._CleanupDuplicateGroup` → `..._CleanupGroup`,
  `Get_CleanupGroups(rows, category, filter)`, `Get_IsGroupedCleanupCategory`, and the catalog rule relaxed from
  "exactly one action per category" to "at most one" with the null asserted by name.

Landed the same day as **P9**:

- **§5, shared severity iconography.** Four authored SVGs at the icon-set root, `ck::debug_axes::Get_ToneIconId`
  beside the colour ramps, and the five `Skull` sites that all meant *failure* swept onto one glyph.
- **§2 gaps 1–3, the three narrowing filters.** Path scope, suggested-fix, and a `StableKey`-keyed mute set with a
  visible count and a `MUTED` chip.
- **§3, four of the five proposed fixes** (coverage 10 → 14 of 28), plus the `ChangesBehavior` flag from resolution
  #2. `Texture.MaxSize` was dropped per resolution #3.

Still proposals: the report export (§1 gap 1), the naming-convention family (§1 gap 2), the score/trend and `Passed`
outcome (§1 gap 3, gated on the export), the CI commandlet (§1 gap 4), deeper Blueprint checks (§1 gap 5), and the
link-outs to Size Map / Reference Viewer (§1 gap 6). §2 gaps 4–7 (per-check solo, over-budget ratio, presets,
cross-page pivot) are also untouched.
