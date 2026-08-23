#include "SCkDebug_WindowChrome.h"

#include "CkCore/Ensure/CkEnsure.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"
#include "CkDebuggerCommon/Navigation/CkDebug_EntityTarget.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Window/SCkDebugger_RefreshControls.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_UseEcsSelection.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconButton.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSpeedControl.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Framework/Docking/TabManager.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

auto SCkDebug_WindowChrome::Construct(const FArguments& InArgs) -> void
{
    const auto HasWindowId = NOT InArgs._WindowId.IsNone();
    CK_ENSURE_IF_NOT(HasWindowId, TEXT("Debugger window chrome requires a stable window id"))
    {}
    _WindowId = HasWindowId ? InArgs._WindowId : FName{TEXT("CkDebugger")};

    const auto HasToolTabId = NOT InArgs._ToolTabId.IsNone();
    CK_ENSURE_IF_NOT(HasToolTabId, TEXT("Debugger window chrome requires its registered tool tab id"))
    {}
    _ToolTabId = HasToolTabId ? InArgs._ToolTabId : _WindowId;

    _StatusText = InArgs._StatusText;

    const auto MenuActions = InArgs._MenuActionsContent.Widget;
    const auto CommonActions = InArgs._CommonActionsContent.Widget;
    const auto Toolbar = InArgs._ToolbarContent.Widget;
    const auto Content = InArgs._Content.Widget;
    const auto Status = InArgs._StatusContent.Widget;
    const auto HasMenuActions = MenuActions != SNullWidget::NullWidget;
    const auto HasCommonActions = CommonActions != SNullWidget::NullWidget;
    const auto HasToolbar = Toolbar != SNullWidget::NullWidget;
    const auto HasStatusWidget = Status != SNullWidget::NullWidget;
    auto CommandGroups = InArgs._CommandGroups;
    if (HasMenuActions)
    {
        CommandGroups.Add(FCkDebug_CommandGroup::Primary(
            TEXT("LegacyMenuActions"),
            FText::FromString(TEXT("Debugger view actions")),
            MenuActions));
    }
    if (HasToolbar)
    {
        CommandGroups.Add(FCkDebug_CommandGroup::Context(
            TEXT("LegacyToolbar"),
            FText::FromString(TEXT("Debugger commands")),
            Toolbar));
    }

    const auto EffectiveStatus = HasStatusWidget
        ? Status
        : StaticCastSharedRef<SWidget>(
            SNew(STextBlock)
                .Text(this, &SCkDebug_WindowChrome::Get_DefaultStatusText)
                .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                .ColorAndOpacity(CkStyle::TextMute()));

    ChildSlot
    [
        // Window ground (depth 0) with a header strip and a status strip (depth 1) on it — the
        // SurfaceElevation axis' canonical two-tier shape.
        SNew(SBorder)
            .BorderImage_Lambda([]{ return ck::debug_axes::Get_SurfaceBrush(0); })
            .BorderBackgroundColor_Lambda([]{ return FSlateColor{ck::debug_axes::Get_SurfaceTint(0)}; })
            .Padding(0.0f)
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SCkDebug_CommandBar)
                    .Groups(MoveTemp(CommandGroups))
                    .UtilityContent()
                    [
                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                        [
                            SNew(SBox)
                            .MaxDesiredWidth(320.0f)
                            .Clipping(EWidgetClipping::ClipToBounds)
                            .Visibility_Lambda([this, HasStatusWidget]()
                            {
                                return HasStatusWidget || NOT _StatusText.Get().IsEmpty()
                                    ? EVisibility::Visible
                                    : EVisibility::Collapsed;
                            })
                            [
                                EffectiveStatus
                            ]
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                        [
                            SNew(SBox)
                            .Visibility_Lambda([this]()
                            {
                                return FCkDebug_EntityTargetRegistry::Get().Has_Route(_ToolTabId)
                                    ? EVisibility::Visible
                                    : EVisibility::Collapsed;
                            })
                            [
                                SNew(SCkDebug_UseEcsSelection)
                                .TargetTabId(_ToolTabId)
                            ]
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                        [
                            SNew(SBox)
                            .Visibility(InArgs._ShowRefreshControls ? EVisibility::Visible : EVisibility::Collapsed)
                            [
                                SNew(SCkDebugger_RefreshControls)
                                .WindowId(_WindowId)
                            ]
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                        [
                            SNew(SCkDebug_WorldSpeedControl)
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                        [
                            SNew(SBox)
                            .Visibility(HasCommonActions ? EVisibility::Visible : EVisibility::Collapsed)
                            [
                                CommonActions
                            ]
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SCkDebug_IconButton)
                            .IconId(ECk_Icon::Diagnostics)
                            .Label(FText::FromString(TEXT("Open CK Debugger Launcher")))
                            .IsEnabled_Lambda([]()
                            {
                                return FGlobalTabmanager::Get()->HasTabSpawner(ck::debugger_tabs::LauncherTabId);
                            })
                            .OnClicked(this, &SCkDebug_WindowChrome::OnOpenLauncher)
                        ]
                    ]
                ]

                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    Content
                ]

            ]
    ];
}

auto SCkDebug_WindowChrome::OnOpenLauncher() const -> FReply
{
    ck::debugger_tabs::Invoke_DebuggerTab(ck::debugger_tabs::LauncherTabId);

    return FReply::Handled();
}

auto SCkDebug_WindowChrome::Get_DefaultStatusText() const -> FText
{
    const auto Status = _StatusText.Get();
    return Status;
}

// --------------------------------------------------------------------------------------------------------------------
