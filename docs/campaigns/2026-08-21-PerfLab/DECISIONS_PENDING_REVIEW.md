# PerfLab — decisions pending Adam's review

> Every judgment call the executor made **without** a Fable ruling, plus every Fable ruling it did
> get. Adam reviews this file; each row must be reversible from what is written here alone.
> Append newest at the bottom of the table. Do not delete rows — mark them `Reviewed: ✔`/`Reverted`.

Row format: **What was asked** · **Options weighed** · **Decision + why** · **Advisor**
(`Fable` / `self — Fable unavailable`) · **Blast radius** · **How to reverse**.

---

### D-001 — Which branch does campaign work land on?

- **Asked:** PHASE_0 entry says create `perflab/phase-0` in both submodules. Reality: Adam had
  already created **`feature/perf-lab`** in CkGameplayDebugger and committed the campaign docs to it
  (`c1afdc4`, 18 files, 1 commit ahead of `dev`, 0 behind). CkFoundation/CkTests are clean on `dev`.
- **Options:** (a) adopt `feature/perf-lab` as the campaign branch in every touched submodule;
  (b) create `perflab/phase-0` as planned and leave `feature/perf-lab` orphaned; (c) create
  per-phase branches stacked on `feature/perf-lab`.
- **Decision: (a).** Adam named the branch after this exact campaign and committed the plan to it —
  the repo is the authority over a plan doc written before it existed (`ck-methodology`: code wins).
  Per-phase branches (c) buy nothing here because nothing is pushed and phases land sequentially.
  CkFoundation gets the same branch name when Phase 1 first touches it.
- **Advisor:** self — decision is low-blast, reversible, and matches an explicit signal from Adam;
  a Fable consult would not change it.
- **Blast radius:** naming only. No history rewritten; no push.
- **Reverse:** `git -C Plugins/CkGameplayDebugger branch -m feature/perf-lab perflab/phase-0`
  (or branch off at any point). PLAN.md/PHASE_* mention the branch name in prose only.

### D-002 — Baseline captured with `--build --test`, full suite, auto-sized lanes

- **Asked:** Phase 0 requires a full-suite baseline. Build first or test the existing binary? Serial
  or parallel lanes?
- **Options:** (a) `--test` only against whatever binary exists — fast, but a stale binary makes the
  baseline non-comparable to later phases' runs; (b) `--build --test` serial (`--parallel 1`) —
  correct but ~23 min of test time; (c) `--build --test` auto-sized lanes — correct and ~2.4× faster.
- **Decision: (c).** The build-test skill's caveat against lanes under `--build --test` is
  specifically about codegen regen races when a build *changes* codegen; Phase 0 changes no source
  at all, so no regen occurs and the race cannot trigger. Building first is required because later
  phases diff against this number on freshly built binaries.
- **Advisor:** self — mechanical, and the skill's own reasoning settles it.
- **Blast radius:** the baseline number itself. If it proves noisy, re-run.
- **Reverse:** re-run `./CkAuto/UnrealToolbox.exe --build --target=Editor --test --parallel 1` and
  replace the baseline recorded in PROGRESS.md.

### D-003 — What content validates the evidence gate? **(Fable-ruled)**

