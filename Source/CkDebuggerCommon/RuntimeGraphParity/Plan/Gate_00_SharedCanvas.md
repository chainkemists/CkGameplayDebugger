# Gate 0 — contract and shared canvas

> **Status:** Implemented, pending build-machine verification
> **Depends on:** packaged debugger feature branch
> **Estimate:** re-date after the Scheduler vertical slice exposes integration cost

## Goal

After this gate, CkDebuggerCommon supplies a runtime-safe graph surface that can host the existing
debugger card visuals, draw semantic directed edges, and provide the viewport and selection behavior
needed to replace SGraphEditor in both Editor and packaged builds.

## Entry criteria

- [x] Current graph and fallback source inspected on `8083322a`.
- [x] Existing shared layout and style primitives inspected.
- [x] Editor-only engine dependency boundary confirmed.
- [x] Per-debugger parity checklists recorded in the mission and progress documents.

## Work items

1. Add a value-only graph scene contract with stable node IDs, positions, layers, arbitrary child widgets, semantic edges, anchors, arrow and dash styling, selection, and copy payload hooks.
2. Add a runtime Slate graph panel that arranges child cards, clips and culls, draws background/grid/wires/arrows, and performs hit testing without GraphEditor.
3. Implement background pan, cursor-centered wheel zoom, frame-all, view reset, click/empty-space selection, and viewport-state preservation across scene updates.
4. Add pure geometry and transform tests, plus stable-identity scene update tests.
5. Exercise the surface through the Scheduler adapter before declaring the API sufficient.

## Expected observations

| Run | Expected | If instead | Response |
|---|---|---|---|
| Runtime canvas automation | Deterministic transforms, fit bounds, edge endpoints, and selection transitions | Floating-point or geometry drift | Move calculations into pure helpers and compare with explicit tolerances. |
| Scheduler vertical slice | Existing cards and layout fit without feature-specific branches in Common | Common requires Scheduler types or custom paint callbacks | Narrow the contract to semantic scene data and keep feature adapters outside Common. |
| Packaged Development compile | No GraphEditor/UnrealEd dependency enters CkDebuggerCommon | UBT resolves an editor module | Audit Build.cs and includes before proceeding to feature adapters. |

## Exit criteria

- [ ] Shared canvas compiles in Editor and packaged Development targets.
- [ ] Focused automation tests pass on the final Gate 0 artifact.
- [ ] Scheduler graph renders through the shared surface in Editor and packaged builds with matching behavior.
- [x] Comment audit complete.
- [x] `PLAN.md`, this status header, `PROGRESS.md`, and `CkDebuggerCommon/CLAUDE.md` updated in the landing commit.
