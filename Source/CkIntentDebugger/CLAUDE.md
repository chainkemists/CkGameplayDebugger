# CkIntentDebugger — "why did nothing happen when I pressed X"

> **Read `CkDebuggerCommon/CLAUDE.md` first.** It covers the shared conventions this module obeys
> without restating them: copy-selectable text, the two `SListView` click-traps, row pointer
> identity, search bars, PIE world lifecycle, and the safety rules. This file covers only
> CkIntentDebugger's own architecture.

---

## What this module does

The editor debugger for CkFoundation's `CkIntent` and the `CkInput` layer stack underneath it. It
answers the question the whole intent stack makes possible to ask and impossible to answer without
tooling: **a player did a motion, nothing came out — where did it go?**

Four places can swallow it, and the module gives each one a view: a layer above consumed the key
(layer stack), the press is still being held open by a deferral (timeline), the direction the
sampler recorded was not the one the player felt (key/state), or the backward scan ran and missed
(near misses). It opens as a tab via **CK Intent Debugger**, the `ck.IntentDebugger` console
command, or the launcher's Interface group.

---

## The read-only contract — the one rule this module exists under

**Every value rendered here is one the sampler, the matcher or the router already computed and
STORED.** This module performs no matching, derives no octant, cleans no SOCD pair and evaluates no
deferral verdict.

That is not fastidiousness. A debugger that recomputed would share a bug with the thing it is
debugging, and the single most valuable signal the tool can produce — the recorded row disagreeing
with the behaviour on screen — would be invisible because both sides would be computing the same
wrong answer from the same inputs. The rosette makes the point concretely: it lights the row's own
`_Octant` and draws the conditioned axis pair as a separate dot, so a dot sitting inside one wedge
while a neighbour is lit **is** the hysteresis, rendered rather than re-derived.

**Reads are read-only, and they are reads of two kinds:**

- **Public Utils first.** Source discovery is `UCk_InputSource_Subsystem::Get_InputSource()` per
  local player — the subsystem owns the one source per player, so nothing guesses. The frame record
  is `UCk_Utils_IntentSampler_UE::{Get_FrameCount, Get_LatestFrame, TryGet_FrameAtOffset}`; the
  near-miss ring is `UCk_Utils_IntentMatcher_UE::{Get_ScanDiagnostics, Get_ScanDiagnosticsEnabled}`;
  layer captures are `UCk_Utils_InputLayer_UE::Get_Captures`; button↔key is
  `UCk_Utils_InputButtonMap_UE::Get_KeysForButton` — a Mapped button may resolve to more than one key
  (one per bound slot), so the resolution table and the held-button rows render every key the button
  carries, primary first, not just the first slot; the deferral verdict is
  `UCk_Utils_IntentGrammar_UE::Get_DeferralVerdict`.
- **Const fragment reads where no Util reaches**, matching the `CkAggroDebugger` /
  `CkGoapDebugger` precedent. Four gaps, all of them state CkIntent deliberately does not expose to
  gameplay: the layer stack has no enumeration Util (a registry view over
  `ck::FFragment_InputLayer_Params` filtered to the source, sorted by `Get_Priority` descending —
  priority is unique per source, so that is a total order); the active `FCk_Intent_CompiledSet`;
  `FIntentMatcher_PhaseRow::Get_PhaseFrame` (the CURRENT phase's span start, which
  `TryGet_CompletionFrame` gates away for `Pending` and `Failed`); and
  `FIntentMatcher_PendingEpisode` / `FIntentMatcher_HoldAccumulator` for blocked-by and charge
  readings.

All of it goes through `Data/CkIntentDebugger_DataCollector` and nowhere else. **Never mutate,
never add or remove a fragment, never call a `Request_*` from this module** — with ONE ruled
carve-out ([P11-D15]): the timeline's "history" field calls
`UCk_Utils_IntentDebugHistory_UE::Request_SetCapacity` on the source's IntentDebugHistory fragment.
That fragment is tooling state (compiled out in Shipping) whose retuning cannot perturb the
sampler, router or matcher being measured — which is the property the contract exists to protect.
Production CkIntent/CkInput state remains untouchable.

---

## Window anatomy

