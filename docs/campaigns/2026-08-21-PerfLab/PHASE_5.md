# Phase 5 — Host orchestration: subprocess, watchdog, session store

> **Status:** ⏳ Pending
> **Depends on:** Phase 4 ✅
> **Estimate:** 1 session
> **Change class:** 2

## Goal

After this phase: host-side `CkPerfLab` can launch the child for a request, stream its heartbeat into
a progress model, enforce an outer timeout with kill, and enumerate/load completed sessions from
`Saved/CkPerfLab/Sessions/` — all UI-free (the window binds to it in Phase 7).

## Entry criteria

- [ ] Phase 0 addendum's launcher decision (GitLink `CreateProc` shape vs `FMonitoredProcess`) —
      implement the recorded decision, don't re-decide.
- [ ] Read `Plugins/GitLink/Source/GitLink/Private/GitLink_Subprocess.{h,cpp}` again with intent to
      copy structure (thread-safety, pipe handling, teardown).
- [ ] Branch `perflab/phase-5`; baselines: `Ck.PerfLab`, `Ck.OptimizationDebugger` (76/76).

## Work items

### 5.1 Launcher (`Private/CkPerfLab/Host/CkPerfLab_Subprocess.{h,cpp}`)

- Resolve the running editor's own binary dir for `UnrealEditor-Cmd.exe`
  (`FPlatformProcess::ExecutablePath()` family / `FPaths::EngineDir()` — Phase 0 addendum names the
  exact call) + `FPaths::GetProjectFilePath()`. Compose the Phase-4-proven command line; single quote
  discipline for spaced paths.
- Non-blocking: launch returns a handle object owning PID + session dir; polling API
  (`Get_Heartbeat()`, `Get_State()`, `Request_Cancel()` → graceful file-flag then `TerminateProc`
  after grace, `Get_IsAlive()`).
- Outer watchdog: request budget + margin; expiry → kill + session marked `Aborted_HostTimeout`
  (distinct from the child's own `Failed_Timeout` — the distinction is diagnostic).
- **File the adjudication row** (PROMPT.md A-PerfLab-1) in
  `Plugins/CkFoundation/.claude/reports/ADJUDICATIONS.md`: subprocess utility placement, both sides,
  interim = private to CkPerfLab. Do not relocate code yourself.

### 5.2 Session store (`Public/CkPerfLab/Host/CkPerfLab_SessionStore.h/.cpp`)

- Enumerate `Saved/CkPerfLab/Sessions/*/session.json` → lightweight rows (id, map, mode, timestamp,
  state, score-if-analysed) without full decode (read the header fields only — codec gains a
  `TryRead_SessionSummary`).
- Full load on demand via the Phase 2 codec; delete-session (files only, with the store owning path
  validation — never delete outside its root).
- One live-run registry on the host (multiple queued runs = v2; second Run while one is live →
  refuse with reason. Simplicity rule.)

### 5.3 Specs

1. `Ck.PerfLab.Store.Enumerate` — fixture dirs (good, partial, corrupt, wrong-version) → correct rows
   + typed states; corrupt never throws, never half-loads.
2. `Ck.PerfLab.Store.SummaryFastPath` — summary read touches only header fields (pin by feeding a
   file whose body is deliberately invalid past the header).
3. `Ck.PerfLab.Subprocess.CommandLine` — pure command-line composer → exact expected string for a
   fixture request (paths injected).
4. Launch/watchdog live evidence: executor runs host-side launch against the test map **from a
   commandlet or the Phase-9 CI entry if landed early — otherwise via the same Bash-driven child as
   Phase 4** and pastes captured output; full in-window launch is `[EDITOR-VERIFY]`.

## Expected observations — branches

| I will run | I expect | If instead | Response |
|---|---|---|---|
| Store specs on fixtures | Green | Corrupt fixture crashes decode | Codec whole-or-nothing contract violated — fix codec, add the regression spec there, not a try/catch here |
| Live launch + mid-run `Request_Cancel` | Child exits ≤ grace; session `Aborted_*` | Orphan child persists | Kill-tree semantics wrong (`TerminateProc` child-process flag) — fix before exit; an orphaned editor process is a machine-state hazard |
| `Ck.OptimizationDebugger` | 76/76 | Δ | Should be untouched this phase — A/B stash; anything unexplained → STOP |

## Exit criteria — same commit

- [ ] Specs green vs baseline; live launch+cancel evidence in PROGRESS.md.
- [ ] ADJUDICATIONS row filed (verbatim path recorded in PROGRESS.md).
- [ ] `CkPerfLab/CLAUDE.md` §Host: lifecycle diagram, timeout taxonomy
      (`Failed_Timeout` vs `Aborted_HostTimeout` vs `Aborted_UserCancel`), single-live-run rule.
- [ ] PLAN.md row + Status header + PROGRESS.md entry.

## Fences

- No Slate in this phase; the window binds later.
- Never launch via CkAuto/UnrealToolbox from product code (PROMPT.md rejected-approaches row).
- The store never writes outside `Saved/CkPerfLab/`.
