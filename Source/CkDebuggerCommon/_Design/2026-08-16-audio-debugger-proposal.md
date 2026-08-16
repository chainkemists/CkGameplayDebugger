# CkAudioDebugger — proposal

| | |
|---|---|
| **Date** | 2026-08-16 |
| **Status** | Design only. No module exists. Mockups were rendered in-session; this file is the durable record. |
| **Feature under test** | `CkFoundation/Source/CkAudio` — `FCk_Handle_AudioDirector` holding a Record of `FCk_Handle_AudioTrack` |

## Why this one is cheap to build

**Every number it needs already exists.** No new instrumentation, no new fragment, no processor change:

| Source | What it gives |
|---|---|
| `ck::FFragment_AudioTrack_Current` | `_State` (Stopped/Playing/FadingIn/FadingOut/Paused), `_CurrentVolume`, `_TargetVolume`, `_FadeSpeed` (units/sec), `_PlaybackPercent`, `_IsVirtualized`, the weak `_AudioComponent` |
| `FCk_Fragment_AudioTrack_ParamsData` | `_Sound` (soft), `_Priority`, `_OverrideBehavior` (Interrupt/Crossfade/Queue), `_LoopBehavior`, `_DefaultFadeInTime`/`Out`, and the three library assets — attenuation, concurrency, sound class |
| `FCk_Fragment_AudioDirector_ParamsData` | `_MaxConcurrentTracks`, `_DefaultCrossfadeDuration`, `_SamePriorityBehavior` |
| Nine signals | `PlaybackStarted`, `PlaybackFinished`, `FadeCompleted`, `PlayStateChanged`, `VirtualizationChanged`, `PlaybackPercent`, `SingleEnvelope`, `MultiEnvelope`, `AudioFinished` |
| `ck::FFragment_AudioTrack_Debug` + 4 registered processors | `_StateColor`, `_CurrentPulseScale`, `_HUDSlotIndex`, and existing spatial/non-spatial debug draw |

## Pages

1. **Directors** — world-scoped list; tracks-active/max, concurrency pressure, same-priority behaviour. Selecting one scopes the rest of the window.
2. **Tracks (mixer)** — the headline. One row per live track: state pill, label, sound asset, a volume lane showing **current filling toward a target marker**, fade rate and time-to-target, playback percent, priority/loop/override chips, and a virtualization badge.
3. **Crossfade** — a rolling `SCkDebug_Sparkline` of `_CurrentVolume` per track over the last N seconds, so a real crossfade reads as two crossing curves. Not visible anywhere today.
4. **Spatial** — see below.
5. **Events** — `SCkDebug_EventLog` fed by the nine signals, filterable by track and kind. `PlaybackPercent` and the envelope signals default OFF; they fire every frame.
6. **Overlay** — wires the four existing debug-draw processors (`FTag_AudioTrack_DebugDraw`) into the window instead of console-only.

## The Spatial page — the one that answers "why is it that loud?"

Requested 2026-08-16. **The mixer shows `_CurrentVolume` but never why it holds that value.** For a spatial track the
answer is the attenuation curve evaluated at the current listener distance, and today that only exists as debug-draw
in the world (`FProcessor_AudioTrack_DebugDraw_Individual_Spatial`) — visible only if you are looking at the right
place in the viewport while it happens.

Per selected track:

- **Top-down radar**, listener at centre with a facing arrow, the track plotted by bearing and distance, its **inner
  and falloff radii as rings**. Reading "is it inside falloff" off a picture is instant; reading it off two numbers is
  arithmetic.
- **Falloff curve** with the current distance marked on it, so the gain is shown as a POSITION on the curve rather
  than asserted as a number.
- **The multiplication written out**: attenuation gain × track volume = audible. A track at volume 1.0 that nobody can
  hear, and a track at volume 0.25 that is perfectly audible, must both be explainable from this panel alone.
- **Out-of-range and virtualized tracks are called out**, not merely plotted. Those are the two states where "Playing"
  is true and the sound is inaudible — the single most common audio bug and the one the mixer page cannot show.

Reference note: the request cited "what the old car debugger did". **No car or vehicle debugger exists in this repo**
(`SCkDebug_OrientationCube` is the only spatial-widget precedent found), so the above is derived from what `CkAudio`
actually exposes. If that reference had something this lacks, this section is the thing to correct.

## Contracts that differ from CkOptimizationDebugger

The optimization debugger's no-live-handle invariant does **not** apply here, and copying it would be wrong:

- **It IS a live-handle debugger.** It needs the `FEditorDelegates::EndPIE` handle clear and the
  `FCoreDelegates::OnEnginePreExit` Slate teardown that handle-holding debuggers require, and `SCkDebug_EntityRef` is
  **legal** here — a track row names a live entity.
- **Refresh must be gated.** `_CurrentVolume` changes every frame; bind through `SCkDebug_RefreshControls` /
  `ck::DebugRequestGate` with a live-vs-paused pill, never a raw per-frame read.
- **Rows are keyed by track ENTITY**, not by index — a director's Record reorders.
- **Read-only by default.** Solo/Mute is the one write worth having, and it must route through
  `UCk_Utils_AudioTrack_UE::Request_SetVolume`, never `UAudioComponent` directly (CkAudio anti-pattern #1). Solo
  remembers each track's `_TargetVolume`, zeroes the others with a short fade, and restores on un-solo — and that
  remembered state must be dropped at a PIE boundary, because the entities it names will not exist.
- **Virtualization is a first-class state, never a footnote.** A track reporting `Playing` at 0.8 that is virtualized
  is inaudible.

## Launcher placement

`ECkDebuggerToolCategory::Systems`, with its own tab id added to
`CkDebuggerLauncher/Private/Tests/CkDebuggerLauncherCatalog.spec.cpp` in the same change — that spec asserts the exact
tool census and a unique category/order slot.