```
SCkIntentDebuggerWindow  (SCkDebug_WindowChrome)
├── toolbar    Input HUD toggle/settings · world selector · local-player source selector · refresh controls
├── LEFT       SCkIntentDebugger_LayerStackPanel — the selection surface
└── RIGHT
    ├── SCkIntentDebugger_TimelineDock   (shared SCkDebug_EventTimeline)
    └── SCkDebug_UnderlineTabs + switcher
        ├── SCkIntentDebugger_KeyStatePanel   (+ SCkIntentDebugger_OctantDial)
        ├── SCkIntentDebugger_ResolutionPanel
        └── SCkIntentDebugger_NearMissPanel
```

The **HUD settings** popover owns operational QA controls: Metadata/Frame notation (the same
per-user values previewed in Style Lab), overlay mode/scale/corner/opacity, and the shared project
history/fade/tap-hold/frame defaults. Palette, density, borders and semantic colors remain in
Style Lab. Both surfaces edit `UCk_InputHud_UserSettings` in the Runtime overlay module; the
runtime module never depends back on either DeveloperTool UI.

| View | Shows | Reads |
|---|---|---|
| **Timeline** | one lane per layer (frames it ended a routing walk on), one lane per intent of the selected set (spans per witnessed phase), and a BLOCKED lane of pending-episode markers | routed events on the record row; `_PhaseFrame`; pending episodes |
| **Layer stack** | stack top-down, each layer's declarative captures, and the matcher's active-set summary + registered terminal keys | `FFragment_InputLayer_Params` view; `Get_Captures`; matcher inspect Utils |
| **Key / state** | held set with each button's current key and press/release edge, conditioned axes, octant + rosette, SOCD-cleaned cardinals, the frame's routing shape | one `FCk_Intent_FrameRecord` |
| **Resolution table** | per terminal button: the candidates in bake order, the key it resolves to, and the deferral verdict | the active `FCk_Intent_CompiledSet` + `Get_DeferralVerdict` |
| **Near misses** | the scan-diagnostic ring newest-first, with per-step `Matched` / `NotSatisfied` / `WindowExhausted` / `ContiguityBroken` and frames examined | `Get_ScanDiagnostics` |

**The near-miss view is off by default and says so.** The ring is written only while
`ck.Intent.RecordScanDiagnostics` is on, so an empty list is ambiguous — the panel shows a banner
naming the CVar rather than letting silence read as "the matcher never scanned".

**The timeline axis is the sampler's logic-frame counter, not seconds.** A wall-clock axis would
disagree with the record on the first hitch, which is exactly when a reader needs them to agree.
Marker `SelectionId`s are frame indices, so a click scrubs directly to a frame.

---

## Entity targeting — "Open In ▸ CK Intent Debugger"

The module registers one `FCkDebug_EntityTargetRoute` after its tab spawner and unregisters it by
tab id + registration id before removing the spawner. Both halves of what the debugger shows are
entities, so **either** is a valid target: the input SOURCE (frame record, button map) or an input
LAYER (captures, matcher). Resolution is
`ck::DebugSelectionSync::Resolve_ClosestLineageMatch` over that predicate, so an exact, ancestor or
descendant selection lands — selecting a pawn that carries a layer targets that layer.

**The route really targets; it does not merely open the tab** (an open-only route is invalid — see
`CkDebuggerCommon/CLAUDE.md`). `SCkIntentDebuggerWindow::OpenForEntity` reduces the resolved handle
to **plain values** — the owning local player's index, and for a layer its priority — then hands
them to the window as a pending target. A layer target therefore selects both its owning source in
the toolbar and its own row in the layer-stack panel.

**The pending target is two `int32`s and never a handle.** A just-opened tab has no snapshot until
the first collector pass, so the target has to survive that gap; retaining an `FCk_Handle` across it
is precisely what makes a debugger crash at the next PIE start. It is cleared before the setters
run, because each setter broadcasts `OnChanged` straight back into the apply path.

---

## The witnessed-phase ring

The matcher keeps ONE phase per intent and the frame it was stamped on; there is no phase history
to read. The ViewModel therefore records the transitions it observes while the window is open —
each carrying the frame the runtime recorded, never a time this module invented — and closes the
previous span when a new one opens. Spans still open are drawn to the latest frame.

