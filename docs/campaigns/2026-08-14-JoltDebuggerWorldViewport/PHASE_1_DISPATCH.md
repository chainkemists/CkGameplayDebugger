# Phase 1 — dispatch plan (orchestrator working doc)

> Volatile; superseded line-by-line as units complete. Units are dispatched by the orchestrator
> (Fable) to Opus executors. Executors follow PHASE_1.md + this file; they STOP on any
> unenumerated observation, any design fork, or two failed attempts on one step.

## Unit I — wireframe-material investigation (READ-ONLY, Opus, may run during the baseline gate)

Scope: resolve P1-D13's branches with evidence. No file edits.
1. Branch (a) — engine-supplied: is `GEngine->WireframeMaterial` (and/or another
   wireframe-flagged engine material, e.g. under `/Engine/EngineDebugMaterials/`) loadable and
   usable in PACKAGED Development builds (not `WITH_EDITORONLY_DATA`)? Evidence: UEngine header
   member + config (`BaseEngine.ini` `*MaterialName` entries) + cooking implications. Narrow
   engine-source reads are AUTHORIZED for exactly this (UEngine header, material flag headers).
2. Branch (b) — generated asset: how does CkUsf generate its `M_CkUsf_Look_*` material assets
   (`Plugins/CkFoundation/Content/CkUsf/GeneratedLooks/`)? Is that pipeline reusable to emit one
   unlit, vertex-param-colored material with `Wireframe=true`? Where would the generator +
   committed asset live?
3. Report: per branch — feasibility, exact mechanism, packaged-build safety, cost. NO decision;
   the orchestrator rules and records it.

## Unit II — CkJolt facility implementation (Opus, AFTER the baseline gate completes)

Scope: PHASE_1.md work items 1–5 in `Plugins/CkFoundation/Source/CkJolt/` (+ minimal spec files
where the suite's C++ specs live — mimic existing Jolt test placement). OUT of scope: anything in
CkGameplayDebugger, any UI, any content asset creation unless the P1-D13 ruling (delivered with
the dispatch) says branch (b) and names the location.
Constraints: root doctrine (trailing returns, `CK_ENSURE_IF_NOT`, `_Member`+`CK_PROPERTY`, named
namespaces, no same-line if-bodies); CkJolt module rules (RunAfter WaitForAsync edge, never a
second PhysicsSystem, no entity resolution in Jolt callbacks); behavior of the existing in-world
debug draw preserved bit-for-bit (same CVars/gate/opacity).
Return format: files touched with roles; spec names + local compile evidence if runnable; the
verbatim output of anything unexpected; explicit confirmed/inferred split. No prose padding.

## Unit III — gate + audit (orchestrator)

Orchestrator re-runs the full toolbox gate (delta-zero vs baseline), then routes the diff through
a fresh adversarial-review agent (meta-adversarial-review five-point check) before accepting into
PROGRESS.md.

## Sequencing

Unit I now (read-only, parallel with the baseline gate) → gate completes → orchestrator rules
P1-D13 → Unit II → Unit III → docs weld + phase exit checklist.
