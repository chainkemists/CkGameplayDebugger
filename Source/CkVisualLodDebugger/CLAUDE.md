# CkVisualLodDebugger

Shared-widget authoring rules, click-traps, and the new-module checklist live in
[Source/CkDebuggerCommon/CLAUDE.md](../CkDebuggerCommon/CLAUDE.md); framework doctrine in
[CkFoundation/CLAUDE.md](../../../CkFoundation/CLAUDE.md). This doc is only what is specific to
this module.

**Purpose:** Standalone Slate debugger for `CkVisualLod` — budgeted SKMC-proxy ↔ GPU-batched-crowd
LOD arbitration. Per-domain view of budgets (near/lock/unbudgeted charges), crowd pool occupancy,
the resolved local view, per-tick flip activity, a default-collapsed, separately scrollable runtime
tuner workspace, a sortable member roster (representation,
distance, in-view, fade, slot, flags), a member detail rail (fade phase, anim caches, rendering
participation), an event log, fault alerts (view unresolved, pool exhausted), a **Freeze** hold
for eject-and-inspect, **PMG world markers** colored by representation, and default-collapsed
runtime tuners for the selected arbiter and every crowd render band, including each band's far
renderer profile.

Launcher: category **Systems / 50**, tab `CkVisualLodDebugger`, console `ck.VisualLodDebugger`,
icon `ECk_Icon::IsmRenderer`.

## Visual spec

[Mockups/mockup_visuallod_debugger.html](Mockups/mockup_visuallod_debugger.html) is the
maintainer-approved visual spec (GoapDebugger precedent) — layout, information hierarchy, tones,
copy, and the marker shape language (diamond = near skel mesh · Accent, dot = far GPU member · Ok,
ring = unrendered · TextMute, hidden skipped). UI wording: the near representation is labeled
**SkelMesh**, not "Proxy" — the framework's `PromotedProxy`/IskmProxy vocabulary is renderer-side
(the pooled SKMC proxies a batched-crowd instance) and reads backwards to a user, for whom the
near one IS the real skeletal mesh. Keep code identifiers framework-named; keep UI labels
user-named. Far shadow/lighting profiles are now authored CkVisualLod render bands; this module
renders their effective flags and runtime values without retaining their assets. Runtime tuner controls
mutate only the arbiter-owned complete snapshot; they never modify config/profile assets.

## Architecture

Aggro-pattern collector + window:

- `Data/CkVisualLodDebugger_Types.h` — POD snapshots (arbiter info incl. complete authored/live
  tuner trees, view, pools, authored profile names as identity labels, rendering flags; member rows
  with current render band). Never retain a data asset in a Slate attribute.
- `Data/CkVisualLodDebugger_DataCollector` — `Collect(UWorld*)` walks arbiter then member fragment
  views via the transient entity, reads ONLY public Utils plus the documented pure-debug fragment
  reads (each carries a why-comment).
- `Window/SCkVisualLodDebuggerWindow` — the rendering policy is documented at the top of its
  header and is load-bearing: **structure built once; values via TAttribute lambdas over `_Live`;
  rebuilds only on three signatures** (arbiter set, crowd topology, alert set). Member tallies are
  computed once per gated tick (`FTallies`), never per-cell. The overview is monitoring-only:
  `Budgets & crowd pools | View | Activity`, with each pane bounded and locally scrollable. The
  full-width **Tuners** disclosure sits immediately below it; collapsed means its splitter slot uses
  `SizeToContent` with a collapsed child and reserves no expanded height. Expanded, the tuner body and
  investigation workspace share a resizable 5px vertical splitter, while the tuner's own 5px horizontal splitter divides bounded,
  independently scrollable `Arbiter runtime tuners | Crowd tuners` panes. Crowd monitoring and
  crowd tuning have separate retained hosts and are populated/cleared together on topology changes.
  The investigation row remains `roster | (detail | recent activity)` with 5px splitters and no
  layout-local fixed rail or event-log dimensions.

## The CkFoundation debug surface this module consumes

Added for this debugger (C++-only statics; no BP/AS exposure by design):

- Arbiter: `Get_LastView`, `Get_{Promotes,Demotes,Preempts}ThisTick` ("flips STARTED by the last
  update"), `Get_IsFrozen`/`Request_SetFrozen`, `Get_NumCrowds`/`Get_CrowdPoolDebugInfo`.
- Runtime tuning: `Get_RuntimeTuners`, `Request_SetRuntimeTuners`, and
  `Request_ResetRuntimeTuners`. Inline controls cover arbiter decisions/fade anchors, each crowd's
  speed-driven animation and band boundaries, plus every active render band's renderer-profile
  flags, draw distances, bounds, light channels, and far-animation policy. Profile controls are
  index-bound to the crowd/band snapshot, never the selected member, so they remain editable when a
  band is empty or its crowd is lazy. Every edit sends the complete validated snapshot so same-frame
  commits accumulate and malformed values publish nothing.
  LOD scale and authored LOD-distance fields are deliberately not surfaced until CkIskm has a
  production consumer.
- Member: `Get_LastDistance`/`Get_LastInView` ("as of the last update that RANKED this member";
  -1 = never ranked), `Get_FadePhase`, `Get_PromotedViaLock`/`Get_PromotedUnbudgeted`/
  `Get_PreemptDemote`, `Get_{Proxy,Far}SequenceIndex`/`Get_{Proxy,Far}Rate`, and
  `Get_RenderBandIndex`.

## Semantics worth knowing before trusting the numbers

- **Freeze** holds *decisions*, not the world: no gather/rank/flips/far-anim; in-flight fades step
  to completion (reversal suppressed) and mid-fade members keep tracking their crowd slot; the
  fail-closed recovery still runs. Entities keep moving, so far members' transforms drift while
  frozen — expected. Frozen + unresolved view = the invalid-view whole-batch skip wins (nothing
  runs at all).
- **Per-tick counters are sampled** at the debugger's refresh rate, not accumulated per arbiter
  update — the sparklines are a trend, and the pane says so.
- **Distance/in-view are ranking-time values** — stale for members the arbiter skipped (suspended,
  hidden without a slot, exhaustion-fallback promotes) and for every member of a frozen or
  view-less arbiter.
- **The crossfade is a material contract** (dithered, complementary): the config's fade slots
  (crowd per-instance custom data + near-mesh custom primitive data) carry the same alpha, and the
  view pane's Fade row names both. Whether a material actually consumes its channel is invisible
  from C++ — a member that pops mid-fade means its material ignores the contract.

## Anti-patterns

1. Don't add a second selection path — roster click, viewport pick, incoming SelectionSync, and
   the entity-target route all funnel into one selected member (broadcast only the user-originated
   two; `ESelectInfo::Direct` never re-broadcasts).
2. Don't rebuild the body from `Tick` — the three signature-gated hosts are the only structural
   entry points. New live values are new TAttribute lambdas over `_Live`, not new rebuilds.
3. Markers are retained PMG shapes moved per tick — never one-frame `DrawDebug*` from the gated
   path, and the whole set clears on toggle-off, domain switch, world/session invalidation, EndPIE.
4. `Is_VisualLodPickCandidate` is THE ONE predicate (picker filter + entity-target route). Two
   predicates would be two answers to one question.
5. Don't write the config data asset or arbiter fragments from Slate. Every tuner commit is a
   public deferred request; Reset restores the authored snapshot without dirtying content. Use the
   window's optimistic complete-snapshot path, never a local per-control shadow value.