**Declared degradation:** transitions that happened before the window opened are absent by
construction. That is honest — the debugger is reporting what it saw, not reconstructing what it
did not. Do not "fix" this by replaying signals: `FireIfPayloadInFlight*` delivers only the LAST
payload, so a replayed history would be a fiction.

---

## Teardown — crash-grade, both contracts apply

- **Session invalidation / world change** — `FCkIntentDebugger_ViewModel` clears its whole snapshot
  synchronously through `ck::DebugSessionLifecycle::Get_OnSessionInvalidated()` and the world
  selector's `OnWorldChanged`. `CkDebuggerCommon` translates editor BeginPIE/EndPIE into that shared,
  package-safe lifecycle signal. Handles hold the registry by value; one outliving its world registry
  access-violates on destruct at the next session start.
- **`FCoreDelegates::OnEnginePreExit`** — `FCkIntentDebuggerModule::HandleEnginePreExit` drops the
  window and tab refs. NOT `ShutdownModule`: by then the registry's shared state is freed. No
  `RequestCloseTab` there either — the editor's window teardown runs first and the tab's weak-self
  can already be cleared.

**The snapshot is the only handle-bearing state in this module.** Every panel's row struct holds
plain values (`FName`, `FKey`, ints, enums) — keep it that way, or that panel joins the reset chain.

---

## Refresh discipline

- The window `Tick` is gated by `FCkDebuggerRefreshGate::Should_RefreshNow(WindowId)` and does one
  thing: `ViewModel->Tick()`. The ViewModel broadcasts `OnChanged`; the window fans out.
- The key/state panel is **fully attribute-bound** — it has no refresh method and never rebuilds.
  Its held-button rows are a fixed pool toggled by `Visibility`.
- List panels keep row `TSharedPtr` identity across refreshes (keyed by stack-node key / terminal
  label / ring ordinal) and call `RequestListRefresh` only when the row SET changes. Programmatic
  selection restores arrive as `ESelectInfo::Direct` and are ignored on the way back in.
- The timeline's lane labels are a construction argument, so a changed lane SET recreates that one
  widget — a context change, the only path on which rebuilding a tree is allowed. Data flows through
  `Set_Content`.

---

## Common gotchas

- **Row content is plain visual widgets only.** `SCkDebug_HistoryRow` and
  `SCkDebug_SelectableLabel` consume left-click and make `STableRow` unselectable — see
  `CkDebuggerCommon/CLAUDE.md` §"List / tree rows". Copy menus hang off
  `OnContextMenuOpening`, not off the row body.
- **Named, filename-derived namespaces** (`ck_intent_debugger_<file>`) — the module compiles unity
  and same-named anonymous-namespace helpers collide.
- **Weak captures are named `WeakPanel`, never `WeakThis`** — `WeakThis` shadows
  `TSharedFromThis::WeakThis` and C4458 is an error here.
- **An empty resolution table is not a bug.** A matcher with no active set captures nothing and
  answers `Idle` for everything; the layer-stack row says so in words.
- **A `<unbound>` terminal key is a player-reachable state**, not a defect — a mapped button whose
  key the player cleared in a settings screen. It is tinted as an error because a swap naming it
  would be rejected wholesale.
- **A key shared by several hold-graded resolutions renders the SMALLEST `HoldSiblingFrames`.**
  Key↔button is many-to-many, so the device snapshot's verdict map combines on collision with
  `FMath::Min` rather than overwriting — the cap fills to full at the earliest threshold any owning
  button grades on, which is the first frame the key's hold-ness matters to anyone. Never revert
  that to a plain `TMap::Add`: last-writer-wins makes the cap show whichever resolution iterated
  last, which is a decoration, not a verdict.

---

## See also

- `CkIntent/Claude.md` (in CkFoundation) — the record, the notation, the bake, the matcher, the
  scan-diagnostic ring. The contract this module renders.
- `CkInput/Claude.md` — the layer stack, declarative captures, delivery outcomes, the button space.
- `CkDebuggerCommon/CLAUDE.md` — shared widgets, row contracts, safety rules.
- `CkDebuggerLauncher/CLAUDE.md` — the descriptor census this module's tab id is enforced against.
