#include "CkGoapDebugger/Window/SCkGoapDebuggerWindow.h"

#include "CkGoapDebugger/CkGoapDebugger_Module.h"
#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/Data/CkGoapDebugger_DataCollector.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_AgentListPanel.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_Breadcrumb.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_GraphPane.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_PrimaryPane.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_Sidebar.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_WorldStateRail.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

// ====================================================================================================================

const FName SCkGoapDebuggerWindow::WindowId = FName(TEXT("GoapDebugger"));

namespace
{
    // Build a small color-swatch + label for the legend row.
    auto MakeLegendItem(const FLinearColor& InColor, const FString& InText) -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, 4.0f, 0.0f)
                [
                    SNew(SBox)
                        .WidthOverride(10.0f)
                        .HeightOverride(10.0f)
                        [
                            SNew(SBorder)
                                .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                                .BorderBackgroundColor(InColor)
                                .Padding(FMargin(0.0f))
                                [ SNew(SSpacer) ]
                        ]
                ]
            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SCkDebug_SelectableLabel)
                        .Text(FText::FromString(InText))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Secondary))
                ];
    }
}

// ====================================================================================================================
// LIFETIME
// ====================================================================================================================

SCkGoapDebuggerWindow::~SCkGoapDebuggerWindow()
{
    if (_OnEndPieHandle.IsValid())
    { FEditorDelegates::EndPIE.Remove(_OnEndPieHandle); }

    if (_OnBeginPieHandle.IsValid())
    { FEditorDelegates::BeginPIE.Remove(_OnBeginPieHandle); }
}

