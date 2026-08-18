#include "CkInsightsDebugger/Window/SCkInsightsAnalyzerTab.h"

#include "CkInsightsDebugger_Module.h"

#include "CkInsightsAnalyzer/Core/CkFrameAnalyzer.h"
#include "CkInsightsAnalyzer/Core/CkTimerCategorizer.h"
#include "CkInsightsAnalyzer/Report/CkJsonReport.h"
#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"

#include <Brushes/SlateDynamicImageBrush.h>
#include <DesktopPlatformModule.h>
#include <Framework/MultiBox/MultiBoxBuilder.h>
#include <HAL/FileManager.h>
#include <HAL/PlatformApplicationMisc.h>
#include <HAL/PlatformTime.h>
#include <IImageWrapperModule.h>
#include <ImageCore.h>
#include <Misc/FileHelper.h>
#include <Modules/ModuleManager.h>
#include <Styling/AppStyle.h>
#include <Widgets/Images/SImage.h>
#include <Widgets/Input/SComboButton.h>
#include <Widgets/Input/SSpinBox.h>
#include <Widgets/Layout/SBorder.h>
#include <Widgets/Layout/SBox.h>
#include <Widgets/Layout/SExpandableArea.h>
#include <Widgets/Layout/SScaleBox.h>
#include <Widgets/Layout/SScrollBox.h>
#include <Widgets/Layout/SSplitter.h>
#include <Widgets/SOverlay.h>
#include <Widgets/Notifications/SProgressBar.h>
#include <Widgets/Views/SExpanderArrow.h>
#include <Widgets/Views/SHeaderRow.h>
#include <Widgets/Views/STableRow.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_insights_analyzer_tab
{
    constexpr float PanelPadding = 8.0f;
    constexpr float SectionSpacing = 8.0f;
    constexpr float ChartHeight = 200.0f;
    constexpr float SideBarWidth = 90.0f;    // proportion bar width in side panels
    constexpr double TargetFrameMs = 16.67;
    constexpr int32 TopTimerCount = 15;
    constexpr int32 MaxEagerScreenshotThumbnails = 12;

    // ---- Fonts ----
    // Same CkStyle roles as before, now through ck::debug_axes::ScaledFont so they follow TextScale.
    // Bound at the call sites through .Font_Static, so a Style Lab flip resizes text that was built
    // long before the flip — no rebuild, no invalidation.

    auto BodyFont()     -> FSlateFontInfo { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeBody()); }
    auto BodyBoldFont() -> FSlateFontInfo { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeBody()); }
    auto SmallFont()    -> FSlateFontInfo { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall()); }
    auto ItalicFont()   -> FSlateFontInfo { return ck::debug_axes::ScaledFont("Italic", CkStyle::FontSizeSmall()); }
    auto MicroFont()    -> FSlateFontInfo { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeMicro()); }
    auto TileFont()     -> FSlateFontInfo { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeH2()); }

    // ---- Metrics ----
    // RowDensity as a DELTA on this tab's own 1px row gutter, so Classic is unchanged and Compact /
    // Comfortable move the side-panel rows with every other list in the suite.

    auto Get_SidePanelRowPadding() -> FMargin
    {
        return ck::debug_axes::Apply_RowDensity(FMargin{0.0f, 1.0f});
    }

    auto Get_SidePanelSubRowPadding() -> FMargin
    {
        return ck::debug_axes::Apply_RowDensity(FMargin{CkStyle::SpaceM, 0.0f, 0.0f, 1.0f});
    }

    auto Get_HotPathRowPadding() -> FMargin
    {
        return ck::debug_axes::Apply_RowDensity(FMargin{0.0f, 1.0f});
    }

    // ---- Colors ----

    // Same thresholds as FCk_TimerCategorizer::SeverityIcon so text and icons agree.
    auto SeverityColor(double InMs) -> FLinearColor
    {
        if (InMs >= 5.0) { return CkStyle::Err(); }
        if (InMs >= 2.0) { return CkStyle::Warn(); }
        if (InMs >= 1.0) { return CkStyle::Info(); }
        return CkStyle::TextDim();
    }

    auto FrameBudgetColor(double InFrameMs) -> FLinearColor
    {
        if (InFrameMs > TargetFrameMs * 2.0) { return CkStyle::Err(); }
        if (InFrameMs > TargetFrameMs)       { return CkStyle::Warn(); }
        return CkStyle::Ok();
    }

    auto FrameBudgetTone(double InFrameMs) -> ECk_Tone
    {
        if (InFrameMs > TargetFrameMs * 2.0) { return ECk_Tone::Err; }
        if (InFrameMs > TargetFrameMs)       { return ECk_Tone::Warn; }
        return ECk_Tone::Ok;
    }

    // Stable per-category accent (hash into a small style-derived palette).
    auto CategoryColor(const FString& InName) -> FLinearColor
    {
        switch (GetTypeHash(InName) % 8u)
        {
            case 0:  return CkStyle::Info();
            case 1:  return CkStyle::Ok();
            case 2:  return CkStyle::Value_Tag();
            case 3:  return CkStyle::Accent();
            case 4:  return CkStyle::Value_Handle();
            case 5:  return CkStyle::Relationship();
            case 6:  return CkStyle::Value_String();
            default: return CkStyle::Value_Numeric();
        }
    }

    // ---- Small widget builders ----

    // The suite's section header rather than a hand-rolled uppercase STextBlock: same Bold 9pt caps,
    // but it follows SectionHeaderStyle + TextScale live. It carries its own bottom gutter, so the
    // slots that used to add one below MakeHeading no longer do (see MakePanel / the hot-path panel).
    auto MakeHeading(const FString& InText) -> TSharedRef<SWidget>
    {
        return SNew(SCkDebug_SectionHeader)
            .Label(FText::FromString(InText));
    }

    // A severity / category marker, not a glyph — so it stays an SImage rather than SCkDebug_Icon
    // (an icon well behind a solid dot is noise), but its box tracks IconSize live.
    auto MakeDot(const FLinearColor& InColor, float InSize = 7.0f) -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .WidthOverride_Lambda([InSize]() -> FOptionalSize
            { return FOptionalSize{ck::debug_axes::Apply_IconSize(InSize)}; })
            .HeightOverride_Lambda([InSize]() -> FOptionalSize
            { return FOptionalSize{ck::debug_axes::Apply_IconSize(InSize)}; })
            [
                SNew(SImage)
                .Image(CkStyle::GetFilledBrush())
                .ColorAndOpacity(InColor)
            ];
    }

    auto MakeProportionBar(double InPct, const FLinearColor& InFillColor, float InWidth) -> TSharedRef<SWidget>
    {
        const float Frac = FMath::Clamp(static_cast<float>(InPct) / 100.0f, 0.0f, 1.0f);
        return SNew(SBox)
            .WidthOverride(InWidth)
            .HeightOverride(6.0f)
            .VAlign(VAlign_Fill)
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                [
                    SNew(SImage)
                    .Image(CkStyle::GetFilledBrush())
                    .ColorAndOpacity(CkStyle::Bg3())
                ]
                + SOverlay::Slot()
                .HAlign(HAlign_Left)
                [
                    SNew(SBox)
                    .WidthOverride(FMath::Max(1.0f, InWidth * Frac))
                    [
                        SNew(SImage)
                        .Image(CkStyle::GetFilledBrush())
                        .ColorAndOpacity(InFillColor)
                    ]
                ]
            ];
    }

    auto MakeStatTile(const FString& InLabel, const FString& InValue, const FLinearColor& InValueColor) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(CkStyle::GetFilledBrush())
            .BorderBackgroundColor(CkStyle::Bg2())
            .Padding(FMargin(CkStyle::SpaceL, CkStyle::SpaceS))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(InLabel.ToUpper()))
                    .Font_Static(&MicroFont)
                    .ColorAndOpacity(CkStyle::TextMute())
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 1.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(InValue))
                    .Font_Static(&TileFont)
                    .ColorAndOpacity(InValueColor)
                ]
            ];
    }

    auto MakePanel(const FString& InHeading, const TSharedRef<SWidget>& InContent) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(CkStyle::GetFilledBrush())
            .BorderBackgroundColor(CkStyle::Bg1())
            .Padding(CkStyle::SpaceM)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    MakeHeading(InHeading)
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    InContent
                ]
            ];
    }

    /** Format an integer with comma separators (e.g. 4160 → "4,160"). */
    auto FormatWithCommas(uint64 Value) -> FString
    {
        FString Raw = FString::Printf(TEXT("%llu"), Value);
        FString Result;
        const int32 Len = Raw.Len();
        for (int32 i = 0; i < Len; ++i)
        {
            if (i > 0 && (Len - i) % 3 == 0)
            {
                Result.AppendChar(TEXT(','));
            }
            Result.AppendChar(Raw[i]);
        }
        return Result;
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Hot-path tree row
// --------------------------------------------------------------------------------------------------------------------

class SCkInsights_HotPathRow : public SMultiColumnTableRow<TSharedPtr<FCk_HotPathNode>>
{
public:
    SLATE_BEGIN_ARGS(SCkInsights_HotPathRow)
        : _FrameDurationMs(0.0)
    {}
        SLATE_ARGUMENT(TSharedPtr<FCk_HotPathNode>, Node)
        SLATE_ARGUMENT(double, FrameDurationMs)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable) -> void
    {
        _Node = InArgs._Node;
        _FrameDurationMs = InArgs._FrameDurationMs;

        SMultiColumnTableRow<TSharedPtr<FCk_HotPathNode>>::Construct(
            FSuperRowType::FArguments()
                .Padding(TAttribute<FMargin>::CreateStatic(&ck_insights_analyzer_tab::Get_HotPathRowPadding))
                .ShowSelection(true),
            InOwnerTable);
    }

    virtual auto GenerateWidgetForColumn(const FName& InColumnName) -> TSharedRef<SWidget> override
    {
        using namespace ck_insights_analyzer_tab;

        if (ck::Is_NOT_Valid(_Node))
        {
            return SNullWidget::NullWidget;
        }

        if (InColumnName == TEXT("Timer"))
        {
            const TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);

            Row->AddSlot()
               .AutoWidth()
               [
                   SNew(SExpanderArrow, SharedThis(this))
                   .IndentAmount(12.0f)
               ];

            // The synthetic "(+N below threshold)" row renders muted — it aggregates pruned
            // children rather than naming a timer.
            Row->AddSlot()
               .AutoWidth()
               .VAlign(VAlign_Center)
               .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
               [
                   MakeDot(_Node->bIsAggregate ? CkStyle::TextMute() : SeverityColor(_Node->InclusiveMs))
               ];

            Row->AddSlot()
               .AutoWidth()
               .VAlign(VAlign_Center)
               [
                   SNew(STextBlock)
                   .Text(FText::FromString(_Node->DisplayName))
                   .Font_Lambda([IsAggregate = _Node->bIsAggregate]()
                   { return IsAggregate ? ItalicFont() : BodyFont(); })
                   .ColorAndOpacity(_Node->bIsAggregate ? CkStyle::TextDim() : CkStyle::Text())
                   .ToolTipText(FText::FromString(_Node->RawName))
               ];

            const FString Breadcrumb = DoBuildBreadcrumbSuffix();
            if (NOT Breadcrumb.IsEmpty())
            {
                Row->AddSlot()
                   .AutoWidth()
                   .VAlign(VAlign_Center)
                   [
                       SNew(STextBlock)
                       .Text(FText::FromString(Breadcrumb))
                       .Font_Static(&ItalicFont)
                       .ColorAndOpacity(CkStyle::TextMute())
                   ];
            }

            return Row;
        }

        if (InColumnName == TEXT("Incl"))
        {
            return DoMakeCell(
                FCk_TimerCategorizer::FormatMs(_Node->InclusiveMs),
                &BodyBoldFont,
                _Node->bIsAggregate ? CkStyle::TextDim() : SeverityColor(_Node->InclusiveMs));
        }

        if (InColumnName == TEXT("Self"))
        {
            const bool ShowSelf = _Node->ExclusiveMs > 0.05;
            return DoMakeCell(
                ShowSelf ? FCk_TimerCategorizer::FormatMs(_Node->ExclusiveMs) : FString(),
                &BodyFont, CkStyle::TextDim());
        }

        if (InColumnName == TEXT("Count"))
        {
            return DoMakeCell(
                (_Node->Count > 1) ? FCk_TimerCategorizer::FormatCount(_Node->Count) : FString(),
                &BodyFont, CkStyle::TextMute());
        }

        if (InColumnName == TEXT("Pct"))
        {
            const double Pct = (_FrameDurationMs > 0.0)
                ? (_Node->InclusiveMs / _FrameDurationMs) * 100.0
                : 0.0;

            return SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceS, 0.0f)
                [
                    MakeProportionBar(Pct, CkStyle::OverlayOf(SeverityColor(_Node->InclusiveMs), 0.85f), 60.0f)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("%.0f%%"), Pct)))
                    .Font_Static(&SmallFont)
                    .ColorAndOpacity(CkStyle::TextDim())
                ];
        }

        return SNullWidget::NullWidget;
    }

