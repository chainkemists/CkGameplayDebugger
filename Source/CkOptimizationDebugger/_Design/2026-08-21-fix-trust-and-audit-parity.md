# CkOptimizationDebugger — fix trust, and audit parity

| | |
|---|---|
| **Written** | 2026-08-21 |
| **Trigger** | Three user-reported defects in the Fix action, plus a request for the trust affordances a competing tool ships |
| **Reference tool** | Plimsoll (fab.com listing `70d351e3-5125-46ed-ac5d-5dd00db06150`) — its feature list is the parity bar |
| **Status** | **All five phases implemented and gated, 2026-08-21.** `Ck.OptimizationDebugger` 60/60 at baseline → 76/76 after; every phase's gate ran with `--discover-fresh`. The `[EDITOR-VERIFY]` sweep in [CLAUDE.md](../CLAUDE.md) is outstanding and is the only thing between this and shipped |

The user report, verbatim:

> Note on the "Fix" option, on the debugger:
> * Will grab anything its Grouped to.
> * Tags stop working (ex: Expansion tags)
> * Will mess up Moving Materials
>
> They would also want undo support, assets marked dirty and listed instead of saving behind their back for them etc.

Two of those three are one defect wearing three hats, and the fourth sentence is mostly already true — which is
itself a finding, because a user who believes the tool saves behind their back has been given no evidence it does not.

---

## 1. The three defects, and what actually causes them

All three land on `Actor.InstancingCandidate` (`Fixes/CkOptimizationDebugger_Fixes.cpp:876`) and its convertibility
gate (`:839`). That gate has five clauses — plain `AStaticMeshActor`, mesh matches, static mobility, no attach
parent, no attached children, exactly one scene component — and every one of them is about the SHAPE of the
placement. Nothing in this module reads what the placement CARRIES: a search for
`Tags|GroupActor|DataLayer|HLOD|CustomPrimitiveData|Layers` over `Analysis/` and `Fixes/` returns nothing at all.

### 1.1 "Will grab anything its Grouped to" — two readings, both real

**(a) The editor Group is silently broken.** `AGroupActor` membership is never consulted. The engine's own delete
path removes an actor from its parent group BEFORE destroying it —
`Editor/UnrealEd/Private/EditorActor.cpp:1218-1221`, `AGroupActor::GetParentForActor(Actor)` then
`ActorParentGroup->Remove(*Actor)`. This module calls `UWorld::EditorDestroyActor` directly (`:1025`, and again at
`:792` for `Actor.EmptyStaticMesh`), which performs no such removal, so the surviving `AGroupActor` keeps a
reference to a destroyed member. The same omission is why the module already has to check `FLevelUtils::IsLevelLocked`
by hand: bypassing `edactDeleteSelected` means inheriting every guarantee it was making.

**(b) The fix converts a group the reader never chose.** `Apply_ConvertToInstances` re-derives its own conversion
groups from the live world (`:908-949`), sorts them by size, and converts the largest — every matching placement in
the level, not the one the reader ticked. The finding row says "up to N" and the apply message reports the true
count, which is honest, but honesty after the fact is not the same as consent before it. This is the same complaint
the preview work in §2 answers generally.

### 1.2 "Tags stop working (ex: Expansion tags)" — CONFIRMED, and it is data loss

`AActor::Tags` and `UActorComponent::ComponentTags` are never read. An HISM instance is a transform in a component,
not an actor: there is nowhere for a tag to go. So a placement carrying `Expansion_02` is deleted, its tag with it,
and every `GetAllActorsWithTag` / tag-gated query that named it silently returns one fewer actor. Nothing warns,
nothing fails, and the level still renders — the exact failure mode this module's own doctrine calls worse than a
crash.

The same argument extends past tags to everything else that makes a placement addressable or distinct: actor label,
folder path, editor Layers, World Partition Data Layers, HLOD layer assignment, and any inbound reference from a
Level Blueprint, another actor, or a soft path. The engine's delete path warns about the reference cases
(`EditorActor.cpp:985-1175`); this one does not.

### 1.3 "Will mess up Moving Materials" — the reporter was not sure which fix; three candidates, all real

Answered as *not sure*, so all three get hardened.

**(a) Per-actor material state is discarded by the conversion.** The group key is level + material-path signature
only (`:922-923`), and the conversion copies materials from the template actor alone (`:1003-1008`). Two placements
identical in mesh and material paths but differing in **custom primitive data**, a **dynamic material instance**,
custom-depth/stencil, lighting channels, cast-shadow flags, `LDMaxDrawDistance`, `bReceivesDecals`,
translucency sort priority, collision profile, `MinLOD` or `OverriddenLightMapRes` are merged into one component
carrying the FIRST actor's values. Custom primitive data is how a per-placement animated ("moving") material
parameter is normally driven, which makes this the most likely reading of the report.

