# Milestone 1

1. Add bounded frame batches and immutable frame/timer snapshots from active TraceServices sessions.
2. Append closed frames during parsing and retain user zoom and selection.
3. Queue single-frame analysis off the UI thread, with one active request and one latest pending request; invalidate old results when selecting or replacing traces.
4. Label provisional detail results and refresh periodically and at completion. Keep range analysis and final export unavailable during provisional analysis.
5. Verify core snapshot failure and final parity, request cancellation/coalescing, chart viewport preservation, and relevant existing Insights tests.

Gate: incremental UnrealToolbox build and focused Insights automation, with fresh log review. Record any missing live-trace or visual evidence explicitly. No publication is part of this milestone.

# Background multi-frame selection

Authorized follow-up: move completed-trace multi-selection and manual worst-frame analysis onto the existing latest-request queue. Prepare categories, top timers, wait rows, merged trees, and reports on the worker; the tab only adopts results and refreshes widgets. Add cooperative cancellation through the multi-frame report loops and a Cancel analysis action. Keep the legacy session thread-id cache initialized on the owner thread before multi-frame dispatch. Range analysis during trace parsing remains deferred.

Verify cancellation discards all partial statistics, mixed single/multi supersession, and actual tab application of prepared results without provider reads. Run scoped module tests through UnrealToolbox; account explicitly for any automatic discovery editor launches.

# Stable layout and running hot paths

Completed: reserve summary height using the same dynamic tile fonts as populated results; stream independent processed-prefix multi-frame snapshots from one incremental pool; throttle publication; keep only the latest pending update; preserve merged-tree interaction state; replace partial output with the final report. Final combined verification is recorded in PROGRESS.md.
