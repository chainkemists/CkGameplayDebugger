# CkAudioDebugger — proposal

| | |
|---|---|
| **Date** | 2026-08-16 |
| **Status** | Implemented — `CkAudioDebugger`. All six pages are built. |
| **Feature under test** | `CkFoundation/Source/CkAudio` — `FCk_Handle_AudioDirector` holding a Record of `FCk_Handle_AudioTrack` |

## The mockups ARE the spec — open them before changing the window

[`2026-08-16-audio-debugger-mockups/tracks-page.html`](2026-08-16-audio-debugger-mockups/tracks-page.html)
and [`spatial-panel.html`](2026-08-16-audio-debugger-mockups/spatial-panel.html). Open them in a browser; the tracks
page animates a live crossfade.

They live here because the first revision of this file recorded only that they had been "rendered in-session", and the
implementation then drifted from them with nothing to diff against — a one-line row with no pills, no chips, no target
marker and no lane, which read as a different design rather than as an incomplete one. Prose cannot hold a layout. If
a future change means to depart from the picture, change the picture in the same commit.

Slate mapping, so the next reader does not re-derive it:

| Mockup element | Widget |
|---|---|
| Tab strip with count badges | `SCkDebug_UnderlineTabs` (`FCkDebug_UnderlineTabDesc::CountText` / `ShowWarnDot`) |
| The four stat cards | `SCkDebug_Card` |
| State pill (`Playing` / `Fading out`) | `SCkDebug_StatusPill`, tone-bound |
| Filter state pills (clickable) | `SCkDebug_ToggleSurface` wrapping a `SCkDebug_StatusPill` |
| `p60` / `Loop` / `Crossfade` / `Virtualized` | `SCkDebug_Chip` (`Unsatisfied` kind for virtualized) |
| Volume lane, fill + target marker | `SCkDebug_MeterBar` — `TargetFraction` was ADDED to the shared widget for this |
| Crossfade lane, two crossing curves | `SCkDebug_Sparkline` (`Samples` + `BandSamples` in ONE widget) |
| `e:4127 v:3` on the director header | `SCkDebug_EntityRef` |
| Virtualized third line | `SCkDebug_Icon` on `ck::debug_axes::Get_ToneIconId(ECk_Tone::Err)` |
| Title bar, `PIE · Client 0`, `Live · 10 hz`, refresh | `SCkDebug_WindowChrome` — `StatusText` + `ShowRefreshControls` |
| Spatial radar / falloff curve | `SCkAudioDebugger_Radar` / `SCkAudioDebugger_FalloffCurve` (local, see below) |
| Events log | `SCkDebug_EventLog`, fed by a snapshot diff (see below) |

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

### As built

Two bespoke leaf widgets, `SCkAudioDebugger_Radar` and `SCkAudioDebugger_FalloffCurve`, kept LOCAL to this module.
An observer-relative radar would plausibly serve perception/aggro/spatial-query as well, but it has one consumer
today — promote it on the second, rather than guessing now at the shared shape.

The load-bearing decision: **every attenuation number comes from the engine's own
`FBaseAttenuationSettings::Evaluate`**, both the marked point and the swept curve, and the settings are read off the
live `UAudioComponent` rather than off the track's library asset. That matters twice over — a SoundCue with
`bOverrideAttenuation` beats the library asset and the Setup processor writes a synthetic default when neither
exists, so the component is the only place the *effective* attenuation exists; and a debugger that reimplemented the
falloff maths would eventually disagree with the mixer while being believed over it.

The listener is the audio device's own (`FAudioDevice::GetListenerTransform`), falling back to the
`PlayerCameraManager`. Which one answered is printed in the page footer: the two coincide in most projects and
diverge in exactly the ones where a distance is worth debugging.

## Events and Overlay, as built

**Events is derived by diffing consecutive snapshots, NOT by binding the nine signals.** The deciding fact is that
**no debugger in this suite binds `CK_SIGNAL_BIND`** — all ~20 are snapshot/poll based, and this one would have been
the first Slate widget holding delegates into live PIE entities, which is the lifetime hazard the contract section
above already flags. The cost is real and is stated on the page itself rather than hidden here: a transition that
begins and ends between two ticks of the refresh gate is not recorded, and timestamps are gate-quantised. If exact
per-fire history is ever needed, that is the moment to design the binding lifecycle properly — not to bolt it on.

The diff reports state changes, fade completions, virtualization flips, and track add/remove. The first pass after
the window opens records a baseline and reports nothing, so the log never opens with a fabricated burst of "started"
lines for tracks that were simply already there.

**Overlay is the one page that writes**, and it says so on itself. It drives
`UCk_Utils_AudioTrack_UE::Request_Enable/DisableDebugDraw` — never `FTag_AudioTrack_DebugDraw` directly, which is
CkAudio's internal gate and not a debugger's to set.

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