**(b) Enabling Nanite changes what a World-Position-Offset material does.** `Mesh.NaniteCandidate` and
`Mesh.MissingLods` never ask whether any slot material animates vertices. Wind, panners and any WPO-driven
displacement behave differently under Nanite (evaluation gating and WPO disable distance), so a "moving" material is
exactly the case where flipping Nanite on is not cost-neutral.

**(c) `bUsedWithNanite` is written to a SHARED base material.** `Mesh.NaniteMaterialIncompatible` edits the base
material (`:566-571`) — correct, and deliberately so, but the blast radius is every asset in the project using that
parent, and the result message names only the mesh the reader clicked.

### 1.4 The shape of the fix

A convertibility gate is the wrong instrument, because it answers yes/no with no reason and the reader never sees
what it excluded. Replace it with an **audit**: per placement, a list of named reasons it cannot become an instance,
computed against the template rather than in isolation.

- Refusal reasons that are absolute: any `Tags` / `ComponentTags`, `AGroupActor` membership, a Data Layer or HLOD
  assignment, a `UMaterialInstanceDynamic` in a slot, any inbound reference the engine's own referencer walk finds.
- Refusal reasons that are comparative: any per-component property that differs from the template. The list is
  enumerated in code, not prose, and it is the ONLY thing the group key needs — an audit that compares every
  property makes the material-path signature redundant.
- Everything refused is REPORTED, not silently dropped: the preview (§2) lists the excluded placements with their
  reason, so "40 candidates, 12 refused because they carry tags" is what the reader sees before pressing anything.
- Deleting any actor goes through `AGroupActor::GetParentForActor` then `Remove`, mirroring the engine path.

And the Nanite fixes gain a material-side precondition, with the same treatment: a WPO material is named in the
preview and the fix refuses rather than warns, because "the wind stopped working" is not a cost regression the
reader would connect back to a checkbox they ticked a week earlier.

---

## 2. Parity: what the reference tool has that we do not