private:
    auto DoBuildBreadcrumbSuffix() const -> FString
    {
        constexpr auto MaxShownWrappers = 2;

        FString Breadcrumb;
        const int32 Start = FMath::Max(0, _Node->Breadcrumbs.Num() - MaxShownWrappers);
        for (int32 i = Start; i < _Node->Breadcrumbs.Num(); ++i)
        {
            const FString Simplified = FCk_TimerCategorizer::SimplifyName(_Node->Breadcrumbs[i]);
            if (Simplified.Len() > 30)
            {
                continue;
            }
            if (NOT Breadcrumb.IsEmpty())
            {
                Breadcrumb += TEXT(" → ");
            }
            Breadcrumb += Simplified;
        }
        return Breadcrumb.IsEmpty() ? Breadcrumb : FString::Printf(TEXT("  (%s)"), *Breadcrumb);
    }

    // Takes the font GETTER, not a font: bound through .Font_Static the cell keeps following
    // TextScale after the row widget is built.
    using FFontGetter = FSlateFontInfo (*)();

    auto DoMakeCell(const FString& InText, FFontGetter InFont, const FLinearColor& InColor) const -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .Padding(FMargin(ck_insights_analyzer_tab::SectionSpacing * 0.5f, 0.0f))
            .VAlign(VAlign_Center)
            .HAlign(HAlign_Right)
            [
                SNew(STextBlock)
                .Text(FText::FromString(InText))
                .Font_Static(InFont)
                .ColorAndOpacity(InColor)
            ];
    }

private:
    TSharedPtr<FCk_HotPathNode> _Node;
    double _FrameDurationMs = 0.0;
};

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInsightsAnalyzerTab::
    Construct(const FArguments& InArgs)
    -> void
{
    using namespace ck_insights_analyzer_tab;

    _DepthOptions.Add(MakeShared<FString>(TEXT("Full")));
    _DepthOptions.Add(MakeShared<FString>(TEXT("Standard")));
    _DepthOptions.Add(MakeShared<FString>(TEXT("Concise")));
    _DepthOptions.Add(MakeShared<FString>(TEXT("Hot Paths Only")));

    const auto Content = SNew(SBorder)
        .BorderImage(CkStyle::GetFilledBrush())
        .BorderBackgroundColor(CkStyle::BgRoot())
        .Padding(PanelPadding)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, SectionSpacing)
            [
                DoCreateSummaryStrip()
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, SectionSpacing)
            [
                SNew(SBox)
                .HeightOverride(ChartHeight)
                [
                    SAssignNew(_FrameBarChart, SCkFrameBarChart)
                    .TargetFrameMs(TargetFrameMs)
                    .OnFrameSelectionChanged(
                        FOnFrameSelectionChanged::CreateSP(this, &SCkInsightsAnalyzerTab::DoOnFrameSelectionChanged))
                    .OnScreenshotMarkerClicked(
                        FOnScreenshotMarkerClicked::CreateSP(this, &SCkInsightsAnalyzerTab::DoOnScreenshotMarkerClicked))
                ]
            ]

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(0.0f, 0.0f, 0.0f, SectionSpacing)
            [
                DoCreateResultsArea()
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                DoCreateRawReportArea()
            ]
        ];

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome)
        .WindowId(TEXT("CkInsightsAnalyzer"))
        .ToolTabId(TEXT("CkInsightsAnalyzerTab"))
        .CommandGroups({
            FCkDebug_CommandGroup::Primary(
                TEXT("Capture"),
                FText::FromString(TEXT("Timed trace capture")),
                DoCreateCaptureControls()),
            FCkDebug_CommandGroup::Primary(
                TEXT("Profiling"),
                FText::FromString(TEXT("Profiling event and stat controls")),
                DoCreateProfilingActions()),
            FCkDebug_CommandGroup::Primary(
                TEXT("Limits"),
                FText::FromString(TEXT("Profiling row limits")),
                DoCreateLimitsControls()),
            FCkDebug_CommandGroup::Context(
                TEXT("TraceSource"),
                FText::FromString(TEXT("Trace file selection")),
                DoCreateTraceSourceControls()),
            FCkDebug_CommandGroup::Context(
                TEXT("Analysis"),
                FText::FromString(TEXT("Trace analysis and export commands")),
                DoCreateAnalysisControls()),
        })
        .Content()
        [
            Content
        ]
        .StatusContent()
        [
            DoCreateStatus()
        ]
    ];

    _CaptureUiTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateSP(this, &SCkInsightsAnalyzerTab::DoOnCaptureUiTick),
        0.1f);
}

SCkInsightsAnalyzerTab::~SCkInsightsAnalyzerTab()
{
    DoCancelCaptureUiTick();
    DoCancelAutoOpenTrace();
    DoCancelLoading();
    DoClearScreenshots();
}

// --------------------------------------------------------------------------------------------------------------------
// UI Construction
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInsightsAnalyzerTab::
    DoCreateProfilingActions()
    -> TSharedRef<SWidget>
{
    auto* Capture = &FCkInsightsDebuggerModule::Get().Get_CaptureController();
    auto Actions = TArray<FCkDebug_IconToggleAction>{};

    Actions.Add(FCkDebug_IconToggleAction{
        TEXT("NamedEvents"),
        ECk_Icon::Microphone,
        FText::FromString(TEXT("Named events")),
        FText::FromString(TEXT("Globally enable or disable stat-backed named CPU events while profiling.")),
        TAttribute<bool>::CreateLambda([Capture]() { return Capture->Get_NamedEventsEnabled(); }),
        FOnCkDebug_IconToggleChanged::CreateLambda([this, Capture](bool InEnabled)
        {
            auto Error = FString{};
            if (NOT Capture->TrySet_NamedEventsEnabled(InEnabled, Error))
            {
                DoSetStatus(Error, ECk_Tone::Err);
                return;
            }

            DoSetStatus(
                InEnabled ? TEXT("Named events enabled.") : TEXT("Named events disabled."),
                ECk_Tone::Ok);
        })});

    const auto AddStatProfile = [this, &Actions, Capture](
        ECkInsightsStatProfile InProfile,
        FName InId,
        ECk_Icon InIcon,
        const TCHAR* InLabel,
        const TCHAR* InToolTip)
    {
        Actions.Add(FCkDebug_IconToggleAction{
            InId,
            InIcon,
            FText::FromString(InLabel),
            FText::FromString(InToolTip),
            TAttribute<bool>::CreateLambda([Capture, InProfile]()
            {
                return Capture->Get_StatProfileEnabled(InProfile);
            }),
            FOnCkDebug_IconToggleChanged::CreateLambda(
                [this, Capture, InProfile, Label = FString{InLabel}](bool InEnabled)
            {
                auto Error = FString{};
                if (NOT Capture->TrySet_StatProfileEnabled(InProfile, InEnabled, Error))
                {
                    DoSetStatus(Error, ECk_Tone::Err);
                    return;
                }

                DoSetStatus(
                    FString::Printf(
                        TEXT("%s %s for the active viewport and trace collection."),
                        *Label,
                        InEnabled ? TEXT("enabled") : TEXT("disabled")),
                    ECk_Tone::Ok);
            })});
    };

    AddStatProfile(
        ECkInsightsStatProfile::CkProcessors,
        TEXT("CkProcessorStats"),
        ECk_Icon::Hardware,
        TEXT("CK processor stats"),
        TEXT("Run stat CkProcessors and stat CkProcessors_Details for the active viewport."));
    AddStatProfile(
        ECkInsightsStatProfile::CkScheduler,
        TEXT("CkSchedulerStats"),
        ECk_Icon::Timer,
        TEXT("CK scheduler stats"),
        TEXT("Run stat CkScheduler for the active viewport."));
    AddStatProfile(
        ECkInsightsStatProfile::Script,
        TEXT("ScriptStats"),
        ECk_Icon::Log,
        TEXT("Script stats"),
        TEXT("Run stat CkScript for the active viewport."));
    AddStatProfile(
        ECkInsightsStatProfile::UObjects,
        TEXT("UObjectStats"),
        ECk_Icon::Payload,
        TEXT("UObject stats"),
        TEXT("Run stat UObjects for the active viewport."));
    AddStatProfile(
        ECkInsightsStatProfile::Rhi,
        TEXT("RhiStats"),
        ECk_Icon::Settings,
        TEXT("RHI stats"),
        TEXT("Run stat RHI for the active viewport."));
    AddStatProfile(
        ECkInsightsStatProfile::Rendering,
        TEXT("RenderingStats"),
        ECk_Icon::Display,
        TEXT("Rendering stats"),
        TEXT("Run stat SceneRendering and the render-thread stat groups for the active viewport."));

    Actions.Add(FCkDebug_IconToggleAction{
        TEXT("ShowAllChildren"),
        ECk_Icon::Foliage,
        FText::FromString(TEXT("Show all")),
        FText::FromString(TEXT(
            "Show every child in the hot-path tree instead of folding small ones into "
            "'(+N below threshold)' rows. Tree depth still follows the Depth preset.")),
        TAttribute<bool>::CreateLambda([this]() { return DoGet_ShowAllChildren(); }),
        FOnCkDebug_IconToggleChanged::CreateSP(this, &SCkInsightsAnalyzerTab::DoOnShowAllChildrenChanged)});

    return SNew(SCkDebug_IconToolbar)
        .Actions(MoveTemp(Actions));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInsightsAnalyzerTab::
    DoCreateCaptureControls()
    -> TSharedRef<SWidget>
{
    auto* Capture = &FCkInsightsDebuggerModule::Get().Get_CaptureController();

    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(SBox)
            .WidthOverride(210.0f)
            .HeightOverride(26.0f)
            [
                SNew(SButton)
                .ContentPadding(0.0f)
                .ToolTipText(FText::FromString(TEXT(
                    "Start a timed file trace, or stop it early. Choose 0-12 automated screenshots; 0 disables them, "
                    "and the default 3 are embedded at 10%, 50%, and 90%. "
                    "The completed trace opens automatically and produces same-name Markdown and JSON reports.")))
                .IsEnabled_Lambda([this, Capture]()
                {
                    return NOT _AutoOpenTickerHandle.IsValid()
                        && NOT DoIsLoading()
                        && Capture->Can_ToggleTracing();
                })
                .OnClicked(this, &SCkInsightsAnalyzerTab::DoOnCaptureClicked)
                [
                    SNew(SOverlay)
                    + SOverlay::Slot()
                    [
                        SNew(SProgressBar)
                        .Percent_Lambda([Capture]() -> TOptional<float>
                        {
                            const auto Snapshot = Capture->Get_Snapshot();
                            return Snapshot.State == ECkInsightsCaptureState::Idle
                                ? TOptional<float>{0.0f}
                                : TOptional<float>{static_cast<float>(Snapshot.Progress)};
                        })
                        .FillColorAndOpacity(CkStyle::Accent())
                    ]
                    + SOverlay::Slot()
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Font(ck_insights_analyzer_tab::SmallFont())
                        .Text_Lambda([this, Capture]()
                        {
                            const auto Snapshot = Capture->Get_Snapshot();
                            switch (Snapshot.State)
                            {
                            case ECkInsightsCaptureState::Starting:
                                return FText::FromString(TEXT("Starting trace..."));
                            case ECkInsightsCaptureState::Recording:
                                return FText::FromString(FString::Printf(
                                    TEXT("Stop trace — %.1fs remaining"), Snapshot.RemainingSeconds));
                            case ECkInsightsCaptureState::Stopping:
                                return FText::FromString(TEXT("Finalizing trace..."));
                            default:
                                return Snapshot.bIsTracing
                                    ? FText::FromString(TEXT("External trace active"))
                                    : FText::FromString(FString::Printf(
                                        TEXT("Capture %ds trace"), _CaptureDurationSeconds));
                            }
                        })
                    ]
                ]
            ]
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(CkStyle::SpaceXS, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SBox)
                .WidthOverride(64.0f)
                [
                    SNew(SSpinBox<int32>)
                    .MinValue(1)
                    .MinSliderValue(1)
                    .MaxValue(3600)
                    .MaxSliderValue(300)
                    .Delta(5)
                    .Value_Lambda([this]() { return _CaptureDurationSeconds; })
                    .IsEnabled_Lambda([this, Capture]()
                    {
                        const auto Snapshot = Capture->Get_Snapshot();
                        return Snapshot.State == ECkInsightsCaptureState::Idle
                            && NOT Snapshot.bIsTracing
                            && NOT _AutoOpenTickerHandle.IsValid()
                            && NOT DoIsLoading();
                    })
                    .OnValueCommitted_Lambda([this](int32 InValue, ETextCommit::Type)
                    {
                        _CaptureDurationSeconds = FMath::Clamp(InValue, 1, 3600);
                    })
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceXS, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("s")))
                .Font(ck_insights_analyzer_tab::SmallFont())
                .ColorAndOpacity(CkStyle::TextDim())
            ]
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SBox)
                .WidthOverride(52.0f)
                [
                    SNew(SSpinBox<int32>)
                    .MinValue(FCkInsightsCaptureController::MinTimedCaptureScreenshotCount)
                    .MinSliderValue(FCkInsightsCaptureController::MinTimedCaptureScreenshotCount)
                    .MaxValue(FCkInsightsCaptureController::MaxTimedCaptureScreenshotCount)
                    .MaxSliderValue(FCkInsightsCaptureController::MaxTimedCaptureScreenshotCount)
                    .Delta(1)
                    .ToolTipText(FText::FromString(TEXT(
                        "Automated screenshots per timed capture (0-12). 0 disables them; 1 uses 50%; 2 use 10% "
                        "and 90%; 3-12 are evenly spaced from 10% through 90%.")))
                    .Value_Lambda([this]() { return _CaptureScreenshotCount; })
                    .IsEnabled_Lambda([this, Capture]()
                    {
                        const auto Snapshot = Capture->Get_Snapshot();
                        return Snapshot.State == ECkInsightsCaptureState::Idle
                            && NOT Snapshot.bIsTracing
                            && NOT _AutoOpenTickerHandle.IsValid()
                            && NOT DoIsLoading();
                    })
                    .OnValueChanged_Lambda([this](int32 InValue)
                    {
                        _CaptureScreenshotCount = FMath::Clamp(
                            InValue,
                            FCkInsightsCaptureController::MinTimedCaptureScreenshotCount,
                            FCkInsightsCaptureController::MaxTimedCaptureScreenshotCount);
                    })
                    .OnValueCommitted_Lambda([this](int32 InValue, ETextCommit::Type)
                    {
                        _CaptureScreenshotCount = FMath::Clamp(
                            InValue,
                            FCkInsightsCaptureController::MinTimedCaptureScreenshotCount,
                            FCkInsightsCaptureController::MaxTimedCaptureScreenshotCount);
                    })
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceXS, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("shots")))
                .Font(ck_insights_analyzer_tab::SmallFont())
                .ColorAndOpacity(CkStyle::TextDim())
            ]
        ];
}

