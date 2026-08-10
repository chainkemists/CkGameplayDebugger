#include "SCkDebug_WindowChrome.h"

#include "CkCore/Ensure/CkEnsure.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"
#include "CkDebuggerCommon/Navigation/CkDebug_EntityTarget.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_UseEcsSelection.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Framework/Docking/TabManager.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
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

    _DisplayName = InArgs._DisplayName;
    _StatusText = InArgs._StatusText;

    const auto MenuActions = InArgs._MenuActionsContent.Widget;
    const auto Toolbar = InArgs._ToolbarContent.Widget;
    const auto Content = InArgs._Content.Widget;
    const auto Status = InArgs._StatusContent.Widget;
    const auto HasMenuActions = MenuActions != SNullWidget::NullWidget;
    const auto HasToolbar = Toolbar != SNullWidget::NullWidget;
    const auto HasStatusWidget = Status != SNullWidget::NullWidget;
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
                    SNew(SBorder)
                        .BorderImage_Lambda([]{ return ck::debug_axes::Get_SurfaceBrush(1); })
                        .BorderBackgroundColor_Lambda([]{ return FSlateColor{ck::debug_axes::Get_SurfaceTint(1)}; })
                        .Padding(FMargin{CkStyle::SpaceM, CkStyle::SpaceS})
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .HAlign(HAlign_Left)
                            .VAlign(VAlign_Center)
                            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                            [
                                SNew(STextBlock)
                                    .Text(_DisplayName)
                                    .Font(CkStyle::BoldFont(CkStyle::FontSizeSmall()))
                                    .ColorAndOpacity(CkStyle::TextStrong())
                                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                            ]
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .HAlign(HAlign_Fill)
                            .VAlign(VAlign_Center)
                            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                            [
                                SNew(SBox)
                                    .Visibility(HasMenuActions ? EVisibility::Visible : EVisibility::Collapsed)
                                    [
                                        MenuActions
                                    ]
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            [
                                SNew(SComboButton)
                                    .ContentPadding(FMargin{CkStyle::SpaceS, 1.0f})
                                    .ToolTipText(FText::FromString(TEXT("Open another CK debugger")))
                                    .OnGetMenuContent(this, &SCkDebug_WindowChrome::Build_DebuggerMenu)
                                    .ButtonContent()
                                    [
                                        SNew(STextBlock)
                                            .Text(FText::FromString(TEXT("Debuggers")))
                                            .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                                    ]
                            ]
                        ]
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SBox)
                        .Visibility(HasToolbar ? EVisibility::Visible : EVisibility::Collapsed)
                        [
                            Toolbar
                        ]
                ]

                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    Content
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SBorder)
                        .BorderImage_Lambda([]{ return ck::debug_axes::Get_SurfaceBrush(1); })
                        .BorderBackgroundColor_Lambda([]{ return FSlateColor{ck::debug_axes::Get_SurfaceTint(1)}; })
                        .Padding(FMargin{CkStyle::SpaceM, CkStyle::SpaceXS})
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .VAlign(VAlign_Center)
                            [
                                EffectiveStatus
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
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
                        ]
                ]
            ]
    ];
}

auto SCkDebug_WindowChrome::Build_DebuggerMenu() const -> TSharedRef<SWidget>
{
    auto ToolsBox = SNew(SVerticalBox);
    const auto Tools = FCkDebuggerToolRegistry::Get().Get_Tools();

    if (Tools.IsEmpty())
    {
        ToolsBox->AddSlot()
            .AutoHeight()
            .Padding(FMargin{CkStyle::SpaceM})
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("No debugger tools registered")))
                    .ColorAndOpacity(CkStyle::TextMute())
            ];
    }

    for (const auto& Tool : Tools)
    {
        const auto TabId = Tool.Get_TabId();
        ToolsBox->AddSlot()
            .AutoHeight()
            [
                SNew(SButton)
                    .Text(Tool.Get_DisplayName())
                    .ToolTipText(Tool.Get_Tooltip())
                    .HAlign(HAlign_Left)
                    .IsEnabled(TabId != _ToolTabId && FGlobalTabmanager::Get()->HasTabSpawner(TabId))
                    .OnClicked(this, &SCkDebug_WindowChrome::OnOpenDebugger, TabId)
            ];
    }

    return SNew(SScrollBox)
        .Orientation(Orient_Vertical)
        + SScrollBox::Slot()
        [
            ToolsBox
        ];
}

auto SCkDebug_WindowChrome::OnOpenDebugger(FName InTabId) const -> FReply
{
    if (NOT InTabId.IsNone() && FGlobalTabmanager::Get()->HasTabSpawner(InTabId))
    { FGlobalTabmanager::Get()->TryInvokeTab(FTabId{InTabId}); }

    return FReply::Handled();
}

auto SCkDebug_WindowChrome::Get_DefaultStatusText() const -> FText
{
    const auto Status = _StatusText.Get();
    if (NOT Status.IsEmpty())
    { return Status; }

    const auto Name = _DisplayName.Get();
    return FText::Format(
        FText::FromString(TEXT("{0} | {1} debugger tools available")),
        Name.IsEmpty() ? FText::FromName(_WindowId) : Name,
        FText::AsNumber(FCkDebuggerToolRegistry::Get().Get_Tools().Num()));
}

// --------------------------------------------------------------------------------------------------------------------
