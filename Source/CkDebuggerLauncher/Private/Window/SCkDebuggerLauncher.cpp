#include "SCkDebuggerLauncher.h"

#include "Styles/CkDebuggerLauncherStyle.h"

#include "CkCore/Ensure/CkEnsure.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

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
    // RowDensity and IconSize both apply as a DELTA on the rail's own geometry: the rail is denser
    // and its glyphs larger than the axes' own absolute scales, so only the offset between options
    // is the axes' business. The default options leave the rail exactly as it shipped.
    auto Apply_RowDensity(
        const FMargin& InBase)
        -> FMargin
    {
        const auto Baseline = ck::debug_axes::Get_RowPadding(FCkDebuggerStyleSelection{});
        const auto Current  = ck::debug_axes::Get_RowPadding(UCkDebuggerStyleSettings::Get_Selection());

        const auto DeltaX = Current.Left - Baseline.Left;
        const auto DeltaY = Current.Top  - Baseline.Top;

        return FMargin
        {
            FMath::Max(0.0f, InBase.Left   + DeltaX),
            FMath::Max(0.0f, InBase.Top    + DeltaY),
            FMath::Max(0.0f, InBase.Right  + DeltaX),
            FMath::Max(0.0f, InBase.Bottom + DeltaY)
        };
    }

    auto Apply_IconSize(
        float InBase)
        -> float
    {
        const auto Baseline = ck::debug_axes::Get_IconSize(FCkDebuggerStyleSelection{});
        const auto Current  = ck::debug_axes::Get_IconSize(UCkDebuggerStyleSettings::Get_Selection());

        return FMath::Max(1.0f, InBase + (Current - Baseline));
    }

    auto Get_SeparatorThickness()
        -> float
    {
        return ck::debug_axes::Get_SeparatorThickness(UCkDebuggerStyleSettings::Get_Selection());
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
        .Padding(ck_debugger_launcher::Apply_RowDensity(FMargin{0.0f, 2.0f}))
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
            { FGlobalTabmanager::Get()->TryInvokeTab(FTabId{TabId}); }

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
                    return FOptionalSize{ck_debugger_launcher::Apply_IconSize(28.0f)};
                })
                .HeightOverride_Lambda([]() -> FOptionalSize
                {
                    return FOptionalSize{ck_debugger_launcher::Apply_IconSize(28.0f)};
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
