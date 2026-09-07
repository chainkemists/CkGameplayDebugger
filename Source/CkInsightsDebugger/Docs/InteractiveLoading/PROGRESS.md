# Progress

Completed and verified: interactive single-frame loading, background multi-selection/manual Worst 10 analysis, a stable summary strip, and running hot-path updates for completed-trace multi-selections.

## Behavior

- Closed frames appear while TraceServices parses; selected single-frame details are provisional until the final refresh.
- Single/multi requests share one active worker plus one latest pending request. Cancellation and request generations reject stale results. Sessions remain owned by workers until they finish.
- Multi-frame reports build one incremental hot-path pool. Independent progress snapshots use valid processed-frame denominators and prefix samples; they do not re-analyze earlier frames. Publication defaults to 0.75 seconds and adapts to snapshot cost.
- The worker prepares category/top-timer rows and reports. Move-only payloads and TFuture::Consume avoid large UI-thread copies. A latest-only mailbox bounds pending progress and prioritizes ready final results.
- The hot-path tree preserves expanded/collapsed branches, selected rows, and scroll offset across partial/final updates. Partial status is explicit; final-only percentiles are deferred.
- A hidden font-scaled stat tile reserves the summary strip's height during empty, clear, single, and multi states.
- JSON export remains blocked for intermediate results or active analysis. Range analysis while the trace is still parsing remains outside this milestone.

## Final verification

`Saved/Logs/InsightsRunningTotals-Verified.log` records an incremental Development editor build success and 34 tests passed, 0 failed, 0 skipped, 0 contaminated. Toolbox used two fresh editor boots: 28 cached tests and 6 newly discovered tests, in one serial invocation filtered with Ck.Insights. Test duration was 1m 3s.

Coverage includes the real saved trace's provisional snapshot/final parity, eight-frame background multi-report analysis, independent progress snapshots and processed denominators, deterministic cancellation from a progress callback, cleared/stale single/multi requests, actual tab application of prepared results, partial-to-final UI state preservation, chart viewport preservation, and summary Slate desired-height stability.

The log contains no ensure, assertion, fatal, automation-controller error, reload-contamination, or fixture-SKIPPED diagnostics. Both submodules pass git diff --check. This gate also validates the async multi-frame edits that were previously awaiting a build. No manual visual UI pass or massive-trace performance benchmark was performed; functional tests do not establish a measured speedup.

Validation startup notes: redirecting a child launch-error file into Saved/Logs caused the editor-open guard to mistake that lock for a live editor. Removing the redirection resolved it without overriding the guard. The successful run then waited normally for another machine-wide build. No other checkout or process was modified. The earlier broad Insights filter's engine-fixture failures are superseded by this narrow green plugin gate.

No commits or publication performed for these feature milestones. Unrelated existing working changes remain outside scope.
