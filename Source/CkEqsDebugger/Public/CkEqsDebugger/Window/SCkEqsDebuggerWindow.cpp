#include "CkEqsDebugger/Window/SCkEqsDebuggerWindow.h"

#include "CkEqsDebugger/Settings/CkEqsDebuggerSettings.h"
#include "CkEqsDebugger/ViewModel/CkEqsDebugger_ViewModel.h"
#include "CkEqsDebugger/Window/SCkEqsDebugger_QueryList.h"
#include "CkEqsDebugger/Window/SCkEqsDebugger_CandidatePanel.h"
#include "CkEqsDebugger/Window/SCkEqsDebugger_TestBreakdownPanel.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkDebuggerCommon/Window/SCkDebugger_RefreshControls.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "SCkEqsDebuggerWindow"

const FName SCkEqsDebuggerWindow::WindowId = FName("CkEqsDebugger");

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    auto MakeOverlayToggle(
        FName InId,
        FName InIconId,
        bool UCkEqsDebuggerSettings::* InMember,
        const TCHAR* InLabel,
        const TCHAR* InToolTip,
        bool InIsMaster = false) -> FCkDebug_IconToggleAction
    {
        return FCkDebug_IconToggleAction{
            InId,
            InIconId,
            FText::FromString(InLabel),
            FText::FromString(InToolTip),
            TAttribute<bool>::CreateLambda([InMember]()
            {
                const auto* Settings = UCkEqsDebuggerSettings::Get();
                return Settings != nullptr && Settings->*InMember;
            }),
            FOnCkDebug_IconToggleChanged::CreateLambda([InMember](bool InNewState)
            {
                auto* Settings = UCkEqsDebuggerSettings::GetMutable();
                if (Settings == nullptr)
                { return; }

                Settings->*InMember = InNewState;
                Settings->SaveConfig();
            }),
            TAttribute<bool>::CreateLambda([InIsMaster]()
            {
                return InIsMaster || (UCkEqsDebuggerSettings::Get() != nullptr
                    && UCkEqsDebuggerSettings::Get()->Show_Overlay);
            })};
    }
}

namespace ck_eqs_debugger_window
{
    auto Get_TitleFont() -> FSlateFontInfo
    {
        return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeH3());
    }

    auto Get_SeparatorThickness() -> float
    {
        return ck::debug_axes::Get_SeparatorThickness(UCkDebuggerStyleSettings::Get_Selection());
    }

    // SSeparator::Thickness is a construction-time argument with no setter, which is why
    // ck::debug_axes::Make_AxisSeparator carries the axis on an SBox HEIGHT override. The toolbar's
    // divider is vertical, so it needs the width twin — same shape, same collapse-at-zero rule.
    auto Make_VerticalAxisSeparator() -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .WidthOverride_Lambda([]() -> FOptionalSize
            {
                return FOptionalSize{Get_SeparatorThickness()};
            })
            .Visibility_Lambda([]()
            {
                return Get_SeparatorThickness() > 0.0f ? EVisibility::Visible : EVisibility::Collapsed;
            })
            [
                SNew(SSeparator)
                .Orientation(Orient_Vertical)
                .Thickness(1.0f)
            ];
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkEqsDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = MakeShared<FCkEqsDebugger_ViewModel>();
    _WorldModel = MakeShared<FCkDebuggerModel_WorldSelector>();

#if WITH_EDITOR
    _EndPIEHandle   = FEditorDelegates::EndPIE.AddSP(this, &SCkEqsDebuggerWindow::OnEndPIE);
    _BeginPIEHandle = FEditorDelegates::BeginPIE.AddSP(this, &SCkEqsDebuggerWindow::OnBeginPIE);
