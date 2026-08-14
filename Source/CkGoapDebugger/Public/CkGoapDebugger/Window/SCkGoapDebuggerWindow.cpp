#include "CkGoapDebugger/Window/SCkGoapDebuggerWindow.h"

#include "CkGoapDebugger/CkGoapDebugger_Module.h"
#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/Data/CkGoapDebugger_DataCollector.h"
#include "CkGoapDebugger/Data/CkGoapDebugger_DecisionModel.h"
#include "CkGoapDebugger/Data/CkGoapDebugger_Targeting.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_AgentColumn.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_AgentListPanel.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_CatalogPanel.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_DecisionPanel.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_GraphPane.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_SearchTracePanel.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_Sidebar.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_SquadTable.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_TimelineDock.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_WorldStateRail.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/EditorOnly/CkEditorOnly_Utils.h"

#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkGoap/WorldState/CkGoap_WorldState_Utils.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"
#include "CkDebuggerCommon/Styles/CkDebuggerCommonStyle.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkDebuggerCommon/Picker/CkDebug_ViewportPicker.h"
#include "CkDebuggerCommon/Picker/SCkDebug_ViewportPickerControls.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_AlertRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_MeterBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameDepthCycler.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkGoapDebugger/CkGoapDebugger_Axes.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Switch.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_UnderlineTabs.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Text/STextBlock.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

#if WITH_EDITOR
    #include "Editor.h"
#endif

// ====================================================================================================================

const FName SCkGoapDebuggerWindow::WindowId = FName(TEXT("GoapDebugger"));

const FName SCkGoapDebuggerWindow::Tab_Squad     = FName(TEXT("Squad"));
const FName SCkGoapDebuggerWindow::Tab_Inspector = FName(TEXT("Inspector"));
const FName SCkGoapDebuggerWindow::Tab_Catalog   = FName(TEXT("Catalog"));

const FName SCkGoapDebuggerWindow::CTab_Decision = FName(TEXT("Decision"));
const FName SCkGoapDebuggerWindow::CTab_Graph    = FName(TEXT("Graph"));
const FName SCkGoapDebuggerWindow::CTab_Search   = FName(TEXT("Search"));

// ====================================================================================================================
// LIFETIME
// ====================================================================================================================

SCkGoapDebuggerWindow::~SCkGoapDebuggerWindow()
{
    if (_WorldModel.IsValid() && _WorldChangedHandle.IsValid())
    { _WorldModel->OnWorldChanged.Remove(_WorldChangedHandle); }

    if (_SessionInvalidatedHandle.IsValid())
    { ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(_SessionInvalidatedHandle); }
}

auto
    SCkGoapDebuggerWindow::
    HandleWorldTornDown()
    -> void
{
    // Stop picker reads first — it holds a sticky focus handle and reads the
    // marker snapshot every tick.
    if (_ViewportPicker.IsValid())
    { _ViewportPicker->Deactivate(); }

    _CachedWorld.Reset();
    FCkGoapDebugger_DataCollector::Reset_ForWorldChange();

    // Chrome picker arrays hold FCk_Handle copies — drop them while the
    // registry lives.
    _AgentPickerLabels.Reset();
    _AgentPickerHandles.Reset();
    _PlannerPickerLabels.Reset();
    _PlannerPickerHandles.Reset();
    if (_AgentPicker.IsValid())   { _AgentPicker->RefreshOptions(); }
    if (_PlannerPicker.IsValid()) { _PlannerPicker->RefreshOptions(); }

    // Agent rows hold FCk_Handle copies — drop them while the registry lives.
    if (_AgentList.IsValid())
    { _AgentList->Reset_ForWorldChange(); }

    if (_Sidebar.IsValid())
    { _Sidebar->Reset_ForWorldChange(); }

    if (_AgentColumn.IsValid())
    { _AgentColumn->Reset_ForWorldChange(); }

    if (_DecisionPanel.IsValid())
    { _DecisionPanel->Reset_ForWorldChange(); }

    if (_TimelineDock.IsValid())
    { _TimelineDock->Reset_ForWorldChange(); }

    if (_SquadTable.IsValid())
    { _SquadTable->Reset_ForWorldChange(); }

    if (_CatalogPanel.IsValid())
    { _CatalogPanel->Reset_ForWorldChange(); }

    // Clear the graph BEFORE the ViewModel resets — graph node snapshots
    // hold FCk_Handle copies, and they must be released while the registry
    // is still live.
    if (_GraphPane.IsValid())
    { _GraphPane->Reset_ForWorldChange(); }

    if (_ViewModel.IsValid())
    { _ViewModel->Reset_ForWorldChange(); }
}

auto SCkGoapDebuggerWindow::HandleWorldChanged(UWorld*) -> void
{
    HandleWorldTornDown();
}

auto SCkGoapDebuggerWindow::Request_PauseExecution() -> void
{
#if WITH_EDITOR
    // Keep the PIE breakpoint pause semantics unchanged while proving both
    // prerequisites before entering the editor-only helper.
    if (GEditor != nullptr && GEditor->PlayWorld != nullptr)
    { UCk_Utils_EditorOnly_UE::Request_DebugPauseExecution(); }
#else
    auto* World = _CachedWorld.Get();
    const auto IsWorldValid = ck::IsValid(World);
    CK_ENSURE_IF_NOT(IsWorldValid, TEXT("GOAP pause-on event requires the selected runtime world"))
    {}
    if (NOT IsWorldValid)
    { return; }

    UGameplayStatics::SetGamePaused(World, true);
#endif
}

// ====================================================================================================================
// CONSTRUCT
// ====================================================================================================================

