#include "CkEqsDebugger/Window/SCkEqsDebuggerWindow.h"

#include "CkEqsDebugger/CkEqsDebuggerStyle.h"
#include "CkEqsDebugger/Settings/CkEqsDebuggerSettings.h"
#include "CkEqsDebugger/ViewModel/CkEqsDebugger_ViewModel.h"
#include "CkEqsDebugger/Window/SCkEqsDebugger_QueryList.h"
#include "CkEqsDebugger/Window/SCkEqsDebugger_CandidatePanel.h"
#include "CkEqsDebugger/Window/SCkEqsDebugger_TestBreakdownPanel.h"

#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkDebuggerCommon/Window/SCkDebugger_RefreshControls.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SMenuAnchor.h"

#define LOCTEXT_NAMESPACE "SCkEqsDebuggerWindow"

const FName SCkEqsDebuggerWindow::WindowId = FName("CkEqsDebugger");

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    // One row inside the View-menu popover: a checkbox bound to a UCkEqsDebuggerSettings bool field.
    // Pointer-to-member-bool keeps the popover construction concise: BuildSettingRow(&UCk...::Show_X, "Label").
    auto BuildSettingRow(
        bool UCkEqsDebuggerSettings::* InMember,
        const FText&                   InLabel) -> TSharedRef<SWidget>
    {
        return SNew(SCheckBox)
            .IsChecked_Lambda([InMember]()
            {
                return UCkEqsDebuggerSettings::Get()->*InMember
                    ? ECheckBoxState::Checked
                    : ECheckBoxState::Unchecked;
            })
            .OnCheckStateChanged_Lambda([InMember](ECheckBoxState InState)
            {
                auto* Mutable = UCkEqsDebuggerSettings::GetMutable();
                Mutable->*InMember = (InState == ECheckBoxState::Checked);
                Mutable->SaveConfig();
            })
            [
                SNew(STextBlock)
                .Text(InLabel)
                .ColorAndOpacity(FSlateColor{FCkEqsDebuggerStyle::Color_Text_Primary})
                .Margin(FMargin{4.0f, 0.0f, 0.0f, 0.0f})
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
        SNew(SCkDebug_WindowChrome).WindowId(Get_WindowId()).ToolTabId(TEXT("CkEqsDebugger")).DisplayName(Get_WindowDisplayName()).Content()
        [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(FMargin{6.0f})
        [
            BuildToolbar()
        ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SSeparator).Orientation(Orient_Horizontal).Thickness(1.0f)
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
    BuildToolbar()
    -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)

        // Title
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin{0.0f, 0.0f, 12.0f, 0.0f})
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("CK EQS Debugger")))
            .ColorAndOpacity(FSlateColor{FCkEqsDebuggerStyle::Color_Text_Primary})
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
        ]

        // World selector (shared across all CK debuggers)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin{0.0f, 0.0f, 12.0f, 0.0f})
        [
            SNew(SCkDebug_WorldSelector, _WorldModel)
            .ShowHeaderLabel(false)
        ]

        // Pause toggle — flips ViewModel._IsPaused on click. Color-tinted when paused so it's obvious.
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin{0.0f, 0.0f, 6.0f, 0.0f})
        [
            SNew(SButton)
            .ToolTipText(FText::FromString(TEXT("Pause / resume the data collector")))
            .ButtonColorAndOpacity_Lambda([this]() -> FSlateColor
            {
                return _ViewModel.IsValid() && _ViewModel->Get_Paused()
                    ? FSlateColor{FCkEqsDebuggerStyle::Color_Status_Cancelled}
                    : FSlateColor{FCkEqsDebuggerStyle::Color_Panel_Background};
            })
            .OnClicked_Lambda([this]() -> FReply
            {
                if (_ViewModel.IsValid())
                { _ViewModel->Set_Paused(NOT _ViewModel->Get_Paused()); }
                return FReply::Handled();
            })
            [
                SNew(STextBlock)
                .Text_Lambda([this]() -> FText
                {
                    return _ViewModel.IsValid() && _ViewModel->Get_Paused()
                        ? FText::FromString(TEXT("Resume"))
                        : FText::FromString(TEXT("Pause"));
                })
                .ColorAndOpacity(FSlateColor{FCkEqsDebuggerStyle::Color_Text_Primary})
            ]
        ]

        // View menu — overlay toggles popover. Anchor + button + popover-builder lambda.
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin{0.0f, 0.0f, 6.0f, 0.0f})
        [
            SAssignNew(_ViewMenuAnchor, SMenuAnchor)
            .Placement(MenuPlacement_BelowAnchor)
            .OnGetMenuContent(this, &SCkEqsDebuggerWindow::Build_ViewMenuPopover)
            [
                SNew(SButton)
                .ToolTipText(FText::FromString(TEXT("Toggle in-world overlay features (saved per user)")))
                .OnClicked_Lambda([this]() -> FReply
                {
                    if (_ViewMenuAnchor.IsValid())
                    { _ViewMenuAnchor->SetIsOpen(NOT _ViewMenuAnchor->IsOpen()); }
                    return FReply::Handled();
                })
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("View ▾")))   // ▾
                    .ColorAndOpacity(FSlateColor{FCkEqsDebuggerStyle::Color_Text_Primary})
                ]
            ]
        ]

        // Separator
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin{6.0f, 0.0f})
        [
            SNew(SSeparator).Orientation(Orient_Vertical).Thickness(1.0f)
        ]

        // Status text (query count + pause indicator). FillWidth so refresh controls right-align.
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
        [
            SAssignNew(_StatusLabel, SCkDebug_SelectableLabel)
            .Text(FText::FromString(TEXT("(no PIE world)")))
            .ColorAndOpacity(FSlateColor{FCkEqsDebuggerStyle::Color_Text_Secondary})
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
    Build_ViewMenuPopover()
    -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .Padding(FMargin{8.0f})
        .BorderBackgroundColor(FCkEqsDebuggerStyle::Color_Panel_Background)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 0.0f, 0.0f, 4.0f})
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("In-World Overlay")))
                .ColorAndOpacity(FSlateColor{FCkEqsDebuggerStyle::Color_Text_Secondary})
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 2.0f})
            [ BuildSettingRow(&UCkEqsDebuggerSettings::Show_Overlay,                FText::FromString(TEXT("Show overlay (master)"))) ]

            + SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 2.0f})
            [ BuildSettingRow(&UCkEqsDebuggerSettings::Show_AllQueriesAlways,       FText::FromString(TEXT("Show ALL queries (ignore selection)"))) ]

            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SSeparator).Orientation(Orient_Horizontal).Thickness(1.0f) ]

            + SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 2.0f})
            [ BuildSettingRow(&UCkEqsDebuggerSettings::Show_AllCandidateSpheres,    FText::FromString(TEXT("All candidate spheres"))) ]

            + SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 2.0f})
            [ BuildSettingRow(&UCkEqsDebuggerSettings::Show_BestCandidateHighlight, FText::FromString(TEXT("Highlight best pick"))) ]

            + SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 2.0f})
            [ BuildSettingRow(&UCkEqsDebuggerSettings::Show_FailedCandidates,       FText::FromString(TEXT("Show failed candidates"))) ]

            + SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 2.0f})
            [ BuildSettingRow(&UCkEqsDebuggerSettings::Show_QuerierMarker,          FText::FromString(TEXT("Querier marker"))) ]

            + SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 2.0f})
            [ BuildSettingRow(&UCkEqsDebuggerSettings::Show_BestLocationLine,       FText::FromString(TEXT("Querier-to-best line"))) ]

            + SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 2.0f})
            [ BuildSettingRow(&UCkEqsDebuggerSettings::Show_GridLines,              FText::FromString(TEXT("Grid lattice (SimpleGrid / Grid)"))) ]

            + SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 6.0f, 0.0f, 0.0f})
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Sizes editable in Project Settings → Plugins → Ck EQS Debugger")))
                .ColorAndOpacity(FSlateColor{FCkEqsDebuggerStyle::Color_Text_Muted})
                .Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
            ]
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
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

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
