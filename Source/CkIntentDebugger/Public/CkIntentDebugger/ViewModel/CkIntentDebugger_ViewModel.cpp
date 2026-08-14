#include "CkIntentDebugger/ViewModel/CkIntentDebugger_ViewModel.h"

#include "CkIntentDebugger/Data/CkIntentDebugger_DataCollector.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"

#include "CkInput/CkInputLayer_Utils.h"
#include "CkInput/Subsystem/CkInputSource_Subsystem.h"

#include "CkIntent/Debug/CkIntentDebugHistory_Utils.h"
#include "CkIntent/CkIntentSampler_Utils.h"

#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_intent_debugger_viewmodel
{

    // Bounded for the same reason the matcher's diagnostic ring is: this is a list a human scrolls after feeling a
    // move not come out, not a trace.
    constexpr auto MaxPhaseEvents = 512;
}

// --------------------------------------------------------------------------------------------------------------------

FCkIntentDebugger_ViewModel::
    FCkIntentDebugger_ViewModel()
{
    _WorldModel = MakeShared<FCkDebuggerModel_WorldSelector>();

    _WorldChangedHandle = _WorldModel->OnWorldChanged.AddLambda([this](UWorld*)
    {
        Reset_ForWorldChange();
    });

    _SessionInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddRaw(
        this, &FCkIntentDebugger_ViewModel::Reset_ForWorldChange);

    if (GEditor != nullptr)
    {
        _EndPieHandle = FEditorDelegates::EndPIE.AddLambda([this](const bool)
        {
            Reset_ForWorldChange();
        });
    }
}

