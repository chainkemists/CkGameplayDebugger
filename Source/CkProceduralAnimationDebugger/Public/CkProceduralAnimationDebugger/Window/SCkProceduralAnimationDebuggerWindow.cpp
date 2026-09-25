#include "CkProceduralAnimationDebugger/Window/SCkProceduralAnimationDebuggerWindow.h"

#include "CkProceduralAnimationDebugger/CkProceduralAnimationDebugger_Module.h"
#include "CkProceduralAnimationDebugger/Viewport/CkProceduralAnimationDebugger_Preview.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"
#include "CkDebuggerCommon/Picker/CkDebug_ViewportPicker.h"
#include "CkDebuggerCommon/Picker/SCkDebug_ViewportPickerControls.h"
#include "CkDebuggerCommon/Search/SCkDebug_SearchBar.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityHealthList.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EvidenceList.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_PaneHost.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Sparkline.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"

#include <Algo/BinarySearch.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/Layout/SBox.h>
#include <Widgets/Layout/SSplitter.h>
#include <Widgets/SBoxPanel.h>
#include <Widgets/SNullWidget.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_procedural_debug_window
{
    auto
        Get_LegState(
            const FCk_ProceduralAnimation_DebugLeg& InLeg)
        -> FString
    {
        const auto Contact = InLeg.Get_ContactTrusted() ? TEXT("contact")
            : InLeg.Get_ProbeState() == ck::EProceduralFootProbeState::Guessing ? TEXT("grace")
            : TEXT("lost");
        return ck::Format_UE(TEXT("{} / {}"), InLeg.Get_Planted() ? TEXT("planted") : TEXT("swing"), Contact);
    }

    auto
        Get_RigState(
            const FCk_ProceduralAnimation_DebugLeg& InLeg)
        -> FString
    {
        if (NOT InLeg.Get_HasRig())
        { return TEXT("no rig"); }

        if (InLeg.Get_RigFailure() != ECk_ProceduralRig_Failure::None)
        { return ck::Format_UE(TEXT("failed: {}"), InLeg.Get_RigFailure()); }

        return InLeg.Get_RigReady() ? TEXT("ready") : TEXT("waiting for gait");
    }

    auto
        Get_SequenceSelectionId(
            uint64 InSequence)
        -> int32
    {
        return static_cast<int32>(FMath::Min<uint64>(InSequence, static_cast<uint64>(MAX_int32)));
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    Construct(
        const FArguments&)
    -> void
{
    Register_WithGate();
    _Model = MakeShared<FCkProceduralAnimationDebugger_Model>();
    _WorldModel = MakeShared<FCkDebuggerModel_WorldSelector>();
    _SpeedSamples = MakeShared<TArray<float>>();
    _SupportSamples = MakeShared<TArray<float>>();
    _Preview = MakeShared<FCkProceduralAnimationDebugger_Preview>();
    const auto WeakWindow = TWeakPtr<SCkProceduralAnimationDebuggerWindow>{SharedThis(this)};
    const auto WeakModel = TWeakPtr<FCkProceduralAnimationDebugger_Model>{_Model};

    _Picker = MakeShared<FCkDebug_ViewportPicker>();
    _Picker->Construct({
        [WeakModel]() -> UWorld*
        {
            const auto Model = WeakModel.Pin();
            return Model.IsValid() ? Model->Get_World() : nullptr;
        },
        [WeakModel](const FCk_Handle& InEntity)
        {
            if (const auto Model = WeakModel.Pin())
            { Model->Select(InEntity, ECkProceduralAnimationDebugger_SelectionSync::Broadcast); }
        },
        [](const FCk_Handle& InEntity)
        {
            return Is_ProceduralEntity(InEntity);
        }});

    _Roster = SNew(SCkDebug_EntityHealthList)
        .SelectedEntity_Lambda([WeakModel]()
        {
            const auto Model = WeakModel.Pin();
            return Model.IsValid() ? Model->Get_SelectedHandle() : FCk_Handle{};
        })
        .OnSelected_Lambda([WeakModel](const FCk_Handle& InEntity)
        {
            if (const auto Model = WeakModel.Pin())
            { Model->Select(InEntity, ECkProceduralAnimationDebugger_SelectionSync::Broadcast); }
        });

    _Legs = SNew(SCkDebug_EvidenceList).MaxItems(64)
        .EmptyText(FText::FromString(TEXT("Select a gait entity to inspect its legs.")))
        .OnSelectionChanged_Lambda([WeakWindow](int32 InLegIndex)
        {
            if (const auto Window = WeakWindow.Pin())
            { Window->SelectLegAt(InLegIndex); }
        });

    _Viewport = SNew(SCkDebug_3dPreviewViewport).Descriptor(FCkDebug3dPreviewDescriptor{}).Adapter(_Preview)
        .SafeAreaOverlay(SNew(SCkDebug_SelectableLabel)
            .Text(FText::FromString(TEXT("Recorded diagnostic rig · goal cubes · actual probe rays / normals"))));
    _Preview->Initialize(_Viewport->Get_PreviewWorld());
    _PreviewSelectedHandle = _Preview->Get_OnLegSelected().AddRaw(this, &SCkProceduralAnimationDebuggerWindow::SelectLeg);
    _TimelineHost = SNew(SBox).MinDesiredHeight(120.0f);

    const auto Toolbar = SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [SNew(SCkDebug_WorldSelector, _WorldModel)]
        + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceM, 0.0f)
        [SNew(SCkDebug_IconToggle).Tag(TEXT("ProceduralAnimation.Hold")).IconId(ECk_Icon::Recording)
            .Label(FText::FromString(TEXT("Hold"))).ShowLabel(true)
            .ToolTip(FText::FromString(TEXT("Hold the inspected sample; gameplay and bounded recording continue.")))
            .IsOn_Lambda([WeakModel]()
            {
                const auto Model = WeakModel.Pin();
                return Model.IsValid() && NOT Model->Get_History().Get_IsLive();
            })
            .OnStateChanged_Lambda([WeakModel](bool InHold)
            {
                const auto Model = WeakModel.Pin();
                if (NOT Model.IsValid())
                { return; }

                if (InHold)
                { Model->Request_Hold(); }
                else
                { Model->Request_GoLive(); }
            })]
        + SHorizontalBox::Slot().AutoWidth()
        [SNew(SButton).Tag(TEXT("ProceduralAnimation.Live")).Text(FText::FromString(TEXT("Live")))
            .ToolTipText(FText::FromString(TEXT("Show the newest recorded sample.")))
            .OnClicked_Lambda([WeakModel]()
            {
                if (const auto Model = WeakModel.Pin())
                { Model->Request_GoLive(); }
                return FReply::Handled();
            })]
        + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceM, 0.0f)
        [SNew(SButton).Tag(TEXT("ProceduralAnimation.FrameRig")).Text(FText::FromString(TEXT("Frame rig")))
            .OnClicked_Lambda([WeakWindow]()
            {
                const auto Window = WeakWindow.Pin();
                if (Window.IsValid() && Window->_Viewport.IsValid() && NOT Window->_Released)
                { Window->_Viewport->Apply_CameraPreset(ECkDebug3dCameraPreset::FrameAll); }
                return FReply::Handled();
            })];

    const auto Sidebar = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceS)
        [SNew(SCkDebug_SearchBar).HintText(FText::FromString(TEXT("Filter gait entities / segments")))
            .OnSearchTextChanged_Lambda([WeakWindow](const FString& InText)
            {
                const auto Window = WeakWindow.Pin();
                if (NOT Window.IsValid() || Window->_Released)
                { return; }

                Window->_Filter = InText;
                Window->RefreshPresentation();
            })]
        + SVerticalBox::Slot().FillHeight(1.0f)[_Roster.ToSharedRef()];

    const auto Detail = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceS)
        [SNew(SCkDebug_SelectableLabel).Text_Lambda([WeakWindow]()
        {
            const auto Window = WeakWindow.Pin();
            return Window.IsValid() ? Window->Get_DetailText() : FText{};
        })]
        + SVerticalBox::Slot().FillHeight(1.0f)
        [SNew(SSplitter)
            + SSplitter::Slot().Value(0.60f)
            [SNew(SCkDebug_PaneHost).ContentMode(ECkDebugPaneContent::OpaqueRenderer)[_Viewport.ToSharedRef()]]
            + SSplitter::Slot().Value(0.40f)
            [SNew(SCkDebug_PaneHost)[_Legs.ToSharedRef()]]]
        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceS)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [SNew(SCkDebug_SelectableLabel).Text(FText::FromString(TEXT("Speed cm/s")))]
            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(CkStyle::SpaceM, 0.0f)
            [SNew(SCkDebug_Sparkline).Samples(_SpeedSamples).Color_Lambda([]() { return CkStyle::Info(); })
                .DesiredSize(FVector2D{200.0, 35.0})]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [SNew(SCkDebug_SelectableLabel).Text(FText::FromString(TEXT("Trusted feet")))]
            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(CkStyle::SpaceM, 0.0f)
            [SNew(SCkDebug_Sparkline).Samples(_SupportSamples).Color_Lambda([]() { return CkStyle::Ok(); })
                .DesiredSize(FVector2D{200.0, 35.0})]]
        + SVerticalBox::Slot().AutoHeight()
        [SNew(SCkDebug_PaneHost).ContentMode(ECkDebugPaneContent::OpaqueRenderer)[_TimelineHost.ToSharedRef()]]
        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceS)
        [SNew(SCkDebug_SelectableLabel).Text(FText::FromString(
            TEXT("Sampled history: markers are observations; gaps are unsampled frames. Click or drag to scrub. Hold never pauses gameplay.")))];

    ChildSlot[SNew(SCkDebug_WindowChrome).WindowId(Get_WindowId()).ToolTabId(TEXT("CkProceduralAnimationDebugger"))
        .ShowRefreshControls(true)
        .StatusText_Lambda([WeakWindow]()
        {
            const auto Window = WeakWindow.Pin();
            return Window.IsValid() ? Window->Get_StatusText() : FText{};
        })
        .ToolbarContent()[Toolbar]
        .CommonActionsContent()[SNew(SCkDebug_ViewportPickerControls).Picker(_Picker)
            .PickTooltip(FText::FromString(TEXT("Pick a procedural gait entity, one of its legs or an owned limb.")))]
        .Content()[SNew(SSplitter)
            + SSplitter::Slot().Value(0.24f)[SNew(SCkDebug_PaneHost)[Sidebar]]
            + SSplitter::Slot().Value(0.76f)[SNew(SCkDebug_PaneHost)[Detail]]]];

    _ModelChangedHandle = _Model->Get_OnChanged().AddRaw(this, &SCkProceduralAnimationDebuggerWindow::RefreshPresentation);
    _WorldChangedHandle = _WorldModel->OnWorldChanged.AddLambda([WeakModel](UWorld* InWorld)
    {
        if (const auto Model = WeakModel.Pin())
        { Model->Set_World(InWorld); }
    });
    _WorldModel->Ensure_AutoSelect();
    _Model->Set_World(_WorldModel->Get_SelectedWorld());
    RefreshPresentation();
}