auto
    SCkGoapDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    Register_WithGate();

    _ViewModel = MakeShared<FCkGoapDebugger_ViewModel>();
    _WorldModel = MakeShared<FCkDebuggerModel_WorldSelector>();

    // Shared viewport picker, specialized to GOAP: only roster entities (plus
    // their owner chain up to the NPC representative) are previewed/pickable.
    // A picked entity resolves through the lineage-aware roster resolver, so
    // clicking the NPC root or the planner sub-entity both land on the roster
    // agent.
    _ViewportPicker = MakeShared<FCkDebug_ViewportPicker>();
    {
        auto PickerParams = FCkDebug_ViewportPicker::FParams{};
        PickerParams.Get_TargetWorld =
            [WeakWorld = TWeakPtr<FCkDebuggerModel_WorldSelector>(_WorldModel)]() -> UWorld*
            {
                const auto Pinned = WeakWorld.Pin();
                return Pinned.IsValid() ? Pinned->Get_SelectedWorld() : nullptr;
            };
        PickerParams.TargetFilter =
            [](const FCk_Handle& InCandidate)
            {
                return ck_goap_debugger::IsGoapRosterEntity(InCandidate);
            };
        PickerParams.OnEntityPicked =
            [](const FCk_Handle& InPicked)
            {
                const auto Resolved = ck_goap_debugger::ResolveGoapTarget(InPicked);
                if (ck::Is_NOT_Valid(Resolved))
                { return; }

                // Adopt into other open debuggers, then re-front this tab with
                // the resolved agent selected (the game viewport took focus
                // while picking).
                ck::DebugSelectionSync::Broadcast(Resolved, TEXT("GoapDebugger"));
                SCkGoapDebuggerWindow::OpenForEntity(Resolved);
            };
        _ViewportPicker->Construct(MoveTemp(PickerParams));
    }

    _WorldChangedHandle = _WorldModel->OnWorldChanged.AddSP(
        this, &SCkGoapDebuggerWindow::HandleWorldChanged);
    _SessionInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddSP(
        this, &SCkGoapDebuggerWindow::HandleWorldTornDown);

    _ActiveTab = Tab_Inspector;
    _CenterTab = CTab_Decision;

    SAssignNew(_Sidebar, SCkGoapDebugger_Sidebar, _ViewModel);
    SAssignNew(_AgentColumn, SCkGoapDebugger_AgentColumn)
        .ViewModel(_ViewModel);
    SAssignNew(_AgentList, SCkGoapDebugger_AgentListPanel)
        .ViewModel(_ViewModel);
    SAssignNew(_WorldStateRail, SCkGoapDebugger_WorldStateRail)
        .ViewModel(_ViewModel);

    auto SquadView     = BuildSquadView();
    auto InspectorView = BuildInspectorView();
    auto CatalogView   = BuildCatalogView();

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome)
            .WindowId(WindowId)
            .ToolTabId(TEXT("CkGoapDebugger"))
            .DisplayName(FText::FromString(TEXT("CK GOAP Debugger")))
            .MenuActionsContent()
            [
                BuildMenuActions()
            ]
            .Content()
            [
                SNew(SBorder)
            .BorderImage(CkStyle::GetFilledBrush())
            .BorderBackgroundColor(FSlateColor(CkStyle::Bg1()))
            .Padding(FMargin(0.0f))
            [
                SNew(SVerticalBox)

                    // Chrome bar (pickers, transport, nerd toggle)
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            BuildChromeBar()
                        ]

                    // Nerd strip — search internals; only in nerd mode.
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            BuildNerdStrip()
                        ]

                    // Alert strip — sandbox banner + fallback-active warning.
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            BuildAlertStrip()
                        ]

                    // Top tabs: Squad / Agent Inspector / Catalog Audit
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            BuildTopTabs()
                        ]

                    // Active view
                    + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        [
                            SNew(SWidgetSwitcher)
                                .WidgetIndex_Lambda([this]() -> int32
                                {
                                    if (_ActiveTab == Tab_Squad)   { return 0; }
                                    if (_ActiveTab == Tab_Catalog) { return 2; }
                                    return 1;
                                })

                                + SWidgetSwitcher::Slot() [ SquadView ]
                                + SWidgetSwitcher::Slot() [ InspectorView ]
                                + SWidgetSwitcher::Slot() [ CatalogView ]
                        ]
            ]
            ]
    ];

    // Subscribe to ViewModel changes so the sidebar's structural-hash check
    // gets a chance to fire on selection / snapshot changes.
    if (_ViewModel.IsValid())
    {
        _ViewModel->OnChanged.AddLambda([this]()
        {
            RefreshPickers();
            if (_Sidebar.IsValid())
            { _Sidebar->RefreshFromViewModel(); }
            if (_AgentColumn.IsValid())
            { _AgentColumn->RefreshFromViewModel(); }
            if (_DecisionPanel.IsValid())
            { _DecisionPanel->RefreshFromViewModel(); }
            if (_SearchTracePanel.IsValid())
            { _SearchTracePanel->RefreshFromViewModel(); }
            if (_TimelineDock.IsValid())
            { _TimelineDock->RefreshFromViewModel(); }
            if (_SquadTable.IsValid())
            { _SquadTable->RefreshFromViewModel(); }
            if (_CatalogPanel.IsValid())
            { _CatalogPanel->RefreshFromViewModel(); }
            if (_WorldStateRail.IsValid())
            { _WorldStateRail->RefreshFromViewModel(); }
        });
    }
}

// ====================================================================================================================
// TICK
// ====================================================================================================================

auto
    SCkGoapDebuggerWindow::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    // MUST be the direct base, not SCompoundWidget: the base's Tick is what polls the Layer-B style
    // revision behind the refresh gate. Calling the grandparent kills live style-apply silently.
    SCkDebugger_WindowBase::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    // Viewport-picker ticks stay ungated so input handling keeps working even
    // when the panel refresh is paused.
    if (_ViewportPicker.IsValid() && _ViewportPicker->IsActive())
    { _ViewportPicker->Tick(InDeltaTime); }

    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    _WorldModel->Ensure_AutoSelect();
    _CachedWorld = _WorldModel->Get_SelectedWorld();

    auto* World = _CachedWorld.Get();
    if (ck::Is_NOT_Valid(World) || NOT World->HasBegunPlay())
    { return; }

    _ViewModel->Tick(World);

    if (_PendingExternalEntity.IsSet())
    {
        const auto TargetEntity = _PendingExternalEntity.GetValue();
        _PendingExternalEntity.Reset();
        const auto* RosterEntry = _ViewModel->Get_Roster().FindByPredicate(
            [TargetEntity](const FCkGoapDebugger_RosterEntry& InEntry)
            { return InEntry.EntityHandle.Get_Entity() == TargetEntity; });
        if (RosterEntry != nullptr)
        {
            _ViewModel->SetSelectedEntity(RosterEntry->EntityHandle);
            // One extra targeted collection makes the first-open jump complete
            // in this frame instead of leaving the detail tier one tick behind.
            _ViewModel->Tick(World);
        }
    }

    RefreshAgentList();
    if (_AgentList.IsValid())
    { _AgentList->RestoreSelectionFromViewModel(); }
}

// ====================================================================================================================
// STYLE LIVE-APPLY
// ====================================================================================================================