auto
    SCkGoapDebuggerWindow::
    HandleWorldTornDown()
    -> void
{
    _CachedWorld.Reset();

    // Agent rows hold FCk_Handle copies — drop them while the registry lives.
    if (_AgentList.IsValid())
    { _AgentList->Reset_ForWorldChange(); }

    if (_Sidebar.IsValid())
    { _Sidebar->Reset_ForWorldChange(); }

    // Clear the graph BEFORE the ViewModel resets — graph node snapshots
    // hold FCk_Handle copies, and they must be released while the registry
    // is still live.
    if (_GraphPane.IsValid())
    { _GraphPane->Reset_ForWorldChange(); }

    if (_ViewModel.IsValid())
    { _ViewModel->Reset_ForWorldChange(); }
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

    _OnBeginPieHandle = FEditorDelegates::BeginPIE.AddLambda([this](bool)
    { HandleWorldTornDown(); });

    _OnEndPieHandle = FEditorDelegates::EndPIE.AddLambda([this](bool)
    { HandleWorldTornDown(); });

    auto SidebarWidget   = SAssignNew(_Sidebar, SCkGoapDebugger_Sidebar, _ViewModel);
    auto AgentListWidget = SAssignNew(_AgentList, SCkGoapDebugger_AgentListPanel)
                                .ViewModel(_ViewModel);
    auto CenterColumn    = BuildCenterColumn();
    auto WsRailWidget    = SAssignNew(_WorldStateRail, SCkGoapDebugger_WorldStateRail)
                                .ViewModel(_ViewModel);

    ChildSlot
    [
        SNew(SBorder)
            .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Root")))
            .Padding(FMargin(0.0f))
            [
                SNew(SVerticalBox)

                    // Mode bar (top, fixed height)
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            BuildModeBar()
                        ]

                    // Toolbar
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            BuildToolbar()
                        ]

                    // Legend
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            BuildLegend()
                        ]

                    // App body: [sidebar(tree) | center | ws rail] on top, full-width history dock below.
                    // The history widget is built by the sidebar (Get_HistoryWidget) but parented here so
                    // it spans the whole window width — refresh/reset still route through _Sidebar.
                    + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        [
                            SNew(SSplitter)
                                .Orientation(Orient_Vertical)

                                + SSplitter::Slot()
                                    .Value(0.70f)
                                    [
                                        SNew(SSplitter)
                                            .Orientation(Orient_Horizontal)

                                            + SSplitter::Slot()
                                                .Value(0.22f)
                                                .MinSize(220.0f)
                                                [
                                                    // Agents list (replaces the toolbar combo) stacked
                                                    // over the planner tree, both user-resizable.
                                                    SNew(SSplitter)
                                                        .Orientation(Orient_Vertical)

                                                        + SSplitter::Slot()
                                                            .Value(0.35f)
                                                            .MinSize(110.0f)
                                                            [
                                                                AgentListWidget
                                                            ]

                                                        + SSplitter::Slot()
                                                            .Value(0.65f)
                                                            [
                                                                SidebarWidget
                                                            ]
                                                ]

                                            + SSplitter::Slot()
                                                .Value(0.55f)
                                                [
                                                    CenterColumn
                                                ]

                                            + SSplitter::Slot()
                                                .Value(0.23f)
                                                .MinSize(220.0f)
                                                [
                                                    WsRailWidget
                                                ]
                                    ]

                                + SSplitter::Slot()
                                    .Value(0.30f)
                                    .MinSize(100.0f)
                                    [
                                        _Sidebar->Get_HistoryWidget()
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
            if (_Sidebar.IsValid())
            { _Sidebar->RefreshFromViewModel(); }
            if (_Breadcrumb.IsValid())
            { _Breadcrumb->RefreshFromViewModel(); }
            if (_PrimaryPane.IsValid())
            { _PrimaryPane->RefreshFromViewModel(); }
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
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    _WorldModel->Ensure_AutoSelect();
    _CachedWorld = _WorldModel->Get_SelectedWorld();

    auto* World = _CachedWorld.Get();
    if (ck::Is_NOT_Valid(World) || NOT World->HasBegunPlay())
    { return; }

    _ViewModel->Tick(World);

    RefreshAgentList();
}

// ====================================================================================================================
// BUILD — MODE BAR
// ====================================================================================================================

auto
    SCkGoapDebuggerWindow::
    BuildModeBar()
    -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Black")))
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
        [
            SNew(SHorizontalBox)

                // "Standalone Window" — active in D2
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("Standalone Window")))
                            .IsEnabled(true)
                            .ToolTipText(FText::FromString(TEXT("Top-level debugger window (active)")))
                    ]

                // "Open ECS Inspector" — invokes the ECS debugger tab.
                // The Goap summary card lives inside that window's entity inspector
                // via SCkGoapDebugger_InspectorGateway (D7), so this is just a
                // convenience jump.
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("Open ECS Inspector")))
                            .ToolTipText(FText::FromString(TEXT("Open the CkEcsDebugger window; the Goap summary card lives in its entity inspector panel.")))
                            .OnClicked_Lambda([]() -> FReply
                            {
                                FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("CkEcsDebugger")));
                                return FReply::Handled();
                            })
                    ]

                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)

                // "Simulate PlanFailed" — visual stub for D2
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("Simulate PlanFailed")))
                            .IsEnabled(false)
                            .ToolTipText(FText::FromString(TEXT("Force a plan-failed event for the selected entity (later phase)")))
                    ]
        ];
}

// ====================================================================================================================
// BUILD — TOOLBAR
// ====================================================================================================================

