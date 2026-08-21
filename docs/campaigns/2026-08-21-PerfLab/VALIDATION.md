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

Phase 1:
- [ ] BP: place `[Ck] Get Thread Timings` — five floats + availability pin present.
- [ ] AS: after editor boot, `utils_stats::Get_ThreadTimings()` resolves in a script.

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