auto
    SCkGoapDebuggerWindow::
    OnStyleRevisionChanged()
    -> void
{
    // Mission Control's panels are hash-debounced rebuild-on-data surfaces: with PIE at rest their
    // content hash never moves, so an axis flip would sit invisible until the next replan. Flip each
    // panel's debounce and re-run its normal refresh — same code path as a data change, no new
    // rebuild entry points.
    //
    // GraphPane needs an in-place geometry pass because its runtime canvas owns fixed card bounds.
    // That path preserves the model and card widgets, so axis changes update fit/wire geometry
    // without taking the destructive topology rebuild path.
    if (_GraphPane.IsValid())         { _GraphPane->Refresh_ForStyleChange(); }
    if (_Sidebar.IsValid())          { _Sidebar->Invalidate_StyleCache(); }
    if (_AgentColumn.IsValid())      { _AgentColumn->Invalidate_StyleCache(); }
    if (_DecisionPanel.IsValid())    { _DecisionPanel->Invalidate_StyleCache(); }
    if (_SearchTracePanel.IsValid()) { _SearchTracePanel->Invalidate_StyleCache(); }
    if (_TimelineDock.IsValid())     { _TimelineDock->Invalidate_StyleCache(); }
    if (_SquadTable.IsValid())       { _SquadTable->Invalidate_StyleCache(); }
    if (_CatalogPanel.IsValid())     { _CatalogPanel->Invalidate_StyleCache(); }
    if (_WorldStateRail.IsValid())   { _WorldStateRail->Invalidate_StyleCache(); }
}

// ====================================================================================================================
// BUILD — CHROME BAR (Mission Control)
// ====================================================================================================================

auto
    SCkGoapDebuggerWindow::
    BuildChromeBar()
    -> TSharedRef<SWidget>
{
    // Enter Scrub mode at the given history index (clamped); INDEX_NONE-safe.
    const auto ScrubTo = [this](int32 InIndex) -> void
    {
        if (NOT _ViewModel.IsValid()) { return; }
        const auto Entity = _ViewModel->GetSelectedEntity();
        if (NOT ck::IsValid(Entity)) { return; }

        const auto& Hist = FCkGoapDebugger_DataCollector::GetHistory(Entity);
        if (Hist.IsEmpty()) { return; }

        const auto Clamped = FMath::Clamp(InIndex, 0, Hist.Num() - 1);
        _ViewModel->SetScrubEventIndex(Clamped);
        if (ck::IsValid(Hist[Clamped].ActionSetHandle))
        { _ViewModel->SetSelectedActionSet(Hist[Clamped].ActionSetHandle); }
        _ViewModel->SetMode(FCkGoapDebugger_ViewModel::EMode::Scrub);
    };

    return SNew(SBorder)
        .BorderImage(CkStyle::GetFilledBrush())
        .BorderBackgroundColor(FSlateColor(CkStyle::Bg2()))
        .Padding(FMargin(CkStyle::SpaceL, CkStyle::SpaceM))
        [
            SNew(SHorizontalBox)

                // Product mark
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, CkStyle::SpaceL, 0.0f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                            [
                                SNew(SBox)
                                    .WidthOverride_Lambda([]() -> FOptionalSize
                                    { return FOptionalSize{ck_goap_debugger_axes::Get_DotSize()}; })
                                    .HeightOverride_Lambda([]() -> FOptionalSize
                                    { return FOptionalSize{ck_goap_debugger_axes::Get_DotSize()}; })
                                [
                                    SNew(SImage)
                                        .Image(CkStyle::GetRoundedBrush_Small())
                                        .ColorAndOpacity(FSlateColor(CkStyle::Accent()))
                                ]
                            ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("GOAP Mission Control")))
                                    .Font_Lambda([]() -> FSlateFontInfo
                                    { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeBody()); })
                                    .ColorAndOpacity(FSlateColor(CkStyle::Text()))
                            ]
                    ]

                // World selector (shared across all CK debuggers)
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, CkStyle::SpaceL, 0.0f)
                    [
                        SNew(SCkDebug_WorldSelector, _WorldModel)
                            .ShowHeaderLabel(false)
                    ]

                // Agent picker — the mockup's chrome agent dropdown.
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                    [
                        SAssignNew(_AgentPicker, STextComboBox)
                            .OptionsSource(&_AgentPickerLabels)
                            .OnSelectionChanged(this, &SCkGoapDebuggerWindow::HandleAgentPicked)
                            .ToolTipText(FText::FromString(TEXT("Select the agent under inspection.")))
                            .Font_Lambda([]() -> FSlateFontInfo
                            { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall()); })
                    ]

                // Planner picker — top-level Planners on the selected agent
                // (+sub count); the Sidebar tree drills into sub-Planners.
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                    [
                        SAssignNew(_PlannerPicker, STextComboBox)
                            .OptionsSource(&_PlannerPickerLabels)
                            .OnSelectionChanged(this, &SCkGoapDebuggerWindow::HandlePlannerPicked)
                            .ToolTipText(FText::FromString(TEXT("Select a top-level Planner on this agent. Sub-Planners live in the Inspector tree.")))
                            .Font_Lambda([]() -> FSlateFontInfo
                            { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall()); })
                    ]

                // Selected agent pill (ID|Version) — click navigates to ECS debugger
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, CkStyle::SpaceL, 0.0f)
                    [
                        SNew(SCkDebug_EntityRef)
                            .Entity_Lambda([this]() -> FCk_Handle
                            {
                                if (NOT _ViewModel.IsValid()) { return FCk_Handle{}; }
                                return _ViewModel->GetSelectedEntity();
                            })
                    ]

                // Viewport picker (shared) — click a GOAP agent in the world.
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, CkStyle::SpaceL, 0.0f)
                    [
                        SNew(SCkDebug_ViewportPickerControls)
                            .Picker(_ViewportPicker)
                            .PickTooltip(FText::FromString(TEXT(
                                "Enter pick mode: click a GOAP agent in the viewport to inspect it.\n"
                                "Only entities with GOAP (and their owning NPC) are shown and pickable.")))
                    ]

                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)

                // Transport: LIVE / step-back / step-forward — the mockup's
                // timeline transport. LIVE returns to live; steps scrub the
                // history ring.
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                    [
                        SNew(SButton)
                            .ButtonStyle(&FCkDebuggerCommonStyle::Get_FlatButtonStyle())
                            .ContentPadding(FMargin(2.0f))
                            .ToolTipText(FText::FromString(TEXT("Live — follow the game. Scrubbing the timeline pauses on a recorded event.")))
                            .OnClicked_Lambda([this]() -> FReply
                            {
                                if (_ViewModel.IsValid())
                                {
                                    _ViewModel->SetMode(FCkGoapDebugger_ViewModel::EMode::Live);
                                    _ViewModel->SetScrubEventIndex(INDEX_NONE);
                                }
                                return FReply::Handled();
                            })
                            [
                                SNew(SCkDebug_StatusPill)
                                    .Text_Lambda([this]() -> FText
                                    {
                                        const auto IsLive = NOT _ViewModel.IsValid()
                                            || _ViewModel->GetMode() == FCkGoapDebugger_ViewModel::EMode::Live;
                                        if (IsLive) { return FText::FromString(TEXT("LIVE")); }
                                        return FText::FromString(FString::Printf(TEXT("PAUSED @ #%d"),
                                            _ViewModel->GetScrubEventIndex() + 1));
                                    })
                                    .Tone_Lambda([this]() -> ECk_Tone
                                    {
                                        const auto IsLive = NOT _ViewModel.IsValid()
                                            || _ViewModel->GetMode() == FCkGoapDebugger_ViewModel::EMode::Live;
                                        return IsLive ? ECk_Tone::Ok : ECk_Tone::Warn;
                                    })
                            ]
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                            .ButtonStyle(&FCkDebuggerCommonStyle::Get_FlatButtonStyle())
                            .ContentPadding(FMargin(6.0f, 2.0f))
                            .ToolTipText(FText::FromString(TEXT("Step one recorded event back (enters Scrub)")))
                            .OnClicked_Lambda([this, ScrubTo]() -> FReply
                            {
                                const auto Current = _ViewModel.IsValid() ? _ViewModel->GetScrubEventIndex() : INDEX_NONE;
                                ScrubTo(Current == INDEX_NONE ? MAX_int32 - 1 : Current - 1);
                                return FReply::Handled();
                            })
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("|◀")))
                                    .Font_Lambda([]() -> FSlateFontInfo
                                    { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeBody()); })
                                    .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                            ]
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, CkStyle::SpaceL, 0.0f)
                    [
                        SNew(SButton)
                            .ButtonStyle(&FCkDebuggerCommonStyle::Get_FlatButtonStyle())
                            .ContentPadding(FMargin(6.0f, 2.0f))
                            .ToolTipText(FText::FromString(TEXT("Step one recorded event forward")))
                            .OnClicked_Lambda([this, ScrubTo]() -> FReply
                            {
                                const auto Current = _ViewModel.IsValid() ? _ViewModel->GetScrubEventIndex() : INDEX_NONE;
                                if (Current != INDEX_NONE) { ScrubTo(Current + 1); }
                                return FReply::Handled();
                            })
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("▶|")))
                                    .Font_Lambda([]() -> FSlateFontInfo
                                    { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeBody()); })
                                    .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                            ]
                    ]

                // REC — history ring indicator; click clears the recording.
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, CkStyle::SpaceL, 0.0f)
                    [
                        SNew(SButton)
                            .ButtonStyle(&FCkDebuggerCommonStyle::Get_FlatButtonStyle())
                            .ContentPadding(FMargin(2.0f))
                            .ToolTipText(FText::FromString(TEXT("History recording is always on. Click to CLEAR the selected agent's recorded events.")))
                            .OnClicked_Lambda([this]() -> FReply
                            {
                                if (_ViewModel.IsValid())
                                {
                                    const auto Entity = _ViewModel->GetSelectedEntity();
                                    if (ck::IsValid(Entity))
                                    { FCkGoapDebugger_DataCollector::ClearHistoryForEntity(Entity); }
                                    _ViewModel->SetScrubEventIndex(INDEX_NONE);
                                    _ViewModel->SetMode(FCkGoapDebugger_ViewModel::EMode::Live);
                                }
                                return FReply::Handled();
                            })
                            [
                                SNew(SCkDebug_StatusPill)
                                    .Text(FText::FromString(TEXT("REC")))
                                    .Tone(ECk_Tone::Err)
                            ]
                    ]

                // Nerd mode — reveals search internals.
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Nerd mode")))
                            .Font_Lambda([]() -> FSlateFontInfo
                            { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeSmall()); })
                            .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                            .ToolTipText(FText::FromString(TEXT("Reveal search internals: budget usage, states expanded, the regressive trace, entity handles.")))
                    ]
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, CkStyle::SpaceL, 0.0f)
                    [
                        SNew(SCkDebug_Switch)
                            .IsOn_Lambda([this] { return _NerdMode; })
                            .OnStateChanged_Lambda([this](bool InNew)
                            {
                                _NerdMode = InNew;
                                if (_ViewModel.IsValid()) { _ViewModel->Broadcast_Changed(); }
                            })
                    ]

                // Name depth verbosity — the shared cycler widget. Depth lives
                // on the ViewModel; every pane that renders class names
                // re-renders off the Changed broadcast.
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
                    [
                        SNew(SCkDebug_NameDepthCycler)
                            .Depth_Lambda([this]() -> int32
                            {
                                return _ViewModel.IsValid() ? _ViewModel->Get_NameDepth() : 1;
                            })
                            .MaxDepth_Lambda([this]() -> int32
                            {
                                return _GraphPane.IsValid() ? _GraphPane->Get_MaxNameDepth() : 1;
                            })
                            .OnDepthChanged(FOnCkDebug_NameDepthChanged::CreateLambda([this](int32 InNewDepth)
                            {
                                if (NOT _ViewModel.IsValid()) { return; }
                                _ViewModel->Set_NameDepth(InNewDepth);
                                _ViewModel->Broadcast_Changed();
                            }))
                    ]
        ];
}

