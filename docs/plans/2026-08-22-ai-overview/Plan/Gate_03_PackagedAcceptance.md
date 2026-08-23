# Gate 3 — Packaged picker root cause and final acceptance

> **Status:** Instrumented; build-machine packaged reproduction pending
> **Depends on:** Gate 2

## Goal

After this gate, packaged Development picking exposes valid targets or an exact reason none are available, the reproduced missing-entity mechanism is removed, and the full campaign passes its final evidence audit.

## Work items

1. Add total gather statistics: target matches, transform-bearing representations, cull/filter exclusions, and final candidates; surface the last empty reason in the shared picker control.
2. Run the packaged Development discriminating experiment: matches=0 identifies filter/module data; matches>0 but representations=0 identifies hierarchy/visual mapping; candidates>0 but no pick identifies input/hit resolution.
3. Fix the proved mechanism and add its exact failing topology as a regression test.
4. Validate mesh-backed, ISM/ISKM, transform-less-feature, local-pawn-ignore, and streamer-mode cases.
5. Run local non-cook Development Game/Editor builds and focused/full suites; run packaged Development acceptance only on the build machine.
6. Run adversarial review, comment audit, commonality audit, and requirement-by-requirement completion audit.

## Exit criteria

- [ ] Root-cause sentence explains what, when, and where; packaged reproduction is red then green.
- [ ] Development packaged launcher, AI Overview, picker, world speed, and behavior override workflows are observed.
- [ ] Final full gate has no new failures beyond the named baseline.
- [ ] Fresh logs are clean of touched-file AS warnings, new ensures, module-load errors, and failed tests.
- [ ] Every explicit campaign success criterion has direct evidence.
- [ ] PLAN, status, PROGRESS, and permanent docs updated together.

## Current evidence

- Development Game target compiles and links the debugger suite, including `CkAiDebugger`.
- Launcher descriptor automation proves DeveloperTool inclusion in Win64 Game Development/DebugGame and exclusion from Test/Shipping.
- Shared picker now reports value-only candidate counts/reasons without rerunning feature predicates.
- A cooked/staged live reproduction on the build machine is still required to identify and repair the originally reported missing-entity mechanism; no mechanism change is authorized by compile evidence alone.
- Global environment boundary: local Codex workspaces must not cook, package, stage, or archive. Gate 3 is a build-machine handoff, not a local verification action.
