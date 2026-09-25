#include "CkProceduralAnimationDebugger/Window/SCkProceduralAnimationDebuggerWindow.h"
#include "CkProceduralAnimationDebugger/Viewport/CkProceduralAnimationDebugger_Preview.h"
#include "CkProceduralAnimationDebugger/CkProceduralAnimationDebugger_Module.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityHealthList.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EvidenceList.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_PaneHost.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EventTimeline.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Sparkline.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Picker/CkDebug_ViewportPicker.h"
#include "CkDebuggerCommon/Picker/SCkDebug_ViewportPickerControls.h"
#include "CkDebuggerCommon/Search/SCkDebug_SearchBar.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"

namespace ck_procedural_debug_window
{
    auto LegState(const FCk_ProceduralAnimation_DebugLeg& InLeg) -> FString
    {
        const auto Contact = InLeg.Get_ContactTrusted() ? TEXT("contact")
            : InLeg.Get_ProbeState() == ck::EProceduralFootProbeState::Guessing ? TEXT("grace") : TEXT("lost");
        return ck::Format_UE(TEXT("{} / {}"), InLeg.Get_Planted() ? TEXT("planted") : TEXT("swing"), Contact);
    }
}

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
        [WeakModel]() { const auto Model = WeakModel.Pin(); return Model.IsValid() ? Model->Get_World() : nullptr; },
        [WeakModel](const FCk_Handle& InEntity) { if (const auto Model = WeakModel.Pin()) { Model->Select(InEntity, true); } },
        [](const FCk_Handle& InEntity) { return Is_ProceduralEntity(InEntity); }});
    _Roster = SNew(SCkDebug_EntityHealthList)
        .SelectedEntity_Lambda([WeakModel]() { const auto Model = WeakModel.Pin(); return Model.IsValid() ? Model->Get_SelectedHandle() : FCk_Handle{}; })
        .OnSelected_Lambda([WeakModel](const FCk_Handle& InEntity) { if (const auto Model = WeakModel.Pin()) { Model->Select(InEntity, true); } });
    _Legs = SNew(SCkDebug_EvidenceList).MaxItems(64)
        .EmptyText(FText::FromString(TEXT("Select a gait entity to inspect its legs.")))
        .OnSelectionChanged_Lambda([WeakWindow](int32 InIndex) { if (const auto Window = WeakWindow.Pin()) { Window->SelectLeg(InIndex); } });
    _Viewport = SNew(SCkDebug_3dPreviewViewport).Descriptor(FCkDebug3dPreviewDescriptor{}).Adapter(_Preview)
        .SafeAreaOverlay(SNew(SCkDebug_SelectableLabel).Text(FText::FromString(TEXT("Recorded diagnostic rig · goal cubes · actual probe rays / normals"))));
    _Preview->Initialize(_Viewport->Get_PreviewWorld());
    _PreviewSelectedHandle = _Preview->Get_OnLegSelected().AddRaw(this, &SCkProceduralAnimationDebuggerWindow::SelectLeg);
    _TimelineHost = SNew(SBox).MinDesiredHeight(120.0f);

    const auto Toolbar = SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [SNew(SCkDebug_WorldSelector, _WorldModel)]
        + SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f)
        [SNew(SCkDebug_IconToggle).Tag(TEXT("ProceduralAnimation.Hold")).IconId(ECk_Icon::Recording).Label(FText::FromString(TEXT("Hold"))).ShowLabel(true)
            .ToolTip(FText::FromString(TEXT("Hold the inspected sample; gameplay and bounded recording continue.")))
            .IsOn_Lambda([WeakModel]() { const auto Model = WeakModel.Pin(); return Model.IsValid() && NOT Model->Get_History().Get_IsLive(); })
            .OnStateChanged_Lambda([WeakModel](bool InHold)
            {
                if (const auto Model = WeakModel.Pin())
                { if (InHold) { Model->Request_Hold(); } else { Model->Request_GoLive(); } }
            })]
        + SHorizontalBox::Slot().AutoWidth()
        [SNew(SButton).Tag(TEXT("ProceduralAnimation.Live")).Text(FText::FromString(TEXT("Live"))).ToolTipText(FText::FromString(TEXT("Show the newest recorded sample.")))
            .OnClicked_Lambda([WeakModel]() { if (const auto Model = WeakModel.Pin()) { Model->Request_GoLive(); } return FReply::Handled(); })]
        + SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f)
        [SNew(SButton).Tag(TEXT("ProceduralAnimation.FrameRig")).Text(FText::FromString(TEXT("Frame rig")))
            .OnClicked_Lambda([WeakWindow]()
            {
                if (const auto Window = WeakWindow.Pin(); Window.IsValid() && Window->_Viewport.IsValid() && NOT Window->_Released)
                { Window->_Viewport->Apply_CameraPreset(ECkDebug3dCameraPreset::FrameAll); }
                return FReply::Handled();
            })];

    const auto Sidebar = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(6.0f)
        [SNew(SCkDebug_SearchBar).HintText(FText::FromString(TEXT("Filter gait entities / segments")))
            .OnSearchTextChanged_Lambda([WeakWindow](const FString& InText)
            { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && NOT Window->_Released) { Window->_Filter = InText; Window->RefreshPresentation(); } })]
        + SVerticalBox::Slot().FillHeight(1.0f)[_Roster.ToSharedRef()];
    const auto Detail = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(6.0f)
        [SNew(SCkDebug_SelectableLabel).Text_Lambda([WeakWindow]() { const auto Window = WeakWindow.Pin(); return Window.IsValid() ? Window->Get_DetailText() : FText{}; })]
        + SVerticalBox::Slot().FillHeight(1.0f)
        [SNew(SSplitter)
            + SSplitter::Slot().Value(0.60f)
            [SNew(SCkDebug_PaneHost).ContentMode(ECkDebugPaneContent::OpaqueRenderer)[_Viewport.ToSharedRef()]]
            + SSplitter::Slot().Value(0.40f)
            [SNew(SCkDebug_PaneHost)[_Legs.ToSharedRef()]]]
        + SVerticalBox::Slot().AutoHeight().Padding(6.0f)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SCkDebug_SelectableLabel).Text(FText::FromString(TEXT("Speed cm/s")))]
            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(8.0f, 0.0f)[SNew(SCkDebug_Sparkline).Samples(_SpeedSamples).Color_Lambda([]() { return CkStyle::Info(); }).DesiredSize(FVector2D{200.0, 35.0})]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SCkDebug_SelectableLabel).Text(FText::FromString(TEXT("Trusted feet")))]
            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(8.0f, 0.0f)[SNew(SCkDebug_Sparkline).Samples(_SupportSamples).Color_Lambda([]() { return CkStyle::Ok(); }).DesiredSize(FVector2D{200.0, 35.0})]]
        + SVerticalBox::Slot().AutoHeight()
        [SNew(SCkDebug_PaneHost).ContentMode(ECkDebugPaneContent::OpaqueRenderer)[_TimelineHost.ToSharedRef()]]
        + SVerticalBox::Slot().AutoHeight().Padding(6.0f)
        [SNew(SCkDebug_SelectableLabel).Text(FText::FromString(TEXT("Sampled history: markers are observations; gaps are unsampled frames. Click or drag to scrub. Hold never pauses gameplay.")))];

    ChildSlot[SNew(SCkDebug_WindowChrome).WindowId(Get_WindowId()).ToolTabId(TEXT("CkProceduralAnimationDebugger"))
        .ShowRefreshControls(true).StatusText_Lambda([WeakWindow]() { const auto Window = WeakWindow.Pin(); return Window.IsValid() ? Window->Get_StatusText() : FText{}; })
        .ToolbarContent()[Toolbar]
        .CommonActionsContent()[SNew(SCkDebug_ViewportPickerControls).Picker(_Picker).PickTooltip(FText::FromString(TEXT("Pick a procedural gait entity or its owned limb.")))]
        .Content()[SNew(SSplitter)
            + SSplitter::Slot().Value(0.24f)[SNew(SCkDebug_PaneHost)[Sidebar]]
            + SSplitter::Slot().Value(0.76f)[SNew(SCkDebug_PaneHost)[Detail]]]];

    _ModelChangedHandle = _Model->Get_OnChanged().AddRaw(this, &SCkProceduralAnimationDebuggerWindow::RefreshPresentation);
    _WorldChangedHandle = _WorldModel->OnWorldChanged.AddLambda([WeakModel](UWorld* InWorld)
    { if (const auto Model = WeakModel.Pin()) { Model->Set_World(InWorld); } });
    _WorldModel->Ensure_AutoSelect();
    _Model->Set_World(_WorldModel->Get_SelectedWorld());
    RefreshPresentation();
}