// ====================================================================================================================
// CHROME PICKERS
// ====================================================================================================================

auto
    SCkGoapDebuggerWindow::
    RefreshPickers()
    -> void
{
    if (NOT _ViewModel.IsValid()) { return; }

    const auto& Roster = _ViewModel->Get_Roster();
    const auto SelectedEntity = _ViewModel->GetSelectedEntity();
    const auto SelectedPlanner = _ViewModel->GetSelectedActionSet();

    // ---- Agents ---------------------------------------------------------------
    {
        _AgentPickerLabels.Reset();
        _AgentPickerHandles.Reset();

        auto SelectedItem = TSharedPtr<FString>{};
        for (const auto& Entry : Roster)
        {
            auto Label = MakeShared<FString>(Entry.DebugName);
            _AgentPickerLabels.Add(Label);
            _AgentPickerHandles.Add(Entry.EntityHandle);
            if (Entry.EntityHandle == SelectedEntity) { SelectedItem = Label; }
        }

        if (_AgentPicker.IsValid())
        {
            _AgentPicker->RefreshOptions();
            // SetSelectedItem fires OnSelectionChanged as Direct — the handler
            // ignores Direct, so no echo loop.
            _AgentPicker->SetSelectedItem(SelectedItem);
        }
    }

    // ---- Top-level Planners on the selected agent -----------------------------
    {
        _PlannerPickerLabels.Reset();
        _PlannerPickerHandles.Reset();

        auto SelectedItem = TSharedPtr<FString>{};
        if (const auto* Snapshot = _ViewModel->GetCurrentEntitySnapshot())
        {
            for (const auto& Planner : Snapshot->TopLevelPlanners)
            {
                const auto SubCount = Planner.ChildPlanners.Num();
                auto Label = MakeShared<FString>(SubCount > 0
                    ? FString::Printf(TEXT("%s (+%d sub)"), *Planner.DisplayName, SubCount)
                    : Planner.DisplayName);
                _PlannerPickerLabels.Add(Label);
                _PlannerPickerHandles.Add(Planner.PlannerHandle);
                if (static_cast<FCk_Handle>(Planner.PlannerHandle) == static_cast<FCk_Handle>(SelectedPlanner))
                { SelectedItem = Label; }
            }
        }

        if (_PlannerPicker.IsValid())
        {
            _PlannerPicker->RefreshOptions();
            _PlannerPicker->SetSelectedItem(SelectedItem);
        }
    }
}

