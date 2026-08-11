#pragma once

#include "CkIntentDebugger/Data/CkIntentDebugger_Types.h"

#include "CkDebuggerCommon/Devices/CkDebug_DeviceTypes.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkDebuggerModel_WorldSelector;
class UWorld;

// --------------------------------------------------------------------------------------------------------------------

DECLARE_MULTICAST_DELEGATE(FCkIntentDebugger_OnChanged);

// --------------------------------------------------------------------------------------------------------------------
// Owns the snapshot every panel reads, the source/layer selection, and the witnessed-phase ring the timeline draws
// its per-intent spans from.
//
// The ring exists because the matcher keeps ONE phase and the frame it was stamped on — there is no phase history to
// read. What the debugger can honestly show is the transitions it observed while open, each one carrying the frame
// the runtime recorded rather than a wall-clock time this module invented.
// --------------------------------------------------------------------------------------------------------------------

class CKINTENTDEBUGGER_API FCkIntentDebugger_ViewModel
{
public:
    FCkIntentDebugger_ViewModel();
    ~FCkIntentDebugger_ViewModel();
    FCkIntentDebugger_ViewModel(const FCkIntentDebugger_ViewModel&) = delete;
    auto operator=(const FCkIntentDebugger_ViewModel&) -> FCkIntentDebugger_ViewModel& = delete;
    FCkIntentDebugger_ViewModel(FCkIntentDebugger_ViewModel&&) = delete;
    auto operator=(FCkIntentDebugger_ViewModel&&) -> FCkIntentDebugger_ViewModel& = delete;

public:
    auto Tick() -> void;

    // Called every widget frame, OUTSIDE the refresh gate, because it captures EDGES: the router's per-pass event
    // array holds one pass's events, and a release read at a capped cadence is a release missed — which latches a
    // witnessed key down forever. Edge capture is truth, not presentation, so it cannot be rate-limited; it reads
    // two public values per source and stores plain values only.
    auto Tick_WitnessDeviceEdges() -> void;

    // Drops every handle-bearing row synchronously. Called from EndPIE and from the world-selector's own
    // teardown notification, both of which fire while the registry is still alive.
    auto Reset_ForWorldChange() -> void;

public:
    auto Get_WorldModel() const -> TSharedPtr<FCkDebuggerModel_WorldSelector> { return _WorldModel; }
    auto Get_Snapshot() const -> const FCkIntentDebugger_Snapshot& { return _Snapshot; }

    auto Get_SelectedSourceIndex() const -> int32 { return _SelectedSourceIndex; }
    auto Set_SelectedSourceIndex(int32 InIndex) -> void;

    auto Get_SelectedLayerPriority() const -> int32 { return _SelectedLayerPriority; }
    auto Set_SelectedLayerPriority(int32 InPriority) -> void;

    auto TryGet_SelectedSource() const -> const FCkIntentDebugger_SourceSnapshot*;
    auto TryGet_SelectedLayer() const -> const FCkIntentDebugger_LayerRow*;

    auto Get_PhaseEvents() const -> const TArray<FCkIntentDebugger_PhaseEvent>& { return _PhaseEvents; }

    // INDEX_NONE while live. Set by a timeline marker click; every panel that shows "the frame" reads it.
    auto Get_ScrubFrame() const -> int32 { return _ScrubFrame; }
    auto Set_ScrubFrame(int32 InFrame) -> void;

    auto Get_IsLive() const -> bool { return _ScrubFrame == INDEX_NONE; }

    // The record row the panels display: the scrubbed one while scrubbing, the newest one while live.
    auto TryGet_DisplayedFrame() const -> const FCkIntentDebugger_FrameRow*;

    // The selected source's devices, reduced to the device-widget contract (plain values, one frame clock).
    // One snapshot serves every desktop device — each widget looks up only its own keys. Rebuilt every Tick;
    // the Devices panel's paint reads it by pointer.
    auto Get_DeviceSnapshot() const -> const FCkDebug_DeviceSnapshot& { return _DeviceSnapshot; }

    // The one deliberate exception to this module's never-mutate contract: retuning the DEBUG history's
    // retention ([P11-D15]). It touches only the IntentDebugHistory recording — tooling state that cannot perturb
    // the sampler, router or matcher being measured. No-op when the selected source carries no history.
    auto Request_SetHistoryCapacity(int32 InFrames) -> void;

public:
    FCkIntentDebugger_OnChanged OnChanged;

private:
    auto DoRecord_PhaseEvents() -> void;
    auto DoRefresh_DeviceSnapshot() -> void;

private:
    TSharedPtr<FCkDebuggerModel_WorldSelector> _WorldModel;

    FCkIntentDebugger_Snapshot _Snapshot;

    int32 _SelectedSourceIndex = 0;
    int32 _SelectedLayerPriority = MIN_int32;
    int32 _ScrubFrame = INDEX_NONE;

    TArray<FCkIntentDebugger_PhaseEvent> _PhaseEvents;

    // (LayerPriority, IntentName) -> the (Phase, PhaseFrame) pair last witnessed, so a re-completion on a later
    // frame reads as a new event while an unchanged row does not.
    TMap<TPair<int32, FName>, TPair<ECk_Intent_Phase, int32>> _LastWitnessedPhase;

    // The still-open span per key, so a new transition can close its predecessor without a scan.
    TMap<TPair<int32, FName>, int32> _OpenEventIndexByKey;

    // The last edge witnessed per (source index, key) for keys the record carries no row for — same doctrine as
    // the phase ring: what the debugger SAW while open, stamped with the runtime's frame, never replayed history.
    // Fed by Tick_WitnessDeviceEdges (ungated), read by the gated snapshot rebuild.
    struct FWitnessedKeyEdge
    {
        int32 LastPressFrame = INDEX_NONE;
        int32 LastReleaseFrame = INDEX_NONE;
        bool IsDown = false;
    };

    TMap<TPair<int32, FKey>, FWitnessedKeyEdge> _WitnessedKeys;

    FCkDebug_DeviceSnapshot _DeviceSnapshot;

    FDelegateHandle _EndPieHandle;
    FDelegateHandle _SessionInvalidatedHandle;
    FDelegateHandle _WorldChangedHandle;
};

// --------------------------------------------------------------------------------------------------------------------