auto
    SCkInsightsAnalyzerTab::
    DoCreateLimitsControls()
    -> TSharedRef<SWidget>
{
    auto* Capture = &FCkInsightsDebuggerModule::Get().Get_CaptureController();

    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Max/group")))
            .Font_Static(&ck_insights_analyzer_tab::SmallFont)
            .ColorAndOpacity(CkStyle::TextDim())
            .ToolTipText(FText::FromString(TEXT(
                "Maximum stat rows drawn for each enabled stat group (stats.MaxPerGroup).")))
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        [
            SNew(SBox)
            .WidthOverride(64.0f)
            [
                SNew(SSpinBox<int32>)
                .MinValue(1)
                .MinSliderValue(1)
                .MaxSliderValue(250)
                .Delta(5)
                .Value_Lambda([Capture]() { return Capture->Get_StatMaxPerGroup().Get(25); })
                .IsEnabled_Lambda([Capture]() { return Capture->Get_StatMaxPerGroup().IsSet(); })
                .OnValueCommitted_Lambda([this, Capture](int32 InValue, ETextCommit::Type)
                {
                    auto Error = FString{};
                    if (NOT Capture->TrySet_StatMaxPerGroup(InValue, Error))
                    {
                        DoSetStatus(Error, ECk_Tone::Err);
                        return;
                    }

                    DoSetStatus(
                        FString::Printf(TEXT("stats.MaxPerGroup set to %d."), InValue),
                        ECk_Tone::Ok);
                })
            ]
        ];
}

auto
    SCkInsightsAnalyzerTab::
    DoCreateTraceSourceControls()
    -> TSharedRef<SWidget>
{
    using namespace ck_insights_analyzer_tab;

    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(0.0f, 0.0f, SectionSpacing, 0.0f)
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("Open .utrace...")))
            .ToolTipText(FText::FromString(TEXT("Open an Unreal Insights trace file for analysis")))
            .OnClicked(this, &SCkInsightsAnalyzerTab::DoOnOpenTraceClicked)
            .HAlign(HAlign_Center)
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(0.0f, 0.0f, SectionSpacing, 0.0f)
        [
            SNew(SComboButton)
            .ToolTipText(FText::FromString(TEXT(
                "Open a recent trace from Saved/Profiling or the local Unreal Trace Server store")))
            .OnGetMenuContent(this, &SCkInsightsAnalyzerTab::DoMakeRecentTracesMenu)
            .ButtonContent()
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Recent")))
                .Font_Static(&BodyFont)
            ]
        ]

        ;
}

auto
    SCkInsightsAnalyzerTab::
    DoCreateAnalysisControls()
    -> TSharedRef<SWidget>
{
    using namespace ck_insights_analyzer_tab;

    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, SectionSpacing, 0.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Depth:")))
                .Font_Static(&BodyFont)
                .ColorAndOpacity(CkStyle::TextDim())
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SBox)
                .WidthOverride(120.0f)
                [
                    SAssignNew(_DepthCombo, STextComboBox)
                    .OptionsSource(&_DepthOptions)
                    .InitiallySelectedItem(_DepthOptions[1])
                    .OnSelectionChanged(this, &SCkInsightsAnalyzerTab::DoOnDepthSelectionChanged)
                ]
            ]
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(0.0f, 0.0f, SectionSpacing, 0.0f)
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("Analyze Worst 10")))
            .ToolTipText(FText::FromString(TEXT("Find and analyze the 10 worst frames in the trace")))
            .OnClicked(this, &SCkInsightsAnalyzerTab::DoOnAnalyzeWorstClicked)
            .HAlign(HAlign_Center)
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(0.0f, 0.0f, SectionSpacing, 0.0f)
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("Copy Report")))
            .ToolTipText(FText::FromString(TEXT("Copy the current markdown report to the system clipboard")))
            .OnClicked(this, &SCkInsightsAnalyzerTab::DoOnCopyToClipboardClicked)
            .HAlign(HAlign_Center)
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("Export JSON...")))
            .ToolTipText(FText::FromString(TEXT(
                "Save the current analysis as a machine-readable JSON report (same format as the commandlet's -json output)")))
            .OnClicked(this, &SCkInsightsAnalyzerTab::DoOnExportJsonClicked)
            .HAlign(HAlign_Center)
        ];
}

auto
    SCkInsightsAnalyzerTab::
    DoCreateStatus()
    -> TSharedRef<SWidget>
{
    using namespace ck_insights_analyzer_tab;

    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            SNew(SBox)
            .WidthOverride_Lambda([]() -> FOptionalSize
            { return FOptionalSize{ck::debug_axes::Apply_IconSize(8.0f)}; })
            .HeightOverride_Lambda([]() -> FOptionalSize
            { return FOptionalSize{ck::debug_axes::Apply_IconSize(8.0f)}; })
            [
                SNew(SImage)
                .Image(CkStyle::GetFilledBrush())
                .ColorAndOpacity_Lambda([this]() -> FSlateColor
                {
                    return CkStyle::GetToneColor(_StatusTone);
                })
            ]
        ]
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .VAlign(VAlign_Center)
        [
            SAssignNew(_StatusText, STextBlock)
            .Text(FText::FromString(TEXT("No trace loaded. Click \"Open .utrace...\" to begin.")))
            .Font_Static(&BodyFont)
            .ColorAndOpacity(CkStyle::TextDim())
            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
        ];
}

auto
    SCkInsightsAnalyzerTab::
    DoCreateSummaryStrip()
    -> TSharedRef<SWidget>
{
    SAssignNew(_SummaryBox, SHorizontalBox);

    return SNew(SBox)
        .Visibility_Lambda([this]() -> EVisibility
        {
            return (ck::IsValid(_SummaryBox) && _SummaryBox->NumSlots() > 0)
                ? EVisibility::Visible
                : EVisibility::Collapsed;
        })
        [
            _SummaryBox.ToSharedRef()
        ];
}

auto
    SCkInsightsAnalyzerTab::
    DoCreateResultsArea()
    -> TSharedRef<SWidget>
{
    return SNew(SSplitter)
        .Orientation(Orient_Horizontal)
        .PhysicalSplitterHandleSize(2.0f)

        + SSplitter::Slot()
        .Value(0.62f)
        [
            DoCreateHotPathPanel()
        ]

        + SSplitter::Slot()
        .Value(0.38f)
        [
            DoCreateSidePanels()
        ];
}

auto
    SCkInsightsAnalyzerTab::
    DoCreateHotPathPanel()
    -> TSharedRef<SWidget>
{
    using namespace ck_insights_analyzer_tab;

    SAssignNew(_HotPathTree, STreeView<TSharedPtr<FCk_HotPathNode>>)
        .TreeItemsSource(&_HotPathRoots)
        .SelectionMode(ESelectionMode::Single)
        .OnGenerateRow(this, &SCkInsightsAnalyzerTab::DoGenerateHotPathRow)
        .OnGetChildren_Lambda([](TSharedPtr<FCk_HotPathNode> InNode, TArray<TSharedPtr<FCk_HotPathNode>>& OutChildren)
        {
            if (InNode.IsValid())
            {
                OutChildren = InNode->Children;
            }
        })
        .HeaderRow
        (
            SNew(SHeaderRow)
            + SHeaderRow::Column(TEXT("Timer"))
              .DefaultLabel(FText::FromString(TEXT("Timer")))
              .FillWidth(1.0f)
            + SHeaderRow::Column(TEXT("Incl"))
              .DefaultLabel(FText::FromString(TEXT("Incl")))
              .FixedWidth(76.0f)
              .HAlignHeader(HAlign_Right)
            + SHeaderRow::Column(TEXT("Self"))
              .DefaultLabel(FText::FromString(TEXT("Self")))
              .FixedWidth(76.0f)
              .HAlignHeader(HAlign_Right)
            + SHeaderRow::Column(TEXT("Count"))
              .DefaultLabel(FText::FromString(TEXT("Count")))
              .FixedWidth(64.0f)
              .HAlignHeader(HAlign_Right)
            + SHeaderRow::Column(TEXT("Pct"))
              .DefaultLabel(FText::FromString(TEXT("% Frame")))
              .FixedWidth(120.0f)
        );

    return SNew(SBorder)
        .BorderImage(CkStyle::GetFilledBrush())
        .BorderBackgroundColor(CkStyle::Bg1())
        .Padding(CkStyle::SpaceM)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                MakeHeading(TEXT("Game Thread Hot Paths"))
            ]
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                [
                    _HotPathTree.ToSharedRef()
                ]
                + SOverlay::Slot()
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Font_Static(&BodyFont)
                    .ColorAndOpacity(CkStyle::TextMute())
                    .Visibility_Lambda([this]() -> EVisibility
                    {
                        return (_HotPathRoots.Num() == 0)
                            ? EVisibility::HitTestInvisible
                            : EVisibility::Collapsed;
                    })
                    .Text_Lambda([this]() -> FText
                    {
                        switch (_ResultsMode)
                        {
                            case EResultsMode::MultiFrame:
                                return FText::FromString(TEXT("Range analyzed — click a Worst Frame row or a single chart bar to see its hot paths."));
                            case EResultsMode::SingleFrame:
                                return FText::FromString(TEXT("No hot paths found for this frame."));
                            default:
                                return FText::FromString(TEXT("Open a .utrace, then click a frame bar (or drag a range) in the chart."));
                        }
                    })
                ]
            ]
        ];
}

auto
    SCkInsightsAnalyzerTab::
    DoCreateSidePanels()
    -> TSharedRef<SWidget>
{
    using namespace ck_insights_analyzer_tab;

    SAssignNew(_CategoryRowsBox, SVerticalBox);
    SAssignNew(_TopTimerRowsBox, SVerticalBox);
    SAssignNew(_WorstFrameRowsBox, SVerticalBox);
    SAssignNew(_CategoryAvgRowsBox, SVerticalBox);
    SAssignNew(_WaitRowsBox, SVerticalBox);

    const auto MakeGatedPanel =
        [this](const FString& InHeading, const TSharedRef<SWidget>& InRowsBox, TFunction<bool()> InHasData) -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .Visibility_Lambda([InHasData = MoveTemp(InHasData)]() -> EVisibility
            {
                return InHasData() ? EVisibility::Visible : EVisibility::Collapsed;
            })
            .Padding(FMargin(SectionSpacing, 0.0f, 0.0f, SectionSpacing))
            [
                MakePanel(InHeading, InRowsBox)
            ];
    };

    return SNew(SScrollBox)
        .ConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible)

        + SScrollBox::Slot()
        [
            DoCreateScreenshotPanel()
        ]

        + SScrollBox::Slot()
        [
            MakeGatedPanel(TEXT("Worst Frames"), _WorstFrameRowsBox.ToSharedRef(),
                [this]() { return _WorstFrames.Num() > 0; })
        ]

        + SScrollBox::Slot()
        [
            MakeGatedPanel(TEXT("Category Averages"), _CategoryAvgRowsBox.ToSharedRef(),
                [this]() { return _CategoryAverages.Num() > 0; })
        ]

        + SScrollBox::Slot()
        [
            MakeGatedPanel(TEXT("Categories (exclusive time)"), _CategoryRowsBox.ToSharedRef(),
                [this]() { return _Categories.Num() > 0; })
        ]

        + SScrollBox::Slot()
        [
            MakeGatedPanel(TEXT("Waits/Stalls (per thread)"), _WaitRowsBox.ToSharedRef(),
                [this]() { return _WaitRows.Num() > 0; })
        ]

        + SScrollBox::Slot()
        [
            MakeGatedPanel(TEXT("Top Timers (exclusive time)"), _TopTimerRowsBox.ToSharedRef(),
                [this]() { return _TopTimers.Num() > 0; })
        ];
}