auto
    SCkGoapDebuggerWindow::
    HandleAgentPicked(
        TSharedPtr<FString> InItem,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    if (InSelectInfo == ESelectInfo::Direct) { return; }
    if (NOT _ViewModel.IsValid() || NOT InItem.IsValid()) { return; }

    const auto Index = _AgentPickerLabels.IndexOfByKey(InItem);
    if (NOT _AgentPickerHandles.IsValidIndex(Index)) { return; }

    _ViewModel->SetSelectedEntity(_AgentPickerHandles[Index]);
    _ViewModel->Broadcast_Changed();
}

auto
    SCkGoapDebuggerWindow::
    HandlePlannerPicked(
        TSharedPtr<FString> InItem,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    if (InSelectInfo == ESelectInfo::Direct) { return; }
    if (NOT _ViewModel.IsValid() || NOT InItem.IsValid()) { return; }

    const auto Index = _PlannerPickerLabels.IndexOfByKey(InItem);
    if (NOT _PlannerPickerHandles.IsValidIndex(Index)) { return; }

    _ViewModel->SetSelectedActionSet(_PlannerPickerHandles[Index]);
    _ViewModel->Broadcast_Changed();
}

// ====================================================================================================================
// BUILD — NERD STRIP (search internals; nerd-gated)
// ====================================================================================================================

auto
    SCkGoapDebuggerWindow::
    BuildNerdStrip()
    -> TSharedRef<SWidget>
{
    // Everything reads the selected Planner live — the strip is built once.
    const auto SelectedPlanner = [this]() -> const FCkGoapDebugger_PlannerInfo*
    {
        return _ViewModel.IsValid() ? _ViewModel->GetSelectedPlannerInfo() : nullptr;
    };

    const auto MakeStat = [](const FText& InLabel, const TAttribute<FText>& InValue) -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f))
                [
                    SNew(STextBlock)
                        .Text(InLabel)
                        .Font_Lambda([]() -> FSlateFontInfo
                        { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeMicro()); })
                        .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                ]

            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(InValue)
                        .Font_Lambda([]() -> FSlateFontInfo
                        { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeMicro()); })
                        .ColorAndOpacity(FSlateColor(CkStyle::Text()))
                ];
    };

    return SNew(SBorder)
        .BorderImage(CkStyle::GetFilledBrush())
        .BorderBackgroundColor(FSlateColor(CkStyle::Bg3()))
        .Padding(FMargin(CkStyle::SpaceL, CkStyle::SpaceXS))
        .Visibility_Lambda([this]() { return _NerdMode ? EVisibility::Visible : EVisibility::Collapsed; })
        [
            SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceXL, 0.0f))
                    [
                        MakeStat(FText::FromString(TEXT("plan")),
                            TAttribute<FText>::CreateLambda([SelectedPlanner]() -> FText
                            {
                                const auto* Planner = SelectedPlanner();
                                if (Planner == nullptr) { return FText::FromString(TEXT("\x2014")); }
                                return FText::FromString(FString::Printf(TEXT("%lld \x00B5s"),
                                    Planner->SearchStats.Get_ElapsedMicroseconds()));
                            }))
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceS, 0.0f))
                    [
                        MakeStat(FText::FromString(TEXT("budget")),
                            TAttribute<FText>::CreateLambda([SelectedPlanner]() -> FText
                            {
                                const auto* Planner = SelectedPlanner();
                                if (Planner == nullptr) { return FText::FromString(TEXT("\x2014")); }
                                return FText::FromString(FString::Printf(TEXT("%lld \x00B5s"),
                                    Planner->SearchBudgetMicroseconds));
                            }))
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceS, 0.0f))
                    [
                        SNew(SBox)
                            .WidthOverride(72.0f)
                            .ToolTipText(FText::FromString(TEXT("Last search's elapsed time vs the per-slice budget.")))
                            [
                                SNew(SCkDebug_MeterBar)
                                    .Fraction_Lambda([SelectedPlanner]() -> float
                                    {
                                        const auto* Planner = SelectedPlanner();
                                        if (Planner == nullptr || Planner->SearchBudgetMicroseconds <= 0) { return 0.0f; }
                                        return FMath::Clamp(
                                            static_cast<float>(Planner->SearchStats.Get_ElapsedMicroseconds()) /
                                            static_cast<float>(Planner->SearchBudgetMicroseconds),
                                            0.0f, 1.0f);
                                    })
                                    .FillColor(CkStyle::Accent())
                            ]
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceXL, 0.0f))
                    [
                        MakeStat(FText::GetEmpty(),
                            TAttribute<FText>::CreateLambda([SelectedPlanner]() -> FText
                            {
                                const auto* Planner = SelectedPlanner();
                                if (Planner == nullptr || Planner->SearchBudgetMicroseconds <= 0)
                                { return FText::FromString(TEXT("\x2014 slices")); }
                                const auto Slices = FMath::Max(1,
                                    FMath::CeilToInt32(
                                        static_cast<double>(Planner->SearchStats.Get_ElapsedMicroseconds()) /
                                        static_cast<double>(Planner->SearchBudgetMicroseconds)));
                                return FText::FromString(FString::Printf(TEXT("%d slice%s"),
                                    Slices, Slices == 1 ? TEXT("") : TEXT("s")));
                            }))
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceXL, 0.0f))
                    [
                        MakeStat(FText::FromString(TEXT("states expanded")),
                            TAttribute<FText>::CreateLambda([SelectedPlanner]() -> FText
                            {
                                const auto* Planner = SelectedPlanner();
                                if (Planner == nullptr) { return FText::FromString(TEXT("\x2014")); }
                                return FText::AsNumber(Planner->SearchStats.Get_Iterations());
                            }))
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceXL, 0.0f))
                    [
                        MakeStat(FText::FromString(TEXT("state pool")),
                            TAttribute<FText>::CreateLambda([SelectedPlanner]() -> FText
                            {
                                const auto* Planner = SelectedPlanner();
                                if (Planner == nullptr) { return FText::FromString(TEXT("\x2014")); }
                                return FText::AsNumber(Planner->SearchStats.Get_StatePoolSize());
                            }))
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceXL, 0.0f))
                    [
                        MakeStat(FText::FromString(TEXT("gate")),
                            TAttribute<FText>::CreateLambda([SelectedPlanner]() -> FText
                            {
                                const auto* Planner = SelectedPlanner();
                                if (Planner == nullptr) { return FText::FromString(TEXT("\x2014")); }

                                const auto AnyCompositeInPlan = Planner->ChildPlanners.ContainsByPredicate(
                                    [Planner](const FCkGoapDebugger_PlannerInfo& InChild)
                                    {
                                        return Planner->PlanHandles.ContainsByPredicate(
                                            [&InChild](const FCk_Handle_Goap_Action& InStep)
                                            { return static_cast<FCk_Handle>(InStep) == static_cast<FCk_Handle>(InChild.PlannerHandle); });
                                    });
                                return FText::FromString(AnyCompositeInPlan
                                    ? TEXT("sub gated 1 frame while parent in flight")
                                    : TEXT("children not deferred"));
                            }))
                    ]

                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .HAlign(HAlign_Right)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Text_Lambda([SelectedPlanner]() -> FText
                            {
                                const auto* Planner = SelectedPlanner();
                                if (Planner == nullptr) { return FText::GetEmpty(); }
                                return FText::FromString(FString::Printf(TEXT("planner %s \x00B7 WS %s"),
                                    *UCk_Utils_Handle_UE::Get_DebugName(Planner->PlannerHandle).ToString(),
                                    *Planner->WorldStateSourceLabel));
                            })
                            .Font_Lambda([]() -> FSlateFontInfo
                            { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeMicro()); })
                            .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                    ]
        ];
}

