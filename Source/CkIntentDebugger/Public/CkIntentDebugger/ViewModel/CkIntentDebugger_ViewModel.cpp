#include "CkIntentDebugger/ViewModel/CkIntentDebugger_ViewModel.h"

#include "CkIntentDebugger/Data/CkIntentDebugger_DataCollector.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"

#include "Editor.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_intent_debugger_viewmodel
{
    // The sampler's own ring is 120 rows at its default capacity; a debugger window that pulled more than it can
    // legibly draw would pay for rows nobody reads.
    constexpr auto MaxRecordedFrames = 240;

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

    _Snapshot = FCkIntentDebugger_DataCollector::Collect(
        _WorldModel->Get_SelectedWorld(), ck_intent_debugger_viewmodel::MaxRecordedFrames);

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
