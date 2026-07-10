# ECS Debugger Redesign — Campaign Tracker

**Spec (contract of record):** [docs/specs/2026-07-10-ecs-debugger-redesign.md](docs/specs/2026-07-10-ecs-debugger-redesign.md)
**Visual contract:** [docs/specs/ecs-debugger-redesign-mockup.html](docs/specs/ecs-debugger-redesign-mockup.html) (open in browser; Design notes = 12 ideas)
**Mode:** Fable end-to-end, one-shot campaign, phased with hard gates. Per-phase plan is
written into this file at each phase start; the spec is not restated here.

## Decisions log (resolved — do not re-litigate)

| Decision | Outcome |
|---|---|
| Routing | Fable implements all phases; spec-first; no monolithic upfront plan |
| Archetype home | CkEcs (CkFoundation) — framework primitive; debugger/overlay consume |
| Typed `CK_DEFINE_ARCHETYPE` struct | In-campaign, last phase (B), C++ only |
| HAS-rollup boundary | Internals only; stops at primary-entity boundaries |
| AS authoring | Primary path for game archetypes; `Get_FeatureIds` is BPNE (TArray-default gotcha); asset-registry discovery |
| Perf | Feature-flag bit cache (registry-ctx table, EnTT signals, cvar-gated) + incremental model; steady-state = zero O(n); baseline trace in Phase 0 |
| Today-parity | Fold + group are toggles; classic tree reachable |

## Known unknowns (verify, don't assume)

- [ ] EnTT 3.16.0 (vendored, `CkThirdParty/entt-3.16.0`) per-storage `on_construct`/`on_destroy` signals — verify availability given the global `in_place_delete=true` trait (`CkHandle.h:71-77`) (Phase A). Fallback: hook feature Utils' Add paths.
- [ ] Uniform member-type→Cast trait for typesafe handles (Phase B). Fallback: add one trait template in CkEcs.
- [ ] Slate SVG rasterizer vs stroke-based SVGs — visual check in editor (Phase 0 gate). Fallback: stroke→outline conversion pass.
- [ ] Large-map perf baseline needs a real map / stress gym — coordinate with maintainer (Phase 0 gate).
- [ ] AS `asset X of <AS-defined subclass>` + `default` ctor ergonomics — Phase A test asset.

## Phase status

| Phase | Scope (see spec §6) | Status |
|---|---|---|
| 0 | Icons + style + inspector metadata + perf baseline | **IN PROGRESS** |
| A | Feature-flag cache + archetype registry (CkEcs) | pending |
| 1 | Classification/rollup/names/query logic + incremental model | pending |
| 2 | Tree rework (fold/coalesce/badges/identity) + perf gate | pending |
| 3 | Query tokens + rail + pinned/recent | pending |
| 4 | Overview archetype map + Archetypes lens | pending |
| 5 | Activity feed + churn flashes | pending |
| B | `CK_DEFINE_ARCHETYPE` typed struct + TryCast | pending |

Gate for every phase: toolbox build green (`CkAuto/UnrealToolbox.exe`, never raw Build.bat),
debugger fully usable, phase log updated here. Phases 1/A/B add automation specs.
Editor must be closed for builds (PreToolUse hook enforces).

## Phase 0 — plan + status (2026-07-10)

1. ✅ `Resources/Icons/*.svg` — 23 monochrome white glyphs (geometry = mockup `IC` table 1:1;
   stroke-based — see unknowns re: rasterizer).
2. ✅ `FCkDebuggerStyle` — content root → plugin `Resources/` (plugin NAME is `CkDebugger`);
   `CreateIconBrushes` registers `CkDebugger.Icon.<Id>` via `FSlateVectorImageBrush`;
   public `Get_IconBrush(FName)` resolver (unknown id → Slate no-brush).
3. ✅ Inspector metadata: `ICkDebuggerComponentInspector_Base::Get_IconName/Get_FeatureColor`
   (defaulted virtuals — zero behavior change); `FCkDebuggerInspectorMetadata` + registry
   `Get_AllMetadata` propagate both. **Deferred to Phase 2** (when badges render): per-inspector
   overrides across the 40 inspectors + InspectorFilter curated-color migration.
   **Decision:** icons live in `FCkDebuggerStyle` (this plugin's documented brush home), NOT
   CkFoundation's CkEditorStyle — revisit only if the overlay needs the same glyphs.
4. ⬜ Perf baseline: Insights trace with debugger open on the largest available map — needs a
   human/PIE session, coordinate with maintainer. Record numbers + trace path here.
5. Gate status: ✅ toolbox build green (`Result: Succeeded`, Development, 2026-07-10);
   ✅ editor-boot smoke via `--test --test-pattern Timer` → 36/36 passed (brush
   registration runs at module startup, so the boot exercises it). One build iteration:
   `IPluginManager::Get()` → LNK2019 (no Projects dep) → swapped to CkCore's
   `UCk_Utils_IO_UE::Get_PluginsDir(<folder name>)`, the CkEditorStyle pattern.
   ⬜ `[EDITOR-VERIFY]` icon render check below; ⬜ perf baseline (item 4).

`[EDITOR-VERIFY]` Phase 0 icon check (agents cannot PIE):
1. Build, open editor, console `ck.EcsDebugger` to open the tab (Tools menu → Debug section).
2. Temporary check until Phase 2 renders badges: in Widget Reflector or a scratch widget,
   confirm brush `CkDebugger.Icon.Timer` resolves and draws (not empty). Alternative: wait for
   Phase 2 where every tree row exercises the brushes; then this check is subsumed.
3. If glyphs render blank/garbled → stroke-support gap in the SVG rasterizer → run the
   stroke→outline conversion fallback (unknowns above).

## Visible slice (pulled forward from Phase 2) — 2026-07-10

Inspector section headers now render each inspector's feature glyph + color:
`SCkDebug_InspectorPanel` gained optional `IconBrush`/`IconColor` (nullptr = unchanged),
the ECS inspector panel passes them on inspector-titled headers, and **27 inspectors
declare `Get_IconName`/`Get_FeatureColor`** (colors = mockup FEAT table). Commit `1a5aa3e`,
gate: build green + Timer 36/36. **This IS the `[EDITOR-VERIFY]` for the SVG pipeline** —
user checks headers for crisp glyphs; blank/mangled → stroke→outline fallback per icon.
Remaining without glyphs (need new SVGs or a mapping decision, Phase 2): EntityInfo,
Relationships, DynamicFragments, EntityCollections, Tween, Physics, Shapes, OverlapBody,
Resolver, AStar, UI, MontagePlayer, AnimPlans.

## Next up — Phase A (CkFoundation/CkEcs; fresh session recommended)

1. Read CkEcs module Claude.md + `ck-methodology` skill (doc-set naming for CkFoundation work).
2. Verify EnTT 3.16 storage signals under global `in_place_delete=true` (vendored entt-3.16.0).
3. Feature-flag bit cache (registry-ctx table, signal-maintained, cvar-gated) + seed scan.
4. Archetype descriptor + registry + `UCk_ArchetypeDefinition` + AS test asset.
5. Automation specs for both. Commits stay in CkFoundation's dev.

## Session log

- **2026-07-10 (Fable):** Spec + mockup written and reviewed (user + colleague ideas
  incorporated: IS/HAS, archetypes, perf architecture). Campaign tracker created. Phase 0
  implemented: 23 SVGs, style content root + icon brushes + `Get_IconBrush`, inspector
  icon/color metadata plumbing (registry + base interface). Toolbox build launched.
  Docs commit to `dev` pending user approval.
