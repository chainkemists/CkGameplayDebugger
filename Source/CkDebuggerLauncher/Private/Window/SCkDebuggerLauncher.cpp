#include "SCkDebuggerLauncher.h"

#include "Styles/CkDebuggerLauncherStyle.h"

#include "CkCore/Ensure/CkEnsure.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SCkDebuggerLauncher"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debugger_launcher
{
    auto Get_SeparatorThickness()
        -> float
    {
        return ck::debug_axes::Get_SeparatorThickness(UCkDebuggerStyleSettings::Get_Selection());
    }

    // RowDensity on the rail's tool buttons. Slot padding is a Slate attribute, so the axis moves
    // buttons that were built on the last registry change.
    auto Get_ToolButtonPadding()
        -> FMargin
    {
        return ck::debug_axes::Apply_RowDensity(FMargin{0.0f, 2.0f});
    }

    // The two rail text styles bake their font at style-set creation, so TextScale can only reach
    // them as a per-widget Font override. Sizes mirror CkDebuggerLauncherStyle exactly: the section
    // caps are a deliberate 7pt outlier (no CkStyle role goes below FontSizeMicro's 8), the tool
    // label is the FontSizeSmall role.
    auto Get_SectionFont()
        -> FSlateFontInfo
    {
        return ck::debug_axes::ScaledFont("Bold", 7);
    }

    auto Get_ToolLabelFont()
        -> FSlateFontInfo
    {
        return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall());
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkDebuggerLauncher::Construct(const FArguments& InArgs) -> void
{
    _RegistryChangedHandle = FCkDebuggerToolRegistry::Get().Get_OnChanged().AddSP(
        this,
        &SCkDebuggerLauncher::RebuildTools);

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FCkDebuggerLauncherStyle::Get_BackgroundBrush())
        .Padding(FMargin{4.0f})
        [
            SNew(SBox)
            .MinDesiredWidth(52.0f)
            [
                SNew(SScrollBox)
                .Orientation(Orient_Vertical)
                + SScrollBox::Slot()
                [
                    SAssignNew(_ToolList, SVerticalBox)
                ]
            ]
        ]
    ];

    // Seed the baseline so the first tick can only report a change the user actually made.
    if (const auto* Settings = UCkDebuggerStyleSettings::Get())
    { _LastSeenStyleRevision = Settings->Get_Revision(); }

    RebuildTools();
}

auto SCkDebuggerLauncher::Poll_StyleRevision() -> void
{
    const auto* Settings = UCkDebuggerStyleSettings::Get();

    if (Settings == nullptr)
    { return; }

    const auto Revision = Settings->Get_Revision();

    if (Revision == _LastSeenStyleRevision)
    { return; }

    _LastSeenStyleRevision = Revision;

    // The rail's construct-baked reads (row density on the button slots, the separator between
    // category groups) live inside RebuildTools — the same entry point the registry uses.
    RebuildTools();
}

SCkDebuggerLauncher::~SCkDebuggerLauncher()
{
    if (_RegistryChangedHandle.IsValid())
    {
        FCkDebuggerToolRegistry::Get().Get_OnChanged().Remove(_RegistryChangedHandle);
        _RegistryChangedHandle.Reset();
    }
}

auto SCkDebuggerLauncher::Tick(
    const FGeometry& InAllottedGeometry,
    double InCurrentTime,
    float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    Poll_StyleRevision();

    const auto Width = InAllottedGeometry.GetLocalSize().X;
    auto ShowLabels = _ShowLabels;

    if (NOT ShowLabels && Width >= 180.0f) { ShowLabels = true; }
    else if (ShowLabels && Width <= 150.0f) { ShowLabels = false; }

    if (ShowLabels != _ShowLabels)
    {
        _ShowLabels = ShowLabels;
        Invalidate(EInvalidateWidgetReason::Layout);
    }
}

auto SCkDebuggerLauncher::RebuildTools() -> void
{
    // Registry-changed broadcasts also arrive during UnloadModulesAtShutdown —
    // every feature debugger unregisters its descriptor from ShutdownModule,
    // AFTER Slate itself has shut down. Constructing widgets then (SButton's
    // ToolTipText goes through FSlateApplicationBase::Get()) fires the
    // IsInitialized ensure on exit. Nothing to rebuild for — the app is dying.
    if (NOT FSlateApplication::IsInitialized())
    { return; }

    if (NOT _ToolList.IsValid())
    { return; }

    _ToolList->ClearChildren();
    const auto Tools = FCkDebuggerToolRegistry::Get().Get_Tools();

    if (Tools.IsEmpty())
    {
        _ToolList->AddSlot()
        .AutoHeight()
        .Padding(FMargin{4.0f})
        [
            SNew(STextBlock)
            .Text(LOCTEXT("NoTools", "No debugger tools registered"))
            .AutoWrapText(true)
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ];
        return;
    }

    auto CurrentCategory = ECkDebuggerToolCategory::Invalid;
    for (const auto& Tool : Tools)
    {
        if (Tool.Get_Category() != CurrentCategory)
        {
            const auto AddSeparator = CurrentCategory != ECkDebuggerToolCategory::Invalid;
            CurrentCategory = Tool.Get_Category();

            _ToolList->AddSlot()
            .AutoHeight()
            [
                Build_CategoryHeader(CurrentCategory, AddSeparator)
            ];
        }

        _ToolList->AddSlot()
        .AutoHeight()
        .HAlign(HAlign_Fill)
        .Padding(TAttribute<FMargin>::CreateStatic(&ck_debugger_launcher::Get_ToolButtonPadding))
        [
            Build_ToolButton(Tool)
        ];
    }
}