// --------------------------------------------------------------------------------------------------------------------

SCkProceduralAnimationDebuggerWindow::
    ~SCkProceduralAnimationDebuggerWindow()
{
    ReleaseSession();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    Get_WindowDisplayName() const
    -> FText
{
    return FText::FromString(TEXT("Procedural Animation"));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    Tick(
        const FGeometry& InGeometry,
        double InTime,
        float InDeltaTime)
    -> void
{
    SCkDebugger_WindowBase::Tick(InGeometry, InTime, InDeltaTime);
    if (_Released)
    { return; }

    _Picker->Tick(InDeltaTime);
    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(Get_WindowId()))
    { return; }

    _WorldModel->Ensure_AutoSelect();
    if (ck::Is_NOT_Valid(_Model->Get_World()))
    { _Model->Set_World(_WorldModel->Get_SelectedWorld()); }

    _Model->Refresh();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    Request_Refresh()
    -> void
{
    if (_Released || NOT _Model.IsValid())
    { return; }

    _Model->Refresh();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    RefreshPresentation()
    -> void
{
    if (_Released || NOT _Model.IsValid() || NOT _Roster.IsValid())
    { return; }

    if (ck::Is_NOT_Valid(_Model->Get_World()))
    { _Picker->Deactivate(); }

    if (ck::IsValid(_Model->Get_World()) && _WorldModel->Get_SelectedWorld() != _Model->Get_World())
    { _WorldModel->Set_SelectedWorld(_Model->Get_World()); }

    RefreshRoster();

    const auto* Sample = _Model->Get_History().Get_Displayed();
    if (Sample == nullptr)
    {
        _Legs->Clear_Items();
        _Preview->Reset();
        ResetTimelineData();
        _FramedEntity.Reset();
        if (_Timeline.IsValid())
        { _Timeline->Set_Content(0.0, 1.0, {}, {}); }
        return;
    }

    RefreshLegs(*Sample);
    _Preview->Show(*Sample, _Model->Get_SelectedLegId());
    if (_FramedEntity != Sample->Get_EntityId())
    {
        _FramedEntity = Sample->Get_EntityId();
        _Viewport->Apply_CameraPreset(ECkDebug3dCameraPreset::FrameAll);
    }
    RefreshTimeline();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    RefreshRoster()
    -> void
{
    auto Items = TArray<FCkDebug_EntityHealthItem>{};
    for (const auto& Row : _Model->Get_Rows())
    {
        const auto& Summary = Row.Summary;
        if (NOT _Filter.IsEmpty() && NOT Row.Label.Contains(_Filter) && NOT Summary.Get_EntityId().Contains(_Filter))
        { continue; }

        const auto Failed = Summary.Get_GaitFailed() || Summary.Get_RigFailed();
        auto Item = FCkDebug_EntityHealthItem{};
        Item.RowIdentity = Row.Entity;
        Item.SelectionTarget = Row.Entity;
        Item.Name = FText::FromString(Row.Label);
        Item.Context = FText::FromString(Summary.Get_EntityId());
        Item.Summary = FText::FromString(ck::Format_UE(TEXT("{} legs · {} enabled · {} planted"),
            Summary.Get_LegCount(), Summary.Get_EnabledLegCount(), Summary.Get_PlantedCount()));
        Item.Status = FText::FromString(Failed ? TEXT("Failed") : Summary.Get_GaitReady() ? TEXT("Tracking") : TEXT("Pending"));
        Item.Tone = Failed ? ECk_Tone::Err : Summary.Get_GaitReady() ? ECk_Tone::Ok : ECk_Tone::Warn;
        Items.Add(MoveTemp(Item));
    }

    const auto RosterAccepted = _Roster->Set_Items(MoveTemp(Items));
    CK_ENSURE_IF_NOT(RosterAccepted,
        TEXT("Procedural animation roster rejected its rows (an invalid or duplicated entity); the roster is cleared."))
    { _Roster->Clear_Items(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    RefreshLegs(
        const FCk_ProceduralAnimation_DebugSnapshot& InSample)
    -> void
{
    auto Legs = TArray<FCkDebug_EvidenceItem>{};
    for (auto Index = 0; Index < InSample.Get_Legs().Num(); ++Index)
    {
        const auto& Leg = InSample.Get_Legs()[Index];
        if (Leg.Get_LegEntityId().IsEmpty())
        { continue; }

        const auto EnabledState = Leg.Get_Enabled() ? TEXT("Enabled") : TEXT("Disabled");
        auto Item = FCkDebug_EvidenceItem{};
        Item.Key = Leg.Get_LegEntityId();
        Item.Source = FText::FromString(Leg.Get_Id().ToString());
        Item.Headline = FText::FromString(ck::Format_UE(TEXT("{} · {}"), EnabledState, ck_procedural_debug_window::Get_LegState(Leg)));
        Item.Tone = NOT Leg.Get_Enabled() ? ECk_Tone::Neutral : Leg.Get_ContactTrusted() ? ECk_Tone::Ok : ECk_Tone::Warn;
        Item.RightLabel = FText::FromString(ck::Format_UE(TEXT("{:.0f}%"), Leg.Get_SwingAlpha() * 100.0f));
        Item.Detail = FText::FromString(ck::Format_UE(
            TEXT("{} · phase {:.2f} · error {:.1f}/{:.1f} cm\nProbe {}: hit {} · fraction {:.4f} · missing {:.3f}s\nRig {}"),
            EnabledState, Leg.Get_PhaseOffset(), FVector::Distance(Leg.Get_PlantedPosition(), Leg.Get_IdealTarget()),
            Leg.Get_StepThreshold(), Leg.Get_ProbeAttemptCount(), Leg.Get_ProbeHit(), Leg.Get_ProbeHitFraction(),
            Leg.Get_MissingContact().Get_Seconds(), ck_procedural_debug_window::Get_RigState(Leg)));
        Item.CopyText = ck::Format_UE(TEXT("{}\n{}\n{}\nfoot {}\ntarget {}\nnormal {}"), Item.Key, Item.Headline.ToString(),
            Item.Detail.ToString(), Leg.Get_FootPosition().ToString(), Leg.Get_IdealTarget().ToString(), Leg.Get_Normal().ToString());
        Item.SelectionId = Index;
        Legs.Add(MoveTemp(Item));
    }

    const auto LegsAccepted = _Legs->Set_Items(MoveTemp(Legs));
    CK_ENSURE_IF_NOT(LegsAccepted,
        TEXT("Procedural animation leg list rejected the legs of [{}] (an empty or duplicated leg identity); the list is cleared."),
        InSample.Get_EntityId())
    { _Legs->Clear_Items(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    RefreshTimeline()
    -> void
{
    const auto& History = _Model->Get_History();
    const auto* Displayed = History.Get_Displayed();
    if (Displayed == nullptr)
    { return; }

    auto Labels = TArray<FString>{};
    for (const auto& Leg : Displayed->Get_Legs())
    { Labels.Add(Leg.Get_Id().ToString()); }

    if (Labels != _LaneLabels || NOT _Timeline.IsValid())
    {
        if (_Timeline.IsValid() && _Timeline->Get_IsInteracting())
        { return; }

        RecreateTimeline(Labels);
    }

    if (_TimelineRevision == _Model->Get_HistoryRevision())
    { return; }

    _TimelineRevision = _Model->Get_HistoryRevision();
    const auto* First = History.Get_Sample(0);
    const auto* Last = History.Get_Sample(History.Get_Count() - 1);
    if (First == nullptr || Last == nullptr)
    {
        ResetTimelineData();
        _Timeline->Set_Content(0.0, 1.0, {}, {});
        return;
    }

    if (First->Get_EntityId() != _TimelineEntityId)
    {
        ResetTimelineData();
        _TimelineEntityId = First->Get_EntityId();
    }

    const auto FirstSequence = First->Get_Sequence();
    const auto EvictedSamples = Algo::LowerBound(_TimelineSampleSequences, FirstSequence);
    if (EvictedSamples > 0)
    {
        _TimelineSampleSequences.RemoveAt(0, EvictedSamples);
        _SpeedSamples->RemoveAt(0, EvictedSamples);
        _SupportSamples->RemoveAt(0, EvictedSamples);
    }

    const auto FirstSelectionId = ck_procedural_debug_window::Get_SequenceSelectionId(FirstSequence);
    const auto EvictedEvents = Algo::LowerBoundBy(_TimelineEvents, FirstSelectionId,
        [](const FCkDebug_TimelineEvent& InEvent) { return InEvent.SelectionId; });
    if (EvictedEvents > 0)
    { _TimelineEvents.RemoveAt(0, EvictedEvents); }

    const auto EvictedSpans = Algo::LowerBound(_TimelineSpanSequences, FirstSequence);
    if (EvictedSpans > 0)
    {
        _TimelineSpanSequences.RemoveAt(0, EvictedSpans);
        _TimelineSpans.RemoveAt(0, EvictedSpans);
    }

    const auto LastPresented = _TimelineSampleSequences.IsEmpty() ? uint64{0} : _TimelineSampleSequences.Last();
    for (auto Index = 0; Index < History.Get_Count(); ++Index)
    {
        if (History.Get_Sample(Index)->Get_Sequence() > LastPresented)
        { AppendTimelineSample(Index); }
    }

    const auto StartSeconds = First->Get_Time().Get_Seconds();
    const auto EndSeconds = FMath::Max(StartSeconds + 0.001, Last->Get_Time().Get_Seconds());
    _Timeline->Set_Content(StartSeconds, EndSeconds, _TimelineEvents, _TimelineSpans);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    RecreateTimeline(
        const TArray<FString>& InLaneLabels)
    -> void
{
    const auto HadTimeline = _Timeline.IsValid();
    const auto ViewStart = HadTimeline ? _Timeline->Get_ViewStart() : 0.0;
    const auto ViewDuration = HadTimeline ? _Timeline->Get_ViewDuration() : 0.0;
    const auto FollowLive = NOT HadTimeline || _Timeline->Get_IsFollowingLive();
    const auto WeakWindow = TWeakPtr<SCkProceduralAnimationDebuggerWindow>{SharedThis(this)};

    _LaneLabels = InLaneLabels;
    _Timeline = SNew(SCkDebug_EventTimeline).LaneLabels(InLaneLabels)
        .DesiredHeight(FMath::Clamp(32.0f + InLaneLabels.Num() * 18.0f, 120.0f, 220.0f))
        .AllowPanZoom(true)
        .InitialViewDuration(10.0)
        .CursorTime_Lambda([WeakWindow]() -> TOptional<double>
        {
            const auto Window = WeakWindow.Pin();
            const auto* Sample = Window.IsValid() && Window->_Model.IsValid() ? Window->_Model->Get_History().Get_Displayed() : nullptr;
            return Sample != nullptr ? TOptional<double>{Sample->Get_Time().Get_Seconds()} : TOptional<double>{};
        })
        .OnScrubbed_Lambda([WeakWindow](double InTime)
        {
            if (const auto Window = WeakWindow.Pin())
            { Window->ScrubTime(InTime); }
        })
        .OnEventSelected_Lambda([WeakWindow](int32 InSelectionId)
        {
            const auto Window = WeakWindow.Pin();
            if (Window.IsValid() && Window->_Model.IsValid() && InSelectionId >= 0)
            { Window->_Model->Request_ScrubBySequence(static_cast<uint64>(InSelectionId)); }
        });

    if (HadTimeline)
    {
        _Timeline->Set_View(ViewStart, ViewDuration);
        _Timeline->Set_FollowLive(FollowLive);
    }

    _TimelineHost->SetContent(_Timeline.ToSharedRef());
    ResetTimelineData();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    ResetTimelineData()
    -> void
{
    _TimelineEvents.Reset();
    _TimelineSpans.Reset();
    _TimelineSpanSequences.Reset();
    _TimelineSampleSequences.Reset();
    _SpeedSamples->Reset();
    _SupportSamples->Reset();
    _TimelineEntityId.Reset();
    _TimelineRevision = MAX_uint64;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    AppendTimelineSample(
        int32 InChronologicalIndex)
    -> void
{
    const auto& History = _Model->Get_History();
    const auto* Sample = History.Get_Sample(InChronologicalIndex);
    const auto* Previous = History.Get_Sample(InChronologicalIndex - 1);
    const auto Sequence = Sample->Get_Sequence();
    // A band only joins adjacent simulation frames. A gated capture must not invent
    // a continuous plant or swing across frames it did not observe.
    const auto JoinsPrevious = Previous != nullptr && Previous->Get_Sequence() + 1 == Sequence;

    auto TrustedCount = 0;
    for (auto LegIndex = 0; LegIndex < Sample->Get_Legs().Num(); ++LegIndex)
    {
        const auto& Leg = Sample->Get_Legs()[LegIndex];
        if (Leg.Get_LegEntityId().IsEmpty())
        { continue; }

        TrustedCount += Leg.Get_ContactTrusted() ? 1 : 0;
        const auto Color = NOT Leg.Get_ContactTrusted() ? CkStyle::Warn() : Leg.Get_Planted() ? CkStyle::Ok() : CkStyle::Info();

        auto Event = FCkDebug_TimelineEvent{};
        Event.LaneIndex = LegIndex;
        Event.TimeSeconds = Sample->Get_Time().Get_Seconds();
        Event.Shape = ECkDebug_TimelineMarker::Diamond;
        Event.Color = Color;
        Event.Tooltip = ck::Format_UE(TEXT("{} · sample {} · {}"), Leg.Get_Id(), Sequence, ck_procedural_debug_window::Get_LegState(Leg));
        Event.SelectionId = ck_procedural_debug_window::Get_SequenceSelectionId(Sequence);
        _TimelineEvents.Add(MoveTemp(Event));

        if (JoinsPrevious)
        {
            _TimelineSpans.Add({LegIndex, Previous->Get_Time().Get_Seconds(), Sample->Get_Time().Get_Seconds(), Color,
                TEXT("Adjacent observed frames")});
            _TimelineSpanSequences.Add(Sequence);
        }
    }

    _SpeedSamples->Add(Sample->Get_Velocity().Size());
    _SupportSamples->Add(TrustedCount);
    _TimelineSampleSequences.Add(Sequence);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    SelectLeg(
        const FString& InLegEntityId)
    -> void
{
    if (_Released || NOT _Model.IsValid())
    { return; }

    _Model->Request_SelectLeg(InLegEntityId);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    SelectLegAt(
        int32 InLegIndex)
    -> void
{
    if (_Released || NOT _Model.IsValid())
    { return; }

    const auto* Sample = _Model->Get_History().Get_Displayed();
    if (Sample == nullptr || NOT Sample->Get_Legs().IsValidIndex(InLegIndex))
    { return; }

    SelectLeg(Sample->Get_Legs()[InLegIndex].Get_LegEntityId());
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    ScrubTime(
        double InSeconds)
    -> void
{
    if (_Released || NOT FMath::IsFinite(InSeconds))
    { return; }

    const auto& History = _Model->Get_History();
    auto Best = int32{INDEX_NONE};
    auto Distance = TNumericLimits<double>::Max();
    for (auto Index = 0; Index < History.Get_Count(); ++Index)
    {
        const auto Delta = FMath::Abs(History.Get_Sample(Index)->Get_Time().Get_Seconds() - InSeconds);
        if (Delta < Distance)
        {
            Distance = Delta;
            Best = Index;
        }
    }
    _Model->Request_Scrub(Best);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    Get_StatusText() const
    -> FText
{
    if (_Released || NOT _Model.IsValid())
    { return FText::FromString(TEXT("Released")); }

    return FText::FromString(ck::Format_UE(TEXT("{} gait entities · {} / {} samples · {}{}"),
        _Model->Get_Rows().Num(), _Model->Get_History().Get_Count(), _Model->Get_History().Get_Capacity(),
        _Model->Get_History().Get_IsLive() ? TEXT("Live") : TEXT("Held"),
        _Model->Get_SelectedGone() ? TEXT(" · entity gone, history retained") : TEXT("")));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    Get_DetailText() const
    -> FText
{
    const auto* Sample = _Model.IsValid() ? _Model->Get_History().Get_Displayed() : nullptr;
    if (Sample == nullptr)
    { return FText::FromString(TEXT("Select a gait entity. Capture starts when its first accepted solve is observed.")); }

    const auto* Status = _Model->Get_History().Get_IsLive() ? _Model->Get_LiveStatus() : Sample;
    const auto Detail = ck::Format_UE(TEXT("{} · t {:.3f}s · sample {} · cadence {:.2f}x · {}{}"),
        Sample->Get_EntityName(), Sample->Get_Time().Get_Seconds(), Sample->Get_Sequence(), Sample->Get_CadenceScale(),
        Sample->Get_Airborne() ? TEXT("Airborne") : TEXT("Grounded"),
        Status != nullptr && Status->Get_GaitFailed() ? TEXT(" · GAIT FAILED")
            : Sample->Get_RigPosePending() || NOT Sample->Get_RigMatchesGaitSequence() ? TEXT(" · rig pose not synchronized")
            : TEXT(""));
    return FText::FromString(Detail);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    ReleaseSession()
    -> void
{
    if (_Released)
    { return; }

    _Released = true;
    if (_Picker.IsValid())
    { _Picker->Deactivate(); }

    if (_Roster.IsValid())
    { _Roster->Clear_Items(); }

    if (_Legs.IsValid())
    { _Legs->Clear_Items(); }

    if (_WorldModel.IsValid())
    { _WorldModel->OnWorldChanged.Remove(_WorldChangedHandle); }

    if (_Model.IsValid())
    {
        _Model->Get_OnChanged().Remove(_ModelChangedHandle);
        _Model->ReleaseSession();
    }

    if (_Preview.IsValid())
    { _Preview->On_ViewportTeardown(); }

    if (_Viewport.IsValid())
    { _Viewport->Teardown(); }

    ChildSlot[SNullWidget::NullWidget];
    _Preview.Reset();
    _Viewport.Reset();
    _Timeline.Reset();
    _TimelineHost.Reset();
    _Roster.Reset();
    _Legs.Reset();
    _Picker.Reset();
    _WorldModel.Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    Is_ProceduralEntity(
        const FCk_Handle& InEntity)
    -> bool
{
    return FCkProceduralAnimationDebugger_DataCollector::Is_Supported(InEntity);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    OpenForEntity(
        const FCk_Handle& InEntity)
    -> void
{
    auto& Module = FCkProceduralAnimationDebuggerModule::Get();
    Module.OpenDebugger();
    if (const auto Window = Module.Get_DebuggerWindow())
    { Window->Get_Model()->Select(InEntity, ECkProceduralAnimationDebugger_SelectionSync::Silent); }
}

// --------------------------------------------------------------------------------------------------------------------
