# Continuation prompt — debugger pane layout parity

## 1. One-line summary

Bring every modern Ck debugger except AI Overview to the approved AI Overview pane grammar: Cards must have the same clean rounded surfaces/corners, Workbench must have the same clean contiguous edge-to-edge surfaces, panes must remain resizable, and the retired Outlined option must stay absent.

## 2. Repo state

- Work only in `E:\Repos\BusterBlock_Other`; do not touch `D:\Repos\BusterBlock`, create a worktree/clone/copy, or move the campaign to another checkout.
- BusterBlock root: branch `feature/handheld-performance`, HEAD `9b4094e20c394999ee27fcca7c065c004ebbac11`.
- CkGameplayDebugger submodule: `E:\Repos\BusterBlock_Other\Plugins\CkGameplayDebugger`, branch `feature/ai-overview`, HEAD `e8d7bf38412668b5eab3343c87e7f5b50a131c0d`.
- Nothing from this campaign is committed or pushed. The plugin has a large intentional in-flight AI Overview, Common-widget, Style Lab, and suite-migration diff. Preserve it.
- Root dirt to preserve includes:
  - `Config/DefaultGameplayTags.ini`
  - `Plugins/BusterBlockTests/Script/Generated/BusterBlockTests_AutoTestActors.as`
  - dirty `Plugins/CkGameplayDebugger`
  - `Script/Npc/AI/BB_NpcAI_Processor_StuckMovement.as`
  - `Source/BusterBlock/BusterBlock.Build.cs`
  - `Source/BusterBlock/BusterBlock.cpp`
  - `Content/__ExternalActors__/BusterBlock/Map/AutoTests/AutoTests_BB_MAP/1/RL/`
  - `Plugins/BusterBlockTests/Script/Tests/NpcAI/BB_AutoTest_NpcAI_StuckRecoverySuppression.as`
  - `Source/BusterBlock/AI/Debug/`
  - `Source/BusterBlock/Tests/Debugging/`
- Do not restore, stash, clean, broadly stage, or rewrite unrelated dirt. This handoff is currently an untracked plugin document and must be preserved.
- Engine truth has two distinct sources:
  - `E:\Repos\BusterBlock_Other\Intermediate\ProjectFiles\BusterBlock.vcxproj` is correctly generated against `D:\Repos\UnrealEngineAngelscript_Other` (118 `_Other` references, zero primary-engine references when checked on 2026-08-23).
  - `BusterBlock.uproject` contains `{E4464C1C-43B7-7194-E787-E7B5711509D4}`, currently registered in HKCU as `D:\Repos\UnrealEngineAngelscript`; therefore `CkAuto\Get-ProjectEnginePath.ps1` misleadingly returns the primary engine. `_Other` is registered separately as `{F5534390-E8D1-4B55-83EF-4DA731819D09}`.
  - Do not edit the `.uproject`, registry, generated projects, or engine association. Build only via UnrealToolbox and stop immediately if its output names the non-`_Other` engine.
- Never cook, package, stage, or archive locally. Those operations are build-machine-only.

## 3. Active bugs or open questions

The CTO's wording is authoritative:

> All debuggers, *except* for the AI Overview debugger, still have the same problem: the Cards or Workbench layout is not the same as the approved AI Overview layout.

The supplied visual comparison is:

`C:\Users\sulfu\AppData\Local\Temp\codex-clipboard-0e47eb39-c03b-46e3-97b4-ef847bfdbdd7.png`

Acceptance contract:

- AI Overview is the golden painted reference and must not regress.
- Cards mode: clean rounded Common-card perimeter, consistent card spacing/surface ladder, no square child fill overwriting corners, no doubled or broken seams.
- Workbench mode: clean square contiguous panes, no decorative gap, one coherent shared boundary at each splitter, still independently resizable.
- Style Lab owns the global Cards/Workbench choice. Do not add another debugger-local style toggle.
- Outlined is retired. It must not reappear in Style Lab or any profile. Its hidden legacy wire value may remain solely for old config loading and must resolve/migrate to Flat.
- Preserve each debugger's information architecture, splitter orientation/ratios/minimum sizes, selection, refresh, teardown, and bespoke graph/canvas/viewport semantics unless a concrete acceptance failure requires a deliberate change.
- Commonize the pane-host solution. Do not patch each corner with arbitrary colors, padding, or feature-local brushes.