#endif

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome)
        .WindowId(Get_WindowId())
        .ToolTabId(TEXT("CkEqsDebugger"))
        .DisplayName(Get_WindowDisplayName())
        .MenuActionsContent()
        [
            BuildMenuActions()
        ]
        .Content()
        [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(FMargin{6.0f})
        [
            BuildToolbar()
        ]
        + SVerticalBox::Slot().AutoHeight()
        [
            ck::debug_axes::Make_AxisSeparator()
        ]
        + SVerticalBox::Slot().FillHeight(1.0f).Padding(FMargin{6.0f, 6.0f, 6.0f, 6.0f})
        [
            SNew(SSplitter)
            .Orientation(Orient_Horizontal)
            + SSplitter::Slot().Value(0.30f)
            [
                SNew(SBorder).Padding(FMargin{4.0f})
                [
                    SAssignNew(_QueryList, SCkEqsDebugger_QueryList).ViewModel(_ViewModel)
                ]
            ]
            + SSplitter::Slot().Value(0.35f)
            [
                SNew(SBorder).Padding(FMargin{4.0f})
                [
                    SAssignNew(_CandidatePanel, SCkEqsDebugger_CandidatePanel).ViewModel(_ViewModel)
                ]
            ]
            + SSplitter::Slot().Value(0.35f)
            [
                SNew(SBorder).Padding(FMargin{4.0f})
                [
                    SAssignNew(_TestBreakdownPanel, SCkEqsDebugger_TestBreakdownPanel).ViewModel(_ViewModel)
                ]
            ]
        ]
        ]
    ];

    Register_WithGate();
}

// --------------------------------------------------------------------------------------------------------------------