auto SCkDebuggerLauncher::Build_ToolButton(const FCkDebuggerToolDescriptor& InTool) -> TSharedRef<SWidget>
{
    const auto TabId = InTool.Get_TabId();
    auto IconBrush = FCkDebuggerLauncherStyle::Get_IconBrush(InTool.Get_IconId());

    CK_ENSURE_IF_NOT(IconBrush != nullptr,
        TEXT("Debugger launcher tool [{}] references missing icon [{}]"),
        TabId,
        InTool.Get_IconId())
    { IconBrush = FAppStyle::GetBrush(TEXT("Icons.Warning")); }

    const auto Tooltip = FText::Format(
        LOCTEXT("ToolTooltipFormat", "{0}\n{1}"),
        InTool.Get_DisplayName(),
        InTool.Get_Tooltip());

    const auto WeakPanel = TWeakPtr<SCkDebuggerLauncher>{SharedThis(this)};

    return SNew(SButton)
        .ButtonStyle(&FCkDebuggerLauncherStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("CkDebuggerLauncher.ToolButton")))
        .ContentPadding(FMargin{5.0f})
        .HAlign(HAlign_Fill)
        .ToolTipText(Tooltip)
        .IsEnabled_Lambda([TabId]()
        {
            return FGlobalTabmanager::Get()->HasTabSpawner(TabId);
        })
        .OnClicked_Lambda([TabId]()
        {
            if (FGlobalTabmanager::Get()->HasTabSpawner(TabId))
            { ck::debugger_tabs::Invoke_DebuggerTab(TabId); }

            return FReply::Handled();
        })
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(3.0f)
                .HeightOverride(24.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FCkDebuggerLauncherStyle::Get().GetBrush(TEXT("CkDebuggerLauncher.ActiveMarker")))
                    .Visibility_Lambda([TabId]()
                    {
                        return FGlobalTabmanager::Get()->FindExistingLiveTab(FTabId{TabId}).IsValid()
                            ? EVisibility::Visible
                            : EVisibility::Hidden;
                    })
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            .Padding(FMargin{7.0f, 0.0f})
            [
                SNew(SBox)
                .WidthOverride_Lambda([]() -> FOptionalSize
                {
                    return FOptionalSize{ck::debug_axes::Apply_IconSize(28.0f)};
                })
                .HeightOverride_Lambda([]() -> FOptionalSize
                {
                    return FOptionalSize{ck::debug_axes::Apply_IconSize(28.0f)};
                })
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                [
                    SNew(SImage)
                    .Image(IconBrush)
                    .ColorAndOpacity(FSlateColor::UseForeground())
                ]
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .Padding(FMargin{4.0f, 0.0f, 8.0f, 0.0f})
            [
                SNew(STextBlock)
                .Text(InTool.Get_DisplayName())
                .TextStyle(&FCkDebuggerLauncherStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("CkDebuggerLauncher.ToolText")))
                .Font_Static(&ck_debugger_launcher::Get_ToolLabelFont)
                .Visibility_Lambda([WeakPanel]()
                {
                    const auto Panel = WeakPanel.Pin();
                    return Panel.IsValid() ? Panel->Get_LabelVisibility() : EVisibility::Collapsed;
                })
            ]
        ];
}

auto SCkDebuggerLauncher::Build_CategoryHeader(
    ECkDebuggerToolCategory InCategory,
    bool InAddSeparator) -> TSharedRef<SWidget>
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin{2.0f, InAddSeparator ? 6.0f : 2.0f, 2.0f, 3.0f})
        [
            // SSeparator::Thickness is a construction-time argument with no setter, and the rail
            // only rebuilds when the registry changes, so the axis is carried by an SBox override.
            SNew(SBox)
            .HeightOverride_Lambda([]() -> FOptionalSize
            {
                return FOptionalSize{ck_debugger_launcher::Get_SeparatorThickness()};
            })
            .Visibility_Lambda([InAddSeparator]()
            {
                return InAddSeparator && ck_debugger_launcher::Get_SeparatorThickness() > 0.0f
                    ? EVisibility::Visible
                    : EVisibility::Collapsed;
            })
            [
                SNew(SSeparator)
                .SeparatorImage(FCkDebuggerLauncherStyle::Get_SeparatorBrush())
                .Thickness(1.0f)
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Center)
        .Padding(FMargin{2.0f, 0.0f, 2.0f, 2.0f})
        [
            SNew(STextBlock)
            .Text(Get_CategoryDisplayName(InCategory))
            .TextStyle(&FCkDebuggerLauncherStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("CkDebuggerLauncher.SectionText")))
            .Font_Static(&ck_debugger_launcher::Get_SectionFont)
        ];
}

auto SCkDebuggerLauncher::Get_LabelVisibility() const -> EVisibility
{
    return _ShowLabels ? EVisibility::Visible : EVisibility::Collapsed;
}

auto SCkDebuggerLauncher::Get_CategoryDisplayName(ECkDebuggerToolCategory InCategory) -> FText
{
    switch (InCategory)
    {
        case ECkDebuggerToolCategory::Core: return LOCTEXT("CategoryCore", "CORE");
        case ECkDebuggerToolCategory::Ai: return LOCTEXT("CategoryAi", "AI");
        case ECkDebuggerToolCategory::Systems: return LOCTEXT("CategorySystems", "SYSTEMS");
        case ECkDebuggerToolCategory::Interface: return LOCTEXT("CategoryInterface", "INTERFACE");
        case ECkDebuggerToolCategory::Tools: return LOCTEXT("CategoryTools", "TOOLS");
        case ECkDebuggerToolCategory::Invalid: return FText::GetEmpty();
    }

    return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------