// ====================================================================================================================
// BUILD — ALERT STRIP (sandbox banner + fallback-active warning)
// ====================================================================================================================

auto
    SCkGoapDebuggerWindow::
    BuildAlertStrip()
    -> TSharedRef<SWidget>
{
    const auto SelectedWs = [this]() -> FCk_Handle_Goap_WorldState
    {
        if (NOT _ViewModel.IsValid()) { return {}; }
        const auto* Planner = _ViewModel->GetSelectedPlannerInfo();
        if (Planner == nullptr) { return {}; }
        return Planner->WorldStateSourceResolved;
    };

    static const auto DebugUiLayerName = FName{TEXT("DebugUI")};

    // Fallback-active: any tier of the selected chain whose Plan[0] is a
    // fallback-cost action.
    const auto IsFallbackActive = [this]() -> bool
    {
        if (NOT _ViewModel.IsValid()) { return false; }
        const auto* Cursor = _ViewModel->GetSelectedPlannerInfo();
        while (Cursor != nullptr && NOT Cursor->PlanClassNames.IsEmpty())
        {
            const auto& StepName = Cursor->PlanClassNames[0];
            const auto* StepInfo = Cursor->ChildActions.FindByPredicate(
                [&StepName](const FCkGoapDebugger_ActionInfo& In) { return In.ClassName == StepName; });
            if (StepInfo != nullptr && StepInfo->Cost >= ck_goap_debugger_decision_model::k_FallbackCostFloor)
            { return true; }

            const FCkGoapDebugger_PlannerInfo* Next = nullptr;
            if (Cursor->PlanHandles.IsValidIndex(0))
            {
                const auto& Step = Cursor->PlanHandles[0];
                Next = Cursor->ChildPlanners.FindByPredicate(
                    [&Step](const FCkGoapDebugger_PlannerInfo& In)
                    { return static_cast<FCk_Handle>(In.PlannerHandle) == static_cast<FCk_Handle>(Step); });
            }
            Cursor = Next;
        }
        return false;
    };

    return SNew(SVerticalBox)

        // Sandbox banner — DebugUI layer active on the selected WS.
        + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBox)
                    .Visibility_Lambda([this, SelectedWs]() -> EVisibility
                    {
                        auto Ws = SelectedWs();
                        if (ck::Is_NOT_Valid(Ws)) { return EVisibility::Collapsed; }

                        // Armed-but-untouched sandbox still banners — flipping the
                        // rail switch must give immediate, unmissable feedback.
                        const auto Armed = _WorldStateRail.IsValid() && _WorldStateRail->Get_IsSandboxMode();
                        return Armed || UCk_Utils_Goap_WorldState_UE::Get_LayerKeyCount(Ws, DebugUiLayerName) > 0
                            ? EVisibility::Visible
                            : EVisibility::Collapsed;
                    })
                    [
                        SNew(SCkDebug_AlertRow)
                            .Tone(ECk_Tone::Warn)
                            .Glyph(FText::FromString(TEXT("\x25C6")))   // ◆ — emoji glyphs tofu in the editor font
                            .LeadText(FText::FromString(TEXT("Sandbox active")))
                            .BodyText_Lambda([SelectedWs]() -> FText
                            {
                                auto Ws = SelectedWs();
                                if (ck::Is_NOT_Valid(Ws)) { return FText::GetEmpty(); }

                                const auto KeyCount = UCk_Utils_Goap_WorldState_UE::Get_LayerKeyCount(Ws, DebugUiLayerName);
                                if (KeyCount == 0)
                                {
                                    return FText::FromString(TEXT(
                                        "\x2014 armed. Click a value pill in the World State rail to shadow that key in the \"DebugUI\" layer."));
                                }
                                auto Body = FString::Printf(
                                    TEXT("\x2014 \"DebugUI\" override layer (%d key%s). Reads are shadowed; base store untouched."),
                                    KeyCount, KeyCount == 1 ? TEXT("") : TEXT("s"));

                                const auto Subscribers = UCk_Utils_Goap_WorldState_UE::Get_SubscriberCount(Ws);
                                if (Subscribers > 1)
                                {
                                    Body += FString::Printf(
                                        TEXT("  ! This WS is shared \x2014 %d subscribers replan under this sandbox."),
                                        Subscribers);
                                }
                                return FText::FromString(Body);
                            })
                            .ActionText(FText::FromString(TEXT("Pop layer")))
                            .OnAction(FOnCkDebug_AlertAction::CreateLambda([SelectedWs]()
                            {
                                auto Ws = SelectedWs();
                                if (ck::Is_NOT_Valid(Ws)) { return; }
                                UCk_Utils_Goap_WorldState_UE::Pop_Override_ByName(Ws, DebugUiLayerName);
                            }))
                    ]
            ]

        // Fallback-active warning.
        + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBox)
                    .Visibility_Lambda([IsFallbackActive]() -> EVisibility
                    { return IsFallbackActive() ? EVisibility::Visible : EVisibility::Collapsed; })
                    [
                        SNew(SCkDebug_AlertRow)
                            .Tone(ECk_Tone::Warn)
                            .Glyph(FText::FromString(TEXT("!")))
                            .LeadText(FText::FromString(TEXT("Fallback plan active")))
                            .BodyText(FText::FromString(TEXT("\x2014 no affordable path to the goal from the current world state.")))
                            .FixText(FText::FromString(TEXT("Check the Decision panel: what's blocked?")))
                    ]
            ];
}