Open question to resolve from source and live paint before bulk edits: which immediate child roots are pure redundant pane chrome and can become transparent, and which fills are domain-semantic (graph canvas, 3D viewport, map, timeline visualization) and must remain opaque inside an intentionally inset/frame-owned region?

## 4. Why prior fixes or investigations were insufficient

- Gate 4 wrapped 17 legacy debugger panes in `SCkDebug_Card` with `BodyPadding(0)`. That made the global extent live but preserved each pane's existing opaque square root `SBorder`/background.
- `SCkDebug_Card` paints a rounded outer border and rounded body, but Slate does not clip arbitrary descendant paint to that rounded brush. The square child root can therefore cover the rounded body corners or interrupt the ring.
- The wrapper migration also retained legacy outer separators/borders. Combined with the card ring and splitter handle, these produce doubled, stubbed, or broken seams.
- Tuning `GlowExtent`, splitter handles, palette colors, or another layer of padding cannot establish one owner for the pane perimeter. It either wastes space, hides the defect at one size, or breaks Workbench.
- Stock Slate 5.7 exposes rectangular/quad `FSlateClippingZone`, not a rounded descendant clip. A global clipping change cannot solve curved corners and would add stencil/scissor cost without expressing the desired mask.
- Automated construction tests proved widgets existed and style axes were live, but did not validate final antialiased painting. The screenshot is new evidence that the prior automated exit criteria were insufficient.

## 5. Available diagnostics and the first evidence to collect

Available evidence:

- Campaign state: `docs/plans/2026-08-22-ai-overview/PROGRESS.md`.
- Gate decisions and migrated-debugger classification: `docs/plans/2026-08-22-ai-overview/Plan/Gate_04_SuiteStyleMigration.md`.
- Pre-corner-cleanup baseline: `docs/plans/2026-08-22-ai-overview/BASELINE_20260823-035226_CORNER_CLEANUP.md`.
- Last full gate before Outlined retirement: `E:\Repos\BusterBlock_Other\Saved\Logs\AiOverview-SuiteStyleMigration-Final-Debugger.log`: 256 total, 255 passed, one inherited opt-in failure, zero skipped/contaminated.
- Inherited failure: `Bb.Snapshot.SmallLoopGoapDebuggerLoadAcceptance`, requiring `BB_RUN_SMALLLOOP_OLD_SAVE_ACCEPTANCE=1`; do not misclassify or silently fix it.
- The current source has additional unbuilt Outlined-retirement edits. Establish a fresh current-state build/test checkpoint before claiming a regression baseline.

First evidence to collect:

1. Re-open AI Overview beside ECS, GOAP, SM, Crowd, and one viewport/canvas debugger.
2. Capture the same panes in Cards and Workbench at default width and a narrow width.
3. For each visible broken edge, trace the immediate `SCkDebug_Card` child and record every root `SBorder`, background brush/tint, explicit padding, separator, and splitter handle touching that perimeter.
4. Produce a small matrix: debugger/pane, Cards symptom, Workbench symptom, redundant root chrome, semantic fill, proposed owner. Do not edit broadly until this separates chrome from content.

## 6. Symptom-to-cause map

| Likely symptom | Most likely cause | Primary files to inspect |
|---|---|---|
| Rounded card corner is squared off or clipped | Opaque square legacy child paints edge-to-edge inside zero-padding rounded `SCkDebug_Card`; rounded brush is not a child mask | `SCkDebug_Card.cpp`; the debugger window wrapper; child pane root |
| Vertical/horizontal seam is doubled, stubbed, or stops before the corner | Splitter handle plus Common card ring plus retained legacy border/separator all own the same boundary | Debugger window `SSplitter`; wrapped child root; Common card |
| Cards looks almost identical to the old debugger | Child root background covers the Common depth-2 surface, leaving only fragments of outer card chrome visible | Wrapped pane builders and panel root widgets |
| Workbench retains decorative gutters | Explicit slot padding or forced card/glow extent survives instead of resolving through `Get_CardOuterExtent()` | Window splitter composition; `SCkDebug_Card`; `SCkDebug_GlowWrap` |
| Workbench has thick separators | Legacy pane border remains in addition to splitter boundary | ECS center border and analogous legacy root borders |
| Viewport/canvas turns transparent or visually wrong after cleanup | A semantic renderer fill was mistaken for redundant pane chrome | Crowd/Jolt viewport, AStar/Map canvas, graph/timeline bodies |
| Pane loses selection, scrolling, refresh, or teardown | Cleanup replaced the content widget instead of only transferring outer chrome ownership | Feature window/panel construction and existing tests |
| Outlined reappears in packaged Style Lab | A reflected hidden enum was exposed without consulting packaged-safe authored option metadata | `SCkStyleLab_ControlsPane.cpp`; `CkStyleLab_AxisMetadata.cpp` |

