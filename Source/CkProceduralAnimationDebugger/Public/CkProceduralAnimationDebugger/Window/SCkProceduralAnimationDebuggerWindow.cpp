#include "CkProceduralAnimationDebugger/Window/SCkProceduralAnimationDebuggerWindow.h"

#include "CkProceduralAnimationDebugger/CkProceduralAnimationDebugger_Module.h"
#include "CkProceduralAnimationDebugger/Viewport/CkProceduralAnimationDebugger_Preview.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
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

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include "CkProceduralAnimation/Leg/CkProceduralLeg_Utils.h"

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
        const auto Contact = InLeg.Get_Foot().Get_ContactTrusted() ? TEXT("contact")
            : InLeg.Get_Probe().Get_State() == ck::EProceduralFootProbeState::Guessing ? TEXT("grace")
            : TEXT("lost");
        return ck::Format_UE(TEXT("{} / {}"), InLeg.Get_Foot().Get_Planted() ? TEXT("planted") : TEXT("swing"), Contact);
    }

    auto
        Get_RigState(
            const FCk_ProceduralAnimation_DebugLeg& InLeg)
        -> FString
    {
        if (NOT InLeg.Get_Rig().Get_Composed())
        { return TEXT("no rig"); }

        const auto& Rig = InLeg.Get_Rig();
        if (Rig.Get_Failure() != ECk_ProceduralRig_Failure::None)
        { return ck::Format_UE(TEXT("failed: {}"), Rig.Get_Failure()); }

        if (Rig.Get_Status() != ECk_ProceduralAnimation_Status::Ready)
        { return TEXT("waiting for gait"); }

        if (Rig.Get_Clearance() == ECk_ProceduralRig_Clearance::None)
        { return TEXT("ready"); }

        if (Rig.Get_ChainState() == ECk_ProceduralRig_ChainState::Crossing)
        { return ck::Format_UE(TEXT("ready · crossing, {} links through a solid or the body"), Rig.Get_CrossingLinks()); }

        return ck::Format_UE(TEXT("ready · clear, swivel {:.0f} deg"), Rig.Get_SwivelDegrees());
    }

    auto
        Get_SequenceSelectionId(
            uint64 InSequence)
        -> int32
    {
        return static_cast<int32>(FMath::Min<uint64>(InSequence, static_cast<uint64>(MAX_int32)));
    }

    auto
        Get_FootholdSourceName(
            ck::EProceduralFootholdSource InSource)
        -> const TCHAR*
    {
        switch (InSource)
        {
            case ck::EProceduralFootholdSource::None: return TEXT("none");
            case ck::EProceduralFootholdSource::Ideal: return TEXT("ideal");
            case ck::EProceduralFootholdSource::Held: return TEXT("held");
            case ck::EProceduralFootholdSource::Front: return TEXT("front");
            case ck::EProceduralFootholdSource::Inward: return TEXT("inward");
            case ck::EProceduralFootholdSource::Outward: return TEXT("outward");
            case ck::EProceduralFootholdSource::Ring: return TEXT("ring");
        }
        return TEXT("none");
    }

    auto
        Get_FootholdState(
            const FCk_ProceduralAnimation_DebugLeg& InLeg)
        -> FString
    {
        return ck::Format_UE(TEXT("Foothold {} · {} candidates{}"), Get_FootholdSourceName(InLeg.Get_FootholdSource()),
            InLeg.Get_Footholds().Num(), InLeg.Get_PlantOccluded() ? TEXT(" · plant occluded") : TEXT(""));
    }

    auto
        Get_FeetPlaneName(
            ck::EProceduralGaitFeetPlane InFeetPlane)
        -> const TCHAR*
    {
        switch (InFeetPlane)
        {
            case ck::EProceduralGaitFeetPlane::None: return TEXT("none");
            case ck::EProceduralGaitFeetPlane::Fitted: return TEXT("fitted");
            case ck::EProceduralGaitFeetPlane::Held: return TEXT("held");
        }
        return TEXT("none");
    }

    auto
        Get_HeightState(
            const FCk_ProceduralAnimation_DebugSnapshot& InSample)
        -> FString
    {
        const auto RidesFeet = InSample.Get_Motion().Get_HeightSource() == ECk_SurfaceMotion_HeightSource::PlantedFeet;
        return ck::Format_UE(TEXT("Height: {} · feet plane {}"), RidesFeet ? TEXT("planted feet") : TEXT("rays"),
            Get_FeetPlaneName(InSample.Get_Gait().Get_FeetPlane()));
    }

    auto
        Get_WallState(
            const FCk_ProceduralAnimation_DebugSnapshot& InSample)
        -> FString
    {
        const auto& Motion = InSample.Get_Motion();
        const auto Slides = Motion.Get_WallPolicy() == ECk_SurfaceMotion_WallPolicy::Slide;
        const auto Step = Motion.Get_MaxStepHeight() > 0.0f ? ck::Format_UE(TEXT("step {:.0f} cm"), Motion.Get_MaxStepHeight()) : FString{TEXT("no step")};
        const auto Obstructed = Motion.Get_Obstruction() == ECk_SurfaceMotion_Obstruction::Wall;
        return ck::Format_UE(TEXT("Walls: {} · {} · obstruction {}"), Slides ? TEXT("slide") : TEXT("climb"), Step,
            Obstructed ? TEXT("wall") : TEXT("none"));
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_TimelineCache::
    Reset()
    -> void
{
    Events.Reset();
    Spans.Reset();
    SpanSequences.Reset();
    SampleSequences.Reset();
    SpeedSamples->Reset();
    SupportSamples->Reset();
    EntityId.Reset();
    Revision = MAX_uint64;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_TimelineCache::
    EvictBefore(
        uint64 InFirstSequence,
        int32 InFirstSelectionId)
    -> void
{
    const auto EvictedSamples = Algo::LowerBound(SampleSequences, InFirstSequence);
    if (EvictedSamples > 0)
    {
        SampleSequences.RemoveAt(0, EvictedSamples);
        SpeedSamples->RemoveAt(0, EvictedSamples);
        SupportSamples->RemoveAt(0, EvictedSamples);
    }

    const auto EvictedEvents = Algo::LowerBoundBy(Events, InFirstSelectionId,
        [](const FCkDebug_TimelineEvent& InEvent) { return InEvent.SelectionId; });
    if (EvictedEvents > 0)
    { Events.RemoveAt(0, EvictedEvents); }

    const auto EvictedSpans = Algo::LowerBound(SpanSequences, InFirstSequence);
    if (EvictedSpans > 0)
    {
        SpanSequences.RemoveAt(0, EvictedSpans);
        Spans.RemoveAt(0, EvictedSpans);
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
            { Window->DoSelect_LegAt(InLegIndex); }
        });

    _Viewport = SNew(SCkDebug_3dPreviewViewport).Descriptor(FCkDebug3dPreviewDescriptor{}).Adapter(_Preview)
        .SafeAreaOverlay(SNew(SCkDebug_SelectableLabel)
            .Text(FText::FromString(TEXT("Recorded diagnostic rig · goal cubes · actual probe rays / normals · selected leg's foothold candidates"))));
    _Preview->Initialize(_Viewport->Get_PreviewWorld());
    _PreviewSelectedHandle = _Preview->Get_OnLegSelected().AddRaw(this, &SCkProceduralAnimationDebuggerWindow::DoSelect_Leg);
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
        [SNew(SCkDebug_SearchBar).HintText(FText::FromString(TEXT("Filter gait entities")))
            .OnSearchTextChanged_Lambda([WeakWindow](const FString& InText)
            {
                const auto Window = WeakWindow.Pin();
                if (NOT Window.IsValid() || Window->_Released)
                { return; }

                Window->_Filter = InText;
                Window->DoRefresh_Presentation();
            })]
        + SVerticalBox::Slot().FillHeight(1.0f)[_Roster.ToSharedRef()];

    const auto Detail = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceS)
        [SNew(SCkDebug_SelectableLabel).Text_Lambda([WeakWindow]()
        {
            const auto Window = WeakWindow.Pin();
            return Window.IsValid() ? Window->DoGet_DetailText() : FText{};
        })]
        + SVerticalBox::Slot().FillHeight(1.0f)
        [SNew(SSplitter)
            + SSplitter::Slot().Value(0.60f)
            [SNew(SCkDebug_PaneHost).ContentMode(ECkDebugPaneContent::OpaqueRenderer)[_Viewport.ToSharedRef()]]
            + SSplitter::Slot().Value(0.40f)
            [SNew(SCkDebug_PaneHost)
                [SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(CkStyle::SpaceS)[DoBuild_LegActions()]
                    + SVerticalBox::Slot().FillHeight(1.0f)[_Legs.ToSharedRef()]]]]
        + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceS)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [SNew(SCkDebug_SelectableLabel).Text(FText::FromString(TEXT("Speed cm/s")))]
            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(CkStyle::SpaceM, 0.0f)
            [SNew(SCkDebug_Sparkline).Samples(_TimelineCache.SpeedSamples).Color_Lambda([]() { return CkStyle::Info(); })
                .DesiredSize(FVector2D{200.0, 35.0})]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [SNew(SCkDebug_SelectableLabel).Text(FText::FromString(TEXT("Trusted feet")))]
            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(CkStyle::SpaceM, 0.0f)
            [SNew(SCkDebug_Sparkline).Samples(_TimelineCache.SupportSamples).Color_Lambda([]() { return CkStyle::Ok(); })
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
            return Window.IsValid() ? Window->DoGet_StatusText() : FText{};
        })
        .ToolbarContent()[Toolbar]
        .CommonActionsContent()[SNew(SCkDebug_ViewportPickerControls).Picker(_Picker)
            .PickTooltip(FText::FromString(TEXT("Pick a procedural gait entity, one of its legs or an owned limb.")))]
        .Content()[SNew(SSplitter)
            + SSplitter::Slot().Value(0.24f)[SNew(SCkDebug_PaneHost)[Sidebar]]
            + SSplitter::Slot().Value(0.76f)[SNew(SCkDebug_PaneHost)[Detail]]]];

    _ModelChangedHandle = _Model->Get_OnChanged().AddRaw(this, &SCkProceduralAnimationDebuggerWindow::DoRefresh_Presentation);
    _WorldChangedHandle = _WorldModel->OnWorldChanged.AddLambda([WeakModel](UWorld* InWorld)
    {
        if (const auto Model = WeakModel.Pin())
        { Model->Set_World(InWorld); }
    });
    _WorldModel->Ensure_AutoSelect();
    _Model->Set_World(_WorldModel->Get_SelectedWorld());
    DoRefresh_Presentation();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    DoBuild_LegActions()
    -> TSharedRef<SWidget>
{
    const auto WeakModel = TWeakPtr<FCkProceduralAnimationDebugger_Model>{_Model};
    const auto HasSelectedLeg = [WeakModel]() -> bool
    {
        const auto Model = WeakModel.Pin();
        return Model.IsValid() && ck::IsValid(Model->Get_SelectedLeg());
    };

    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth()
        [SNew(SButton).Tag(TEXT("ProceduralAnimation.EnableDisableLeg"))
            .Text_Lambda([WeakModel]()
            {
                const auto Model = WeakModel.Pin();
                const auto Leg = Model.IsValid() ? Model->Get_SelectedLeg() : FCk_Handle_ProceduralLeg{};
                const auto LegIsDisabled = ck::IsValid(Leg)
                    && UCk_Utils_ProceduralLeg_UE::Get_EnableDisable(Leg) == ECk_EnableDisable::Disable;
                return FText::FromString(LegIsDisabled ? TEXT("Enable leg") : TEXT("Disable leg"));
            })
            .ToolTipText(FText::FromString(TEXT(
                "Disable the selected leg: it leaves the step schedule and rides rigidly with the body, like a dead limb. "
                "Press again to enable it; it swings back from where it hangs.")))
            .IsEnabled_Lambda(HasSelectedLeg)
            .OnClicked_Lambda([WeakModel]()
            {
                const auto Model = WeakModel.Pin();
                const auto Leg = Model.IsValid() ? Model->Get_SelectedLeg() : FCk_Handle_ProceduralLeg{};
                if (ck::Is_NOT_Valid(Leg))
                { return FReply::Handled(); }

                Model->Request_EnableDisableSelectedLeg(UCk_Utils_ProceduralLeg_UE::Get_EnableDisable(Leg) == ECk_EnableDisable::Disable
                    ? ECk_EnableDisable::Enable
                    : ECk_EnableDisable::Disable);
                return FReply::Handled();
            })]
        + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceS, 0.0f)
        [SNew(SButton).Tag(TEXT("ProceduralAnimation.DetachLeg")).Text(FText::FromString(TEXT("Detach leg")))
            .ToolTipText(FText::FromString(TEXT(
                "Detach the selected leg. Its parts are released through OnProceduralLeg_Detached, where the game decides "
                "whether they ragdoll, and the survivors adapt per the gait's leg-loss policy. Irreversible; the parts "
                "stay owned by the body.")))
            .IsEnabled_Lambda(HasSelectedLeg)
            .OnClicked_Lambda([WeakModel]()
            {
                if (const auto Model = WeakModel.Pin())
                { Model->Request_DetachSelectedLeg(ECk_ProceduralLeg_ReleasedPartsOwnership::KeepBodyOwned); }
                return FReply::Handled();
            })];
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
    DoRefresh_Presentation()
    -> void
{
    if (_Released || NOT _Model.IsValid() || NOT _Roster.IsValid())
    { return; }

    if (ck::Is_NOT_Valid(_Model->Get_World()))
    { _Picker->Deactivate(); }

    if (ck::IsValid(_Model->Get_World()) && _WorldModel->Get_SelectedWorld() != _Model->Get_World())
    { _WorldModel->Set_SelectedWorld(_Model->Get_World()); }

    DoRefresh_Roster();

    const auto* Sample = _Model->Get_History().Get_Displayed();
    if (Sample == nullptr)
    {
        _Legs->Clear_Items();
        _Preview->Reset();
        _TimelineCache.Reset();
        _FramedEntity.Reset();
        if (_Timeline.IsValid())
        { _Timeline->Set_Content(0.0, 1.0, {}, {}); }
        return;
    }

    DoRefresh_Legs(*Sample);
    _Preview->Show(*Sample, _Model->Get_SelectedLegId());
    if (_FramedEntity != Sample->Get_EntityId())
    {
        _FramedEntity = Sample->Get_EntityId();
        _Viewport->Apply_CameraPreset(ECkDebug3dCameraPreset::FrameAll);
    }
    DoRefresh_Timeline();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    DoRefresh_Roster()
    -> void
{
    auto Items = TArray<FCkDebug_EntityHealthItem>{};
    for (const auto& Row : _Model->Get_Rows())
    {
        const auto& Summary = Row.Summary;
        if (NOT _Filter.IsEmpty() && NOT Row.Label.Contains(_Filter) && NOT Summary.Get_EntityId().Contains(_Filter))
        { continue; }

        const auto Failed = Summary.Get_GaitStatus() == ECk_ProceduralAnimation_Status::Failed
            || (Summary.Get_HasRig() && Summary.Get_RigStatus() == ECk_ProceduralAnimation_Status::Failed);
        const auto Tracking = Summary.Get_GaitStatus() == ECk_ProceduralAnimation_Status::Ready;
        auto Item = FCkDebug_EntityHealthItem{};
        Item.RowIdentity = Row.Entity;
        Item.SelectionTarget = Row.Entity;
        Item.Name = FText::FromString(Row.Label);
        Item.Context = FText::FromString(Summary.Get_EntityId());
        Item.Summary = FText::FromString(ck::Format_UE(TEXT("{} legs · {} enabled · {} planted"),
            Summary.Get_LegCount(), Summary.Get_EnabledLegCount(), Summary.Get_PlantedCount()));
        Item.Status = FText::FromString(Failed ? TEXT("Failed") : Tracking ? TEXT("Tracking") : TEXT("Pending"));
        Item.Tone = Failed ? ECk_Tone::Err : Tracking ? ECk_Tone::Ok : ECk_Tone::Warn;
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
    DoRefresh_Legs(
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
        Item.Tone = NOT Leg.Get_Enabled() ? ECk_Tone::Neutral : Leg.Get_Foot().Get_ContactTrusted() ? ECk_Tone::Ok : ECk_Tone::Warn;
        Item.RightLabel = FText::FromString(ck::Format_UE(TEXT("{:.0f}%"), Leg.Get_Foot().Get_SwingAlpha() * 100.0f));
        Item.Detail = FText::FromString(ck::Format_UE(
            TEXT("{} · phase {:.2f} · error {:.1f}/{:.1f} cm\nProbe {}: hit {} · fraction {:.4f} · missing {:.3f}s\n{}\nRig {}"),
            EnabledState, Leg.Get_Foot().Get_PhaseOffset(), FVector::Distance(Leg.Get_Foot().Get_PlantedPosition(), Leg.Get_Targeting().Get_IdealTarget()),
            Leg.Get_Targeting().Get_StepThreshold(), Leg.Get_Probe().Get_AttemptCount(), Leg.Get_Probe().Get_Hit(), Leg.Get_Probe().Get_HitFraction(),
            Leg.Get_Probe().Get_MissingContact().Get_Seconds(), ck_procedural_debug_window::Get_FootholdState(Leg),
            ck_procedural_debug_window::Get_RigState(Leg)));
        Item.CopyText = ck::Format_UE(TEXT("{}\n{}\n{}\nfoot {}\ntarget {}\nnormal {}"), Item.Key, Item.Headline.ToString(),
            Item.Detail.ToString(), Leg.Get_Foot().Get_Position().ToString(), Leg.Get_Targeting().Get_IdealTarget().ToString(), Leg.Get_Foot().Get_Normal().ToString());
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
    DoRefresh_Timeline()
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

        DoRecreate_Timeline(Labels);
    }

    if (_TimelineCache.Revision == _Model->Get_HistoryRevision())
    { return; }

    _TimelineCache.Revision = _Model->Get_HistoryRevision();
    const auto* First = History.Get_Sample(0);
    const auto* Last = History.Get_Sample(History.Get_Count() - 1);
    if (First == nullptr || Last == nullptr)
    {
        _TimelineCache.Reset();
        _Timeline->Set_Content(0.0, 1.0, {}, {});
        return;
    }

    if (First->Get_EntityId() != _TimelineCache.EntityId)
    {
        _TimelineCache.Reset();
        _TimelineCache.EntityId = First->Get_EntityId();
    }

    const auto FirstSequence = First->Get_Sample().Get_Sequence();
    _TimelineCache.EvictBefore(FirstSequence, ck_procedural_debug_window::Get_SequenceSelectionId(FirstSequence));

    const auto LastPresented = _TimelineCache.SampleSequences.IsEmpty() ? uint64{0} : _TimelineCache.SampleSequences.Last();
    for (auto Index = 0; Index < History.Get_Count(); ++Index)
    {
        if (History.Get_Sample(Index)->Get_Sample().Get_Sequence() > LastPresented)
        { DoAppend_TimelineSample(Index); }
    }

    const auto StartSeconds = First->Get_Sample().Get_Time().Get_Seconds();
    const auto EndSeconds = FMath::Max(StartSeconds + 0.001, Last->Get_Sample().Get_Time().Get_Seconds());
    _Timeline->Set_Content(StartSeconds, EndSeconds, _TimelineCache.Events, _TimelineCache.Spans);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    DoRecreate_Timeline(
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
            return Sample != nullptr ? TOptional<double>{Sample->Get_Sample().Get_Time().Get_Seconds()} : TOptional<double>{};
        })
        .OnScrubbed_Lambda([WeakWindow](double InTime)
        {
            if (const auto Window = WeakWindow.Pin())
            { Window->DoScrub_Time(InTime); }
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
    _TimelineCache.Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    DoAppend_TimelineSample(
        int32 InChronologicalIndex)
    -> void
{
    const auto& History = _Model->Get_History();
    const auto* Sample = History.Get_Sample(InChronologicalIndex);
    const auto* Previous = History.Get_Sample(InChronologicalIndex - 1);
    const auto Sequence = Sample->Get_Sample().Get_Sequence();
    // A band only joins adjacent simulation frames. A gated capture must not invent
    // a continuous plant or swing across frames it did not observe.
    const auto JoinsPrevious = Previous != nullptr && Previous->Get_Sample().Get_Sequence() + 1 == Sequence;
    const auto SampleSeconds = Sample->Get_Sample().Get_Time().Get_Seconds();
    const auto SelectionId = ck_procedural_debug_window::Get_SequenceSelectionId(Sequence);

    auto TrustedCount = 0;
    for (auto LegIndex = 0; LegIndex < Sample->Get_Legs().Num(); ++LegIndex)
    {
        const auto& Leg = Sample->Get_Legs()[LegIndex];
        if (Leg.Get_LegEntityId().IsEmpty())
        { continue; }

        TrustedCount += Leg.Get_Foot().Get_ContactTrusted() ? 1 : 0;
        const auto Color = NOT Leg.Get_Foot().Get_ContactTrusted() ? CkStyle::Warn() : Leg.Get_Foot().Get_Planted() ? CkStyle::Ok() : CkStyle::Info();

        auto Event = FCkDebug_TimelineEvent{};
        Event.LaneIndex = LegIndex;
        Event.TimeSeconds = SampleSeconds;
        Event.Shape = ECkDebug_TimelineMarker::Diamond;
        Event.Color = Color;
        Event.Tooltip = ck::Format_UE(TEXT("{} · sample {} · {}"), Leg.Get_Id(), Sequence, ck_procedural_debug_window::Get_LegState(Leg));
        Event.SelectionId = SelectionId;
        _TimelineCache.Events.Add(MoveTemp(Event));

        // Samples are gated, so a plant or lift is only known to have happened somewhere between two observations.
        const auto* PreviousLeg = Previous != nullptr && Previous->Get_Legs().IsValidIndex(LegIndex) ? &Previous->Get_Legs()[LegIndex] : nullptr;
        if (PreviousLeg != nullptr && PreviousLeg->Get_Id() == Leg.Get_Id()
            && PreviousLeg->Get_Foot().Get_Planted() != Leg.Get_Foot().Get_Planted())
        {
            auto Footfall = FCkDebug_TimelineEvent{};
            Footfall.LaneIndex = LegIndex;
            Footfall.TimeSeconds = SampleSeconds;
            Footfall.Shape = ECkDebug_TimelineMarker::Square;
            Footfall.Color = Leg.Get_Foot().Get_Planted() ? CkStyle::Ok() : CkStyle::Info();
            Footfall.Tooltip = ck::Format_UE(TEXT("{} {} between samples {} and {}"), Leg.Get_Id(),
                Leg.Get_Foot().Get_Planted() ? TEXT("planted") : TEXT("lifted"), Previous->Get_Sample().Get_Sequence(), Sequence);
            Footfall.SelectionId = SelectionId;
            _TimelineCache.Events.Add(MoveTemp(Footfall));
        }

        if (JoinsPrevious)
        {
            _TimelineCache.Spans.Add({LegIndex, Previous->Get_Sample().Get_Time().Get_Seconds(), Sample->Get_Sample().Get_Time().Get_Seconds(), Color,
                TEXT("Adjacent observed frames")});
            _TimelineCache.SpanSequences.Add(Sequence);
        }
    }

    _TimelineCache.SpeedSamples->Add(Sample->Get_Gait().Get_Velocity().Size());
    _TimelineCache.SupportSamples->Add(TrustedCount);
    _TimelineCache.SampleSequences.Add(Sequence);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    DoSelect_Leg(
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
    DoSelect_LegAt(
        int32 InLegIndex)
    -> void
{
    if (_Released || NOT _Model.IsValid())
    { return; }

    const auto* Sample = _Model->Get_History().Get_Displayed();
    if (Sample == nullptr || NOT Sample->Get_Legs().IsValidIndex(InLegIndex))
    { return; }

    DoSelect_Leg(Sample->Get_Legs()[InLegIndex].Get_LegEntityId());
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    DoScrub_Time(
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
        const auto Delta = FMath::Abs(History.Get_Sample(Index)->Get_Sample().Get_Time().Get_Seconds() - InSeconds);
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
    DoGet_StatusText() const
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
    DoGet_DetailText() const
    -> FText
{
    const auto* Sample = _Model.IsValid() ? _Model->Get_History().Get_Displayed() : nullptr;
    if (Sample == nullptr)
    { return FText::FromString(TEXT("Select a gait entity. Capture starts when its first accepted solve is observed.")); }

    const auto* Status = _Model->Get_History().Get_IsLive() ? _Model->Get_LiveStatus() : Sample;
    const auto Detail = ck::Format_UE(TEXT("{} · t {:.3f}s · sample {} · cadence {:.2f}x · rays {}/solve · {}{}"),
        Sample->Get_EntityName(), Sample->Get_Sample().Get_Time().Get_Seconds(), Sample->Get_Sample().Get_Sequence(), Sample->Get_Gait().Get_CadenceScale(),
        Sample->Get_Gait().Get_RaysLastSolve(), Sample->Get_Gait().Get_Airborne() ? TEXT("Airborne") : TEXT("Grounded"),
        Status != nullptr && Status->Get_Status().Get_GaitStatus() == ECk_ProceduralAnimation_Status::Failed ? TEXT(" · GAIT FAILED")
            : Sample->Get_Freshness().Get_RigPosePending() || NOT Sample->Get_Freshness().Get_RigMatchesGaitSequence() ? TEXT(" · rig pose not synchronized")
            : TEXT(""));

    const auto& BodyPose = Sample->Get_BodyPose();
    const auto BodyPoseLine = BodyPose.Get_Composed()
        ? ck::Format_UE(TEXT("Body pose: {}, drop {:.1f} cm, tilt {:.1f} deg"), BodyPose.Get_Status(), -BodyPose.Get_Offset().GetLocation().Z,
            FMath::RadiansToDegrees(BodyPose.Get_Offset().GetRotation().AngularDistance(FQuat::Identity)))
        : FString{TEXT("Body pose: none")};
    if (NOT Sample->Get_Status().Get_HasSurfaceMotion())
    { return FText::FromString(ck::Format_UE(TEXT("{}\n{}"), Detail, BodyPoseLine)); }

    return FText::FromString(ck::Format_UE(TEXT("{}\n{}\n{}\n{}"), Detail, BodyPoseLine, ck_procedural_debug_window::Get_HeightState(*Sample),
        ck_procedural_debug_window::Get_WallState(*Sample)));
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
    Resolve_ProceduralEntity(
        const FCk_Handle& InEntity)
    -> FCk_Handle
{
    return ck::DebugSelectionSync::Resolve_ClosestLineageMatch(InEntity, [](const FCk_Handle& InCandidate) -> bool
    {
        return FCkProceduralAnimationDebugger_DataCollector::Is_Supported(InCandidate);
    });
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAnimationDebuggerWindow::
    Is_ProceduralEntity(
        const FCk_Handle& InEntity)
    -> bool
{
    // The picker previews each match's owner chain itself, so this filter only has to accept a gait body and its
    // descendants (legs, limb parts). Walking up, not searching the lineage both ways, keeps it cheap per gathered entity.
    constexpr auto MaxAncestorDepth = 64;
    if (ck::Is_NOT_Valid(InEntity))
    { return false; }

    const auto Transient = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(InEntity);
    auto Candidate = InEntity;
    for (auto Depth = 0; Depth < MaxAncestorDepth; ++Depth)
    {
        if (FCkProceduralAnimationDebugger_DataCollector::Is_Supported(Candidate))
        { return true; }

        if (NOT Candidate.Has<ck::FFragment_LifetimeOwner>())
        { return false; }

        Candidate = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Candidate);
        if (ck::Is_NOT_Valid(Candidate) || Candidate == Transient)
        { return false; }
    }

    return false;
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