// ====================================================================================================================
// BUILD — TOP TABS + VIEWS (Mission Control)
// ====================================================================================================================

auto
    SCkGoapDebuggerWindow::
    BuildTopTabs()
    -> TSharedRef<SWidget>
{
    auto Tabs = TArray<FCkDebug_UnderlineTabDesc>{};

    {
        auto Squad = FCkDebug_UnderlineTabDesc{};
        Squad.Id = Tab_Squad;
        Squad.Label = FText::FromString(TEXT("Squad"));
        Squad.CountText = TAttribute<FText>::CreateLambda([this]() -> FText
        {
            if (NOT _ViewModel.IsValid()) { return FText::GetEmpty(); }
            const auto Count = _ViewModel->Get_Roster().Num();
            return Count > 0 ? FText::AsNumber(Count) : FText::GetEmpty();
        });
        Tabs.Add(MoveTemp(Squad));
    }
    {
        auto Inspector = FCkDebug_UnderlineTabDesc{};
        Inspector.Id = Tab_Inspector;
        Inspector.Label = FText::FromString(TEXT("Agent Inspector"));
        Tabs.Add(MoveTemp(Inspector));
    }
    {
        auto Catalog = FCkDebug_UnderlineTabDesc{};
        Catalog.Id = Tab_Catalog;
        Catalog.Label = FText::FromString(TEXT("Catalog Audit"));
        // Warn dot — cheap field reads only (no per-paint lint): unregistered
        // goal keys, dependency cycles, or a missing fallback all warrant a look.
        Catalog.ShowWarnDot = TAttribute<bool>::CreateLambda([this]() -> bool
        {
            if (NOT _ViewModel.IsValid()) { return false; }
            const auto* Planner = _ViewModel->GetSelectedPlannerInfo();
            if (Planner == nullptr) { return false; }
            return Planner->InvalidGoalAuthored.Num() > 0
                || Planner->DependencyCyclesDisplay.Num() > 0
                || (NOT Planner->HasUnconditionalFallback && NOT Planner->AllowPlanFailed);
        });
        Tabs.Add(MoveTemp(Catalog));
    }

    return SNew(SBorder)
        .BorderImage(CkStyle::GetFilledBrush())
        .BorderBackgroundColor(FSlateColor(CkStyle::Bg2()))
        .Padding(FMargin(CkStyle::SpaceL, 0.0f, CkStyle::SpaceL, 0.0f))
        [
            SNew(SCkDebug_UnderlineTabs)
                .Tabs(Tabs)
                .ActiveTabId_Lambda([this] { return _ActiveTab; })
                .OnTabSelected_Lambda([this](FName InTab) { _ActiveTab = InTab; })
        ];
}

auto
    SCkGoapDebuggerWindow::
    BuildSquadView()
    -> TSharedRef<SWidget>
{
    // One row per top-level Planner world-wide; Inspect flips to the Agent
    // Inspector with that planner selected. (_AgentList stays constructed —
    // it still receives cross-debugger selection-sync broadcasts — but the
    // table carries the tab; the list retires with the P9 sweep.)
    return SAssignNew(_SquadTable, SCkGoapDebugger_SquadTable)
        .ViewModel(_ViewModel)
        .OnInspect(FOnCkGoapDebug_SquadInspect::CreateLambda(
            [this](FCk_Handle InEntity, FCk_Handle_Goap_Planner InPlanner)
            {
                if (NOT _ViewModel.IsValid()) { return; }
                _ViewModel->SetSelectedEntity(InEntity);
                _ViewModel->SetSelectedActionSet(InPlanner);
                _ViewModel->Broadcast_Changed();
                _ActiveTab = Tab_Inspector;
            }));
}

auto
    SCkGoapDebuggerWindow::
    BuildInspectorView()
    -> TSharedRef<SWidget>
{
    auto SidebarWidget = _Sidebar.ToSharedRef();

    return SNew(SSplitter)
        .Orientation(Orient_Vertical)

        + SSplitter::Slot()
            .Value(0.70f)
            [
                SNew(SSplitter)
                    .Orientation(Orient_Horizontal)

                    // LEFT — mockup agent column stacked over the Planner tree.
                    // The tree stays as the planner-selection surface until a
                    // chrome planner-picker exists.
                    + SSplitter::Slot()
                        .Value(0.28f)
                        .MinSize(260.0f)
                        [
                            SNew(SSplitter)
                                .Orientation(Orient_Vertical)

                                + SSplitter::Slot()
                                    .Value(0.62f)
                                    [
                                        _AgentColumn.ToSharedRef()
                                    ]

                                + SSplitter::Slot()
                                    .Value(0.38f)
                                    .MinSize(120.0f)
                                    [
                                        SidebarWidget
                                    ]
                        ]

                    + SSplitter::Slot()
                        .Value(0.49f)
                        [
                            BuildCenterColumn()
                        ]

                    + SSplitter::Slot()
                        .Value(0.23f)
                        .MinSize(220.0f)
                        [
                            _WorldStateRail.ToSharedRef()
                        ]
            ]

        + SSplitter::Slot()
            .Value(0.30f)
            .MinSize(100.0f)
            [
                SAssignNew(_TimelineDock, SCkGoapDebugger_TimelineDock)
                    .ViewModel(_ViewModel)
                    .PauseOnReplan_Lambda([this]() -> bool { return _PauseOnReplan; })
                    .PauseOnPlanFailed_Lambda([this]() -> bool { return _PauseOnPlanFailed; })
                    .PauseExecution(FSimpleDelegate::CreateSP(this, &SCkGoapDebuggerWindow::Request_PauseExecution))
            ]
    ;
}