## 7. Critical files

1. `Source/CkDebuggerCommon/Public/CkDebuggerCommon/Widgets/SCkDebug_Card.cpp` — current rounded/card surface owner; body is not a descendant clip.
2. `Source/CkDebuggerCommon/Public/CkDebuggerCommon/Widgets/SCkDebug_Card.h` — reusable card arguments and the likely home for any explicit pane-host contract.
3. `Source/CkDebuggerCommon/Public/CkDebuggerCommon/Styles/CkDebuggerAxes.cpp` — Cards/Workbench brush, tint, outer extent, profiles, and hidden legacy Outlined fallback.
4. `Source/CkAiDebugger/Public/CkAiDebugger/Window/SCkAiDebuggerWindow.cpp` — golden native Common-card composition; preserve it and compare every proposed host behavior against it.
5. `Source/CkGoapDebugger/Public/CkGoapDebugger/Window/SCkGoapDebuggerWindow.cpp` — zero-padding wrapper exemplar around legacy panes.
6. `Source/CkEcsDebugger/Public/CkEcsDebugger/Window/CkDebuggerWindow_Main.cpp` — screenshot repro; center pane currently nests an explicit legacy border inside the Common card.
7. `Source/CkStyleLabDebugger/Public/CkStyleLabDebugger/Widgets/SCkStyleLab_ControlsPane.cpp` — packaged-safe Cards/Workbench exposure and live grouped controls.
8. `Source/CkStyleLabDebugger/Public/CkStyleLabDebugger/Styles/CkStyleLab_AxisMetadata.cpp` — authored runtime option inventory; currently only Cards and Workbench.
9. `Source/CkDebuggerCommon/Private/Tests/CkDebuggerStyle.spec.cpp` — live style/card geometry and legacy Outlined compatibility coverage.
10. `docs/plans/2026-08-22-ai-overview/Plan/Gate_04_SuiteStyleMigration.md` — suite classification, architectural decisions, and visual acceptance gate.

## 8. Things ruled out

- The screenshot is not merely a palette mismatch; source shows competing pane-perimeter owners.
- The defect is not fixed by changing only splitter ratios, minimum sizes, or handle size.
- AI Overview is not the bad outlier; it is the approved clean reference.
- There is no stock rounded Slate child-clip API in the inspected engine that can centrally mask arbitrary descendants.
- Outlined is not a required third pane treatment and must not return.
- No cook/package result is needed locally for this visual parity pass.
- The generated Visual Studio project is not on the primary engine; it is rooted in `_Other`. The primary-engine result came from the stale/inconsistent `.uproject GUID -> registry` helper path.

## 9. Architecture notes and gotchas

- One widget must own the outer pane perimeter. For Cards this should be Common card chrome; legacy child roots should be transparent or begin inside a deliberate content frame. For Workbench the same Common host should collapse to a contiguous flat surface.
- Do not use `ClipToBounds` as a purported rounded fix; it is rectangular.
- Avoid RetainerWidget/custom-mask work across every debugger unless source proves no ordinary chrome-ownership solution is possible; it is heavier and unnecessary for most passive panes.
- Keep graph/canvas/viewport/timeline fills semantic. Transfer only outer pane chrome.
- Preserve zero inter-pane layout padding in Workbench and existing resizable `SSplitter` topology. Cards spacing must come from the Common style axis, not per-debugger slot padding.
- Commonize any new reusable host/widget even if initially consumed once, unless it is demonstrably impossible to reuse.
- Never store raw pointers. Preserve existing managed Slate/UObject ownership patterns.
- Do not change the `.uproject`, engine registry, generated solution, or engine checkout. Do not trigger a clean rebuild or delete build artifacts.
- Do not cook, package, stage, or archive locally.