auto
    SCkInsightsAnalyzerTab::
    DoCreateScreenshotPanel()
    -> TSharedRef<SWidget>
{
    using namespace ck_insights_analyzer_tab;

    return SNew(SBox)
        .Visibility_Lambda([this]() -> EVisibility
        {
            return _SelectedScreenshotId.IsSet() ? EVisibility::Visible : EVisibility::Collapsed;
        })
        .Padding(FMargin(SectionSpacing, 0.0f, 0.0f, SectionSpacing))
        [
            MakePanel(
                TEXT("Captured Screenshot"),
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SCkDebug_SelectableLabel)
                    .Text_Lambda([this]() -> FText
                    {
                        const auto Screenshot = _SelectedScreenshotId.IsSet()
                            ? DoFindScreenshot(_SelectedScreenshotId.GetValue())
                            : nullptr;
                        return FText::FromString(Screenshot != nullptr ? Screenshot->Name : FString{});
                    })
                    .Font(BodyBoldFont())
                    .ColorAndOpacity(CkStyle::Accent())
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, CkStyle::SpaceXS, 0.0f, CkStyle::SpaceS)
                [
                    SNew(SCkDebug_SelectableLabel)
                    .Text_Lambda([this]() -> FText
                    {
                        const auto Screenshot = _SelectedScreenshotId.IsSet()
                            ? DoFindScreenshot(_SelectedScreenshotId.GetValue())
                            : nullptr;
                        if (Screenshot == nullptr)
                        {
                            return FText::GetEmpty();
                        }

                        const auto GameFrame = Screenshot->GameFrameIndex != INDEX_NONE
                            ? FString::Printf(
                                TEXT("%sgame frame #%lld"),
                                Screenshot->bIsInsideGameFrame ? TEXT("") : TEXT("nearest "),
                                Screenshot->GameFrameIndex)
                            : FString{TEXT("no game-frame mapping")};
                        const auto RenderFrame = Screenshot->RenderFrameIndex != INDEX_NONE
                            ? FString::Printf(
                                TEXT("%srender frame #%lld"),
                                Screenshot->bIsInsideRenderFrame ? TEXT("") : TEXT("nearest "),
                                Screenshot->RenderFrameIndex)
                            : FString{TEXT("no render-frame mapping")};

                        return FText::FromString(FString::Printf(
                            TEXT("%.6fs  |  %s  |  %s  |  %ux%u"),
                            Screenshot->TimestampSeconds,
                            *GameFrame,
                            *RenderFrame,
                            Screenshot->Width,
                            Screenshot->Height));
                    })
                    .Font(SmallFont())
                    .ColorAndOpacity(CkStyle::TextDim())
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SBox)
                    .HeightOverride(300.0f)
                    .Visibility_Lambda([this]() -> EVisibility
                    {
                        return ck::IsValid(_SelectedScreenshotBrush)
                            ? EVisibility::Visible
                            : EVisibility::Collapsed;
                    })
                    [
                        SNew(SBorder)
                        .BorderImage(CkStyle::GetFilledBrush())
                        .BorderBackgroundColor(CkStyle::BgRoot())
                        .Padding(CkStyle::SpaceXS)
                        [
                            SNew(SScaleBox)
                            .Stretch(EStretch::ScaleToFit)
                            .StretchDirection(EStretchDirection::Both)
                            [
                                SNew(SImage)
                                .Image_Lambda([this]() -> const FSlateBrush*
                                {
                                    return ck::IsValid(_SelectedScreenshotBrush)
                                        ? _SelectedScreenshotBrush.Get()
                                        : nullptr;
                                })
                            ]
                        ]
                    ]
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(STextBlock)
                    .Visibility_Lambda([this]() -> EVisibility
                    {
                        return _SelectedScreenshotError.IsEmpty()
                            ? EVisibility::Collapsed
                            : EVisibility::Visible;
                    })
                    .Text_Lambda([this]() { return FText::FromString(_SelectedScreenshotError); })
                    .Font(SmallFont())
                    .ColorAndOpacity(CkStyle::Warn())
                    .AutoWrapText(true)
                ])
        ];
}

auto
    SCkInsightsAnalyzerTab::
    DoCreateRawReportArea()
    -> TSharedRef<SWidget>
{
    using namespace ck_insights_analyzer_tab;

    return SNew(SBox)
        .Visibility_Lambda([this]() -> EVisibility
        {
            return _CurrentReport.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
        })
        [
            SNew(SExpandableArea)
            .InitiallyCollapsed(true)
            .HeaderContent()
            [
                MakeHeading(TEXT("Raw Markdown Report"))
            ]
            .BodyContent()
            [
                SNew(SBox)
                .MaxDesiredHeight(260.0f)
                [
                    SNew(SBorder)
                    .BorderImage(CkStyle::GetFilledBrush())
                    .BorderBackgroundColor(CkStyle::Bg1())
                    .Padding(CkStyle::SpaceS)
                    [
                        SNew(SScrollBox)
                        .ConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible)
                        + SScrollBox::Slot()
                        [
                            SAssignNew(_ReportText, SMultiLineEditableText)
                            .IsReadOnly(true)
                            .AutoWrapText(false)
                            .Text(FText::GetEmpty())
                        ]
                    ]
                ]
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------
// Hot-path tree
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInsightsAnalyzerTab::
    DoGenerateHotPathRow(TSharedPtr<FCk_HotPathNode> InNode,
                         const TSharedRef<STableViewBase>& InOwnerTable)
    -> TSharedRef<ITableRow>
{
    return SNew(SCkInsights_HotPathRow, InOwnerTable)
        .Node(InNode)
        .FrameDurationMs(_AnalyzedFrameMs);
}

auto
    SCkInsightsAnalyzerTab::
    DoExpandHotPathDefaults()
    -> void
{
    if (ck::Is_NOT_Valid(_HotPathTree))
    {
        return;
    }

    for (const auto& Root : _HotPathRoots)
    {
        _HotPathTree->SetItemExpansion(Root, true);
        for (const auto& Child : Root->Children)
        {
            _HotPathTree->SetItemExpansion(Child, true);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Side panel row rebuilds
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInsightsAnalyzerTab::
    DoRebuildCategoryRows()
    -> void
{
    using namespace ck_insights_analyzer_tab;

    if (ck::Is_NOT_Valid(_CategoryRowsBox))
    {
        return;
    }
    _CategoryRowsBox->ClearChildren();

    for (const FCk_CategorySummaryEntry& Cat : _Categories)
    {
        _CategoryRowsBox->AddSlot()
        .AutoHeight()
        .Padding(TAttribute<FMargin>::CreateStatic(&Get_SidePanelRowPadding))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                MakeDot(CategoryColor(Cat.Name))
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Cat.Name))
                .Font_Static(&BodyFont)
                .ColorAndOpacity(CkStyle::Text())
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceS, 0.0f)
            [
                MakeProportionBar(Cat.PctOfFrame, CkStyle::OverlayOf(CategoryColor(Cat.Name), 0.85f), SideBarWidth)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(60.0f)
                .HAlign(HAlign_Right)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(FCk_TimerCategorizer::FormatMs(Cat.ExclusiveMs)))
                    .Font_Static(&BodyBoldFont)
                    .ColorAndOpacity(SeverityColor(Cat.ExclusiveMs))
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(44.0f)
                .HAlign(HAlign_Right)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("%.0f%%"), Cat.PctOfFrame)))
                    .Font_Static(&SmallFont)
                    .ColorAndOpacity(CkStyle::TextMute())
                ]
            ]
        ];
    }
}

auto
    SCkInsightsAnalyzerTab::
    DoRebuildWaitRows()
    -> void
{
    using namespace ck_insights_analyzer_tab;

    if (ck::Is_NOT_Valid(_WaitRowsBox))
    {
        return;
    }
    _WaitRowsBox->ClearChildren();

    for (const FCk_WaitThreadSummary& Wait : _WaitRows)
    {
        const double PctOfWall = Wait.WallMs > 0.0 ? (Wait.WaitMs / Wait.WallMs) * 100.0 : 0.0;

        _WaitRowsBox->AddSlot()
        .AutoHeight()
        .Padding(TAttribute<FMargin>::CreateStatic(&Get_SidePanelRowPadding))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Wait.ThreadName))
                .Font_Lambda([IsGameThread = Wait.bIsGameThread]()
                { return IsGameThread ? BodyBoldFont() : BodyFont(); })
                .ColorAndOpacity(CkStyle::Text())
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceS, 0.0f)
            [
                MakeProportionBar(PctOfWall, CkStyle::OverlayOf(SeverityColor(Wait.WaitMs), 0.85f), SideBarWidth)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(60.0f)
                .HAlign(HAlign_Right)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(FCk_TimerCategorizer::FormatMs(Wait.WaitMs)))
                    .Font_Static(&BodyBoldFont)
                    .ColorAndOpacity(SeverityColor(Wait.WaitMs))
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(44.0f)
                .HAlign(HAlign_Right)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("%.0f%%"), PctOfWall)))
                    .Font_Static(&SmallFont)
                    .ColorAndOpacity(CkStyle::TextMute())
                ]
            ]
        ];

        for (const FCk_WaitThreadSummary::FWaitScope& Top : Wait.TopWaits)
        {
            _WaitRowsBox->AddSlot()
            .AutoHeight()
            .Padding(TAttribute<FMargin>::CreateStatic(&Get_SidePanelSubRowPadding))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Top.Name))
                    .Font_Static(&SmallFont)
                    .ColorAndOpacity(CkStyle::TextDim())
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("%s  %s"),
                        *FCk_TimerCategorizer::FormatMs(Top.ExclusiveMs),
                        *FCk_TimerCategorizer::FormatCount(Top.Count))))
                    .Font_Static(&SmallFont)
                    .ColorAndOpacity(CkStyle::TextMute())
                ]
            ];
        }
    }
}

auto
    SCkInsightsAnalyzerTab::
    DoRebuildTopTimerRows()
    -> void
{
    using namespace ck_insights_analyzer_tab;

    if (ck::Is_NOT_Valid(_TopTimerRowsBox))
    {
        return;
    }
    _TopTimerRowsBox->ClearChildren();

    int32 Rank = 0;
    for (const FCk_TopTimerEntry& Timer : _TopTimers)
    {
        ++Rank;

        _TopTimerRowsBox->AddSlot()
        .AutoHeight()
        .Padding(TAttribute<FMargin>::CreateStatic(&Get_SidePanelRowPadding))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SBox)
                .WidthOverride(22.0f)
                .HAlign(HAlign_Right)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("%d."), Rank)))
                    .Font_Static(&SmallFont)
                    .ColorAndOpacity(CkStyle::TextMute())
                ]
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Timer.Name))
                .Font_Static(&BodyFont)
                .ColorAndOpacity(CkStyle::Text())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                .ToolTipText(FText::FromString(FString::Printf(
                    TEXT("%s\nExclusive: %s   Inclusive: %s   Count: %s"),
                    *Timer.Name,
                    *FCk_TimerCategorizer::FormatMs(Timer.ExclusiveMs),
                    *FCk_TimerCategorizer::FormatMs(Timer.InclusiveMs),
                    *FCk_TimerCategorizer::FormatCount(Timer.Count))))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(60.0f)
                .HAlign(HAlign_Right)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(FCk_TimerCategorizer::FormatMs(Timer.ExclusiveMs)))
                    .Font_Static(&BodyBoldFont)
                    .ColorAndOpacity(SeverityColor(Timer.ExclusiveMs))
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(52.0f)
                .HAlign(HAlign_Right)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString((Timer.Count > 1) ? FCk_TimerCategorizer::FormatCount(Timer.Count) : FString()))
                    .Font_Static(&SmallFont)
                    .ColorAndOpacity(CkStyle::TextMute())
                ]
            ]
        ];
    }
}