auto
    SCkGoapDebuggerWindow::
    BuildCatalogView()
    -> TSharedRef<SWidget>
{
    return SAssignNew(_CatalogPanel, SCkGoapDebugger_CatalogPanel)
        .ViewModel(_ViewModel);
}

// ====================================================================================================================
// BUILD — CENTER COLUMN (Decision / Plan graph / Search trace tabs)
// ====================================================================================================================

auto
    SCkGoapDebuggerWindow::
    BuildCenterColumn()
    -> TSharedRef<SWidget>
{
    // Mockup ".ctabs" — Decision is the designer default; the graph is a view,
    // not the centerpiece; Search trace only surfaces in nerd mode.
    auto CenterTabs = TArray<FCkDebug_UnderlineTabDesc>{};
    {
        auto Decision = FCkDebug_UnderlineTabDesc{};
        Decision.Id = CTab_Decision;
        Decision.Label = FText::FromString(TEXT("Decision"));
        CenterTabs.Add(MoveTemp(Decision));
    }
    {
        auto Graph = FCkDebug_UnderlineTabDesc{};
        Graph.Id = CTab_Graph;
        Graph.Label = FText::FromString(TEXT("Plan graph"));
        CenterTabs.Add(MoveTemp(Graph));
    }
    {
        auto Search = FCkDebug_UnderlineTabDesc{};
        Search.Id = CTab_Search;
        Search.Label = FText::FromString(TEXT("Search trace"));
        Search.Visibility = TAttribute<EVisibility>::CreateLambda([this]() -> EVisibility
        { return _NerdMode ? EVisibility::Visible : EVisibility::Collapsed; });
        CenterTabs.Add(MoveTemp(Search));
    }

    SAssignNew(_DecisionPanel, SCkGoapDebugger_DecisionPanel)
        .ViewModel(_ViewModel);
    SAssignNew(_SearchTracePanel, SCkGoapDebugger_SearchTracePanel)
        .ViewModel(_ViewModel);
    SAssignNew(_GraphPane, SCkGoapDebugger_GraphPane)
        .ViewModel(_ViewModel);

    return SNew(SBorder)
        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Root")))
        .Padding(FMargin(0.0f))
        [
            SNew(SVerticalBox)

                + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SBorder)
                            .BorderImage(CkStyle::GetFilledBrush())
                            .BorderBackgroundColor(FSlateColor(CkStyle::Bg2()))
                            .Padding(FMargin(CkStyle::SpaceM, 0.0f, CkStyle::SpaceM, 0.0f))
                            [
                                SNew(SCkDebug_UnderlineTabs)
                                    .Tabs(CenterTabs)
                                    .FontSize(CkStyle::FontSizeSmall())
                                    .TabPadding(FMargin(10.0f, 6.0f))
                                    .ActiveTabId_Lambda([this]
                                    {
                                        // Nerd-off while Search is active → snap back to Decision.
                                        if (_CenterTab == CTab_Search && NOT _NerdMode) { return CTab_Decision; }
                                        return _CenterTab;
                                    })
                                    .OnTabSelected_Lambda([this](FName InTab) { _CenterTab = InTab; })
                            ]
                    ]

                + SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    [
                        SNew(SWidgetSwitcher)
                            .WidgetIndex_Lambda([this]() -> int32
                            {
                                if (_CenterTab == CTab_Graph)               { return 1; }
                                if (_CenterTab == CTab_Search && _NerdMode) { return 2; }
                                return 0;
                            })

                            + SWidgetSwitcher::Slot() [ _DecisionPanel.ToSharedRef() ]
                            + SWidgetSwitcher::Slot() [ _GraphPane.ToSharedRef() ]
                            + SWidgetSwitcher::Slot() [ _SearchTracePanel.ToSharedRef() ]
                    ]
        ];
}

// ====================================================================================================================
// EXTERNAL ENTRY POINT (D7) — used by SCkGoapDebugger_InspectorGateway.
// ====================================================================================================================

auto
    SCkGoapDebuggerWindow::
    OpenForEntity(
        const FCk_Handle& InEntity)
    -> void
{
    if (ck::Is_NOT_Valid(InEntity)) { return; }

    // Spawn / focus the standalone tab. The module's OnSpawnDebuggerTab path
    // is what creates the live SCkGoapDebuggerWindow instance and stores it
    // on FCkGoapDebuggerModule, so this MUST run before we ask the module
    // for the window pointer.
    ck::debugger_tabs::Invoke_DebuggerTab(FCkGoapDebuggerModule::Get_TabName());

    auto Window = FCkGoapDebuggerModule::Get().Get_DebuggerWindow();
    if (Window.IsValid())
    { Window->Set_SelectedEntityExternal(InEntity); }
}

auto
    SCkGoapDebuggerWindow::
    Set_SelectedEntityExternal(
        const FCk_Handle& InEntity)
    -> void
{
    if (NOT _ViewModel.IsValid()) { return; }
    if (ck::Is_NOT_Valid(InEntity)) { return; }

    // A just-opened window has no snapshot rows yet. Defer exactly once until
    // its next collector refresh, then clear the retained PIE handle.
    _PendingExternalEntity = InEntity.Get_Entity();
}

auto
    SCkGoapDebuggerWindow::
    BuildMenuActions()
    -> TSharedRef<SWidget>
{
    return SNew(SCkDebug_IconToolbar)
        .Actions(TArray<FCkDebug_IconToggleAction>{
            FCkDebug_IconToggleAction{
                TEXT("PauseOnReplan"),
                TEXT("Stopwatch"),
                FText::FromString(TEXT("Pause on replan")),
                FText::FromString(TEXT("Pause PIE when a GOAP replan occurs.")),
                TAttribute<bool>::CreateLambda([this]() -> bool { return _PauseOnReplan; }),
                FOnCkDebug_IconToggleChanged::CreateLambda([this](bool InIsOn)
                {
                    _PauseOnReplan = InIsOn;
                })},
            FCkDebug_IconToggleAction{
                TEXT("PauseOnPlanFailed"),
                TEXT("Skull"),
                FText::FromString(TEXT("Pause on plan failed")),
                FText::FromString(TEXT("Pause PIE when GOAP plan construction fails.")),
                TAttribute<bool>::CreateLambda([this]() -> bool { return _PauseOnPlanFailed; }),
                FOnCkDebug_IconToggleChanged::CreateLambda([this](bool InIsOn)
                {
                    _PauseOnPlanFailed = InIsOn;
                })}});
}

// ====================================================================================================================
// AGENT LIST
// ====================================================================================================================

auto
    SCkGoapDebuggerWindow::
    RefreshAgentList()
    -> void
{
    if (_AgentList.IsValid())
    { _AgentList->RefreshFromViewModel(); }
}

// ====================================================================================================================
