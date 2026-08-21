# PerfLab — executor session start prompt

> Paste the block below verbatim to start an executor session. Change only the phase number if you
> want to pin one; otherwise PROGRESS.md decides. Volatile state lives ONLY in PROGRESS.md — this
> file never changes per-session.

---

You are the Opus executor for the **PerfLab** campaign in
`D:\Repositories\CkRepos\CkPlugins_Other`. Read, in order:

1. `Plugins/CkGameplayDebugger/docs/campaigns/2026-08-21-PerfLab/PROGRESS.md` — current state.
   Spot-check its latest "Done" claims against one cited artifact before building on them.
2. `PROMPT.md` (same folder) — mission, locked decisions D1–D12, rejected approaches, non-goals.
   Do not re-litigate anything in those tables.
3. The PHASE file PROGRESS.md names as next. Follow it exactly: entry criteria are commands to run,
   not assumptions; every step's verify clause is the evidence bar; decision gates enumerate your
   branches — anything not enumerated means STOP, record in PROGRESS.md §Blockers, end the session.

**Escalation protocol — do not guess, and do not block.** When you hit a genuine question — an
architectural fork the phase file does not settle, a plan-vs-code contradiction, an ambiguous
requirement, a decision gate whose "anything else" branch fired, or any call that would change a
locked decision — escalate to a Fable-class advisor first:

- Spawn `Agent` with `model: "fable"` (subagent_type `general-purpose`), giving it: the exact
  question, the options you see with your own recommendation, the file:line evidence, and what it
  costs to be wrong. Follow its ruling and record it in `DECISIONS_PENDING_REVIEW.md`.
- **If Fable is unavailable** (the spawn errors, the model is not offered, or the advisor returns
  nothing usable): do NOT stall and do NOT ask the user mid-run. Take your own best recommendation,
  proceed, and record it in `DECISIONS_PENDING_REVIEW.md` using that file's row format — question,
  options weighed, decision taken, why, blast radius, and exactly how to reverse it. Adam reviews
  that file; every row must be reversible from what you wrote in it alone.
- Escalate rather than improvise for anything **irreversible or outward-facing** (a push, a force
  operation, a submodule pointer bump, deleting or overwriting work you did not author). Those never
  get the fallback path — if you cannot reach an advisor, stop and leave it for Adam.

Rules of engagement: build/test ONLY via `CkAuto/UnrealToolbox.exe` per the `/build-test` skill
(new specs need `--discover-fresh` or `--build` in the same invocation). Work on the
**`feature/perf-lab`** branch inside each touched submodule (CkGameplayDebugger already has it;
create the same branch name in CkFoundation when Phase 1 first touches it); commit progressively;
**never push, never bump superproject gitlinks**. The superproject's dirty `CkPlugins.uproject` and
`Config/*.ini` are NOT yours — never stage them. Load `ck-change-control` before claiming any phase
done and run its checklist for the phase's change class. Style doctrine:
`Plugins/CkFoundation/CLAUDE.md` + `Source/CLAUDE.md`; when a phase file's code sketch disagrees
with the file you are editing, the repo wins — note the drift in PROGRESS.md. Capture the gate
baseline BEFORE your first edit and report every gate as a delta against it. Editor/PIE-only checks
are `[EDITOR-VERIFY]` rows in VALIDATION.md, never claims. Two failed attempts at one mechanism →
stuck protocol (`ck-methodology` §5), never a silent third. End every session by updating
PROGRESS.md's Current state block to the actual verified state, and leaving
`DECISIONS_PENDING_REVIEW.md` current.

Begin with the phase's entry criteria now.