auto
    SCkInsightsAnalyzerTab::
    DoRebuildWorstFrameRows()
    -> void
{
    using namespace ck_insights_analyzer_tab;

    if (ck::Is_NOT_Valid(_WorstFrameRowsBox))
    {
        return;
    }
    _WorstFrameRowsBox->ClearChildren();

    for (const FCk_FrameSummary& Frame : _WorstFrames)
    {
        _WorstFrameRowsBox->AddSlot()
        .AutoHeight()
        .Padding(TAttribute<FMargin>::CreateStatic(&Get_SidePanelRowPadding))
        [
            SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .ContentPadding(FMargin(CkStyle::SpaceS, 2.0f))
            .ToolTipText(FText::FromString(TEXT("Analyze this frame (selects it in the chart)")))
            .OnClicked(this, &SCkInsightsAnalyzerTab::DoOnWorstFrameClicked, Frame.FrameIndex)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(76.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(FString::Printf(TEXT("#%s"), *FormatWithCommas(Frame.FrameIndex))))
                        .Font_Static(&BodyBoldFont)
                        .ColorAndOpacity(CkStyle::Accent())
                    ]
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(64.0f)
                    .HAlign(HAlign_Right)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(FCk_TimerCategorizer::FormatMs(Frame.DurationMs)))
                        .Font_Static(&BodyBoldFont)
                        .ColorAndOpacity(FrameBudgetColor(Frame.DurationMs))
                    ]
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Frame.DominantCost))
                    .Font_Static(&ItalicFont)
                    .ColorAndOpacity(CkStyle::TextDim())
                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                ]
            ]
        ];
    }
}

auto
    SCkInsightsAnalyzerTab::
    DoRebuildCategoryAvgRows()
    -> void
{
    using namespace ck_insights_analyzer_tab;

    if (ck::Is_NOT_Valid(_CategoryAvgRowsBox))
    {
        return;
    }
    _CategoryAvgRowsBox->ClearChildren();

    for (const FCk_MultiFrameStats::FCategoryStats& Cat : _CategoryAverages)
    {
        _CategoryAvgRowsBox->AddSlot()
        .AutoHeight()
        .Padding(TAttribute<FMargin>::CreateStatic(&Get_SidePanelRowPadding))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                MakeDot(CategoryColor(Cat.Name))
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Cat.Name))
                .Font_Static(&BodyFont)
                .ColorAndOpacity(CkStyle::Text())
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceS, 0.0f)
            [
                MakeProportionBar(Cat.TotalPct, CkStyle::OverlayOf(CategoryColor(Cat.Name), 0.85f), SideBarWidth)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(64.0f)
                .HAlign(HAlign_Right)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("%s avg"), *FCk_TimerCategorizer::FormatMs(Cat.AvgExclMs))))
                    .Font_Static(&BodyBoldFont)
                    .ColorAndOpacity(SeverityColor(Cat.AvgExclMs))
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(64.0f)
                .HAlign(HAlign_Right)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("%s p95"), *FCk_TimerCategorizer::FormatMs(Cat.P95ExclMs))))
                    .Font_Static(&SmallFont)
                    .ColorAndOpacity(CkStyle::TextMute())
                ]
            ]
        ];
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Button Handlers
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInsightsAnalyzerTab::
    DoOnOpenTraceClicked()
    -> FReply
{
    if (DoIsLoading())
    {
        DoSetStatus(TEXT("Loading in progress..."), ECk_Tone::Warn);
        return FReply::Handled();
    }

    const auto DesktopPlatform = FDesktopPlatformModule::Get();
    if (ck::Is_NOT_Valid(DesktopPlatform, ck::IsValid_Policy_NullptrOnly{}))
    {
        DoSetStatus(TEXT("Desktop platform not available."), ECk_Tone::Err);
        return FReply::Handled();
    }

    TArray<FString> OutFiles;
    const bool Opened = DesktopPlatform->OpenFileDialog(
        FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
        TEXT("Open Unreal Insights Trace"),
        FPaths::ProfilingDir(),
        TEXT(""),
        TEXT("Unreal Trace Files (*.utrace)|*.utrace"),
        EFileDialogFlags::None,
        OutFiles);

    if (NOT Opened || OutFiles.Num() == 0)
    {
        return FReply::Handled();
    }

    DoOpenTracePath(OutFiles[0]);
    return FReply::Handled();
}

auto
    SCkInsightsAnalyzerTab::
    DoOpenTracePath(FString TracePath)
    -> void
{
    if (DoIsLoading())
    {
        DoSetStatus(TEXT("Loading in progress..."), ECk_Tone::Warn);
        return;
    }

    DoClearScreenshots();
    _Session.Close();
    _FrameBarChart->ClearFrameData();
    DoClearResults();

    DoStartAsyncOpen(TracePath);
}

auto
    SCkInsightsAnalyzerTab::
    DoOnCaptureClicked()
    -> FReply
{
    auto& Capture = FCkInsightsDebuggerModule::Get().Get_CaptureController();
    const auto Snapshot = Capture.Get_Snapshot();
    auto Error = FString{};

    if (Snapshot.State == ECkInsightsCaptureState::Idle)
    {
        if (NOT Capture.TryStart_TimedCapture(
                static_cast<double>(_CaptureDurationSeconds),
                _CaptureScreenshotCount,
                Error))
        {
            DoSetStatus(Error, ECk_Tone::Err);
            return FReply::Handled();
        }

        DoSetStatus(
            FString::Printf(
                TEXT("Starting a %ds trace with %d automated screenshot%s..."),
                _CaptureDurationSeconds,
                _CaptureScreenshotCount,
                _CaptureScreenshotCount == 1 ? TEXT("") : TEXT("s")),
            ECk_Tone::Info);
        return FReply::Handled();
    }

    if (Snapshot.State == ECkInsightsCaptureState::Recording)
    {
        if (NOT Capture.TryStop_TimedCapture(Error))
        {
            DoSetStatus(Error, ECk_Tone::Err);
            return FReply::Handled();
        }

        DoSetStatus(TEXT("Stopping trace early. Finalizing the capture..."), ECk_Tone::Info);
    }

    return FReply::Handled();
}

auto
    SCkInsightsAnalyzerTab::
    DoOnCaptureUiTick(float)
    -> bool
{
    if (_AutoOpenTickerHandle.IsValid() || DoIsLoading())
    { return true; }

    auto& Capture = FCkInsightsDebuggerModule::Get().Get_CaptureController();
    auto CompletedTracePath = FString{};
    auto CompletedTraceGuid = FGuid{};
    if (Capture.Get_CompletedCapture(CompletedTracePath, CompletedTraceGuid)
        && CompletedTraceGuid != _SuppressedAutoOpenTraceGuid)
    {
        DoQueueAutoOpenTrace(MoveTemp(CompletedTracePath), CompletedTraceGuid);
    }

    return true;
}

auto SCkInsightsAnalyzerTab::DoCancelCaptureUiTick() -> void
{
    if (_CaptureUiTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(_CaptureUiTickerHandle);
        _CaptureUiTickerHandle.Reset();
    }
}

auto
    SCkInsightsAnalyzerTab::
    DoQueueAutoOpenTrace(FString TracePath, FGuid TraceGuid)
    -> void
{
    const auto PathIsValid = NOT TracePath.IsEmpty();
    CK_ENSURE_IF_NOT(PathIsValid, TEXT("A stopped Insights Analyzer file capture must provide its path"))
    {}
    if (NOT PathIsValid)
    {
        DoSetStatus(TEXT("Trace stopped, but Unreal did not return the capture path."), ECk_Tone::Err);
        return;
    }

    DoCancelAutoOpenTrace();
    if (TraceGuid != _SuppressedAutoOpenTraceGuid)
    {
        _SuppressedAutoOpenTraceGuid.Invalidate();
    }
    _PendingAutoOpenTracePath = MoveTemp(TracePath);
    _PendingAutoOpenTraceGuid = TraceGuid;
    _PendingAutoOpenWriterFinalized = false;
    _AutoOpenDelayWarningShown = false;
    _AutoOpenTraceOpeningStarted = false;
    _AutoOpenReportGenerated = false;
    _AutoOpenDeadlineSeconds = FPlatformTime::Seconds() + 30.0;
    DoSetStatus(TEXT("Trace stopped. Finalizing and opening the capture..."), ECk_Tone::Ok);
    _AutoOpenTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateSP(this, &SCkInsightsAnalyzerTab::DoOnAutoOpenTraceTick),
        0.1f);
}

auto
    SCkInsightsAnalyzerTab::
    DoOnAutoOpenTraceTick(float)
    -> bool
{
    auto& Capture = FCkInsightsDebuggerModule::Get().Get_CaptureController();

    if (_AutoOpenTraceOpeningStarted)
    {
        if (DoIsLoading())
        {
            return true;
        }

        if (NOT _AutoOpenReportGenerated)
        {
            // The report export already surfaced its concrete error. Stop retrying in this tab so
            // capture controls remain usable, but retain the controller record for a fresh tab.
            _SuppressedAutoOpenTraceGuid = _PendingAutoOpenTraceGuid;
            _AutoOpenTickerHandle.Reset();
            DoCancelAutoOpenTrace();
            return false;
        }

        const auto Acknowledged = Capture.Acknowledge_CompletedCapture(_PendingAutoOpenTraceGuid);
        if (NOT Acknowledged)
        {
            // The single retained completion changed while this trace was loading. End only this
            // local attempt; the capture UI ticker will discover and queue the current record.
            _AutoOpenTickerHandle.Reset();
            DoCancelAutoOpenTrace();
            return false;
        }

        // This callback is ending itself, so avoid asking the ticker to remove its currently
        // executing delegate before the normal false return unregisters it.
        _AutoOpenTickerHandle.Reset();
        DoCancelAutoOpenTrace();
        return false;
    }

    if (NOT _PendingAutoOpenWriterFinalized)
    {
        _PendingAutoOpenWriterFinalized = Capture.IsTraceWriterFinalized(_PendingAutoOpenTraceGuid);
        if (NOT _PendingAutoOpenWriterFinalized)
        {
            if (NOT _AutoOpenDelayWarningShown && FPlatformTime::Seconds() >= _AutoOpenDeadlineSeconds)
            {
                _AutoOpenDelayWarningShown = true;
                DoSetStatus(
                    TEXT("The trace writer is still finalizing after 30 seconds. Waiting to open the capture..."),
                    ECk_Tone::Warn);
            }
            return true;
        }
    }

    if (DoIsLoading())
    { return true; }

    const auto TraceExists = IFileManager::Get().FileSize(*_PendingAutoOpenTracePath) > 0;
    if (NOT TraceExists)
    {
        if (NOT _AutoOpenDelayWarningShown && FPlatformTime::Seconds() >= _AutoOpenDeadlineSeconds)
        {
            _AutoOpenDelayWarningShown = true;
            DoSetStatus(
                TEXT("The trace writer finalized, but its file is still empty after 30 seconds. Waiting to open the capture..."),
                ECk_Tone::Warn);
        }
        return true;
    }

    _PendingAutoReportTracePath = _PendingAutoOpenTracePath;
    DoOpenTracePath(_PendingAutoOpenTracePath);
    if (NOT DoIsLoading())
    {
        _PendingAutoReportTracePath.Reset();
        DoSetStatus(
            TEXT("Trace finalized, but opening failed. Close and reopen Insights Analyzer to retry."),
            ECk_Tone::Err);
        _SuppressedAutoOpenTraceGuid = _PendingAutoOpenTraceGuid;
        _AutoOpenTickerHandle.Reset();
        DoCancelAutoOpenTrace();
        return false;
    }

    _AutoOpenTraceOpeningStarted = true;
    // Keep the completion record throughout async loading. DoFinishLoading generates the
    // automatic report before the opening-started branch above observes DoIsLoading() == false
    // and acknowledges the record. Closing this tab at any earlier point therefore leaves the
    // controller record available for the next tab instance to recover.
    return true;
}

auto SCkInsightsAnalyzerTab::DoCancelAutoOpenTrace() -> void
{
    if (_AutoOpenTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(_AutoOpenTickerHandle);
        _AutoOpenTickerHandle.Reset();
    }

    _PendingAutoOpenTracePath.Reset();
    _PendingAutoOpenTraceGuid.Invalidate();
    _PendingAutoOpenWriterFinalized = false;
    _AutoOpenDelayWarningShown = false;
    _AutoOpenTraceOpeningStarted = false;
    _AutoOpenReportGenerated = false;
    _AutoOpenDeadlineSeconds = 0.0;
}