FCkIntentDebugger_ViewModel::
    ~FCkIntentDebugger_ViewModel()
{
    if (_EndPieHandle.IsValid())
    {
        FEditorDelegates::EndPIE.Remove(_EndPieHandle);
        _EndPieHandle.Reset();
    }

    if (_SessionInvalidatedHandle.IsValid())
    {
        ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(_SessionInvalidatedHandle);
        _SessionInvalidatedHandle.Reset();
    }

    if (_WorldModel.IsValid() && _WorldChangedHandle.IsValid())
    {
        _WorldModel->OnWorldChanged.Remove(_WorldChangedHandle);
        _WorldChangedHandle.Reset();
    }

    _Snapshot = {};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkIntentDebugger_ViewModel::
    Tick()
    -> void
{
    _WorldModel->Ensure_AutoSelect();

    _Snapshot = FCkIntentDebugger_DataCollector::Collect(_WorldModel->Get_SelectedWorld());

    if (NOT _Snapshot.Sources.IsValidIndex(_SelectedSourceIndex))
    { _SelectedSourceIndex = 0; }

    if (TryGet_SelectedLayer() == nullptr)
    {
        const auto* Source = TryGet_SelectedSource();
        _SelectedLayerPriority = Source != nullptr && NOT Source->Layers.IsEmpty()
            ? Source->Layers[0].Priority
            : MIN_int32;
    }

    DoRecord_PhaseEvents();
    DoRefresh_DeviceSnapshot();

    OnChanged.Broadcast();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkIntentDebugger_ViewModel::
    Reset_ForWorldChange()
    -> void
{
    _Snapshot = {};
    _PhaseEvents.Reset();
    _LastWitnessedPhase.Reset();
    _OpenEventIndexByKey.Reset();
    _WitnessedKeys.Reset();
    _DeviceSnapshot = {};
    _ScrubFrame = INDEX_NONE;
    _SelectedSourceIndex = 0;
    _SelectedLayerPriority = MIN_int32;

    OnChanged.Broadcast();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkIntentDebugger_ViewModel::
    Set_SelectedSourceIndex(
        int32 InIndex)
    -> void
{
    if (_SelectedSourceIndex == InIndex)
    { return; }

    _SelectedSourceIndex = InIndex;
    _SelectedLayerPriority = MIN_int32;

    OnChanged.Broadcast();
}

auto
    FCkIntentDebugger_ViewModel::
    Set_SelectedLayerPriority(
        int32 InPriority)
    -> void
{
    if (_SelectedLayerPriority == InPriority)
    { return; }

    _SelectedLayerPriority = InPriority;

    OnChanged.Broadcast();
}

auto
    FCkIntentDebugger_ViewModel::
    Set_ScrubFrame(
        int32 InFrame)
    -> void
{
    if (_ScrubFrame == InFrame)
    { return; }

    _ScrubFrame = InFrame;

    OnChanged.Broadcast();
}

auto
    FCkIntentDebugger_ViewModel::
    Request_SetHistoryCapacity(
        int32 InFrames)
    -> void
{
    const auto* Source = TryGet_SelectedSource();
    if (Source == nullptr || NOT Source->HasHistory)
    { return; }

    auto History = Source->History;
    if (ck::Is_NOT_Valid(History))
    { return; }

    UCk_Utils_IntentDebugHistory_UE::Request_SetCapacity(History,
        FCk_Request_IntentDebugHistory_SetCapacity{FMath::Max(InFrames, 120)}, {});

    OnChanged.Broadcast();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkIntentDebugger_ViewModel::
    TryGet_SelectedSource() const
    -> const FCkIntentDebugger_SourceSnapshot*
{
    return _Snapshot.TryGet_Source(_SelectedSourceIndex);
}

auto
    FCkIntentDebugger_ViewModel::
    TryGet_SelectedLayer() const
    -> const FCkIntentDebugger_LayerRow*
{
    const auto* Source = TryGet_SelectedSource();
    if (Source == nullptr)
    { return nullptr; }

    for (const auto& Layer : Source->Layers)
    {
        if (Layer.Priority == _SelectedLayerPriority)
        { return &Layer; }
    }

    return nullptr;
}

auto
    FCkIntentDebugger_ViewModel::
    TryGet_DisplayedFrame() const
    -> const FCkIntentDebugger_FrameRow*
{
    const auto* Source = TryGet_SelectedSource();
    if (Source == nullptr || Source->Frames.IsEmpty())
    { return nullptr; }

    if (_ScrubFrame == INDEX_NONE)
    { return &Source->Frames.Last(); }

    for (auto Index = Source->Frames.Num() - 1; Index >= 0; --Index)
    {
        if (Source->Frames[Index].FrameIndex <= _ScrubFrame)
        { return &Source->Frames[Index]; }
    }

    return &Source->Frames[0];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkIntentDebugger_ViewModel::
    DoRecord_PhaseEvents()
    -> void
{
    const auto* Source = TryGet_SelectedSource();
    if (Source == nullptr)
    { return; }

    const auto LatestFrame = Source->Frames.IsEmpty() ? INDEX_NONE : Source->Frames.Last().FrameIndex;

    for (const auto& Layer : Source->Layers)
    {
        for (const auto& Intent : Layer.Intents)
        {
            const auto Key = TPair<int32, FName>{Layer.Priority, Intent.Name};
            const auto Witnessed = TPair<ECk_Intent_Phase, int32>{Intent.Phase, Intent.PhaseFrame};

            const auto* Last = _LastWitnessedPhase.Find(Key);
            if (Last != nullptr && *Last == Witnessed)
            { continue; }

            _LastWitnessedPhase.Add(Key, Witnessed);

            const auto StartFrame = Intent.PhaseFrame >= 0 ? Intent.PhaseFrame : LatestFrame;

            // Closing the previous span here rather than extending every open span each tick is what keeps this
            // O(transitions) instead of O(intents × ticks) — and an open span's end is the latest frame by
            // definition, which the timeline can answer without storing it.
            if (const auto* OpenIndex = _OpenEventIndexByKey.Find(Key);
                OpenIndex != nullptr && _PhaseEvents.IsValidIndex(*OpenIndex))
            { _PhaseEvents[*OpenIndex].EndFrame = StartFrame; }

            auto Event = FCkIntentDebugger_PhaseEvent{};
            Event.IntentName = Intent.Name;
            Event.Phase = Intent.Phase;
            Event.StartFrame = StartFrame;
            Event.LayerPriority = Layer.Priority;

            _OpenEventIndexByKey.Add(Key, _PhaseEvents.Add(MoveTemp(Event)));
        }
    }

    const auto Overflow = _PhaseEvents.Num() - ck_intent_debugger_viewmodel::MaxPhaseEvents;
    if (Overflow <= 0)
    { return; }

    _PhaseEvents.RemoveAt(0, Overflow, EAllowShrinking::No);

    for (auto It = _OpenEventIndexByKey.CreateIterator(); It; ++It)
    {
        It.Value() -= Overflow;

        if (It.Value() < 0)
        { It.RemoveCurrent(); }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkIntentDebugger_ViewModel::
    DoRefresh_DeviceSnapshot()
    -> void
{
    _DeviceSnapshot = {};

    const auto* Source = TryGet_SelectedSource();
    if (Source == nullptr)
    { return; }

    const auto LiveFrame = NOT Source->Frames.IsEmpty() ? Source->Frames.Last().FrameIndex : INDEX_NONE;

    // Scrubbing moves the DEVICES too: the snapshot is built as-of the displayed frame, so the caps light,
    // fill and flash exactly as they did on that frame — the whole window tells one frame's story.
    const auto IsLive = _ScrubFrame == INDEX_NONE;
    const auto DisplayFrame = IsLive || LiveFrame == INDEX_NONE
        ? LiveFrame
        : FMath::Min(_ScrubFrame, LiveFrame);
    _DeviceSnapshot.LiveFrame = DisplayFrame;

    // The hold-verdict threshold per resolved key, off the selected layer's bake — this is what turns a cap's
    // fill into the matcher's own verdict point rather than a decoration.
    //
    // Key↔button is many-to-many by design (a button carries every key its slots bind, and one key can be bound
    // by several buttons), so two hold-graded resolutions can name the SAME key with different thresholds. A
    // plain Add would last-writer-win on that collision and the cap would silently show whichever resolution
    // happened to iterate last. Combine with MIN instead: the fill saturates at the earliest threshold any
    // owning button grades on, which is the first frame the key's hold-ness means anything to anyone. Max would
    // leave a cap reading "still filling" past the point one of its owners has already called the press a hold.
    auto VerdictByKey = TMap<FKey, int32>{};

    if (const auto* Layer = TryGet_SelectedLayer())
    {
        for (const auto& Resolution : Layer->Resolutions)
        {
            if (Resolution.HoldSiblingFrames <= 0)
            { continue; }

            for (const auto& Key : Resolution.ResolvedKeys)
            {
                if (NOT Key.IsValid())
                { continue; }

                if (auto* Existing = VerdictByKey.Find(Key))
                { *Existing = FMath::Min(*Existing, Resolution.HoldSiblingFrames); }
                else
                { VerdictByKey.Add(Key, Resolution.HoldSiblingFrames); }
            }
        }
    }

    // Minted keys read the record ring — exact at any refresh cadence, because the ring is the module's memory.
    for (const auto& Key : Source->MintedKeys)
    {
        auto State = FCkDebug_DeviceKeyState{};
        State.IsMinted = true;
        State.IsActionable = true;

        if (const auto* Verdict = VerdictByKey.Find(Key))
        { State.HoldVerdictFrames = *Verdict; }

        auto CountingRun = true;

        for (auto Index = Source->Frames.Num() - 1; Index >= 0; --Index)
        {
            const auto& Row = Source->Frames[Index];

            if (Row.FrameIndex > DisplayFrame)
            { continue; }

            const auto* Button = Row.Held.FindByPredicate(
                [&Key](const FCkIntentDebugger_ButtonState& InButton) { return InButton.Keys.Contains(Key); });

            if (CountingRun)
            {
                if (Button != nullptr && NOT Button->WentUp)
                { State.HeldRunFrames++; }
                else
                { CountingRun = false; }
            }

            if (State.LatestPressFrame == INDEX_NONE && Button != nullptr && Button->WentDown)
            { State.LatestPressFrame = Row.FrameIndex; }

            if (State.LatestReleaseFrame == INDEX_NONE && Button != nullptr && Button->WentUp)
            { State.LatestReleaseFrame = Row.FrameIndex; }

            if (NOT CountingRun && State.LatestPressFrame != INDEX_NONE && State.LatestReleaseFrame != INDEX_NONE)
            { break; }
        }

        _DeviceSnapshot.Keys.Add(Key, State);
    }

    // Unminted keys render the witnessed edges — captured every widget frame by Tick_WitnessDeviceEdges, so a
    // release cannot land unseen and latch a key down (the slice-11-1 stuck-key defect).
    for (const auto& Pair : _WitnessedKeys)
    {
        if (Pair.Key.Get<0>() != _SelectedSourceIndex)
        { continue; }

        const auto& Key = Pair.Key.Get<1>();

        if (_DeviceSnapshot.Keys.Contains(Key))
        { continue; }

        auto State = FCkDebug_DeviceKeyState{};

        if (IsLive)
        {
            State.LatestPressFrame = Pair.Value.LastPressFrame;
            State.LatestReleaseFrame = Pair.Value.LastReleaseFrame;

            State.HeldRunFrames =
                Pair.Value.IsDown && LiveFrame != INDEX_NONE && Pair.Value.LastPressFrame != INDEX_NONE
                    ? FMath::Max(1, LiveFrame - Pair.Value.LastPressFrame)
                    : 0;
        }
        else
        {
            // A witnessed key carries only its LATEST edge pair — no ring — so scrubbed rendering is
            // best-effort: shown only when that pair overlaps the scrubbed frame. Minted keys are the
            // exact-history tier; this is the declared difference between the tiers.
            if (Pair.Value.LastPressFrame == INDEX_NONE || Pair.Value.LastPressFrame > DisplayFrame)
            { continue; }

            const auto ReleasedByThen =
                Pair.Value.LastReleaseFrame != INDEX_NONE &&
                Pair.Value.LastReleaseFrame >= Pair.Value.LastPressFrame &&
                Pair.Value.LastReleaseFrame <= DisplayFrame;

            State.LatestPressFrame = Pair.Value.LastPressFrame;
            State.LatestReleaseFrame = ReleasedByThen ? Pair.Value.LastReleaseFrame : int32{INDEX_NONE};
            State.HeldRunFrames = ReleasedByThen ? 0 : FMath::Max(1, DisplayFrame - Pair.Value.LastPressFrame);
        }

        _DeviceSnapshot.Keys.Add(Key, State);
    }

    // Every key ANY layer of this source listens to gets the actionable rim — including keys never pressed, which
    // need an entry minted here just to carry the flag. "The game's live input surface", not "keys with history".
    for (const auto& Layer : Source->Layers)
    {
        auto MarkActionable = [this](const FKey& InKey)
        {
            if (NOT InKey.IsValid())
            { return; }

            _DeviceSnapshot.Keys.FindOrAdd(InKey).IsActionable = true;
        };

        for (const auto& Capture : Layer.Captures)
        {
            if (Capture.MatchMode == ECk_InputLayer_CaptureMatch::Key)
            { MarkActionable(Capture.Key); }
        }

        for (const auto& Key : Layer.RegisteredCaptureKeys)
        { MarkActionable(Key); }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkIntentDebugger_ViewModel::
    Tick_WitnessDeviceEdges()
    -> void
{
    auto* World = _WorldModel.IsValid() ? _WorldModel->Get_SelectedWorld() : nullptr;

    if (ck::Is_NOT_Valid(World) || NOT World->HasBegunPlay())
    { return; }

    auto* GameInstance = World->GetGameInstance();

    if (GameInstance == nullptr)
    { return; }

    // Source indices here must match the collector's enumeration — both walk the local-player list with the same
    // skip conditions, which is what keys the witnessed map and the snapshot to the same source.
    auto SourceIndex = 0;

    for (auto* LocalPlayer : GameInstance->GetLocalPlayers())
    {
        if (LocalPlayer == nullptr)
        { continue; }

        auto* SourceSubsystem = LocalPlayer->GetSubsystem<UCk_InputSource_Subsystem>();

        if (SourceSubsystem == nullptr)
        { continue; }

        const auto Source = SourceSubsystem->Get_InputSource();

        if (ck::Is_NOT_Valid(Source))
        { continue; }

        const auto RoutedEvents = UCk_Utils_InputLayer_UE::Get_RoutedEventsThisFrame(Source);

        if (NOT RoutedEvents.IsEmpty())
        {
            auto SourceHandle = Source.ConvertToHandle();
            auto LiveFrame = int32{INDEX_NONE};

            const auto Sampler = UCk_Utils_IntentSampler_UE::Cast(SourceHandle);

            if (ck::IsValid(Sampler) && UCk_Utils_IntentSampler_UE::Get_FrameCount(Sampler) > 0)
            { LiveFrame = UCk_Utils_IntentSampler_UE::Get_LatestFrame(Sampler).Get_FrameIndex(); }

            for (const auto& Routed : RoutedEvents)
            {
                const auto& Event = Routed.Get_Event();
                const auto EventType = Event.Get_EventType();

                if (EventType == ECk_InputSource_EventType::AnalogAxis)
                { continue; }

                auto& Edge = _WitnessedKeys.FindOrAdd(MakeTuple(SourceIndex, Event.Get_Key()));

                if (EventType == ECk_InputSource_EventType::Pressed)
                {
                    Edge.LastPressFrame = LiveFrame;
                    Edge.IsDown = true;
                }
                else
                {
                    Edge.LastReleaseFrame = LiveFrame;
                    Edge.IsDown = false;
                }
            }
        }

        SourceIndex++;
    }
}

// --------------------------------------------------------------------------------------------------------------------