SCkProceduralAnimationDebuggerWindow::~SCkProceduralAnimationDebuggerWindow()
{ ReleaseSession(); }

auto
    SCkProceduralAnimationDebuggerWindow::
    Get_WindowDisplayName() const
    -> FText

{ return FText::FromString(TEXT("Procedural Animation")); }

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
    if (NOT ck::IsValid(_Model->Get_World()))
    { _Model->Set_World(_WorldModel->Get_SelectedWorld()); }
    _Model->Refresh();
}

auto
    SCkProceduralAnimationDebuggerWindow::
    Request_Refresh()
    -> void

{ if (NOT _Released && _Model.IsValid()) { _Model->Refresh(); } }

auto
    SCkProceduralAnimationDebuggerWindow::
    RefreshPresentation()
    -> void

{
    if (_Released || NOT _Model.IsValid() || NOT _Roster.IsValid())
    { return; }
    if (NOT ck::IsValid(_Model->Get_World()))
    { _Picker->Deactivate(); }
    if (ck::IsValid(_Model->Get_World()) && _WorldModel->Get_SelectedWorld() != _Model->Get_World())
    { _WorldModel->Set_SelectedWorld(_Model->Get_World()); }
    auto Items = TArray<FCkDebug_EntityHealthItem>{};
    for (const auto& Row : _Model->Get_Rows())
    {
        if (NOT _Filter.IsEmpty() && NOT Row.Label.Contains(_Filter) && NOT Row.Snapshot.Get_EntityId().Contains(_Filter))
        { continue; }
        const auto& S = Row.Snapshot;
        const auto Failed = S.Get_GaitFailed() || (S.Get_HasRig() && S.Get_RigFailure() != ECk_ProceduralRig_Failure::None);
        auto Item = FCkDebug_EntityHealthItem{};
        Item.RowIdentity = Row.Entity;
        Item.SelectionTarget = Row.Entity;
        Item.Name = FText::FromString(Row.Label);
        Item.Context = FText::FromString(S.Get_EntityId());
        Item.Summary = FText::FromString(ck::Format_UE(TEXT("{} legs · {:.1f} cm/s · phase {:.2f}"), S.Get_Legs().Num(), S.Get_Velocity().Size(), S.Get_GaitClock()));
        Item.Status = FText::FromString(Failed ? TEXT("Failed") : NOT S.Get_HasAcceptedSample() ? TEXT("Pending") : S.Get_Airborne() ? TEXT("Airborne") : TEXT("Tracking"));
        Item.Tone = Failed ? ECk_Tone::Err : NOT S.Get_HasAcceptedSample() || S.Get_Airborne() ? ECk_Tone::Warn : ECk_Tone::Ok;
        Items.Add(MoveTemp(Item));
    }
    _Roster->Set_Items(MoveTemp(Items));
    const auto* Sample = _Model->Get_History().Get_Displayed();
    if (Sample == nullptr)
    {
        _Legs->Clear_Items();
        _Preview->Reset();
        _SpeedSamples->Reset();
        _SupportSamples->Reset();
        _FramedEntity.Reset();
        _SelectedLeg = INDEX_NONE;
        if (_Timeline.IsValid())
        { _Timeline->Set_Content(0.0, 1.0, {}, {}); }
        return;
    }
    auto Legs = TArray<FCkDebug_EvidenceItem>{};
    for (auto Index = 0; Index < Sample->Get_Legs().Num(); ++Index)
    {
        const auto& Leg = Sample->Get_Legs()[Index];
        auto Item = FCkDebug_EvidenceItem{};
        Item.Key = Leg.Get_Id().ToString();
        Item.Source = FText::FromString(Leg.Get_Id().ToString());
        Item.Headline = FText::FromString(ck_procedural_debug_window::LegState(Leg));
        Item.Tone = Leg.Get_ContactTrusted() ? ECk_Tone::Ok : ECk_Tone::Warn;
        Item.RightLabel = FText::FromString(ck::Format_UE(TEXT("{:.0f}%"), Leg.Get_SwingAlpha() * 100.0f));
        Item.Detail = FText::FromString(ck::Format_UE(TEXT("Phase {:.2f} · error {:.1f}/{:.1f} cm\nProbe {}: hit {} · fraction {:.4f} · missing {:.3f}s"),
            Leg.Get_PhaseOffset(), FVector::Distance(Leg.Get_PlantedPosition(), Leg.Get_IdealTarget()), Leg.Get_StepThreshold(),
            Leg.Get_ProbeAttemptCount(), Leg.Get_ProbeHit(), Leg.Get_ProbeHitFraction(), Leg.Get_MissingContact().Get_Seconds()));
        Item.CopyText = ck::Format_UE(TEXT("{}\n{}\n{}\nfoot {}\ntarget {}\nnormal {}"), Item.Key, Item.Headline.ToString(), Item.Detail.ToString(),
            Leg.Get_FootPosition().ToString(), Leg.Get_IdealTarget().ToString(), Leg.Get_Normal().ToString());
        Item.SelectionId = Index;
        Legs.Add(MoveTemp(Item));
    }
    _Legs->Set_Items(MoveTemp(Legs));
    _Preview->Show(*Sample, _SelectedLeg);
    if (_FramedEntity != Sample->Get_EntityId())
    {
        _FramedEntity = Sample->Get_EntityId();
        _SelectedLeg = INDEX_NONE;
        _Viewport->Apply_CameraPreset(ECkDebug3dCameraPreset::FrameAll);
    }
    RefreshTimeline();
}

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
        _LaneLabels = Labels;
        const auto WeakWindow = TWeakPtr<SCkProceduralAnimationDebuggerWindow>{SharedThis(this)};
        _Timeline = SNew(SCkDebug_EventTimeline).LaneLabels(Labels).DesiredHeight(FMath::Clamp(32.0f + Labels.Num() * 18.0f, 120.0f, 220.0f))
            .AllowPanZoom(true).InitialViewDuration(10.0)
            .CursorTime_Lambda([WeakWindow]() -> TOptional<double>
            {
                const auto Window = WeakWindow.Pin();
                const auto* Sample = Window.IsValid() && Window->_Model.IsValid() ? Window->_Model->Get_History().Get_Displayed() : nullptr;
                return Sample != nullptr ? TOptional<double>{Sample->Get_Time().Get_Seconds()} : TOptional<double>{};
            })
            .OnScrubbed_Lambda([WeakWindow](double InTime) { if (const auto Window = WeakWindow.Pin()) { Window->ScrubTime(InTime); } })
            .OnEventSelected_Lambda([WeakWindow](int32 InSample) { if (const auto Window = WeakWindow.Pin()) { Window->_Model->Request_Scrub(InSample); } });
        _TimelineHost->SetContent(_Timeline.ToSharedRef());
    }
    auto Events = TArray<FCkDebug_TimelineEvent>{};
    auto Spans = TArray<FCkDebug_TimelineSpan>{};
    _SpeedSamples->Reset();
    _SupportSamples->Reset();
    for (auto Index = 0; Index < History.Get_Count(); ++Index)
    {
        const auto* Sample = History.Get_Sample(Index);
        const auto* Previous = History.Get_Sample(Index - 1);
        _SpeedSamples->Add(Sample->Get_Velocity().Size());
        auto TrustedCount = 0;
        for (auto LegIndex = 0; LegIndex < Sample->Get_Legs().Num(); ++LegIndex)
        {
            const auto& Leg = Sample->Get_Legs()[LegIndex];
            TrustedCount += Leg.Get_ContactTrusted() ? 1 : 0;
            const auto Color = NOT Leg.Get_ContactTrusted() ? CkStyle::Warn() : Leg.Get_Planted() ? CkStyle::Ok() : CkStyle::Info();
            auto Event = FCkDebug_TimelineEvent{};
            Event.LaneIndex = LegIndex;
            Event.TimeSeconds = Sample->Get_Time().Get_Seconds();
            Event.Shape = ECkDebug_TimelineMarker::Diamond;
            Event.Color = Color;
            Event.Tooltip = ck::Format_UE(TEXT("{} · sample {} · {}"), Leg.Get_Id(), Sample->Get_Sequence(), ck_procedural_debug_window::LegState(Leg));
            Event.SelectionId = Index;
            Events.Add(MoveTemp(Event));
            // A band only joins adjacent simulation frames. A gated capture must not invent
            // a continuous plant or swing across frames it did not observe.
            if (Previous != nullptr && Previous->Get_Sequence() + 1 == Sample->Get_Sequence())
            { Spans.Add({LegIndex, Previous->Get_Time().Get_Seconds(), Sample->Get_Time().Get_Seconds(), Color, TEXT("Adjacent observed frames")}); }
        }
        _SupportSamples->Add(TrustedCount);
    }
    const auto* First = History.Get_Sample(0);
    const auto* Last = History.Get_Sample(History.Get_Count() - 1);
    if (First != nullptr && Last != nullptr)
    { _Timeline->Set_Content(First->Get_Time().Get_Seconds(), FMath::Max(First->Get_Time().Get_Seconds() + 0.001, Last->Get_Time().Get_Seconds()), MoveTemp(Events), MoveTemp(Spans)); }
}