auto
    SCkInsightsAnalyzerTab::
    DoMakeRecentTracesMenu()
    -> TSharedRef<SWidget>
{
    struct FTraceFileInfo
    {
        FString Path;
        FDateTime MTime;
        int64 SizeBytes = 0;
    };

    // Profiling/ is where -tracefile / Trace.File captures land; captures without one go to
    // the local Unreal Trace Server store.
    const FString TraceStoreDir = FString{FPlatformProcess::UserSettingsDir()} /
        TEXT("UnrealEngine/Common/UnrealTrace/Store/001");
    const TArray<FString> SearchDirs = { FPaths::ProfilingDir(), TraceStoreDir };

    TArray<FTraceFileInfo> TraceFiles;
    for (const FString& Dir : SearchDirs)
    {
        TArray<FString> Found;
        IFileManager::Get().FindFiles(Found, *(Dir / TEXT("*.utrace")), true, false);
        for (const FString& FileName : Found)
        {
            const FString FullPath = Dir / FileName;
            TraceFiles.Add(FTraceFileInfo{
                FullPath,
                IFileManager::Get().GetTimeStamp(*FullPath),
                IFileManager::Get().FileSize(*FullPath)});
        }
    }

    ck::algo::Sort(TraceFiles, [](const FTraceFileInfo& A, const FTraceFileInfo& B)
    {
        return A.MTime > B.MTime;
    });

    constexpr auto MaxRecentTraces = 15;
    constexpr auto CloseAfterSelection = true;
    FMenuBuilder MenuBuilder(CloseAfterSelection, nullptr);

    if (TraceFiles.IsEmpty())
    {
        MenuBuilder.AddWidget(
            SNew(STextBlock)
                .Text(FText::FromString(TEXT("No .utrace files found")))
                .ColorAndOpacity(CkStyle::TextDim()),
            FText::GetEmpty());
        return MenuBuilder.MakeWidget();
    }

    for (int32 Index = 0; Index < FMath::Min(TraceFiles.Num(), MaxRecentTraces); ++Index)
    {
        const FTraceFileInfo& Info = TraceFiles[Index];
        const FString Label = FString::Printf(TEXT("%s    %s    %.1f MB"),
            *FPaths::GetCleanFilename(Info.Path),
            *Info.MTime.ToString(TEXT("%Y-%m-%d %H:%M")),
            static_cast<double>(Info.SizeBytes) / (1024.0 * 1024.0));

        MenuBuilder.AddMenuEntry(
            FText::FromString(Label),
            FText::FromString(Info.Path),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateSP(this, &SCkInsightsAnalyzerTab::DoOpenTracePath, Info.Path)));
    }

    return MenuBuilder.MakeWidget();
}

auto
    SCkInsightsAnalyzerTab::
    DoOnAnalyzeWorstClicked()
    -> FReply
{
    using namespace ck_insights_analyzer_tab;

    if (NOT _Session.IsOpen() || DoIsLoading())
    {
        DoSetStatus(TEXT("No trace loaded. Open a .utrace file first."), ECk_Tone::Warn);
        return FReply::Handled();
    }

    DoSetStatus(TEXT("Analyzing worst 10 frames..."), ECk_Tone::Info);

    FCk_MultiFrameReportConfig Config;
    Config.TargetFrameMs = TargetFrameMs;
    Config.Depth = _ReportDepth;
    Config.ApplyDepth();
    Config.WorstFrameCount = 10; // Override depth's default worst count

    FCk_MultiFrameReport MultiReport(Config);
    DoSetReport(MultiReport.AnalyzeWorstFrames(_Session, 10));
    DoPopulateMultiFrame(MultiReport.GetStats());

    DoSetStatus(TEXT("Worst 10 frames analysis complete. Click a Worst Frame row to drill in."), ECk_Tone::Ok);

    return FReply::Handled();
}

auto
    SCkInsightsAnalyzerTab::
    DoOnCopyToClipboardClicked()
    -> FReply
{
    if (_CurrentReport.IsEmpty())
    {
        DoSetStatus(TEXT("No report to copy."), ECk_Tone::Warn);
        return FReply::Handled();
    }

    FPlatformApplicationMisc::ClipboardCopy(*_CurrentReport);
    DoSetStatus(TEXT("Report copied to clipboard."), ECk_Tone::Ok);

    return FReply::Handled();
}

auto
    SCkInsightsAnalyzerTab::
    DoOnExportJsonClicked()
    -> FReply
{
    using namespace ck_insights_analyzer_tab;

    if (NOT _Session.IsOpen() ||
        (NOT _LastSingleResult.IsSet() && NOT _LastMultiStats.IsSet()))
    {
        DoSetStatus(TEXT("No analysis to export. Analyze a frame or range first."), ECk_Tone::Warn);
        return FReply::Handled();
    }

    const auto DesktopPlatform = FDesktopPlatformModule::Get();
    if (ck::Is_NOT_Valid(DesktopPlatform, ck::IsValid_Policy_NullptrOnly{}))
    {
        DoSetStatus(TEXT("Desktop platform not available."), ECk_Tone::Err);
        return FReply::Handled();
    }

    const FString BaseName = FPaths::GetBaseFilename(_Session.GetFilePath());
    const FString DefaultName = _LastSingleResult.IsSet()
        ? FString::Printf(TEXT("%s_Frame%llu.json"), *BaseName, _LastSingleResult->FrameIndex)
        : FString::Printf(TEXT("%s_Frames.json"), *BaseName);

    TArray<FString> OutFiles;
    const bool Saved = DesktopPlatform->SaveFileDialog(
        FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
        TEXT("Export JSON Report"),
        FPaths::ProjectSavedDir() / TEXT("TraceSessions"),
        DefaultName,
        TEXT("JSON files (*.json)|*.json"),
        EFileDialogFlags::None,
        OutFiles);

    if (NOT Saved || OutFiles.Num() == 0)
    {
        return FReply::Handled();
    }

    FString Json;
    if (_LastSingleResult.IsSet())
    {
        FCk_FrameReportConfig Config;
        Config.TargetFrameMs = TargetFrameMs;
        Config.Depth = _ReportDepth;
        Config.ApplyDepth();
        Config.ShowAllChildren = _ShowAllChildren;

        Json = FCk_JsonReport::GenerateSingleFrame(_Session, *_LastSingleResult, Config);
    }
    else
    {
        FCk_MultiFrameReportConfig Config;
        Config.TargetFrameMs = TargetFrameMs;
        Config.Depth = _ReportDepth;
        Config.ApplyDepth();

        Json = FCk_JsonReport::GenerateMultiFrame(_Session, *_LastMultiStats, Config);
    }

    if (FFileHelper::SaveStringToFile(Json, *OutFiles[0]))
    {
        DoSetStatus(FString::Printf(TEXT("Exported JSON: %s"), *FPaths::GetCleanFilename(OutFiles[0])), ECk_Tone::Ok);
    }
    else
    {
        DoSetStatus(FString::Printf(TEXT("Failed to write: %s"), *OutFiles[0]), ECk_Tone::Err);
    }

    return FReply::Handled();
}

