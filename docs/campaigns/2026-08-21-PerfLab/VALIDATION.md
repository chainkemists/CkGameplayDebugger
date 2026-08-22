# PerfLab — VALIDATION.md (acceptance protocol)

> **Written:** 2026-08-21. Executed in full at Phase 9; `[EDITOR-VERIFY]` rows accumulate from every
> phase and are Adam's checklist — agents must not claim them.

## A. Automated gates (executor runs, final binary, toolbox only)

| # | Command | Expected |
|---|---|---|
| 1 | `CkAuto/UnrealToolbox.exe --build --target=Editor` | Clean build |
| 2 | `--test --test-pattern Ck.PerfLab` | All campaign specs green (count recorded per phase; final count here at close) |
| 3 | `--test --test-pattern Ck.OptimizationDebugger` | ≥ 76 green, zero lost vs the 76/76 baseline |
| 4 | `--test --test-pattern Ck.DebuggerCommon` / `Ck.DebuggerLauncher` / `Ck.Profile` | No regressions vs Phase 0 entry counts |
| 5 | `--test` (full suite) | Failing set == Phase 0 baseline failing set (names diffed, not just counts) |
| 6 | Child run: Phase 4's captured command against the test map (generous budget, e.g. 33.3ms) | Exit 0; `session.json` decodes; state `Done`; score in 100-region; ZERO findings published (moat, under-budget direction, real data) |
| 6a | Re-analyse the SAME captured session with budget 0.5ms (host-side re-analysis; if the analysis API only reads budget from the session request, one extra child pass with `-budgetMs=0.5`) | Every position over budget; among findings ONLY `Perf.General.OverBudgetUnattributed` (and `Perf.Confidence.StreamingUnsettled` if its own condition holds); rules 1–8 ABSENT — the gate suppresses attribution without census signal, on real measured data |
| 6b | The captured thin-map `session.json` is checked in as the canonical fixture seed | Decodes through the same loader the specs use |
| 7 | Child run with `-nullrhi` | Completes; GT present; RT/GPU `Unavailable_*` |
| 8 | Commandlet: `-run=CkPerfLabReport -session=<fixture> -failbelow=101` | Exit 2 (below threshold) |
| 9 | Same with `-failbelow=0` | Exit 0 |
| 10 | Determinism: export the same session twice (same handed-in timestamp) | Byte-identical HTML/CSV/JSON |
| 11 | Packaged Development build (via runreal workflow, Adam's call if heavy) | PerfLab present; Test/Shipping: absent |

## B. `[EDITOR-VERIFY]` — Adam's checklist (grows during the campaign; phases append here)

Phase 1 (shipped 2026-08-21 — these two are the phase's only unverified claims):
- [ ] BP: place `[Ck] Get Thread Timings` — the node exposes Frame / GameThread / RenderThread /
      RhiThread / Gpu times plus a `GpuAvailability` pin.
- [ ] AS: after an editor boot, `utils_stats::Get_ThreadTimings()` resolves in a script and the
      returned struct's getters are callable.
- [ ] With a real RHI (i.e. a normal editor session, not the `-nullrhi` test harness), confirm
      `GpuAvailability` reads `Available` and the GPU time is non-zero. Every automated run so far
      was headless, so the *available* branch has only ever been exercised vacuously.

Phase 7:
- [ ] Performance page: pick test map + 60 fps budget + Standard → Run; editor remains responsive
      throughout; progress advances; results render with score + component table.
- [ ] Cancel mid-run → child exits within grace; session listed as aborted with reason.
- [ ] Contributor verbs: select / focus / open asset work; with the map closed, verbs disabled with
      reason tooltip.
- [ ] All-under-budget session shows the "measured clean" status-strip statement.
- [ ] Enter/exit PIE with the page open → no crash; world-resolving verbs re-validate.

Phase 8:
- [ ] Heatmap toggled on over the open map: markers at measured positions; colour+shape+size track
      the data; legend matches.
- [ ] Click a marker → position selected in the page.
- [ ] After toggling heatmap on/off: no dirty packages, `git status` content-clean.
- [ ] Select a session for a DIFFERENT map → heatmap inert + reason shown.
- [ ] `-CkStreamerMode` → heatmap suppressed.

Phase 9:
- [ ] Run session → optimize something small → run again → compare view shows the delta at the
      touched positions and flags any regression; cross-config compare shows the warning banner.
- [ ] Open exported HTML in a browser: self-contained, score formula table present, limitation
      paragraph present.
- [ ] **Plan determinism on a map with real navmesh** — the one item Phase 9's own fix cannot prove
      from a unit test. Run the same map twice without editing it; the two runs must produce the SAME
      set of position ids. Phase 9 replaced `GetRandomReachablePointInRadius` with a fixed lattice
      projected onto the navmesh (`Project_LatticeOntoNavmesh`), removing the last RNG call from the
      module — but every automated gate here runs on a fixture or on a map with no navigation data,
      so the branch that used to be non-deterministic is **still not exercised by any test**. If the
      ids differ, compare is broken on real maps and D-007 reopens.
- [ ] Open `report.csv` in a spreadsheet: six sections present, no column shifted (machine names and
      object paths carry commas), and the contributors table's disclaimer column present.
- [ ] `report.json` re-imported as a session: it must load, proving the export did not fork the
      schema.
- [ ] Commandlet exit codes end to end, on a real session directory:
      `-run=CkPerfLabReport -session=<dir> -output=<dir>` → 0; add `-failbelow=101` → 2; point
      `-session=` at a directory with no session file → 1.

Post-campaign (downstream, BusterBlock):
- [ ] Run PerfLab against a real BusterBlock level: at least one specific rule (1–8) fires with
      plausible attribution naming position, metric value, and census signal; a known-cheap area
      of the same level contributes zero findings. This is the only end-to-end exercise of the
      signal-present-AND-over-budget conjunction; until it passes, that conjunction is
      spec-verified only.

## C. Definition of done

Routed through `ck-change-control` (final change class 3): every PROMPT.md success criterion checked
with named evidence or listed above; module `CLAUDE.md`s current; the confirmed/inferred split stated
in the final PROGRESS.md entry; branches enumerated with SHAs; nothing pushed; merge order
(CkFoundation → CkGameplayDebugger → gitlink bumps) written down for Adam.