SCkEqsDebuggerWindow::~SCkEqsDebuggerWindow()
{
    // Tear down the in-world overlay BEFORE the registry tears down. ck::IsValid on the overlay-parent handle
    // guards against firing into a half-dead world.
    _OverlayManager.Reset();

#if WITH_EDITOR
    if (_EndPIEHandle.IsValid())   { FEditorDelegates::EndPIE.Remove(_EndPIEHandle); }
    if (_BeginPIEHandle.IsValid()) { FEditorDelegates::BeginPIE.Remove(_BeginPIEHandle); }
#endif
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkEqsDebuggerWindow::
    BuildMenuActions()
    -> TSharedRef<SWidget>
{
    auto Actions = TArray<FCkDebug_IconToggleAction>{
        FCkDebug_IconToggleAction{
            TEXT("PauseCapture"),
            TEXT("Snowflake"),
            FText::FromString(TEXT("Pause capture")),
            FText::FromString(TEXT("Pause or resume the EQS data collector without pausing gameplay.")),
            TAttribute<bool>::CreateLambda([this]() -> bool
            {
                return _ViewModel.IsValid() && _ViewModel->Get_Paused();
            }),
            FOnCkDebug_IconToggleChanged::CreateLambda([this](bool InIsOn)
            {
                if (_ViewModel.IsValid())
                { _ViewModel->Set_Paused(InIsOn); }
            }),
            TAttribute<bool>::CreateLambda([this]() -> bool { return _ViewModel.IsValid(); })},
        MakeOverlayToggle(TEXT("Overlay"), TEXT("World"), &UCkEqsDebuggerSettings::Show_Overlay,
            TEXT("Overlay"), TEXT("Master toggle. When off, no spheres / lines / markers are drawn for the selected query."), true),
        MakeOverlayToggle(TEXT("AllQueries"), TEXT("People"), &UCkEqsDebuggerSettings::Show_AllQueriesAlways,
            TEXT("All queries"), TEXT("Show the overlay for every query in the world, not just the selected query.")),
        MakeOverlayToggle(TEXT("Candidates"), TEXT("Probe"), &UCkEqsDebuggerSettings::Show_AllCandidateSpheres,
            TEXT("Candidates"), TEXT("Draw a sphere at every candidate location, color-lerped by final score.")),
        MakeOverlayToggle(TEXT("Best"), TEXT("Target"), &UCkEqsDebuggerSettings::Show_BestCandidateHighlight,
            TEXT("Best pick"), TEXT("Highlight the top-scoring candidate with a larger amber sphere.")),
        MakeOverlayToggle(TEXT("Failed"), TEXT("Skull"), &UCkEqsDebuggerSettings::Show_FailedCandidates,
            TEXT("Failed candidates"), TEXT("Draw filter-failed candidates as muted gray spheres.")),
        MakeOverlayToggle(TEXT("Querier"), TEXT("Person"), &UCkEqsDebuggerSettings::Show_QuerierMarker,
            TEXT("Querier"), TEXT("Draw a small sphere at the querier's location.")),
        MakeOverlayToggle(TEXT("BestLine"), TEXT("Rail"), &UCkEqsDebuggerSettings::Show_BestLocationLine,
            TEXT("Best line"), TEXT("Draw a line from the querier to the best candidate location.")),
        MakeOverlayToggle(TEXT("GridLines"), TEXT("Grid"), &UCkEqsDebuggerSettings::Show_GridLines,
            TEXT("Grid lattice"), TEXT("For SimpleGrid and Grid generators, draw a wireframe lattice connecting cells."))};

    return SNew(SCkDebug_IconToolbar)
        .Actions(MoveTemp(Actions));
}

auto
    SCkEqsDebuggerWindow::
    BuildToolbar()
    -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)

        // Title
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin{0.0f, 0.0f, 12.0f, 0.0f})
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("CK EQS Debugger")))
            .ColorAndOpacity(FSlateColor{CkStyle::Text()})
            .Font_Static(&ck_eqs_debugger_window::Get_TitleFont)
        ]

        // World selector (shared across all CK debuggers)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin{0.0f, 0.0f, 12.0f, 0.0f})
        [
            SNew(SCkDebug_WorldSelector, _WorldModel)
            .ShowHeaderLabel(false)
        ]

        // Separator
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin{6.0f, 0.0f})
        [
            ck_eqs_debugger_window::Make_VerticalAxisSeparator()
        ]

        // Status text (query count + pause indicator).
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SAssignNew(_StatusLabel, SCkDebug_SelectableLabel)
            .Text(FText::FromString(TEXT("(no PIE world)")))
            .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
        ]

        // Refresh-rate / mode controls — common widget; reads/writes UCkDebuggerWindowSettings.
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin{12.0f, 0.0f, 0.0f, 0.0f})
        [
            SNew(SCkDebugger_RefreshControls).WindowId(WindowId)
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkEqsDebuggerWindow::
    Tick(
        const FGeometry& InAllottedGeometry,
        double           InCurrentTime,
        float            InDeltaTime)
    -> void
{
    // MUST be the WindowBase super, not SCompoundWidget — the base's Tick is what drives the gated
    // style-revision watch that routes into OnStyleRevisionChanged below.
    SCkDebugger_WindowBase::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    _WorldModel->Ensure_AutoSelect();
    _CachedWorld = _WorldModel->Get_SelectedWorld();

    if (NOT _CachedWorld)
    {
        if (_StatusLabel.IsValid())
        { _StatusLabel->SetText(FText::FromString(TEXT("(no PIE world)"))); }
        _OverlayManager.Reset();
        return;
    }

    if (NOT _ViewModel.IsValid())
    { return; }

    _ViewModel->Tick(_CachedWorld, InDeltaTime);

    if (_StatusLabel.IsValid())
    {
        const auto Count = _ViewModel->Get_AllQueries().Num();
        _StatusLabel->SetText(FText::FromString(
            ck::Format_UE(TEXT("queries: {}{}"),
                Count,
                _ViewModel->Get_Paused() ? TEXT("    [PAUSED]") : TEXT(""))));
    }

    // In-world overlay — gated entirely by user settings; manager handles master-toggle off + null selection
    // + the all-queries mode (which iterates Get_AllQueries instead of just the selected one).
    _OverlayManager.Update(
        _CachedWorld,
        &_ViewModel->Get_AllQueries(),
        _ViewModel->Get_CurrentQueryInfo(),
        UCkEqsDebuggerSettings::Get());
}

auto
    SCkEqsDebuggerWindow::
    OnStyleRevisionChanged()
    -> void
{
    // Everything this window composes is attribute-bound, so the visual axes have already landed by
    // the time we get here. The three list panels still regenerate their rows: an STableRow bakes its
    // own style at generation time, and the row bodies are the only surface left that a structural
    // axis option could reshape.
    if (_QueryList.IsValid())          { _QueryList->Rebuild_ForStyleChange(); }
    if (_CandidatePanel.IsValid())     { _CandidatePanel->Rebuild_ForStyleChange(); }
    if (_TestBreakdownPanel.IsValid()) { _TestBreakdownPanel->Rebuild_ForStyleChange(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkEqsDebuggerWindow::
    OnEndPIE(
        const bool /*InWasSimulating*/)
    -> void
{
    // Drop the overlay parent handle BEFORE the registry tears down (CkSmDebugger handle-lifetime contract).
    _OverlayManager.Reset();
    if (_ViewModel.IsValid())
    { _ViewModel->Reset_ForWorldChange(); }
    _CachedWorld = nullptr;
}

auto
    SCkEqsDebuggerWindow::
    OnBeginPIE(
        const bool /*InIsSimulating*/)
    -> void
{
    _OverlayManager.Reset();
    if (_ViewModel.IsValid())
    { _ViewModel->Reset_ForWorldChange(); }
    _CachedWorld = nullptr;
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------
