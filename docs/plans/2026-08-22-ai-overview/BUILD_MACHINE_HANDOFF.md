# CK AI Overview — build-machine packaged acceptance handoff

> Execute only on the build machine. Local Codex workspaces must not cook, package, stage, or archive.

## Preconditions

1. Fetch the delivered BusterBlock root change and the delivered `CkGameplayDebugger` revision containing this campaign.
2. Confirm the build machine's normal Unreal engine association and close any editor using this project.
3. Choose a unique build id and archive root; do not delete or reuse another build's archive.

## Development cook and package

From the BusterBlock repository root in PowerShell:

```powershell
$env:RUNREAL_PROJECT_PATH = (Get-Location).Path
$env:RUNREAL_BUILD_ID = 'ck-ai-overview-dev-<timestamp>'
$env:RUNREAL_BUILD_PATH = '<build-machine-archive-root>'
& '.\.runreal\scripts\cook-with-retry.ps1'
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

runreal buildgraph run '.\.runreal\buildgraph\Build.xml' `
  "-set:BuildId=$env:RUNREAL_BUILD_ID" `
  '-set:ProjectName=BusterBlock' `
  "-set:ProjectPath=$env:RUNREAL_PROJECT_PATH" `
  "-set:OutputPath=$env:RUNREAL_BUILD_PATH" `
  '-set:ClientTargetName=BusterBlock' `
  '-set:ClientConfigurations=Development' `
  '-Target=Package Clients'
```

Do not run Test or Shipping for this debugger acceptance: DeveloperTool debugger modules are intentionally present in Development/DebugGame and absent from Test/Shipping.

## Packaged discriminating experiment

1. Launch the archived Development `BusterBlock.exe` with `-log` and `-ExecCmds="ck.AiDebugger 1"`.
2. Enter a gameplay map containing known NPCs.
3. Confirm the AI Overview and debugger launcher open, the cursor-labelled `Pick entity` control is in the common right-side action lane, and world speed/launcher controls appear in every debugger opened.
4. Select `0.1x` on the listen-server host and confirm the connected client observes the slowed replicated world.
5. Toggle `BusterBlock.Npc.SuppressStuckRecovery`; verify stuck recovery remains suppressed only for the session and starts a fresh episode after re-enable.
6. Activate the picker and click a known visible NPC. Record its availability tooltip/counters before and after the click.
7. In a population where multiple physical Crowd agents resolve to the same conceptual NPC/owner chain, keep AI Overview
   open for at least three roster refreshes. Confirm every physical agent remains a distinct row and the process does not
   emit `WidgetMapToItem`, `ItemsWithGeneratedWidgets`, `Cannot set entity health rows`, or an `SListView` assertion.
8. Click two different physical rows that share a conceptual target. Confirm the overview stays on that conceptual NPC
   while the spatial viewport highlights the exact physical row clicked rather than the collector's first sibling.
9. Travel to another gameplay map with AI Overview still open. Confirm the old roster/selection clears before teardown,
   the new world's roster repopulates, and no stale-handle, fatal, assertion, or ensure is emitted.

Interpret the picker result exactly:

| Observed taxonomy | Proven boundary | Next code action |
|---|---|---|
| `no matching entities` | Packaged target filter/data registration | Audit the selected debugger target predicate and packaged module/data registration. |
| `no transform representation` | Entity exists but has no transform-backed visual mapping | Repair the exact packaged ownership/representation topology and add it as a regression fixture. |
| `all culled/filtered` | Gather-stage visibility/cull policy | Repair only the reported filter for the reproduced entity topology. |
| `all ignored local pawn` | Only the intentional local-pawn exclusion remains | Add another NPC; do not weaken the exclusion. |
| `viable candidates` but click selects nothing | Input projection/hit resolution | Capture cursor/ray/marker evidence and repair hit resolution, not entity gathering. |

## Required evidence to return

- BuildGraph/cook/package exit codes and archive path.
- Packaged startup log with no `Angelscript: Error`, fatal, debugger module-load error, or touched-file warning.
- Screenshot of the AI Overview at normal and narrow width.
- Listen-server/client world-speed observation.
- Behavior-override observation.
- Picker taxonomy/counters plus clicked entity topology.
- Roster row count and physical entity ids across three refreshes, plus the two clicked physical ids.
- Map-travel log excerpt proving the old world invalidated and the new roster repopulated without a Slate invariant error.
- A single root-cause sentence stating what failed, when it failed, and where the value/entity was lost.