auto
    SCkProceduralAnimationDebuggerWindow::
    SelectLeg(
        int32 InLeg)
    -> void

{
    if (_Released)
    { return; }
    _SelectedLeg = InLeg;
    RefreshPresentation();
}

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
        if (Delta < Distance) { Distance = Delta; Best = Index; }
    }
    _Model->Request_Scrub(Best);
}

auto
    SCkProceduralAnimationDebuggerWindow::
    Get_StatusText() const
    -> FText

{
    if (_Released || NOT _Model.IsValid())
    { return FText::FromString(TEXT("Released")); }
    return FText::FromString(ck::Format_UE(TEXT("{} gait entities · {} / {} samples · {}{}"),
        _Model->Get_Rows().Num(), _Model->Get_History().Get_Count(), _Model->Get_History().Get_Capacity(),
        _Model->Get_History().Get_IsLive() ? TEXT("Live") : TEXT("Held"), _Model->Get_SelectedGone() ? TEXT(" · entity gone, history retained") : TEXT("")));
}

auto
    SCkProceduralAnimationDebuggerWindow::
    Get_DetailText() const
    -> FText

{
    const auto* Sample = _Model.IsValid() ? _Model->Get_History().Get_Displayed() : nullptr;
    if (Sample == nullptr)
    { return FText::FromString(TEXT("Select a gait entity. Capture starts when its first accepted solve is observed.")); }
    const auto* Status = _Model->Get_History().Get_IsLive() ? _Model->Get_LiveStatus() : Sample;
    auto Detail = ck::Format_UE(TEXT("{} · t {:.3f}s · sample {} · cadence {:.2f}x · {}{}"),
        Sample->Get_EntityName(), Sample->Get_Time().Get_Seconds(), Sample->Get_Sequence(), Sample->Get_CadenceScale(),
        Sample->Get_Airborne() ? TEXT("Airborne") : TEXT("Grounded"),
        Status != nullptr && Status->Get_GaitFailed() ? TEXT(" · GAIT FAILED") :
        Sample->Get_RigPosePending() || NOT Sample->Get_RigMatchesGaitSequence() ? TEXT(" · rig pose not synchronized") : TEXT(""));
    return FText::FromString(Detail);
}