## 10. Concrete diagnostic and verification flow

1. Read this file, `PROGRESS.md`, Gate 4, the baseline, and the CkGameplayDebugger/Common Slate guidance before editing.
2. Re-run `git status --short`, branch, and HEAD at root and plugin; preserve every unrelated path listed above.
3. Verify `Intermediate\ProjectFiles\BusterBlock.vcxproj` still names only `D:\Repos\UnrealEngineAngelscript_Other`. Treat `Get-ProjectEnginePath.ps1` as non-authoritative until its GUID mapping is reconciled outside this task.
4. Capture Cards/Workbench screenshots and complete the pane-owner matrix for at least ECS, GOAP, SM, Crowd, Jolt, Map, Save, and Optimization before choosing the Common contract.
5. Write the contract first: Common owns outer chrome; feature child owns content/semantic rendering. Decide whether this is an `SCkDebug_Card` mode, a new reusable `SCkDebug_PaneHost`, or a transparent-root convention with explicit tests. Do not begin with a bulk `SBorder` deletion.
6. Implement one representative plain debugger (ECS is the screenshot repro) and one representative bespoke renderer host. Reopen both in Cards and Workbench and compare against AI Overview before propagating.
7. Add/adjust tests that prove:
   - only Cards and Workbench are exposed;
   - existing `Outlined` config resolves to Flat;
   - the representative pane has exactly one outer Common host and no redundant root chrome;
   - already-built cards still react live to the style axis;
   - splitter ratios/minimums/handles remain unchanged.
8. Propagate by family: graph/decision, spatial/runtime, then platform/tool. Preserve feature semantics and document any truly bespoke exception.
9. Close Unreal Editor before building. Use only a detached UnrealToolbox process with explicit project path and visible Toolbox progress UI; never pass `--no-progress-window`. Example build launch:

   ```powershell
   $args = @(
       '--build', '--config=Development', '--target=Editor',
       '--output=Saved/Logs/DebuggerPaneParity-Build.log',
       '--project=E:\Repos\BusterBlock_Other'
   )
   $process = Start-Process -FilePath '.\CkAuto\UnrealToolbox.exe' -ArgumentList $args -PassThru -WindowStyle Hidden
   ```

   Monitor that process and log to completion. Abort if the log names `D:\Repos\UnrealEngineAngelscript` without `_Other`.
10. Run focused style/construction tests through detached UnrealToolbox, then the full gate:

   ```text
   --test --discover-fresh --test-pattern Style --output=Saved/Logs/DebuggerPaneParity-Style.log --project=E:\Repos\BusterBlock_Other
   --test --test-pattern Debugger --output=Saved/Logs/DebuggerPaneParity-Final-Debugger.log --project=E:\Repos\BusterBlock_Other
   ```

11. Read the fresh logs, not only exit codes. Expected full-gate comparison target is 256 total / 255 passed / the same sole inherited `Bb.Snapshot.SmallLoopGoapDebuggerLoadAcceptance` opt-in failure / 0 skipped / 0 contaminated. Because Outlined retirement is currently unbuilt, label this historical until the first fresh gate confirms it.
12. Perform final live editor acceptance in every modern debugger at default and narrow widths, switching Cards and Workbench and dragging every affected splitter. AI Overview must remain visually unchanged.
13. Run `git diff --check`. Do not commit or push unless explicitly requested.

## 11. Suggested first message

I'm continuing debugger pane layout parity. Read this continuation prompt fully before doing anything: E:\Repos\BusterBlock_Other\Plugins\CkGameplayDebugger\docs\plans\2026-08-22-ai-overview\CONTINUATION_PROMPT_DEBUGGER_PANE_LAYOUT_PARITY.md

All modern debuggers except AI Overview must match the approved AI Overview Cards and Workbench pane grammar, with AI Overview preserved as the golden reference and Outlined remaining retired. First verify root/plugin dirt and the `_Other` generated-project association, then build the pane-owner matrix before changing the shared Common host contract; do not cook, package, switch engines, commit, or push.