auto
    SCkGoapDebuggerWindow::
    BuildToolbar()
    -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Surface")))
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
        [
            SNew(SHorizontalBox)

                // World selector (shared across all CK debuggers)
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
                    [
                        SNew(SCkDebug_WorldSelector, _WorldModel)
                            .ShowHeaderLabel(false)
                    ]

                // "Entity:" label — selection now lives in the Agents list panel
                // (left column); the pill next door shows the current selection.
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(TEXT("Entity:")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Secondary))
                    ]

                // EntityRef pill (ID|Version) — click navigates to ECS debugger
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
                    [
                        SNew(SCkDebug_EntityRef)
                            .Entity_Lambda([this]() -> FCk_Handle
                            {
                                if (NOT _ViewModel.IsValid()) { return FCk_Handle{}; }
                                return _ViewModel->GetSelectedEntity();
                            })
                    ]

                // Live / Scrub toggle (two buttons)
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("Live")))
                            .ToolTipText(FText::FromString(TEXT("Live mode — snapshots refresh every tick")))
                            .ButtonColorAndOpacity_Lambda([this]() -> FSlateColor
                            {
                                if (NOT _ViewModel.IsValid()) { return FSlateColor(FLinearColor::White); }
                                const auto Active = _ViewModel->GetMode() == FCkGoapDebugger_ViewModel::EMode::Live;
                                // Tinted when active so the toggle reads at a glance.
                                return Active
                                    ? FSlateColor(FCkGoapDebuggerStyle::Color_Status_PlanFound)
                                    : FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim);
                            })
                            .OnClicked_Lambda([this]() -> FReply
                            {
                                if (NOT _ViewModel.IsValid()) { return FReply::Handled(); }
                                _ViewModel->SetMode(FCkGoapDebugger_ViewModel::EMode::Live);
                                _ViewModel->SetScrubEventIndex(INDEX_NONE);
                                return FReply::Handled();
                            })
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("Scrub")))
                            .ToolTipText(FText::FromString(TEXT("Scrub mode — freeze on a history event. Click a dot or row in the sidebar to jump to it.")))
                            .ButtonColorAndOpacity_Lambda([this]() -> FSlateColor
                            {
                                if (NOT _ViewModel.IsValid()) { return FSlateColor(FLinearColor::White); }
                                const auto Active = _ViewModel->GetMode() == FCkGoapDebugger_ViewModel::EMode::Scrub;
                                return Active
                                    ? FSlateColor(FCkGoapDebuggerStyle::Color_Status_Selected)
                                    : FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim);
                            })
                            .OnClicked_Lambda([this]() -> FReply
                            {
                                if (NOT _ViewModel.IsValid()) { return FReply::Handled(); }

                                // Default to the most recent event if no scrub
                                // selection exists yet — chronological order in
                                // GetHistory means the last element is "newest".
                                if (_ViewModel->GetScrubEventIndex() == INDEX_NONE)
                                {
                                    const auto Entity = _ViewModel->GetSelectedEntity();
                                    if (ck::IsValid(Entity))
                                    {
                                        const auto& Hist = FCkGoapDebugger_DataCollector::GetHistory(Entity);
                                        if (Hist.Num() > 0)
                                        {
                                            _ViewModel->SetScrubEventIndex(Hist.Num() - 1);
                                            if (ck::IsValid(Hist.Last().ActionSetHandle))
                                            { _ViewModel->SetSelectedActionSet(Hist.Last().ActionSetHandle); }
                                        }
                                    }
                                }

                                _ViewModel->SetMode(FCkGoapDebugger_ViewModel::EMode::Scrub);
                                return FReply::Handled();
                            })
                    ]

                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)

                // Name depth verbosity:  [Name]  [<]  value  [>]
                // Shared with the graph pane via the ViewModel — clicking these
                // mutates _ViewModel->_NameDepth and broadcasts a Changed event
                // so every pane that renders class names re-renders. Mirrors
                // the SM debugger's name-depth toolbar layout.
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(TEXT("Name")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                    ]
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("\x25C0")))  // ◀
                            .ToolTipText(FText::FromString(TEXT("Shorter display name (fewer segments)")))
                            .OnClicked_Lambda([this]() -> FReply
                            {
                                if (NOT _ViewModel.IsValid()) { return FReply::Handled(); }
                                const auto Depth = _ViewModel->Get_NameDepth();
                                const auto MaxDepth = _GraphPane.IsValid() ? _GraphPane->Get_MaxNameDepth() : 1;
                                // Cycle: Full(0) -> MaxDepth -> ... -> 2 -> 1 -> Full(0)
                                const auto NewDepth = (Depth == 0) ? MaxDepth : (Depth - 1);
                                _ViewModel->Set_NameDepth(NewDepth);
                                _ViewModel->Broadcast_Changed();
                                return FReply::Handled();
                            })
                    ]
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Text_Lambda([this]() -> FText
                            {
                                if (NOT _ViewModel.IsValid()) { return FText::FromString(TEXT("1")); }
                                const auto D = _ViewModel->Get_NameDepth();
                                return FText::FromString(D == 0 ? TEXT("Full") : FString::Printf(TEXT("%d"), D));
                            })
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                            .Justification(ETextJustify::Center)
                            .MinDesiredWidth(28.0f)
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Primary))
                    ]
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("\x25B6")))  // ▶
                            .ToolTipText(FText::FromString(TEXT("Longer display name (more segments)")))
                            .OnClicked_Lambda([this]() -> FReply
                            {
                                if (NOT _ViewModel.IsValid()) { return FReply::Handled(); }
                                const auto Depth = _ViewModel->Get_NameDepth();
                                const auto MaxDepth = _GraphPane.IsValid() ? _GraphPane->Get_MaxNameDepth() : 1;
                                // Cycle: Full(0) -> 1 -> 2 -> ... -> MaxDepth -> Full(0)
                                const auto NewDepth = (Depth >= MaxDepth) ? 0 : (Depth + 1);
                                _ViewModel->Set_NameDepth(NewDepth);
                                _ViewModel->Broadcast_Changed();
                                return FReply::Handled();
                            })
                    ]

                // Force replan — disabled in Scrub mode (historical chain).
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("Force replan")))
                            .ToolTipText_Lambda([this]() -> FText
                            {
                                if (_ViewModel.IsValid() && _ViewModel->GetMode() == FCkGoapDebugger_ViewModel::EMode::Scrub)
                                { return FText::FromString(TEXT("Disabled in Scrub mode — switch to Live to mutate ECS state")); }
                                return FText::FromString(TEXT("Issue Request_Plan on the currently selected Action"));
                            })
                            .IsEnabled_Lambda([this]() -> bool
                            {
                                if (NOT _ViewModel.IsValid()) { return false; }
                                return _ViewModel->GetMode() == FCkGoapDebugger_ViewModel::EMode::Live;
                            })
                            .OnClicked_Lambda([this]() -> FReply
                            {
                                if (_ViewModel.IsValid())
                                { _ViewModel->ForceReplanOnSelected(); }
                                return FReply::Handled();
                            })
                    ]
        ];
}