auto
    SCkProceduralAnimationDebuggerWindow::
    ReleaseSession()
    -> void

{
    if (_Released)
    { return; }
    _Released = true;
    if (_Picker.IsValid()) { _Picker->Deactivate(); }
    if (_Roster.IsValid()) { _Roster->Clear_Items(); }
    if (_Legs.IsValid()) { _Legs->Clear_Items(); }
    if (_WorldModel.IsValid()) { _WorldModel->OnWorldChanged.Remove(_WorldChangedHandle); }
    if (_Model.IsValid())
    {
        _Model->Get_OnChanged().Remove(_ModelChangedHandle);
        _Model->ReleaseSession();
    }
    if (_Preview.IsValid()) { _Preview->On_ViewportTeardown(); }
    if (_Viewport.IsValid()) { _Viewport->Teardown(); }
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

auto
    SCkProceduralAnimationDebuggerWindow::
    Is_ProceduralEntity(
        const FCk_Handle& InEntity)
    -> bool

{ return FCkProceduralAnimationDebugger_DataCollector::Is_Supported(InEntity); }

auto
    SCkProceduralAnimationDebuggerWindow::
    OpenForEntity(
        const FCk_Handle& InEntity)
    -> void

{
    auto& Module = FCkProceduralAnimationDebuggerModule::Get();
    Module.OpenDebugger();
    if (const auto Window = Module.Get_DebuggerWindow())
    { Window->Get_Model()->Select(InEntity, false); }
}