- **Asked:** The host project is deliberately content-minimal — zero maps in `Content/`, only two
  thin CkTests levels. Nothing here can make a perf rule legitimately fire, so the evidence gate
  (the product's moat) would ship never having seen real measured data.
- **Options:** (a) validate on the thin gym maps only; (b) author a deliberately expensive
  validation map; (c) fixtures in specs + thin-map smoke + real-content validation deferred to an
  `[EDITOR-VERIFY]` against a downstream game project; (d) hybrid.
- **Decision: (c)+(a), amended — plus a budget-manipulation pass.** `budgetMs` is a *request
  parameter*, not a property of the content: re-analysing a real thin-map capture at
  `budgetMs = 0.5` makes every position genuinely over budget with **real measured data**, while
  the thin census means rules 1–8 must stay silent and only
  `Perf.General.OverBudgetUnattributed` may fire. That exercises capture → decode → gate →
  suppression → finding emission end-to-end, with zero content authored. Paired with the
  generous-budget run (score ≈100, zero findings), both directions of the moat now run on real
  data in this repo.
- **Advisor: Fable** (consulted 2026-08-21; ruling applied verbatim).
- **Why (b) lost:** primary disqualifier is *nondeterminism*, not repo pollution — rule-firing on
  a checked-in map depends on machine-relative cost, so the acceptance gate it exists to serve
  could not be deterministic. Repo pollution is the second strike.
- **What the executor had wrong:** I treated "real end-to-end evidence of the gate" as
  unobtainable without heavy content, and would have shipped with measured data never flowing
  through the rule evaluator — the exact false-confidence failure this question was raised about.
- **Honest residual (Adam should know):** even amended, the *positive* conjunction (over budget
  AND signal present → a specific rule fires) never runs end-to-end inside this repo. It is
  spec-verified only until the downstream BusterBlock `[EDITOR-VERIFY]` row in VALIDATION.md §B
  passes. That row now states this explicitly instead of leaving it implicit.
- **Blast radius:** test strategy for Phases 4/6/9; no production-code shape changes beyond making
  the analysis entry point take a `budgetMs` override (pure-side).
- **Reverse:** drop VALIDATION.md rows 6a/6b and PHASE_6 spec items 6–7; the analysis budget
  override is harmless if unused.
- **Amendments applied:** VALIDATION.md §A rows 6/6a/6b + §B post-campaign row; PHASE_6.md §6.4
  items 6–7; PHASE_4.md runner "budget is carried, never baked" bullet.

### D-005 — D2 overturned, then reinstated: the child-binary finding **(Fable-ruled twice)**

**This is the durable artifact of the Phase 0 gate. PHASE_5 cites it.**

- **What happened:** PHASE_0's smoke fired its STOP gate. I misdiagnosed the cause, escalated to
  Fable to re-litigate locked decision D2, and the advisor ruled to **overturn** D2 (child becomes a
  separate *editor* instance). I then found the true root cause, sent the falsifying evidence, and
  the advisor **withdrew its ruling**. D2 stands as originally written.
- **The two commands that settle it:**
  - **FAILS** (engine binary): `D:/Repositories/UnrealEngine-Angelscript/Engine/Binaries/Win64/UnrealEditor-Cmd.exe <proj> /CkTests/TestGyms/TestGyms_CkTests_Level -game -windowed -resx=1280 -resy=720 -unattended -nosplash`
    → exit 1 in ~20 s, `Plugin 'CkFoundation' failed to load because module 'CkIskmRendererVF' could not be found`. **Reproduced identically with `-game` removed** — which is what falsified the `-game` theory.
  - **PASSES** (project's own binary): `D:/Repositories/CkRepos/CkPlugins_Other/Binaries/Win64/CkPluginsEditor-Cmd.exe <proj> "/CkTests/TestGyms/TestGyms_CkTests_Level" -game -windowed -resx=1280 -resy=720 -unattended -nosplash -abslog=... -ExecCmds="t.MaxFPS 0, r.VSync 0"`
    → ran the full 220 s bound (exit 124 = we killed it), D3D12, map loaded in 1.02 s, world up at max tick rate 0, zero plugin errors, zero `LogWindows: Error`.
- **Root cause:** `Source/CkPluginsEditor.Target.cs:18` sets
  `BuildEnvironment = TargetBuildEnvironment.Unique`; the project builds its own
  `CkPluginsEditor-Cmd.exe` + 747 `CkPluginsEditor-*.dll`, which the engine's stock binary cannot
  resolve **in any mode**.
- **Decision:** D2 unchanged except that the launcher uses `FPlatformProcess::ExecutablePath()`
  verbatim (self-correcting for Unique *and* shared-environment consumer projects). Do not
  synthesize a `-Cmd` variant; guard boot-failure legibility by surfacing the child's `-abslog` tail.
- **Advisor: Fable** (ruled, then withdrew on evidence). Its withdrawal reasoning: bootability is now
  equal by construction, and every remaining axis — GameThread validity, no editor chrome in GPU
  numbers, `UGameInstanceSubsystem` viability, boot speed — favours `-game`.
- **What I got wrong (worth Adam's attention):** I escalated a *decision* before finishing the
  *diagnosis*. The correlation I had (stale Game target, zero Game DLLs) was real but causally
  irrelevant. Had the advisor's first ruling been applied, the campaign would have adopted a
  strictly worse architecture on a false premise. The lesson is already doctrine — reproduce before
  claiming cause — and I broke it under time pressure.
- **What the advisor got wrong (I did not follow it):** it advised that `-SCCProvider=None` was
  probably moot because GitSourceControl is Editor-type. It is **`UncookedOnly`** (verified in
  `GitSourceControl.uplugin`), which *does* load in `-game` on an editor binary — so the flag stays.
- **Kept from the overturned ruling** (premise-independent risks, now folded into PHASE_4 §4.0):
  the environment-assertion gate; `t.IdleWhenNotForeground` (default 0, assert anyway);
  **`bSmoothFrameRate`**, which applies in `-game` and silently floors fast frames — verified at
  `UnrealEngine.cpp:11875`, with `bForceDisableFrameRateSmoothing` as the cleaner runtime lever;
  never-minimize-the-window; `-abslog` isolation; shader-compile activity as a validity signal;
  actual-vs-requested viewport size.
- **Blast radius:** architecture of the measurement child — the campaign's single largest technical
  commitment.
- **Reverse:** the editor-child alternative is fully written up in this session's advisor exchange
  and re-rejected in PROMPT.md's rejected-approaches table with its reasoning intact.

### D-006 — Phase 1 judgment calls (self, no ruling needed; recorded for audit)

- **A struct, not scalar getters.** `CkStats_Utils.h` is otherwise all scalar getters, so
  `FCk_Stats_ThreadTimings` is a departure from the file's local shape. Two reasons it wins: the four
  numbers are only comparable when read from the same frame, and a scalar `Get_GpuTimeMs()` could not
  express "unavailable" without a sentinel — which is exactly the zero-as-data trap D5 forbids.
  *Reverse:* split into scalar getters and drop the availability enum (also drops the D5 guarantee).
- **Named namespace in the new test, where its sibling uses an anonymous one.**
  `Test_Profile_ScopedStat.cpp` declares its flags constant in `namespace { }`. Doctrine bans
  anonymous namespaces because unity builds merge translation units, and my file would have declared
  an identically-shaped constant. Used `ck_test_profile_thread_timings` instead. The sibling is left
  alone — not my file to churn. *Reverse:* rename to an anonymous namespace and accept the risk.
- **Added `CK_DEFINE_CUSTOM_FORMATTER_ENUM` although neither existing enum in this file has one.**
  Doctrine requires it on every UENUM, and PerfLab logs the availability reason through
  `ck::Format_UE` `{}`, which does not compile without it. The two pre-existing enums are left alone.
- **Left the existing `Get_FrameTimeMs()` untouched** even though it uses `FApp::GetDeltaTime()`
  while the new snapshot uses stat unit's `GetCurrentTime() - GetLastTime()`. Changing it would be a
  behaviour change to an established API (change class 3) for no benefit to this campaign. The two
  now differ by design; the module doc says why.
- **No `UMETA(DisplayName=...)` on the new enum.** UE humanises the identifiers for display, and the
  session JSON maps enum→string explicitly rather than through display text, so display names would
  add a second place for the same name to drift.

### D-007 — Adversarial review outcomes (2026-08-22), including one I got wrong

Three independent reviewers (correctness, style/doctrine, reuse) went over ~7,000 lines. Findings
were verified before acting — several were real, one "critical" was refuted, and **one refutation of
mine was itself wrong**, which is the entry worth reading.

**The launcher was broken and I initially claimed it wasn't.** A reviewer found that
`Build_ChildCommandLine` emitted `"-CkPerfLab-Request=<path>"` quoted as a WHOLE TOKEN, and that
`FParse::Value` locates its key with `Strifind(..., bSkipQuotedChars = true)` — so the switch is
skipped and reads as absent. I ran a test that appeared to refute it and reported the finding as
wrong. **My test was invalid: bash strips quotes before exec, so I had tested the unquoted form.**
Re-tested passing literal quote characters: the child **never armed** and ran until killed
(exit 124). The reviewer was right; every live capture in this campaign had been hand-launched with
the unquoted form, so `Build_ChildCommandLine`'s output had never once been fed to a child.
Fixed to value-quoting (`-CkPerfLab-Request="<path>"`, matching `-abslog`), and **verified**: armed,
planned, completed, exit 0. The spec that had pinned the broken form now asserts both the correct
form and the absence of the broken one.

**Other confirmed defects fixed:**
- `MeasuredClean` could report true while an over-budget finding was published — `AnyOverBudget` was
  a side effect of the rule loop, so a position over budget on frame time while every individual
  thread sat inside it reported clean. That is the ordinary shape of a pipelined frame. Now decided
  once across all five metrics.
- **Every `GetObjectField` + `IsValid()` guard in the codec was dead.** UE returns a static
  empty-but-VALID object for a missing field, so the guards could never fire: a request without an
  `outlier` block silently decoded `madK = 0` (collapsing the outlier threshold onto the median) and
  `childWallClockBudgetSec = 0` (self-abort on first tick), and a position without a `location`
  decoded to the world origin. Switched to `TryGetObjectField`, which actually fails.
- The contributor disclaimer was defined, exported and spec-pinned but **nothing ever emitted it** —
  the honesty claim rested on a string no consumer could see. Contributors now carry it as a field.
- Confidence never received the outlier count, so `OutlierHeavy` could not fire; and it read the
  settle detector directly, which is rebuilt per direction, so it described only the last direction.
  Both fixed (latched across the position).
- A failed camera placement was ignored at all five call sites, silently measuring the previous view
  under the new position's id. Now fails the run.
- `viewportSizeActual` was never populated despite GPU cost scaling with resolution.
- An empty `directionsPerPosition` would have crashed the runner on `Get_Yaws()[0]`.

**Style/doctrine fixes:** process breadcrumbs removed from shipped comments (five sites naming
"Phase 0", "the campaign", "during development"); `Initialize`/`Deinitialize` moved to trailing
returns per the 18-of-20 house precedent; two same-line `if` bodies split; an alignment-induced typo
(`auto Render= …`) corrected; missing includes added.

**Validity-check correction from Adam mid-review:** `TObjectPtr` and smart pointers take the PLAIN
`ck::IsValid`; the nullptr policy is only for raw pointers that are not UObjects. Swept — the only
remaining policy use is `IConsoleVariable*`, which is correctly not a UObject.

**~~Plan determinism on navmesh maps~~ — FIXED 2026-08-22, opening Phase 9.**
Navmesh seeding called `GetRandomReachablePointInRadius` 512 times, which draws from the engine's
global RNG rather than from the request seed, so candidate points — and therefore position ids —
varied run to run on any map with real navigation data. Sorting the samples afterwards made their
*order* stable but not the *set*, which is the part compare depends on.

Replaced with `Project_LatticeOntoNavmesh`: a fixed 24×24 lattice over the level's XY footprint,
each cell centre projected onto the navmesh via `UNavigationSystemV1::ProjectPointToNavigation`,
deduplicated on the same 50 cm quantisation the position ids use. **No RNG call remains anywhere in
the module** (swept and confirmed). This is also strictly better sampling: random points clump and
leave holes, a lattice does not.

**Not proven by any automated gate.** Every spec here runs on a fixture or a map with no navigation
data, so the branch that was broken is still the branch no test exercises. Promoted to a
VALIDATION.md `[EDITOR-VERIFY]` item: run one real navmesh map twice and diff the position ids. If
they differ, this reopens.

**Known-open, NOT fixed (recorded rather than silently carried):**
- **`session.json` writes enums PascalCase; SCHEMA.md §3.2 says camelCase.** A real captured session
  carries `"availability": "Available"`, not `"available"`. Found in Phase 9 by reading an actual
  session file rather than a fixture-shaped assumption. The **export**'s analysis block conforms to
  §3.2 (spec-pinned, case-sensitively); the session half does not. Not fixed here because changing
  the codec now invalidates the checked-in fixture and every session already captured, and the
  decode path is identifier-based rather than case-sensitive so nothing is currently broken. Ruling
  wanted: conform the codec and regenerate the fixture, or amend §3.2 to state the session file uses
  C++ identifiers.
- Child failure exit codes are swallowed: `RequestExitWithStatus(false, N)` yields process exit 0, so
  the host reads a failed run as a clean one.
- Skeletal meshes contribute zero triangles to the survey, and instanced static meshes are counted
  once rather than per instance.
- An all-zero sample window (e.g. RHI thread when not separate) is published as `Available` and
  scores full marks, which contradicts the never-zero-as-data contract.
- The runner never verifies the loaded map matches the request.
- `_InViewYaws` is written by the codec and populated by nothing, so the evidence gate's static half
  is 360°-unfiltered — triangles behind the camera count toward the signal.

### D-008 — Phase 9 adversarial review (2026-08-22): six defects, all fixed

A fresh reviewer went over the compare module, the three export builders, the commandlet and the new
lattice sampler. Every finding was verified against the code before acting; none were refuted, and
one I had already found and fixed independently.

**The one that mattered most — the score delta was suppressed on a condition that never applied.**
`_ScoreComparable` was set false whenever the two sessions' *requested* budgets differed, on the
reasoning that scores against different targets are not on one scale. That reasoning is wrong here:
`Compute_Score` reads the budget **only** from its parameter (verified — `Get_BudgetMs()` appears
nowhere in `CkPerfLab_Analysis.cpp`), and `Compare_Sessions` passes the SAME budget to both sides.
The scores were always on one scale, and the suppression hid exactly the number a CI job exists to
produce: a run whose mode changed between captures would print `Score 72.0 → 61.0 (not comparable)`
while an 11-point regression went unreported. `_ScoreComparable` is gone; the `DifferentBudget`
warning survives with honest wording (the runs were *requested* differently, so conditions may
differ) and no longer suppresses anything.

**Other confirmed defects fixed:**
- The positions CSV printed `0.000` for `frameWorstMs` / `frameP99Ms` / `frameOnePercentLowMs` when
  the frame metric was unavailable — the three columns bypassed the availability gate the other five
  went through. Sorting that sheet ascending on p99 floats every unmeasured position to the top as
  the fastest place in the level, the exact inversion `Get_PositionOrder` avoids in the HTML. Now
  routed through `Format_MetricValue`, and pinned by an extended spec.
- **Strict-weak-ordering violation in two sort comparators** (found independently by me and by the
  reviewer). `FMath::IsNearlyEqual` is not transitive, so a≈b and b≈c does not give a≈c, and the
  comparator admitted a cycle among closely spaced frame times — which is what a set of frame times
  is. UE's introsort is bounds-guarded so this could not corrupt memory, but the emitted order became
  a function of input permutation, breaking the byte-identical contract. Both now compare exactly;
  true equality still falls to the deterministic id tie-break.
- The navmesh lattice took its bounds from a box over actor **pivots**. A level built as one
  landscape proxy with its pivot at the origin yields a box centimetres across over a kilometre of
  navigable space — all 576 samples land in one quantisation cell and the whole map plans a single
  position. Now uses `UNavigationSystemV1::GetWorldBounds()`, with actor bounds as fallback.
- `-budget=nan` passed the `<= 0.0f` guard (every comparison against NaN is false) and reached
  `SetNumberField`, emitting a bare `nan` token — **report.json was not parseable JSON**. Now gated
  on `FMath::IsFinite` first. Verified: exits 1 with a readable reason.
- CSV location columns were `static_cast<float>` of a double `FVector`, losing centimetre precision
  at large-world coordinates, and were formatted by a function named `Format_Ms`. Now `Format_Cm`,
  double throughout.

**Reviewer confirmed clean:** no map or set iteration order reaches any output (each container
checked individually); no divide-by-zero in the compare path; delta signs and `Reduce_Verdicts`
correct; HTML and CSV escaping ordered correctly.

### D-004 — Plan-vs-code corrections found in Phase 0 (no ruling needed, recorded for audit)

- `PHASE_2.md` claimed CkGameplayDebugger modules use plain `ModuleRules`. **False** —
  `Source/CkOptimizationDebugger/CkOptimizationDebugger.Build.cs` opens
  `public class CkOptimizationDebugger : CkModuleRules`, inheriting across the plugin boundary.
  Corrected in PHASE_2.md and PHASE_8.md.
- Every CkGameplayDebugger module must link `CkEcs` (that file's own comment: "CkCore's SharedPCH
  instantiates global ECS registrations"). Added to both phase files. **Nuance recorded:**
  `CkProfile` does NOT and must NOT link CkEcs — it is tier T1 and CkEcs is T2, and deps may never
  point to a higher band. The rule applies to the debugger-tier modules, not to Phase 1's work.
- `RESEARCH_EngineApis.md` located `stat unit` in `UnrealEngine.cpp`; it is actually
  `UnrealClient.cpp:350`, and `IsAsyncLoading()` does not exist under that name in this checkout.
  Both corrected in `RESEARCH_EngineApis_Addendum.md`, which supersedes.