| Their claim | Ours today | Gap |
|---|---|---|
| Preview: exact before → after, per asset, per property, before anything is written | nothing — the button applies | **P15** |
| Tick only what you want; nothing unselected is touched | selection + a fix queue, but no per-property granularity and no preview | **P15** |
| One undo: whole batch is one transaction | true already (`Partition_ForBatch` + one `FScopedTransaction`) | verify + spec |
| Nothing saved behind your back; fixed assets marked dirty and LISTED | dirty ✓ (`MarkPackageDirty`, no `SavePackage` anywhere); listed ✗ | **P15** |
| Every applied fix logged in plain language, ready to paste into a commit | messages exist but die in the status strip | **P15** |
| Suppress per asset / folder / check, in project Config, travels with the repo, visible in a collapsed section | per-user `MutedStableKeys` only, finding-granular, never committed | **P16** |
| Registry-only first pass that opens no assets; deep pass in the background, progress, ~1s cancel | one modal `FScopedSlowTask` over OPEN LEVELS; nothing project-wide | **P17** |
| Scans the PROJECT | scans the open levels; an asset nobody placed is never checked | **P17** |
| Audio checks (no Sound Class, etc.) | no audio family at all | **P18** |
| Project summary ranking heaviest meshes | resident-memory tables + a disk census, no project-wide mesh ranking | **P18** |
| Reports export to HTML and Markdown | snapshot report only; findings have no export (the module's own known gap) | **P18** |
| Per-rule tests: fires dirty, silent clean, honours suppression; per-fix undo proof | model/projection specs only — decision #9 forbids a spec applying a fix | **P15 makes half of it testable** |

The last row is worth naming: the plan/apply split in P15 turns "what would this fix write" into a PURE function.
That is the first time this module can assert a fix's before→after without mutating a branch, and it does not
disturb decision #9 — the apply half stays `[EDITOR-VERIFY]`.

---

## 3. The phases, as built

| # | Phase | Scope | Blast radius |
|---|---|---|---|
| **P14** | Fix safety | The placement audit and its named reasons; group removal before every destroy; WPO precondition on the two Nanite fixes; blast-radius line on the shared-material fix. No UI beyond the reasons appearing in the existing detail panel | Medium — every actor-mutating fix |
| **P15** | Plan / apply split, preview, dirty list, fix log | Every `Apply_*` becomes `Plan_*` (pure, read-only, returns per-property before→after + refusals + side-effect notes) plus an `Apply_*` that re-plans and refuses on drift. A preview dialog over the fix queue with per-property ticks. A modified-package list after apply, with save going through the editor's own checkout-and-save prompt. A session fix log with "copy as commit message" | Large — the fix engine's shape changes; every fix gets a planner |
| **P16** | Suppressions | One suppression record (scope: Finding / Asset / Folder / Check + reason + author + date), one pure matcher, two stores: project `Config/DefaultCkOptimizationDebugger.ini` (committed, team-inherited) and the existing per-user store. `MutedStableKeys` is MIGRATED into the personal tier and retired as a separate concept — two mechanisms answering "don't show me this" is one too many. Suppressed findings render in a collapsed, counted section, never hidden | Medium — model + settings + one UI section |
| **P17** | Project-wide scan | A registry-only first pass over `/Game` that opens nothing, then a deep pass that is incremental, cancelable and NOT modal. Findings for assets no open level places. The two scans stay separate answers to separate questions (see the memory-analyzer precedent, decision #15) | Large — new scan engine, async model, page changes |
| **P18** | Audio family + findings export + heaviest-mesh ranking | A `Checks_Audio` family; a PURE HTML + Markdown findings report landing WITH its determinism spec, per the module's standing rule; a project-wide heaviest-mesh ranking on the dashboard (needs P17) | Medium |

Order matters: P14 is a correctness defect that can lose content, so it goes first and alone. P15 changes the fix
engine's shape, so every later fix inherits the planner rather than being retrofitted. P16 and P17 are independent
of each other. P18's ranking depends on P17.

---

## 4. Decisions this proposal asks for

1. **Suppressions live in committed project config** — asked and answered 2026-08-21: project config, per entry
   carrying scope, reason, author and date. This does NOT reopen decision #5: a threshold is one person's
   calibration of what they want flagged, a suppression is the team's ruling that a finding is intentional. The two
   were never the same kind of statement.
2. **`MutedStableKeys` is retired into the suppression record's personal tier.** Keeping both would leave two
   answers to one question, which this module has refused twice already (`RequiresRescanOfAssets`, and the
   `IsTransactional` bool).
3. **A refused placement is reported, never silently skipped.** Same rule as `SkippedUnloadedLevelNames`: saying
   nothing about what was excluded is indistinguishable from finding nothing to exclude.
4. **The Nanite fixes REFUSE on a WPO material rather than warning.** A warning inside a batch confirmation is a
   warning the reader dismisses; the material has to be named and the fix has to decline.
5. **The deep project pass is not modal.** `FScopedSlowTask` cannot deliver a one-second cancel over a whole
   project. The window still overrides no `Tick` — the sanctioned route is the repeating `RegisterActiveTimer`
   idiom already used for the deferred threshold rebuild, or an `FTSTicker`, and the choice is P17's to make and
   record.

---

## 5. Risks — and what actually happened

Recorded after the fact, because a risk list nobody revisits is a list nobody learns from.

- **P15 touching every fix** was the big one, and the mitigation held: making `Execute` a closure the PLANNER sets
  meant the writes moved rather than being duplicated, and the plan/apply split compiled on the first build.
- **The audit refusing conversions that used to succeed** is real and intended. Whether it OVER-refuses is the one
  thing a spec cannot answer — see `[EDITOR-VERIFY]` step 4, which exists specifically to catch a property that
  should have been on the exclusion list.
- **A second scan engine** landed smaller than feared, because the registry-only pass shares the level scan's
  finding builders and its exported predicates rather than re-deriving anything.
- **The one-Ctrl+Z claim is still unproven by machine** and stays `[EDITOR-VERIFY]` step 9.
- **Two defects the gate caught that review did not**: the report sorted severity backwards (the enum is declared
  worst-first, so "most severe first" is ascending), and `k_CategoryCount` still said 7 after the Audio category
  landed. Both were shipped-looking and both were wrong — which is the argument for the gate, not against it.

## 6. Original risks



- **P15 touches every fix.** Thirteen planners is thirteen chances to have the plan disagree with what the apply
  actually writes. Mitigation: the apply is implemented ONLY in terms of the plan it re-computes — a property the
  planner does not list is a property the apply cannot write, enforced by construction rather than by review.
- **The audit in P14 will refuse conversions that used to succeed.** That is the point, and it will read as a
  regression to anyone who liked the old behaviour. The preview naming the refusals is what keeps it from reading
  as the tool getting worse.
- **P17 is a second scan engine.** The precedent (memory analyzer, decision #15) says two scans answering different
  questions is correct, but it doubles the surface where "what does this number mean" has to be answered.
- **Undo across a batch is already claimed and never proven.** No spec applies a fix, so the one-Ctrl+Z promise
  rests on reading `Partition_ForBatch`. It stays `[EDITOR-VERIFY]`, and the verify step must be written down.
