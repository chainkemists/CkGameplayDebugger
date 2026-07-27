# CkAggroDebugger

**Purpose:** UncookedOnly Slate debugger for the `CkAggro` system. A read-only inspector listing every Aggro owner in
the world with its full threat table rendered as live meters — threat, selection score, perception state, effective
decay rate, and time-to-forget. Opened via the `ck.AggroDebugger` console command or the shared debugger launcher
(**AI** category, slot 50).

**Depends on:** `CkCore`, `CkEcs`, `CkRecord`, `CkEcsExt`, `CkAggro`, `CkDebuggerCommon`, `CkEditorTools`
(+ Slate/UnrealEd editor modules). Mirrors `CkDialogDebugger`'s module registration.

---

## Anatomy

- `CkAggroDebugger_Module.h/.cpp` — registers the nomad tab + `ck.AggroDebugger` console command + the shared
  `FCkDebuggerToolRegistry` entry.
- `Data/CkAggroDebugger_Types.h` — POD view structs (`FCkAggroDebugger_{TargetInfo,OwnerInfo,Snapshot}`) plus the two
  derived figures that are not readable from any single stored field: `Get_EffectiveDecayRate` (the perceived
  multiplier folded in) and `Get_SecondsToForget`.
- `Data/CkAggroDebugger_DataCollector.h/.cpp` — rebuilds a snapshot each refresh:
  `TransientEntity.View<FFragment_Aggro_Current>()`, then `UCk_Utils_Aggro_UE::ForEach_Target` per owner. Public utils
  where they exist; fragments read directly only for pure debug state (the `PendingForget` / `CannotBecomeActive` /
  `CannotBeForgotten` tags and `FFragment_AggroTarget_Perception`, which have no public getter).
- `Window/SCkAggroDebuggerWindow.h/.cpp` — `SCkDebugger_WindowBase` subclass with the structure/values split
  (see below).

---

## Two things that decide how this window reads

**Bars are normalised per-owner, not absolutely.** The threat clamp defaults to 10000, so an absolute scale renders
every real fight as a flat row of empty bars. Each bar is scaled against that owner's strongest target, which makes
the top contender read full and every other bar a direct visual ratio — exactly the comparison selection performs.

**The switch bar is printed, not drawn.** A challenger must beat `incumbent score x CurrentTargetBias x
SwitchThreshold`. That number lives in the owner's meta line rather than as a marker on the meters, because
`SCkDebug_MeterBar` has no marker affordance and inventing one here would fork the shared widget.

---

## Rendering policy — structure vs values

The refresh gate defaults to Unlimited, so `Tick` can run **every frame**. Rebuilding the row subtree that often tears
down and recreates live widgets mid-layout, which reads on screen as violent flicker and text drawn over itself.

So: `DoBuild_Signature` covers the owner/target **identity set only** (deliberately excluding threat/score/distance,
which change every frame and would defeat the gate entirely). Structure is rebuilt only when that signature changes;
`DoUpdate_LiveValues` writes shared cells that the meters read through their attribute bindings on the next paint, so
a value refresh never touches the widget tree. `_TargetSlots` / `_OwnerSlots` are flat and parallel to the rows the
structure pass emitted, walked in the same order — the signature is what guarantees that correspondence.

This is the lesson `SCkDialogDebuggerWindow` records; it applies identically here.

---

## Lifetime / safety

- `SCkDebugger_WindowBase` — `Register_WithGate()` in `Construct`; the base auto-unregisters in its destructor.
- All world access is read-only; the collector never mutates ECS state.
- The collector holds no long-lived handles across PIE sessions — each `Collect` starts from a fresh
  `FCkAggroDebugger_Snapshot{}`.
- **Aggro is authority-only.** On a client there are genuinely no owners to show, so the window says so explicitly
  rather than rendering an empty list that reads as a broken window.

---

## Seeing it populated

The **BB NpcCombat gym** (`Plugins/BusterBlockTests/Script/Gyms/NpcCombat/`) drives a real NPC's threat table through
the production damage path and is the fastest way to get live data into this window.