auto
    SCkInsightsAnalyzerTab::
    DoOnWorstFrameClicked(uint64 FrameIndex)
    -> FReply
{
    if (NOT _Session.IsOpen() || DoIsLoading())
    {
        return FReply::Handled();
    }

    if (ck::IsValid(_FrameBarChart))
    {
        _FrameBarChart->EnsureFrameVisible(FrameIndex);
        _FrameBarChart->SetSelection(FrameIndex, FrameIndex);
    }
    DoClearScreenshotSelection();
    DoAnalyzeSingleFrame(FrameIndex);

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------
// Depth Selector
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInsightsAnalyzerTab::
    DoOnScreenshotMarkerClicked(uint32 ScreenshotId, uint64 FrameIndex)
    -> void
{
    if (NOT _Session.IsOpen() || _LoadingState == ELoadingState::Opening)
    {
        return;
    }

    const auto Screenshot = DoFindScreenshot(ScreenshotId);
    const auto IsMappedFrame = Screenshot != nullptr
        && Screenshot->GameFrameIndex != INDEX_NONE
        && static_cast<uint64>(Screenshot->GameFrameIndex) == FrameIndex;
    CK_ENSURE_IF_NOT(IsMappedFrame, TEXT("A screenshot marker must resolve to its mapped game frame"))
    {}
    if (NOT IsMappedFrame || NOT DoSelectScreenshot(ScreenshotId))
    {
        DoSetStatus(TEXT("The selected screenshot is no longer available in this trace."), ECk_Tone::Err);
        return;
    }

    if (ck::IsValid(_FrameBarChart))
    {
        _FrameBarChart->EnsureFrameVisible(FrameIndex);
        _FrameBarChart->SetSelection(FrameIndex, FrameIndex);
    }
    DoAnalyzeSingleFrame(FrameIndex);
}

auto
    SCkInsightsAnalyzerTab::
    DoOnDepthSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
    -> void
{
    if (ck::Is_NOT_Valid(NewValue)) return;

    if (*NewValue == TEXT("Full"))           _ReportDepth = ECkReportDepth::Full;
    else if (*NewValue == TEXT("Standard"))  _ReportDepth = ECkReportDepth::Standard;
    else if (*NewValue == TEXT("Concise"))   _ReportDepth = ECkReportDepth::Concise;
    else                                    _ReportDepth = ECkReportDepth::HotPathsOnly;

    DoRerunCurrentSelection();
}

auto
    SCkInsightsAnalyzerTab::
    DoGet_ShowAllChildren() const
    -> bool
{
    return _ShowAllChildren;
}

auto
    SCkInsightsAnalyzerTab::
    DoOnShowAllChildrenChanged(bool InShowAllChildren)
    -> void
{
    _ShowAllChildren = InShowAllChildren;

    DoRerunCurrentSelection();
}

// --------------------------------------------------------------------------------------------------------------------
// Chart Delegate
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInsightsAnalyzerTab::
    DoOnFrameSelectionChanged(uint64 StartFrame, uint64 EndFrame)
    -> void
{
    // Only Opening is blocked — during ReadingFrames the user can inspect loaded frames.
    if (NOT _Session.IsOpen() || _LoadingState == ELoadingState::Opening) return;

    DoClearScreenshotSelection();

    if (StartFrame == EndFrame)
    {
        DoAnalyzeSingleFrame(StartFrame);
    }
    else
    {
        DoAnalyzeFrameRange(StartFrame, EndFrame);
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInsightsAnalyzerTab::
    DoSetStatus(const FString& Text, ECk_Tone InTone)
    -> void
{
    _StatusTone = InTone;
    if (ck::IsValid(_StatusText))
    {
        _StatusText->SetText(FText::FromString(Text));
    }
}

auto
    SCkInsightsAnalyzerTab::
    DoSetReport(const FString& ReportText)
    -> void
{
    _CurrentReport = ReportText;
    if (ck::IsValid(_ReportText))
    {
        _ReportText->SetText(FText::FromString(ReportText));
    }
}

auto
    SCkInsightsAnalyzerTab::
    DoClearResults()
    -> void
{
    DoSetReport(TEXT(""));

    _ResultsMode = EResultsMode::None;
    _AnalyzedFrameMs = 0.0;

    _HotPathRoots.Reset();
    _Categories.Reset();
    _TopTimers.Reset();
    _WorstFrames.Reset();
    _CategoryAverages.Reset();
    _WaitRows.Reset();
    _LastSingleResult.Reset();
    _LastMultiStats.Reset();

    if (ck::IsValid(_HotPathTree)) { _HotPathTree->RequestTreeRefresh(); }
    DoRebuildAllSidePanelRows();

    if (ck::IsValid(_SummaryBox)) { _SummaryBox->ClearChildren(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInsightsAnalyzerTab::
    DoRebuildAllSidePanelRows()
    -> void
{
    DoRebuildCategoryRows();
    DoRebuildTopTimerRows();
    DoRebuildWorstFrameRows();
    DoRebuildCategoryAvgRows();
    DoRebuildWaitRows();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInsightsAnalyzerTab::
    Tick(
        const FGeometry& InAllottedGeometry,
        double           InCurrentTime,
        float            InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    Poll_StyleRevision();
}

auto
    SCkInsightsAnalyzerTab::
    Poll_StyleRevision()
    -> void
{
    // This tab is a plain SCompoundWidget with no refresh gate of its own (its work is driven by
    // FTSTicker, not by Slate Tick), so it carries its own copy of SCkDebugger_WindowBase's watch.
    const auto* Settings = UCkDebuggerStyleSettings::Get();

    if (Settings == nullptr)
    { return; }

    const auto Revision = Settings->Get_Revision();

    if (Revision == _LastSeenStyleRevision)
    { return; }

    _LastSeenStyleRevision = Revision;

    // Everything visual is attribute-bound now; the side-panel rows and the hot-path row widgets are
    // still built imperatively, so they get re-stamped through their existing rebuild entry points.
    DoRebuildAllSidePanelRows();

    if (ck::IsValid(_HotPathTree))
    { _HotPathTree->RebuildList(); }
}

auto
    SCkInsightsAnalyzerTab::
    DoLoadScreenshots()
    -> void
{
    DoClearScreenshots();
    if (NOT _Session.IsOpen())
    {
        return;
    }

    _TraceScreenshots = _Session.GetScreenshots();

    auto Markers = TArray<FCk_FrameScreenshotMarker>{};
    Markers.Reserve(_TraceScreenshots.Num());
    int32 RemainingThumbnailBudget = ck_insights_analyzer_tab::MaxEagerScreenshotThumbnails;
    for (const auto& Screenshot : _TraceScreenshots)
    {
        if (Screenshot.GameFrameIndex == INDEX_NONE
            || Screenshot.GameFrameIndex < 0
            || static_cast<uint64>(Screenshot.GameFrameIndex) >= _Session.GetFrameCount())
        {
            continue;
        }

        auto Thumbnail = TSharedPtr<FSlateDynamicImageBrush>{};
        if (Screenshot.bIsPayloadComplete && RemainingThumbnailBudget > 0)
        {
            --RemainingThumbnailBudget;
            Thumbnail = DoCreateScreenshotBrush(Screenshot.Id, 128, 72, TEXT("Thumbnail"));
            if (ck::IsValid(Thumbnail))
            {
                _ScreenshotThumbnailBrushes.Add(Screenshot.Id, Thumbnail);
            }
        }

        auto& Marker = Markers.AddDefaulted_GetRef();
        Marker.ScreenshotId = Screenshot.Id;
        Marker.FrameIndex = static_cast<uint64>(Screenshot.GameFrameIndex);
        Marker.Label = Screenshot.bIsInsideGameFrame
            ? Screenshot.Name
            : FString::Printf(TEXT("%s (nearest preceding game frame)"), *Screenshot.Name);
        Marker.ThumbnailBrush = Thumbnail;
    }

    if (ck::IsValid(_FrameBarChart))
    {
        _FrameBarChart->SetScreenshotMarkers(MoveTemp(Markers));
    }
}

auto
    SCkInsightsAnalyzerTab::
    DoClearScreenshots()
    -> void
{
    if (ck::IsValid(_FrameBarChart))
    {
        _FrameBarChart->ClearScreenshotMarkers();
    }
    DoClearScreenshotSelection();
    _ScreenshotThumbnailBrushes.Reset();
    _TraceScreenshots.Reset();
}

auto
    SCkInsightsAnalyzerTab::
    DoClearScreenshotSelection()
    -> void
{
    _SelectedScreenshotBrush.Reset();
    _SelectedScreenshotId.Reset();
    _SelectedScreenshotError.Reset();
}

auto
    SCkInsightsAnalyzerTab::
    DoSelectScreenshot(uint32 ScreenshotId)
    -> bool
{
    const auto Screenshot = DoFindScreenshot(ScreenshotId);
    if (Screenshot == nullptr)
    {
        DoClearScreenshotSelection();
        return false;
    }

    if (_SelectedScreenshotId.IsSet()
        && _SelectedScreenshotId.GetValue() == ScreenshotId
        && ck::IsValid(_SelectedScreenshotBrush))
    {
        return true;
    }

    DoClearScreenshotSelection();
    _SelectedScreenshotId = ScreenshotId;
    if (NOT Screenshot->bIsPayloadComplete)
    {
        _SelectedScreenshotError = FString::Printf(
            TEXT("The screenshot payload is incomplete (%u of %u bytes)."),
            Screenshot->PayloadByteSize,
            Screenshot->ExpectedPayloadByteSize);
        return true;
    }

    _SelectedScreenshotBrush = DoCreateScreenshotBrush(ScreenshotId, 1280, 720, TEXT("Preview"));
    if (ck::Is_NOT_Valid(_SelectedScreenshotBrush))
    {
        _SelectedScreenshotError = TEXT("The embedded screenshot could not be decoded.");
    }
    return true;
}

auto
    SCkInsightsAnalyzerTab::
    DoFindScreenshot(uint32 ScreenshotId) const
    -> const FCk_TraceScreenshot*
{
    return _TraceScreenshots.FindByPredicate(
        [ScreenshotId](const FCk_TraceScreenshot& Screenshot)
        {
            return Screenshot.Id == ScreenshotId;
        });
}

auto
    SCkInsightsAnalyzerTab::
    DoCreateScreenshotBrush(uint32 ScreenshotId,
                            int32 MaxWidth,
                            int32 MaxHeight,
                            const TCHAR* ResourceSuffix)
    -> TSharedPtr<FSlateDynamicImageBrush>
{
    const auto DimensionsAreValid = MaxWidth > 0 && MaxHeight > 0 && ResourceSuffix != nullptr;
    CK_ENSURE_IF_NOT(DimensionsAreValid, TEXT("Screenshot brush dimensions and suffix must be valid"))
    {}
    if (NOT DimensionsAreValid)
    {
        return nullptr;
    }

    auto CompressedData = TArray<uint8>{};
    if (NOT _Session.TryCopyScreenshotData(ScreenshotId, CompressedData) || CompressedData.IsEmpty())
    {
        return nullptr;
    }

    auto& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName{TEXT("ImageWrapper")});
    auto DecodedImage = FImage{};
    if (NOT ImageWrapperModule.DecompressImage(CompressedData.GetData(), CompressedData.Num(), DecodedImage)
        || DecodedImage.SizeX <= 0
        || DecodedImage.SizeY <= 0)
    {
        return nullptr;
    }

    const auto Scale = FMath::Min(
        1.0,
        FMath::Min(
            static_cast<double>(MaxWidth) / static_cast<double>(DecodedImage.SizeX),
            static_cast<double>(MaxHeight) / static_cast<double>(DecodedImage.SizeY)));
    const auto DisplayWidth = FMath::Max(1, FMath::RoundToInt(static_cast<double>(DecodedImage.SizeX) * Scale));
    const auto DisplayHeight = FMath::Max(1, FMath::RoundToInt(static_cast<double>(DecodedImage.SizeY) * Scale));

    auto DisplayImage = FImage{};
    DecodedImage.ResizeTo(
        DisplayImage,
        DisplayWidth,
        DisplayHeight,
        ERawImageFormat::BGRA8,
        EGammaSpace::sRGB);

    const auto ResourceName = FName{*FString::Printf(
        TEXT("CkInsightsScreenshot_%u_%s_%llu"),
        ScreenshotId,
        ResourceSuffix,
        ++_ScreenshotBrushGeneration)};
    return FSlateDynamicImageBrush::CreateWithImageData(
        ResourceName,
        FVector2D{static_cast<float>(DisplayWidth), static_cast<float>(DisplayHeight)},
        TArray<uint8>(DisplayImage.RawData));
}

auto
    SCkInsightsAnalyzerTab::
    DoAnalyzeSingleFrame(uint64 FrameIndex)
    -> void
{
    using namespace ck_insights_analyzer_tab;

    DoSetStatus(FString::Printf(TEXT("Analyzing frame %llu..."), FrameIndex), ECk_Tone::Info);

    FCk_FrameAnalysisResult Result = FCk_FrameAnalyzer::AnalyzeFrame(_Session, FrameIndex);
    if (NOT Result.IsValid())
    {
        DoSetStatus(FString::Printf(TEXT("Frame %llu produced no analysis data."), FrameIndex), ECk_Tone::Warn);

        _ResultsMode = EResultsMode::None;
        _AnalyzedFrameMs = 0.0;
        _HotPathRoots.Reset();
        _Categories.Reset();
        _TopTimers.Reset();
        _WaitRows.Reset();
        _LastSingleResult.Reset();
        if (ck::IsValid(_HotPathTree)) { _HotPathTree->RequestTreeRefresh(); }
        DoRebuildCategoryRows();
        DoRebuildTopTimerRows();
        DoRebuildWaitRows();
        DoSetReport(TEXT(""));
        return;
    }

    FCk_FrameReportConfig Config;
    Config.TargetFrameMs = TargetFrameMs;
    Config.Depth = _ReportDepth;
    Config.ApplyDepth();
    Config.ShowAllChildren = _ShowAllChildren;

    FCk_FrameReport FrameReport(Config);
    DoSetReport(FrameReport.Generate(_Session, Result));

    _ResultsMode = EResultsMode::SingleFrame;
    _AnalyzedFrameMs = Result.FrameDurationMs;
    _LastSingleResult = Result;   // retained for Export JSON
    _LastMultiStats.Reset();
    _HotPathRoots = FrameReport.BuildHotPathTree(_Session, Result);

    {
        TraceServices::FAnalysisSessionReadScope ReadScope = _Session.CreateReadScope();
        const FCk_FrameReport::FTimerNameMap TimerNames = FCk_FrameReport::BuildTimerNameMap(_Session);
        _Categories = FrameReport.ComputeCategorySummary(Result, TimerNames);
        _TopTimers = FrameReport.ComputeTopTimers(Result, TimerNames, TopTimerCount);

        // ComputeWaitSummaries reads timer names internally — needs the scope above
        _WaitRows = FCk_FrameReport::ComputeWaitSummaries(_Session, Result, Config.MinWaitMs);
    }

    if (ck::IsValid(_HotPathTree)) { _HotPathTree->RequestTreeRefresh(); }
    DoExpandHotPathDefaults();
    DoRebuildCategoryRows();
    DoRebuildTopTimerRows();
    DoRebuildWaitRows();
    DoRebuildSummaryStrip_SingleFrame(Result);

    auto Status = FString::Printf(TEXT("Frame %llu: %.2fms"), FrameIndex, Result.FrameDurationMs);
    if (_SelectedScreenshotId.IsSet())
    {
        const auto Screenshot = DoFindScreenshot(_SelectedScreenshotId.GetValue());
        if (Screenshot != nullptr && Screenshot->GameFrameIndex == static_cast<int64>(FrameIndex))
        {
            Status += FString::Printf(TEXT("  |  screenshot: %s"), *Screenshot->Name);
        }
    }
    DoSetStatus(Status, FrameBudgetTone(Result.FrameDurationMs));
}

auto
    SCkInsightsAnalyzerTab::
    DoAnalyzeFrameRange(uint64 StartFrame, uint64 EndFrame)
    -> void
{
    using namespace ck_insights_analyzer_tab;

    const uint64 Count = EndFrame - StartFrame + 1;
    DoSetStatus(FString::Printf(TEXT("Analyzing frames %llu-%llu (%llu frames)..."), StartFrame, EndFrame, Count),
        ECk_Tone::Info);

    FCk_MultiFrameReportConfig Config;
    Config.TargetFrameMs = TargetFrameMs;
    Config.Depth = _ReportDepth;
    Config.ApplyDepth();

    FCk_MultiFrameReport MultiReport(Config);
    const uint64 EndFrameExclusive = EndFrame + 1;
    DoSetReport(MultiReport.AnalyzeAndGenerate(_Session, StartFrame, EndFrameExclusive));
    DoPopulateMultiFrame(MultiReport.GetStats());

    DoSetStatus(FString::Printf(TEXT("Analyzed frames %llu-%llu (%llu frames)"), StartFrame, EndFrame, Count),
        ECk_Tone::Ok);
}

auto
    SCkInsightsAnalyzerTab::
    DoRerunCurrentSelection()
    -> void
{
    if (NOT _Session.IsOpen() || DoIsLoading() ||
        ck::Is_NOT_Valid(_FrameBarChart) || NOT _FrameBarChart->HasSelection())
    {
        return;
    }

    DoOnFrameSelectionChanged(_FrameBarChart->GetSelectionStart(), _FrameBarChart->GetSelectionEnd());
}

auto
    SCkInsightsAnalyzerTab::
    DoPopulateMultiFrame(const FCk_MultiFrameStats& Stats)
    -> void
{
    _ResultsMode = EResultsMode::MultiFrame;
    _AnalyzedFrameMs = 0.0;

    _HotPathRoots.Reset();
    _Categories.Reset();
    _TopTimers.Reset();
    _WaitRows.Reset();
    _LastSingleResult.Reset();
    _LastMultiStats = Stats;      // retained for Export JSON

    _WorstFrames = Stats.WorstFrames;
    _CategoryAverages = Stats.CategoryAverages;

    if (ck::IsValid(_HotPathTree)) { _HotPathTree->RequestTreeRefresh(); }
    DoRebuildCategoryRows();
    DoRebuildTopTimerRows();
    DoRebuildWaitRows();
    DoRebuildWorstFrameRows();
    DoRebuildCategoryAvgRows();
    DoRebuildSummaryStrip_MultiFrame(Stats);
}

auto
    SCkInsightsAnalyzerTab::
    DoGenerateAutomatedCaptureReport()
    -> bool
{
    using namespace ck_insights_analyzer_tab;

    const auto TracePath = MoveTemp(_PendingAutoReportTracePath);
    _PendingAutoReportTracePath.Reset();

    const auto CanGenerate = _Session.IsOpen()
        && NOT TracePath.IsEmpty()
        && FPaths::ConvertRelativePathToFull(TracePath) == FPaths::ConvertRelativePathToFull(_Session.GetFilePath());
    CK_ENSURE_IF_NOT(CanGenerate, TEXT("Automated capture report must target the trace that finished loading"))
    {}
    if (NOT CanGenerate)
    {
        DoSetStatus(TEXT("Trace loaded, but its automated report target did not match."), ECk_Tone::Err);
        return false;
    }

    constexpr int32 HotFrameCount = 5;
    FCk_MultiFrameReportConfig Config;
    Config.TargetFrameMs = TargetFrameMs;
    Config.Depth = ECkReportDepth::Standard;
    Config.ApplyDepth();
    Config.WorstFrameCount = HotFrameCount;

    FCk_MultiFrameReport MultiReport(Config);
    const auto Markdown = MultiReport.AnalyzeWorstFrames(_Session, HotFrameCount);
    const auto HasAnalyzableFrames = MultiReport.GetStats().FrameCount > 0;
    const auto Json = FCk_JsonReport::GenerateMultiFrame(
        _Session,
        MultiReport.GetStats(),
        MultiReport.GetConfig());

    DoSetReport(Markdown);
    DoPopulateMultiFrame(MultiReport.GetStats());

    const auto ReportDirectory = FPaths::GetPath(TracePath);
    const auto TraceStem = FPaths::GetBaseFilename(TracePath);
    const auto MarkdownPath = ReportDirectory / (TraceStem + TEXT(".report.md"));
    const auto JsonPath = ReportDirectory / (TraceStem + TEXT(".report.json"));
    const auto SavedMarkdown = FFileHelper::SaveStringToFile(
        Markdown,
        *MarkdownPath,
        FFileHelper::EEncodingOptions::ForceUTF8);
    const auto SavedJson = FFileHelper::SaveStringToFile(
        Json,
        *JsonPath,
        FFileHelper::EEncodingOptions::ForceUTF8);

    if (NOT SavedMarkdown || NOT SavedJson)
    {
        DoSetStatus(
            FString::Printf(
                TEXT("Trace loaded, but automated report export failed (%s%s)."),
                SavedMarkdown ? TEXT("") : TEXT("Markdown "),
                SavedJson ? TEXT("") : TEXT("JSON")),
            ECk_Tone::Err);
        return false;
    }

    DoSetStatus(
        HasAnalyzableFrames
            ? FString::Printf(
                TEXT("Capture analyzed: %s, %s"),
                *FPaths::GetCleanFilename(MarkdownPath),
                *FPaths::GetCleanFilename(JsonPath))
            : FString::Printf(
                TEXT("Capture loaded with no game frames; diagnostic reports saved: %s, %s"),
                *FPaths::GetCleanFilename(MarkdownPath),
                *FPaths::GetCleanFilename(JsonPath)),
        HasAnalyzableFrames ? ECk_Tone::Ok : ECk_Tone::Warn);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
// Summary strip
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInsightsAnalyzerTab::
    DoAddSummaryTile(const FString& InLabel, const FString& InValue, const FLinearColor& InValueColor)
    -> void
{
    using namespace ck_insights_analyzer_tab;

    if (ck::Is_NOT_Valid(_SummaryBox))
    {
        return;
    }

    _SummaryBox->AddSlot()
    .AutoWidth()
    .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
    [
        MakeStatTile(InLabel, InValue, InValueColor)
    ];
}

auto
    SCkInsightsAnalyzerTab::
    DoRebuildSummaryStrip_TraceInfo()
    -> void
{
    using namespace ck_insights_analyzer_tab;

    if (ck::Is_NOT_Valid(_SummaryBox))
    {
        return;
    }
    _SummaryBox->ClearChildren();

    if (NOT _Session.IsOpen())
    {
        return;
    }

    DoAddSummaryTile(TEXT("Trace"), FPaths::GetCleanFilename(_Session.GetFilePath()), CkStyle::Text());
    DoAddSummaryTile(TEXT("Duration"), FString::Printf(TEXT("%.1fs"), _Session.GetDurationSeconds()), CkStyle::Text());
    DoAddSummaryTile(TEXT("Game Frames"), FormatWithCommas(_Session.GetFrameCount()), CkStyle::Text());

    if (NOT _TraceScreenshots.IsEmpty())
    {
        DoAddSummaryTile(TEXT("Screenshots"), FormatWithCommas(_TraceScreenshots.Num()), CkStyle::Accent());
    }

    if (const uint64 RenderFrames = _Session.GetRenderFrameCount();
        RenderFrames > 0)
    {
        DoAddSummaryTile(TEXT("Render Frames"), FormatWithCommas(RenderFrames), CkStyle::Text());
    }

    DoAddSummaryTile(TEXT("Threads"), FormatWithCommas(_Session.GetThreadInfos().Num()), CkStyle::TextDim());
    DoAddSummaryTile(TEXT("Budget"), FString::Printf(TEXT("%.1fms"), TargetFrameMs), CkStyle::TextDim());
}

auto
    SCkInsightsAnalyzerTab::
    DoRebuildSummaryStrip_SingleFrame(const FCk_FrameAnalysisResult& Result)
    -> void
{
    using namespace ck_insights_analyzer_tab;

    DoRebuildSummaryStrip_TraceInfo();

    const double FrameMs = Result.FrameDurationMs;
    const double OverBudget = FrameMs / TargetFrameMs;

    DoAddSummaryTile(TEXT("Frame"), FString::Printf(TEXT("#%s"), *FormatWithCommas(Result.FrameIndex)), CkStyle::Accent());
    DoAddSummaryTile(TEXT("Frame Time"), FString::Printf(TEXT("%.2fms"), FrameMs), FrameBudgetColor(FrameMs));
    DoAddSummaryTile(TEXT("Vs Budget"), FString::Printf(TEXT("%.1fx"), OverBudget), FrameBudgetColor(FrameMs));
}

auto
    SCkInsightsAnalyzerTab::
    DoRebuildSummaryStrip_MultiFrame(const FCk_MultiFrameStats& Stats)
    -> void
{
    using namespace ck_insights_analyzer_tab;

    DoRebuildSummaryStrip_TraceInfo();

    DoAddSummaryTile(TEXT("Analyzed"), FormatWithCommas(Stats.FrameCount), CkStyle::Accent());
    DoAddSummaryTile(TEXT("Avg"), FString::Printf(TEXT("%.2fms"), Stats.AvgFrameMs), FrameBudgetColor(Stats.AvgFrameMs));
    DoAddSummaryTile(TEXT("P95"), FString::Printf(TEXT("%.2fms"), Stats.P95FrameMs), FrameBudgetColor(Stats.P95FrameMs));
    DoAddSummaryTile(TEXT("P99"), FString::Printf(TEXT("%.2fms"), Stats.P99FrameMs), FrameBudgetColor(Stats.P99FrameMs));
    DoAddSummaryTile(TEXT("Max"), FString::Printf(TEXT("%.2fms"), Stats.MaxFrameMs), FrameBudgetColor(Stats.MaxFrameMs));
}

// --------------------------------------------------------------------------------------------------------------------
// Async Loading
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInsightsAnalyzerTab::
    DoStartAsyncOpen(const FString& TracePath)
    -> void
{
    _PendingTracePath = TracePath;
    _LoadingState = ELoadingState::Opening;

    DoSetStatus(FString::Printf(TEXT("Opening: %s ..."), *FPaths::GetCleanFilename(TracePath)), ECk_Tone::Info);

    // Prepare analysis service on game thread (FModuleManager is not thread-safe)
    _PendingSession = MakeShared<FCk_TraceSession>();
    if (NOT _PendingSession->PrepareAnalysisService())
    {
        DoSetStatus(TEXT("Failed to create analysis service."), ECk_Tone::Err);
        _PendingSession.Reset();
        _LoadingState = ELoadingState::Idle;
        return;
    }

    // Must START on the game thread — registered trace modules (e.g. ChaosVD)
    // ensure(IsInGameThread()) inside OnAnalysisBegin, which StartAnalysis fires synchronously.
    // The heavy processing then runs on TraceServices' own thread; the ticker polls completion.
    if (NOT _PendingSession->StartAnalysis(TracePath))
    {
        DoSetStatus(FString::Printf(TEXT("Failed to open: %s"),
            *FPaths::GetCleanFilename(TracePath)), ECk_Tone::Err);
        _PendingSession.Reset();
        _LoadingState = ELoadingState::Idle;
        return;
    }

    _LoadingTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateSP(this, &SCkInsightsAnalyzerTab::DoOnLoadingTick),
        0.1f);
}

auto
    SCkInsightsAnalyzerTab::
    DoOnLoadingTick(float DeltaTime)
    -> bool
{
    switch (_LoadingState)
    {
    case ELoadingState::Opening:
    {
        if (ck::Is_NOT_Valid(_PendingSession))
        {
            _LoadingState = ELoadingState::Idle;
            return false; // stop ticking
        }

        if (NOT _PendingSession->IsAnalysisComplete())
        {
            return true; // keep ticking
        }

        _Session = MoveTemp(*_PendingSession);
        _PendingSession.Reset();

        _TotalFrameCount = _Session.GetFrameCount();
        _LoadedFrameCount = 0;
        _PendingFrameDurations.Reset();
        _PendingFrameDurations.Reserve(static_cast<int32>(_TotalFrameCount));

        if (_TotalFrameCount == 0)
        {
            DoFinishLoading();
            return false;
        }

        _LoadingState = ELoadingState::ReadingFrames;

        DoSetStatus(FString::Printf(
            TEXT("Reading frames: 0 / %llu ..."), _TotalFrameCount), ECk_Tone::Info);

        return true; // continue ticking for frame reading
    }

    case ELoadingState::ReadingFrames:
    {
        if (DoLoadFrameChunk())
        {
            DoFinishLoading();
            return false; // stop ticking
        }
        return true; // keep ticking
    }

    default:
        return false;
    }
}

auto
    SCkInsightsAnalyzerTab::
    DoLoadFrameChunk()
    -> bool
{
    constexpr uint64 ChunkSize = 2000;

    TraceServices::FAnalysisSessionReadScope ReadScope = _Session.CreateReadScope();

    const uint64 EndFrame = FMath::Min(_LoadedFrameCount + ChunkSize, _TotalFrameCount);

    TArray<double> ChunkDurations;
    ChunkDurations.Reserve(static_cast<int32>(EndFrame - _LoadedFrameCount));

    for (uint64 i = _LoadedFrameCount; i < EndFrame; ++i)
    {
        const auto Frame = _Session.GetFrame(i);
        if (ck::IsValid(Frame, ck::IsValid_Policy_NullptrOnly{}))
        {
            const double DurationMs = (Frame->EndTime - Frame->StartTime) * 1000.0;
            ChunkDurations.Add(DurationMs);
        }
        else
        {
            ChunkDurations.Add(0.0);
        }
    }

    _PendingFrameDurations.Append(ChunkDurations);

    if (ck::IsValid(_FrameBarChart))
    {
        _FrameBarChart->AppendFrameData(ChunkDurations);
    }

    _LoadedFrameCount = EndFrame;

    DoSetStatus(FString::Printf(
        TEXT("Reading frames: %llu / %llu ..."),
        _LoadedFrameCount, _TotalFrameCount), ECk_Tone::Info);

    return _LoadedFrameCount >= _TotalFrameCount;
}

auto
    SCkInsightsAnalyzerTab::
    DoFinishLoading()
    -> void
{
    // The chart already has every frame from the progressive appends; rescaling to P95 here
    // (rather than SetFrameData) preserves the user's viewport/zoom.
    if (ck::IsValid(_FrameBarChart))
    {
        _FrameBarChart->RecalculateDisplayMax();
    }
    _PendingFrameDurations.Reset();

    DoLoadScreenshots();

    const double DurationSec = _Session.GetDurationSeconds();
    DoSetStatus(FString::Printf(
        TEXT("Loaded: %s  |  %llu frames  |  %.1fs duration  |  %d screenshots"),
        *FPaths::GetCleanFilename(_PendingTracePath),
        _TotalFrameCount,
        DurationSec,
        _TraceScreenshots.Num()),
        ECk_Tone::Ok);

    DoRebuildSummaryStrip_TraceInfo();

    _LoadingState = ELoadingState::Idle;
    _LoadingTickerHandle.Reset();

    if (NOT _PendingAutoReportTracePath.IsEmpty())
    {
        _AutoOpenReportGenerated = DoGenerateAutomatedCaptureReport();
    }
}

auto
    SCkInsightsAnalyzerTab::
    DoCancelLoading()
    -> void
{
    if (_LoadingTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(_LoadingTickerHandle);
        _LoadingTickerHandle.Reset();
    }

    _PendingSession.Reset();
    _PendingFrameDurations.Reset();
    _LoadingState = ELoadingState::Idle;
}

// --------------------------------------------------------------------------------------------------------------------