// ====================================================================================================================
// BUILD — LEGEND
// ====================================================================================================================

auto
    SCkGoapDebuggerWindow::
    BuildLegend()
    -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Panel")))
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
        [
            SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(TEXT("Action color:")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
                    [
                        MakeLegendItem(FCkGoapDebuggerStyle::Color_Status_PlanningBdr, TEXT("Leaf — atomic action"))
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
                    [
                        MakeLegendItem(FCkGoapDebuggerStyle::Color_Status_Composite, TEXT("Composite — has children"))
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        MakeLegendItem(FCkGoapDebuggerStyle::Color_Status_Failed, TEXT("Failure-blocked"))
                    ]

                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(TEXT("Hover any action to see full path")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
                    ]
        ];
}

// ====================================================================================================================
// BUILD — CENTER COLUMN (breadcrumb + primary + graph stub) / WS RAIL STUB
// ====================================================================================================================

auto
    SCkGoapDebuggerWindow::
    BuildCenterColumn()
    -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Root")))
        .Padding(FMargin(0.0f))
        [
            SNew(SSplitter)
                .Orientation(Orient_Vertical)

                // Breadcrumb (auto height — wraps to its content)
                + SSplitter::Slot()
                    .Value(0.08f)
                    .SizeRule(SSplitter::SizeToContent)
                    [
                        SAssignNew(_Breadcrumb, SCkGoapDebugger_Breadcrumb)
                            .ViewModel(_ViewModel)
                    ]

                // Primary pane (top of remaining space)
                + SSplitter::Slot()
                    .Value(0.42f)
                    [
                        SAssignNew(_PrimaryPane, SCkGoapDebugger_PrimaryPane)
                            .ViewModel(_ViewModel)
                    ]

                // Graph pane (bottom — D5)
                + SSplitter::Slot()
                    .Value(0.50f)
                    [
                        SAssignNew(_GraphPane, SCkGoapDebugger_GraphPane)
                            .ViewModel(_ViewModel)
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
    FGlobalTabmanager::Get()->TryInvokeTab(FCkGoapDebuggerModule::Get_TabName());

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

    // ViewModel::Tick on the next frame will resolve the entity against the
    // freshly-collected snapshot batch — if the entity has a Goap root, the
    // selection sticks; otherwise it auto-picks the first available, which is
    // the documented behaviour.
    _ViewModel->SetSelectedEntity(InEntity);

    // Discrete external selection (inspector gateway) — refresh the rows and
    // stamp the highlight now so it lands without a one-frame delay.
    RefreshAgentList();
    if (_AgentList.IsValid())
    { _AgentList->RestoreSelectionFromViewModel(); }
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
