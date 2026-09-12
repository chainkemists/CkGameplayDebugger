#include "SCkDebug_WindowChrome.h"

#include "CkCore/Ensure/CkEnsure.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"
#include "CkDebuggerCommon/Navigation/CkDebug_EntityTarget.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Window/SCkDebugger_RefreshControls.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_UseEcsSelection.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconButton.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSpeedControl.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Framework/Docking/TabManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_window_chrome
{
    auto Tokens() -> FCkUiView::FTokens
    {
        return {{TEXT("--window-chrome-surface"), TEXT("#") + ck::debug_axes::Get_SurfaceTint(0).ToFColorSRGB().ToHex()}};
    }
}

SCkDebug_WindowChrome::~SCkDebug_WindowChrome()
{
    // The retained frame may own focus, menu sessions, or pointer capture while its region is
    // still mounted by this window. Releasing it first lets FCkUiView retire only that state.
    _AuthoredFrame.Reset();
}

auto SCkDebug_WindowChrome::ActivateNativeFallback(
    const TSharedRef<SWidget>& InCommandBar,
    const TSharedRef<SWidget>& InContent) -> void
{
    ChildSlot
    [
        SNew(SBorder)
            .BorderImage_Lambda([]{ return ck::debug_axes::Get_SurfaceBrush(0); })
            .BorderBackgroundColor_Lambda([]{ return FSlateColor{ck::debug_axes::Get_SurfaceTint(0)}; })
            .Padding(0.0f)
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    InCommandBar
                ]

                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    InContent
                ]
            ]
    ];
}

auto SCkDebug_WindowChrome::ActivateAuthoredFrame(
    const TSharedRef<SWidget>& InCommandBar,
    const TSharedRef<SWidget>& InContent) -> bool
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    if (!FCkDebug_UiRegistry::TryCreate(Registry).Succeeded || !Registry.IsValid())
    { return false; }

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (!Plugin.IsValid()) { return false; }

    FCkUiView::FNativeBindings NativeBindings;
    NativeBindings.Add(TEXT("window-command-bar"), InCommandBar);
    NativeBindings.Add(TEXT("window-content"), InContent);
    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        MoveTemp(NativeBindings), {}, ck_debug_window_chrome::Tokens(), FSlateFontInfo{}, {}, Registry);
    const TSharedRef<SWidget> FrameRegion = Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    const FCkUiLoadResult Result = Candidate->ReloadFiles(
        FPaths::Combine(ResourceRoot, TEXT("DebuggerWindowChrome.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("DebuggerWindowChrome.ui.css")));
    if (!Result.Succeeded) { return false; }

    _AuthoredFrame = Candidate;
    ChildSlot[FrameRegion];
    return true;
}

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

    const TSharedRef<SWidget> CommandBar =
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
        ];

    if (!ActivateAuthoredFrame(CommandBar, Content))
    {
        ActivateNativeFallback(CommandBar, Content);
    }
}

auto SCkDebug_WindowChrome::Tick(
    const FGeometry& InAllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (_AuthoredFrame.IsValid())
    { _AuthoredFrame->PollFiles(ck_debug_window_chrome::Tokens()); }
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
