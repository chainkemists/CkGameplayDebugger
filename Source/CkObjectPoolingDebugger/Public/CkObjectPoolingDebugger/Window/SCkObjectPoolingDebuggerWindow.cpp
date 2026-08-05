#include "CkObjectPoolingDebugger/Window/SCkObjectPoolingDebuggerWindow.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"
#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Sparkline.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatPair.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkDebuggerCommon/Window/SCkDebugger_RefreshControls.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Engine/World.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_object_pooling_debugger_window
{
    static constexpr auto HistoryCapacity = 120; // gated refresh defaults to ~1 Hz → ~2 min of history

    static constexpr auto Col_Class     = 0.30f;
    static constexpr auto Col_Policy    = 0.08f;
    static constexpr auto Col_Occupancy = 0.18f;
    static constexpr auto Col_Trend     = 0.09f;
    static constexpr auto Col_Num       = 0.05f;

    static auto MonoFont(int32 InSize) -> FSlateFontInfo { return FCoreStyle::GetDefaultFontStyle("Mono", InSize); }
    static auto BoldFont(int32 InSize) -> FSlateFontInfo { return FCoreStyle::GetDefaultFontStyle("Bold", InSize); }

    static auto Get_HitRateColor(const FCkObjectPoolingDebugger_PoolRow& InRow) -> FLinearColor
    {
        if (InRow.Get_TotalAcquires() == 0)
        { return CkStyle::TextMute(); }

        const auto HitRate = InRow.Get_HitRate();
        if (HitRate >= 0.95f) { return CkStyle::Ok(); }
        if (HitRate >= 0.80f) { return CkStyle::Warn(); }
        return CkStyle::Err();
    }

    static auto Matches(const FCkObjectPoolingDebugger_PoolRow& InRow, const FString& InQuery) -> bool
    {
        return InQuery.IsEmpty()
            || InRow.ClassName.Contains(InQuery)
            || InRow.DisplayClassName.Contains(InQuery)
            || InRow.ArchetypeName.Contains(InQuery);
    }

    static auto Push_Sample(const TSharedPtr<TArray<float>>& InRing, float InValue) -> void
    {
        if (InRing->Num() >= HistoryCapacity)
        { InRing->RemoveAt(0, 1, EAllowShrinking::No); }
        InRing->Add(InValue);
    }

    // ================================================================================================================
    // Per-row occupancy bar: parked band (ok, dim) under the in-use fill (info), a high-water tick
    // (accent), and — for bounded pools at capacity — an err-tinted ground. All rows share one max
    // scale so widths are comparable across pools. Volatile: reads the live row struct every paint.
    // ================================================================================================================

    class SOccupancyBar : public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(SOccupancyBar) {}
            SLATE_ARGUMENT(TSharedPtr<FCkObjectPoolingDebugger_PoolRow>, Item)
            SLATE_ARGUMENT(TSharedPtr<float>, SharedMax)
        SLATE_END_ARGS()

        auto Construct(const FArguments& InArgs) -> void
        {
            _Item = InArgs._Item;
            _SharedMax = InArgs._SharedMax;
            ForceVolatile(true);
        }

        auto ComputeDesiredSize(float) const -> FVector2D override
        {
            return FVector2D{90.0f, 9.0f};
        }

        auto OnPaint(
            const FPaintArgs& InArgs,
            const FGeometry& InAllottedGeometry,
            const FSlateRect& InCullingRect,
            FSlateWindowElementList& OutDrawElements,
            int32 InLayerId,
            const FWidgetStyle& InWidgetStyle,
            bool InParentEnabled) const -> int32 override
        {
            if (NOT _Item.IsValid() || NOT _SharedMax.IsValid())
            { return InLayerId; }

            const auto& Row = *_Item;
            const auto Size = InAllottedGeometry.GetLocalSize();
            const auto Max = FMath::Max(*_SharedMax, 1.0f);
            const auto* Brush = FCoreStyle::Get().GetBrush("WhiteBrush");

            const auto MakeSegment = [&](float InX, float InWidth, const FLinearColor& InColor, int32 InLayer)
            {
                FSlateDrawElement::MakeBox(
                    OutDrawElements, InLayer,
                    InAllottedGeometry.ToPaintGeometry(
                        FVector2f(InWidth, Size.Y),
                        FSlateLayoutTransform(FVector2f(InX, 0.0f))),
                    Brush, ESlateDrawEffect::None, InColor);
            };

            const auto AtCapacity = Row.IsBoundedCapacity && Row.NumLiveInstances >= Row.MaxSize;
            MakeSegment(0.0f, Size.X, AtCapacity
                ? CkStyle::OverlayOf(CkStyle::Err(), 0.18f)
                : CkStyle::Bg3(), InLayerId);

            const auto LiveWidth = FMath::Clamp(Row.NumLiveInstances / Max, 0.0f, 1.0f) * Size.X;
            MakeSegment(0.0f, LiveWidth, CkStyle::OverlayOf(CkStyle::Ok(), 0.35f), InLayerId + 1);

            const auto InUseWidth = FMath::Clamp(Row.NumInUse / Max, 0.0f, 1.0f) * Size.X;
            MakeSegment(0.0f, InUseWidth, CkStyle::OverlayOf(CkStyle::Info(), 0.85f), InLayerId + 2);

            const auto HighX = FMath::Clamp(Row.HighWaterMark / Max, 0.0f, 1.0f) * (Size.X - 2.0f);
            MakeSegment(HighX, 2.0f, CkStyle::Accent(), InLayerId + 3);

            return InLayerId + 4;
        }

    private:
        TSharedPtr<FCkObjectPoolingDebugger_PoolRow> _Item;
        TSharedPtr<float> _SharedMax;
    };
}

// ====================================================================================================================

const FName SCkObjectPoolingDebuggerWindow::WindowId = FName(TEXT("ObjectPoolingDebugger"));

auto
    SCkObjectPoolingDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    Register_WithGate();

    _WorldModel = MakeShared<FCkDebuggerModel_WorldSelector>();
    _TotalInUseSamples = MakeShared<TArray<float>>();
    _SharedMaxLive = MakeShared<float>(1.0f);

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome)
        .WindowId(WindowId)
        .ToolTabId(TEXT("CkObjectPoolingDebugger"))
        .DisplayName(Get_WindowDisplayName())
        .Content()
        [
            SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(CkStyle::BgRoot())
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight()
                [ BuildToolbar() ]

            + SVerticalBox::Slot().AutoHeight()
                [ BuildOverviewStrip() ]

            + SVerticalBox::Slot().FillHeight(1.0f)
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot().FillWidth(1.0f)
                        [
                            SNew(SVerticalBox)

                            + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceS, CkStyle::SpaceS, CkStyle::SpaceS, 0.0f)
                                [ BuildHeaderRow() ]

                            + SVerticalBox::Slot().FillHeight(1.0f).Padding(CkStyle::SpaceS, 0.0f)
                                [
                                    SAssignNew(_ListView, SListView<ItemPtr>)
                                        .ListItemsSource(&_VisibleItems)
                                        .OnGenerateRow(this, &SCkObjectPoolingDebuggerWindow::OnGenerateRow)
                                        .OnSelectionChanged(this, &SCkObjectPoolingDebuggerWindow::OnSelectionChanged)
                                        .SelectionMode(ESelectionMode::Single)
                                ]

                            + SVerticalBox::Slot().AutoHeight()
                                [ BuildStatusBar() ]
                        ]

                    + SHorizontalBox::Slot().AutoWidth()
                        [ SNew(SSeparator).Orientation(Orient_Vertical).Thickness(1.0f) ]

                    + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SBox).WidthOverride(300.0f)
                            [ BuildInspectorRail() ]
                        ]
                ]
        ]
        ]
    ];

    // start in the "no subsystem" state; DoRefresh_OverviewStats flips the pills on state change
    _PillSubsystemLive->SetVisibility(EVisibility::Collapsed);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkObjectPoolingDebuggerWindow::
    BuildToolbar()
    -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(CkStyle::Bg1())
        .Padding(FMargin(CkStyle::SpaceM, CkStyle::SpaceS))
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [ SNew(SCkDebug_WorldSelector, _WorldModel).ShowHeaderLabel(false) ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
                [
                    SAssignNew(_PillSubsystemLive, SCkDebug_StatusPill)
                        .Text(FText::FromString(TEXT("Subsystem Live")))
                        .Tone(ECk_Tone::Ok)
                ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
                [
                    SAssignNew(_PillNoSubsystem, SCkDebug_StatusPill)
                        .Text(FText::FromString(TEXT("No Subsystem — start PIE")))
                        .Tone(ECk_Tone::Warn)
                ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
                [
                    SAssignNew(_PinnedUniqueText, STextBlock)
                        .Font(ck_object_pooling_debugger_window::MonoFont(CkStyle::FontSizeSmall()))
                        .ColorAndOpacity(CkStyle::TextDim())
                ]

            + SHorizontalBox::Slot().FillWidth(1.0f)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SAssignNew(_SearchBar, SCkDebug_DualSearchBar)
                        .FilterHintText(FText::FromString(TEXT("Filter pools…")))
                        .OnFilterTextChanged_Lambda([this](const FString& InText)
                        {
                            _FilterText = InText;
                            DoRefresh_VisibleItems();
                        })
                        .OnHighlightTextChanged_Lambda([this](const FString& InText)
                        {
                            _HighlightText = InText;
                        })
                ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SButton)
                        .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                        .ContentPadding(FMargin(CkStyle::SpaceS, 2.0f))
                        .ToolTipText(FText::FromString(TEXT("Write a JSON report of every pool (counters + params) "
                            "to Saved/CkReports/ — for offline tuning of undersized-prewarm or over-allocated pools.")))
                        .OnClicked(this, &SCkObjectPoolingDebuggerWindow::DoExport_JsonReport)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Export JSON")))
                                .Font(ck_object_pooling_debugger_window::BoldFont(CkStyle::FontSizeMicro()))
                                .ColorAndOpacity(CkStyle::Accent())
                        ]
                ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
                [ SNew(SCkDebugger_RefreshControls).WindowId(SCkObjectPoolingDebuggerWindow::WindowId) ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkObjectPoolingDebuggerWindow::
    BuildOverviewStrip()
    -> TSharedRef<SWidget>
{
    const auto MakeTile = [](TSharedPtr<SCkDebug_StatPair>& OutStat, const FString& InLabel, const FLinearColor& InColor, const FString& InTooltip = {}) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
            .BorderBackgroundColor(CkStyle::Bg2())
            .Padding(FMargin(CkStyle::SpaceM, CkStyle::SpaceS))
            .ToolTipText(FText::FromString(InTooltip))
            [
                SAssignNew(OutStat, SCkDebug_StatPair)
                    .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
                    .Value(FText::FromString(TEXT("0")))
                    .Label(FText::FromString(InLabel))
                    .ValueColor(InColor)
            ];
    };

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(CkStyle::Bg1())
        .Padding(FMargin(CkStyle::SpaceM, CkStyle::SpaceS))
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [ MakeTile(_StatPools,   TEXT("POOLS"),          CkStyle::TextStrong()) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [ MakeTile(_StatLive,    TEXT("LIVE INSTANCES"), CkStyle::TextStrong()) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [ MakeTile(_StatInUse,   TEXT("IN USE"),         CkStyle::Info()) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [ MakeTile(_StatParked,  TEXT("PARKED"),         CkStyle::Ok()) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [ MakeTile(_StatMisses,  TEXT("MISSES"),         CkStyle::Warn(),
                    TEXT("An Acquire found the pool's free list empty (or only stale GC'd slots) and had to "
                         "spawn a brand-new instance instead of recycling one — under Grow policy — or "
                         "returned null under Fail policy. Frequent misses on a pool mean its PrewarmCount / "
                         "GrowBatchCount is undersized for its real peak demand.")) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [ MakeTile(_StatHitRate, TEXT("HIT RATE"),       CkStyle::Accent(),
                    TEXT("Hits / (Hits + Misses). A Hit is an Acquire satisfied by popping an existing "
                         "recycled instance off the free list — no new UObject allocation.")) ]

            + SHorizontalBox::Slot().FillWidth(1.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
                    .BorderBackgroundColor(CkStyle::Bg2())
                    .Padding(FMargin(CkStyle::SpaceM, CkStyle::SpaceS))
                    [
                        SNew(SVerticalBox)

                        + SVerticalBox::Slot().AutoHeight()
                            [
                                SNew(SHorizontalBox)

                                + SHorizontalBox::Slot().FillWidth(1.0f)
                                    [
                                        SNew(STextBlock)
                                            .Text(FText::FromString(TEXT("IN USE — ALL POOLS")))
                                            .Font(ck_object_pooling_debugger_window::BoldFont(CkStyle::FontSizeMicro()))
                                            .ColorAndOpacity(CkStyle::TextMute())
                                    ]

                                + SHorizontalBox::Slot().AutoWidth()
                                    [
                                        SAssignNew(_HeroNowText, STextBlock)
                                            .Font(ck_object_pooling_debugger_window::MonoFont(CkStyle::FontSizeBody()))
                                            .ColorAndOpacity(CkStyle::Info())
                                    ]
                            ]

                        + SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 2.0f, 0.0f, 0.0f)
                            [
                                SNew(SCkDebug_Sparkline)
                                    .Samples(_TotalInUseSamples)
                                    .Color(CkStyle::Info())
                                    .FillOpacity(0.25f)
                                    .DesiredSize(FVector2D(240.0f, 30.0f))
                            ]
                    ]
                ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkObjectPoolingDebuggerWindow::
    MakeSortableHeader(
        const FString& InLabel,
        ESortColumn InColumn,
        float InFillWidth,
        bool InRightAlign,
        const FString& InTooltip)
    -> TSharedRef<SWidget>
{
    return SNew(SButton)
        .ButtonStyle(FAppStyle::Get(), "SimpleButton")
        .ContentPadding(FMargin(CkStyle::SpaceS, 2.0f))
        .HAlign(InRightAlign ? HAlign_Right : HAlign_Left)
        .ToolTipText(FText::FromString(InTooltip))
        .OnClicked(this, &SCkObjectPoolingDebuggerWindow::OnSortColumnClicked, InColumn)
        [
            SNew(STextBlock)
                .Font(ck_object_pooling_debugger_window::BoldFont(CkStyle::FontSizeMicro()))
                .ColorAndOpacity_Lambda([this, InColumn]() -> FSlateColor
                {
                    return _SortColumn == InColumn ? CkStyle::Accent() : CkStyle::TextDim();
                })
                .Text_Lambda([this, InLabel, InColumn]() -> FText
                {
                    if (_SortColumn != InColumn)
                    { return FText::FromString(InLabel); }
                    return FText::FromString(InLabel + (_SortAscending ? TEXT(" ▲") : TEXT(" ▼")));
                })
        ];
}

auto
    SCkObjectPoolingDebuggerWindow::
    BuildHeaderRow()
    -> TSharedRef<SWidget>
{
    using namespace ck_object_pooling_debugger_window;

    const auto StaticHeader = [](const FString& InLabel) -> TSharedRef<SWidget>
    {
        return SNew(STextBlock)
            .Text(FText::FromString(InLabel))
            .Font(BoldFont(CkStyle::FontSizeMicro()))
            .ColorAndOpacity(CkStyle::TextDim())
            .Margin(FMargin(CkStyle::SpaceS, 2.0f));
    };

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(CkStyle::Bg2())
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(Col_Class)     [ MakeSortableHeader(TEXT("CLASS / ARCHETYPE"), ESortColumn::Class, Col_Class, false) ]
            + SHorizontalBox::Slot().FillWidth(Col_Policy)    [ StaticHeader(TEXT("POLICY")) ]
            + SHorizontalBox::Slot().FillWidth(Col_Occupancy) [ StaticHeader(TEXT("OCCUPANCY")) ]
            + SHorizontalBox::Slot().FillWidth(Col_Trend)     [ StaticHeader(TEXT("TREND")) ]
            + SHorizontalBox::Slot().FillWidth(Col_Num)       [ MakeSortableHeader(TEXT("FREE"),    ESortColumn::Free,    Col_Num, true) ]
            + SHorizontalBox::Slot().FillWidth(Col_Num)       [ MakeSortableHeader(TEXT("IN USE"),  ESortColumn::InUse,   Col_Num, true) ]
            + SHorizontalBox::Slot().FillWidth(Col_Num)       [ MakeSortableHeader(TEXT("LIVE"),    ESortColumn::Live,    Col_Num, true) ]
            + SHorizontalBox::Slot().FillWidth(Col_Num)       [ MakeSortableHeader(TEXT("HIGH"),    ESortColumn::High,    Col_Num, true) ]
            + SHorizontalBox::Slot().FillWidth(Col_Num)       [ MakeSortableHeader(TEXT("HIT %"),   ESortColumn::HitRate, Col_Num, true,
                TEXT("Hits / (Hits + Misses). A Hit is an Acquire satisfied by popping an existing recycled "
                     "instance off the free list — no new UObject allocation.")) ]
            + SHorizontalBox::Slot().FillWidth(Col_Num)       [ MakeSortableHeader(TEXT("MISS"),    ESortColumn::Miss,    Col_Num, true,
                TEXT("An Acquire found the free list empty (or only stale GC'd slots) and had to spawn a "
                     "brand-new instance instead of recycling one — under Grow policy — or returned null "
                     "under Fail policy. Frequent misses mean PrewarmCount / GrowBatchCount is undersized.")) ]
            + SHorizontalBox::Slot().FillWidth(Col_Num)       [ MakeSortableHeader(TEXT("PREWARM"), ESortColumn::Prewarm, Col_Num, true) ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkObjectPoolingDebuggerWindow::
    OnGenerateRow(
        ItemPtr InItem,
        const TSharedRef<STableViewBase>& InTable)
    -> TSharedRef<ITableRow>
{
    using namespace ck_object_pooling_debugger_window;

    const auto& History = Get_HistoryFor(InItem->Get_PoolKeyString());

    const auto NumCell = [InItem](TFunction<FText(const FCkObjectPoolingDebugger_PoolRow&)> InGetter,
                                  TFunction<FLinearColor(const FCkObjectPoolingDebugger_PoolRow&)> InColor) -> TSharedRef<SWidget>
    {
        return SNew(STextBlock)
            .Font(MonoFont(CkStyle::FontSizeSmall()))
            .Margin(FMargin(CkStyle::SpaceS, 2.0f))
            .Justification(ETextJustify::Right)
            .Text_Lambda([InItem, InGetter]() { return InGetter(*InItem); })
            .ColorAndOpacity_Lambda([InItem, InColor]() -> FSlateColor { return InColor(*InItem); });
    };

    const auto PlainColor = [](const FCkObjectPoolingDebugger_PoolRow&) { return CkStyle::Text(); };

    return SNew(STableRow<ItemPtr>, InTable)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("NoBorder"))
            .ColorAndOpacity_Lambda([this, InItem]() -> FLinearColor
            {
                if (_HighlightText.IsEmpty() || Matches(*InItem, _HighlightText))
                { return FLinearColor::White; }
                return FLinearColor(1.0f, 1.0f, 1.0f, 0.32f);
            })
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot().FillWidth(Col_Class).VAlign(VAlign_Center)
                    [
                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().AutoWidth()
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(InItem->DisplayClassName))
                                    .Font(MonoFont(CkStyle::FontSizeSmall()))
                                    .ColorAndOpacity(CkStyle::EntityId())
                                    .Margin(FMargin(CkStyle::SpaceS, 2.0f))
                            ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                            [
                                SNew(SBorder)
                                .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
                                .BorderBackgroundColor(CkStyle::Bg3())
                                .Padding(FMargin(4.0f, 0.0f))
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(InItem->ArchetypeName))
                                        .Font(MonoFont(CkStyle::FontSizeMicro()))
                                        .ColorAndOpacity(InItem->IsArchetypeCDO ? CkStyle::TextMute() : CkStyle::Warn())
                                ]
                            ]
                    ]

                + SHorizontalBox::Slot().FillWidth(Col_Policy).VAlign(VAlign_Center)
                    [
                        SNew(SCkDebug_StatusPill)
                            .Text(FText::FromString(InItem->IsRecyclePolicy ? TEXT("Recycle") : TEXT("Pin-Unique")))
                            .Tone(InItem->IsRecyclePolicy ? ECk_Tone::Info : ECk_Tone::Neutral)
                            .ShowDot(false)
                    ]

                + SHorizontalBox::Slot().FillWidth(Col_Occupancy).VAlign(VAlign_Center).Padding(CkStyle::SpaceS, 0.0f)
                    [
                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                            [
                                SNew(SOccupancyBar)
                                    .Item(InItem)
                                    .SharedMax(_SharedMaxLive)
                            ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
                            [
                                SNew(STextBlock)
                                    .Font(MonoFont(CkStyle::FontSizeMicro()))
                                    .ColorAndOpacity(CkStyle::TextMute())
                                    .Text_Lambda([InItem]()
                                    {
                                        return FText::FromString(ck::Format_UE(TEXT("{}/{}"), InItem->NumInUse, InItem->NumLiveInstances));
                                    })
                            ]
                    ]

                + SHorizontalBox::Slot().FillWidth(Col_Trend).VAlign(VAlign_Center).Padding(CkStyle::SpaceS, 1.0f)
                    [
                        SNew(SCkDebug_Sparkline)
                            .Samples(History.InUseSamples)
                            .Color(CkStyle::Info())
                            .DesiredSize(FVector2D(80.0f, 16.0f))
                    ]

                + SHorizontalBox::Slot().FillWidth(Col_Num).VAlign(VAlign_Center)
                    [ NumCell([](const auto& R) { return FText::AsNumber(R.NumFree); }, PlainColor) ]

                + SHorizontalBox::Slot().FillWidth(Col_Num).VAlign(VAlign_Center)
                    [ NumCell([](const auto& R) { return FText::AsNumber(R.NumInUse); },
                              [](const auto&) { return CkStyle::Info(); }) ]

                + SHorizontalBox::Slot().FillWidth(Col_Num).VAlign(VAlign_Center)
                    [ NumCell([](const auto& R) { return FText::AsNumber(R.NumLiveInstances); }, PlainColor) ]

                + SHorizontalBox::Slot().FillWidth(Col_Num).VAlign(VAlign_Center)
                    [ NumCell([](const auto& R) { return FText::AsNumber(R.HighWaterMark); },
                              [](const auto&) { return CkStyle::Accent(); }) ]

                + SHorizontalBox::Slot().FillWidth(Col_Num).VAlign(VAlign_Center)
                    [ NumCell([](const auto& R)
                              {
                                  if (R.Get_TotalAcquires() == 0)
                                  { return FText::FromString(TEXT("—")); }
                                  return FText::FromString(ck::Format_UE(TEXT("{:.1f}"), R.Get_HitRate() * 100.0f));
                              },
                              [](const auto& R) { return Get_HitRateColor(R); }) ]

                + SHorizontalBox::Slot().FillWidth(Col_Num).VAlign(VAlign_Center)
                    [ NumCell([](const auto& R) { return FText::AsNumber(R.NumMisses); },
                              [](const auto& R) { return R.NumMisses > 0 ? CkStyle::Warn() : CkStyle::TextMute(); }) ]

                + SHorizontalBox::Slot().FillWidth(Col_Num).VAlign(VAlign_Center)
                    [ NumCell([](const auto& R)
                              {
                                  return R.NumPrewarmRemaining > 0
                                      ? FText::FromString(ck::Format_UE(TEXT("{} pre"), R.NumPrewarmRemaining))
                                      : FText::FromString(TEXT("—"));
                              },
                              [](const auto& R) { return R.NumPrewarmRemaining > 0 ? CkStyle::Warn() : CkStyle::TextMute(); }) ]
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkObjectPoolingDebuggerWindow::
    BuildInspectorRail()
    -> TSharedRef<SWidget>
{
    using namespace ck_object_pooling_debugger_window;

    const auto SelectedText = [this](TFunction<FText(const FCkObjectPoolingDebugger_PoolRow&)> InGetter) -> TAttribute<FText>
    {
        return TAttribute<FText>::CreateLambda([this, InGetter]()
        {
            return _SelectedItem.IsValid() ? InGetter(*_SelectedItem) : FText::FromString(TEXT("—"));
        });
    };

    const auto ParamRow = [&](const FString& InKey, TFunction<FText(const FCkObjectPoolingDebugger_PoolRow&)> InGetter,
                              const FLinearColor& InValueColor) -> TSharedRef<SWidget>
    {
        return SNew(SCkDebug_KeyValueRow)
            .KeyText(FText::FromString(InKey))
            .Tone(ECkDebug_KeyValueTone::Custom)
            .CustomValueColor(InValueColor)
            .ValueText(SelectedText(InGetter));
    };

    const auto EnumColor = CkStyle::Value_Enum();
    const auto NumColor = CkStyle::Value_Numeric();

    const auto MakeRateCard = [](TSharedPtr<SCkDebug_StatPair>& OutStat, const FString& InLabel, const FString& InTooltip = {}) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
            .BorderBackgroundColor(CkStyle::Bg2())
            .Padding(FMargin(CkStyle::SpaceS))
            .ToolTipText(FText::FromString(InTooltip))
            [
                SAssignNew(OutStat, SCkDebug_StatPair)
                    .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
                    .Value(FText::FromString(TEXT("—")))
                    .Label(FText::FromString(InLabel))
            ];
    };

    return SNew(SScrollBox)

        + SScrollBox::Slot()
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, CkStyle::SpaceM, CkStyle::SpaceM, 0.0f)
                    [
                        SAssignNew(_SelectedNameText, STextBlock)
                            .Font(BoldFont(CkStyle::FontSizeBody()))
                            .ColorAndOpacity(CkStyle::EntityId())
                            .Text(FText::FromString(TEXT("Select a pool")))
                    ]

                + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, 2.0f, CkStyle::SpaceM, CkStyle::SpaceM)
                    [
                        SAssignNew(_SelectedSubText, STextBlock)
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", CkStyle::FontSizeSmall()))
                            .ColorAndOpacity(CkStyle::TextMute())
                            .Text(FText::FromString(TEXT("click a row to inspect")))
                    ]

                + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SCkDebug_InspectorPanel)
                            .Title(FText::FromString(TEXT("Occupancy — history")))
                            .Body()
                            [
                                SNew(SVerticalBox)

                                + SVerticalBox::Slot().AutoHeight()
                                    [ SAssignNew(_InspectorGraphSlot, SBox).HeightOverride(96.0f) ]

                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
                                    [
                                        SNew(SHorizontalBox)

                                        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                                            [
                                                SNew(STextBlock)
                                                    .Text(FText::FromString(TEXT("■ in use")))
                                                    .Font(MonoFont(CkStyle::FontSizeMicro()))
                                                    .ColorAndOpacity(CkStyle::Info())
                                            ]

                                        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                                            [
                                                SNew(STextBlock)
                                                    .Text(FText::FromString(TEXT("■ live")))
                                                    .Font(MonoFont(CkStyle::FontSizeMicro()))
                                                    .ColorAndOpacity(CkStyle::OverlayOf(CkStyle::Ok(), 0.7f))
                                            ]

                                        + SHorizontalBox::Slot().AutoWidth()
                                            [
                                                SNew(STextBlock)
                                                    .Font(MonoFont(CkStyle::FontSizeMicro()))
                                                    .ColorAndOpacity(CkStyle::Accent())
                                                    .Text_Lambda([this]()
                                                    {
                                                        return _SelectedItem.IsValid()
                                                            ? FText::FromString(ck::Format_UE(TEXT("- - high-water {}"), _SelectedItem->HighWaterMark))
                                                            : FText::FromString(TEXT("- - high-water"));
                                                    })
                                            ]
                                    ]
                            ]
                    ]

                + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SCkDebug_InspectorPanel)
                            .Title(FText::FromString(TEXT("Churn")))
                            .Body()
                            [
                                SNew(SHorizontalBox)

                                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                                    [ MakeRateCard(_RateAcquires, TEXT("ACQUIRES/S")) ]

                                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                                    [ MakeRateCard(_RateReleases, TEXT("RELEASES/S")) ]

                                + SHorizontalBox::Slot().FillWidth(1.0f)
                                    [ MakeRateCard(_RateHitRate, TEXT("HIT RATE"),
                                        TEXT("Hits / (Hits + Misses) for this pool. A Hit is an Acquire "
                                             "satisfied by popping an existing recycled instance off the "
                                             "free list — no new UObject allocation.")) ]
                            ]
                    ]

                + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SCkDebug_InspectorPanel)
                            .Title(FText::FromString(TEXT("Pool Params")))
                            .Body()
                            [
                                SNew(SVerticalBox)

                                + SVerticalBox::Slot().AutoHeight()
                                    [ ParamRow(TEXT("RecyclePolicy"), [](const auto& R)
                                        { return FText::FromString(R.IsRecyclePolicy ? TEXT("Recycle") : TEXT("DestroyOnRelease")); }, EnumColor) ]

                                + SVerticalBox::Slot().AutoHeight()
                                    [ ParamRow(TEXT("CapacityPolicy"), [](const auto& R)
                                        {
                                            return FText::FromString(R.IsBoundedCapacity
                                                ? ck::Format_UE(TEXT("Bounded ({})"), R.MaxSize)
                                                : FString(TEXT("Unbounded")));
                                        }, EnumColor) ]

                                + SVerticalBox::Slot().AutoHeight()
                                    [ ParamRow(TEXT("ExhaustionPolicy"), [](const auto& R)
                                        { return FText::FromString(R.IsGrowOnExhaustion ? TEXT("Grow") : TEXT("Fail")); }, EnumColor) ]

                                + SVerticalBox::Slot().AutoHeight()
                                    [ ParamRow(TEXT("GrowBatchCount"), [](const auto& R)
                                        { return FText::AsNumber(R.GrowBatchCount); }, NumColor) ]

                                + SVerticalBox::Slot().AutoHeight()
                                    [ ParamRow(TEXT("PrewarmCount"), [](const auto& R)
                                        { return FText::AsNumber(R.PrewarmCount); }, NumColor) ]

                                + SVerticalBox::Slot().AutoHeight()
                                    [ ParamRow(TEXT("PrewarmBudget/Tick"), [](const auto& R)
                                        { return FText::AsNumber(R.PrewarmBudgetPerTick); }, NumColor) ]
                            ]
                    ]

                + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SCkDebug_InspectorPanel)
                            .Title(FText::FromString(TEXT("Lifecycle")))
                            .Body()
                            [
                                SNew(SVerticalBox)

                                + SVerticalBox::Slot().AutoHeight()
                                    [ ParamRow(TEXT("Total acquires"), [](const auto& R)
                                        { return FText::AsNumber(R.Get_TotalAcquires()); }, NumColor) ]

                                + SVerticalBox::Slot().AutoHeight()
                                    [ ParamRow(TEXT("Hits / Misses"), [](const auto& R)
                                        { return FText::FromString(ck::Format_UE(TEXT("{} / {}"), R.NumHits, R.NumMisses)); }, NumColor) ]

                                + SVerticalBox::Slot().AutoHeight()
                                    [ ParamRow(TEXT("Live instances"), [](const auto& R)
                                        { return FText::AsNumber(R.NumLiveInstances); }, NumColor) ]

                                + SVerticalBox::Slot().AutoHeight()
                                    [ ParamRow(TEXT("High-water mark"), [](const auto& R)
                                        { return FText::AsNumber(R.HighWaterMark); }, CkStyle::Accent()) ]

                                + SVerticalBox::Slot().AutoHeight()
                                    [ ParamRow(TEXT("Prewarm remaining"), [](const auto& R)
                                        { return FText::AsNumber(R.NumPrewarmRemaining); }, NumColor) ]
                            ]
                    ]
            ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkObjectPoolingDebuggerWindow::
    BuildStatusBar()
    -> TSharedRef<SWidget>
{
    using namespace ck_object_pooling_debugger_window;

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(CkStyle::Bg1())
        .Padding(FMargin(CkStyle::SpaceM, 3.0f))
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().AutoWidth()
                [
                    SAssignNew(_GatherMsText, STextBlock)
                        .Font(MonoFont(CkStyle::FontSizeMicro()))
                        .ColorAndOpacity(CkStyle::TextMute())
                ]

            + SHorizontalBox::Slot().FillWidth(1.0f)

            + SHorizontalBox::Slot().AutoWidth()
                [
                    SAssignNew(_SortDescText, STextBlock)
                        .Font(MonoFont(CkStyle::FontSizeMicro()))
                        .ColorAndOpacity(CkStyle::TextMute())
                ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkObjectPoolingDebuggerWindow::
    OnSelectionChanged(
        ItemPtr InItem,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    _SelectedItem = InItem;
    _SelectedKey = InItem.IsValid() ? InItem->Get_PoolKeyString() : FString{};

    if (InItem.IsValid())
    {
        _SelectedNameText->SetText(FText::FromString(InItem->DisplayClassName));
        _SelectedSubText->SetText(FText::FromString(ck::Format_UE(TEXT("archetype {} · {}"),
            InItem->ArchetypeName,
            InItem->IsRecyclePolicy ? TEXT("recycled pool") : TEXT("pinned-unique"))));
    }
    else
    {
        _SelectedNameText->SetText(FText::FromString(TEXT("Select a pool")));
        _SelectedSubText->SetText(FText::FromString(TEXT("click a row to inspect")));
    }

    DoRebuild_InspectorGraph();
}

auto
    SCkObjectPoolingDebuggerWindow::
    DoRebuild_InspectorGraph()
    -> void
{
    if (NOT _SelectedItem.IsValid())
    {
        _InspectorGraphSlot->SetContent(SNullWidget::NullWidget);
        return;
    }

    const auto& History = Get_HistoryFor(_SelectedKey);
    const auto SelectedItem = _SelectedItem;

    _InspectorGraphSlot->SetContent(
        SNew(SCkDebug_Sparkline)
            .Samples(History.InUseSamples)
            .BandSamples(History.LiveSamples)
            .Color(CkStyle::Info())
            .BandColor(CkStyle::Ok())
            .FillOpacity(0.30f)
            .BandFillOpacity(0.15f)
            .MarkerColor(CkStyle::Accent())
            .MarkerValue_Lambda([SelectedItem]() -> TOptional<float>
            {
                return TOptional<float>{static_cast<float>(SelectedItem->HighWaterMark)};
            })
            .DesiredSize(FVector2D(270.0f, 96.0f)));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkObjectPoolingDebuggerWindow::
    OnSortColumnClicked(
        ESortColumn InColumn)
    -> FReply
{
    if (_SortColumn == InColumn)
    { _SortAscending = NOT _SortAscending; }
    else
    {
        _SortColumn = InColumn;
        _SortAscending = InColumn == ESortColumn::Class;
    }

    DoRefresh_VisibleItems();
    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkObjectPoolingDebuggerWindow::
    DoExport_JsonReport()
    -> FReply
{
    const auto ShowToast = [](const FString& InMessage, bool InSuccess, const FString& InRevealDir)
    {
        auto Info = FNotificationInfo{FText::FromString(InMessage)};
        Info.bFireAndForget = true;
        Info.ExpireDuration = 6.0f;

        if (NOT InRevealDir.IsEmpty())
        {
            Info.Hyperlink = FSimpleDelegate::CreateLambda([InRevealDir]()
            { FPlatformProcess::ExploreFolder(*InRevealDir); });
            Info.HyperlinkText = FText::FromString(TEXT("Show in folder"));
        }

        if (const auto Item = FSlateNotificationManager::Get().AddNotification(Info);
            Item.IsValid())
        { Item->SetCompletionState(InSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail); }
    };

    const auto Snapshot = FCkObjectPoolingDebugger_Snapshot::Gather(_WorldModel->Get_SelectedWorld());

    if (NOT Snapshot.HasSubsystem)
    {
        ShowToast(TEXT("No pooling subsystem in the selected world — start PIE first."), false, {});
        return FReply::Handled();
    }

    const auto Dir = FPaths::ProjectSavedDir() / TEXT("CkReports");
    IFileManager::Get().MakeDirectory(*Dir, true);

    const auto FilePath = Dir / ck::Format_UE(TEXT("CkObjectPooling_{}.json"),
        FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));

    if (FFileHelper::SaveStringToFile(Snapshot.ToJsonString(), *FilePath))
    { ShowToast(ck::Format_UE(TEXT("Pool report written to {}"), FilePath), true, FPaths::ConvertRelativePathToFull(Dir)); }
    else
    { ShowToast(ck::Format_UE(TEXT("Failed to write pool report to {}"), FilePath), false, {}); }

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkObjectPoolingDebuggerWindow::
    DoRefresh_VisibleItems()
    -> void
{
    using namespace ck_object_pooling_debugger_window;

    _VisibleItems.Reset(_Items.Num());
    for (const auto& Item : _Items)
    {
        if (Matches(*Item, _FilterText))
        { _VisibleItems.Emplace(Item); }
    }

    const auto Compare = [this](const ItemPtr& A, const ItemPtr& B) -> bool
    {
        const auto Less = [&]() -> bool
        {
            switch (_SortColumn)
            {
                case ESortColumn::Free:    return A->NumFree < B->NumFree;
                case ESortColumn::InUse:   return A->NumInUse < B->NumInUse;
                case ESortColumn::Live:    return A->NumLiveInstances < B->NumLiveInstances;
                case ESortColumn::High:    return A->HighWaterMark < B->HighWaterMark;
                case ESortColumn::HitRate: return A->Get_HitRate() < B->Get_HitRate();
                case ESortColumn::Miss:    return A->NumMisses < B->NumMisses;
                case ESortColumn::Prewarm: return A->NumPrewarmRemaining < B->NumPrewarmRemaining;
                case ESortColumn::Class:
                default:
                    return A->ClassName == B->ClassName ? A->ArchetypeName < B->ArchetypeName : A->ClassName < B->ClassName;
            }
        }();

        return _SortAscending ? Less : NOT Less;
    };
    _VisibleItems.StableSort(Compare);

    if (_ListView.IsValid())
    { _ListView->RequestListRefresh(); }

    if (_SortDescText.IsValid())
    {
        static const TMap<ESortColumn, FString> ColumnNames = {
            {ESortColumn::Class, TEXT("Class")}, {ESortColumn::Free, TEXT("Free")},
            {ESortColumn::InUse, TEXT("In Use")}, {ESortColumn::Live, TEXT("Live")},
            {ESortColumn::High, TEXT("High")}, {ESortColumn::HitRate, TEXT("Hit %")},
            {ESortColumn::Miss, TEXT("Miss")}, {ESortColumn::Prewarm, TEXT("Prewarm")}};
        _SortDescText->SetText(FText::FromString(ck::Format_UE(TEXT("sorted by {} {} · {} of {} pool(s)"),
            ColumnNames[_SortColumn], _SortAscending ? TEXT("▲") : TEXT("▼"),
            _VisibleItems.Num(), _Items.Num())));
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkObjectPoolingDebuggerWindow::
    Get_HistoryFor(
        const FString& InPoolKey)
    -> FPoolHistory&
{
    if (auto* Existing = _Histories.Find(InPoolKey))
    { return *Existing; }

    auto& History = _Histories.Emplace(InPoolKey);
    History.InUseSamples = MakeShared<TArray<float>>();
    History.LiveSamples = MakeShared<TArray<float>>();
    return History;
}

auto
    SCkObjectPoolingDebuggerWindow::
    Get_SelectedHistory() const
    -> const FPoolHistory*
{
    return _Histories.Find(_SelectedKey);
}

auto
    SCkObjectPoolingDebuggerWindow::
    DoSample_Histories(
        const FCkObjectPoolingDebugger_Snapshot& InSnapshot,
        double InCurrentTime)
    -> void
{
    using namespace ck_object_pooling_debugger_window;

    auto TotalInUse = 0;
    auto MaxLive = 1.0f;

    for (const auto& Pool : InSnapshot.Pools)
    {
        TotalInUse += Pool.NumInUse;
        MaxLive = FMath::Max(MaxLive, static_cast<float>(FMath::Max(Pool.NumLiveInstances, Pool.HighWaterMark)));

        auto& History = Get_HistoryFor(Pool.Get_PoolKeyString());
        Push_Sample(History.InUseSamples, Pool.NumInUse);
        Push_Sample(History.LiveSamples, Pool.NumLiveInstances);

        const auto DeltaTime = InCurrentTime - History.LastSampleTime;
        if (History.LastSampleTime > 0.0 && DeltaTime > KINDA_SMALL_NUMBER)
        {
            const auto DeltaAcquires = Pool.Get_TotalAcquires() - History.LastTotalAcquires;
            const auto DeltaInUse = Pool.NumInUse - History.LastNumInUse;
            History.AcquiresPerSec = FMath::Max(0.0f, static_cast<float>(DeltaAcquires / DeltaTime));
            History.ReleasesPerSec = FMath::Max(0.0f, static_cast<float>((DeltaAcquires - DeltaInUse) / DeltaTime));
        }
        History.LastTotalAcquires = Pool.Get_TotalAcquires();
        History.LastNumInUse = Pool.NumInUse;
        History.LastSampleTime = InCurrentTime;
    }

    *_SharedMaxLive = MaxLive;

    Push_Sample(_TotalInUseSamples, TotalInUse);
    _HeroNowText->SetText(FText::AsNumber(TotalInUse));
}

auto
    SCkObjectPoolingDebuggerWindow::
    DoRefresh_OverviewStats(
        const FCkObjectPoolingDebugger_Snapshot& InSnapshot)
    -> void
{
    using namespace ck_object_pooling_debugger_window;

    auto TotalLive = 0;
    auto TotalInUse = 0;
    auto TotalFree = 0;
    auto TotalHits = 0;
    auto TotalMisses = 0;
    for (const auto& Pool : InSnapshot.Pools)
    {
        TotalLive += Pool.NumLiveInstances;
        TotalInUse += Pool.NumInUse;
        TotalFree += Pool.NumFree;
        TotalHits += Pool.NumHits;
        TotalMisses += Pool.NumMisses;
    }

    _StatPools->SetValue(FText::AsNumber(InSnapshot.Pools.Num()));
    _StatLive->SetValue(FText::AsNumber(TotalLive));
    _StatInUse->SetValue(FText::AsNumber(TotalInUse));
    _StatParked->SetValue(FText::AsNumber(TotalFree));
    _StatMisses->SetValue(FText::AsNumber(TotalMisses));

    const auto TotalAcquires = TotalHits + TotalMisses;
    _StatHitRate->SetValue(TotalAcquires > 0
        ? FText::FromString(ck::Format_UE(TEXT("{:.1f}%"), 100.0f * TotalHits / TotalAcquires))
        : FText::FromString(TEXT("—")));

    _PinnedUniqueText->SetText(FText::FromString(
        ck::Format_UE(TEXT("{} pinned-unique"), InSnapshot.NumPinnedUnique)));

    if (_HasSubsystem != InSnapshot.HasSubsystem)
    {
        _HasSubsystem = InSnapshot.HasSubsystem;
        _PillSubsystemLive->SetVisibility(_HasSubsystem ? EVisibility::Visible : EVisibility::Collapsed);
        _PillNoSubsystem->SetVisibility(_HasSubsystem ? EVisibility::Collapsed : EVisibility::Visible);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkObjectPoolingDebuggerWindow::
    MakeSignature(
        const FCkObjectPoolingDebugger_Snapshot& InSnapshot)
    -> FString
{
    // pool SET identity only (class+archetype) — NOT stats, so scroll survives a stats-only refresh
    auto Sig = FString{};
    Sig.Reserve(InSnapshot.Pools.Num() * 32);
    for (const auto& Pool : InSnapshot.Pools)
    {
        Sig += Pool.ClassName;
        Sig += TEXT("|");
        Sig += Pool.ArchetypeName;
        Sig += TEXT(";");
    }
    return Sig;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkObjectPoolingDebuggerWindow::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    using namespace ck_object_pooling_debugger_window;

    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    _WorldModel->Ensure_AutoSelect();

    const auto GatherStart = FPlatformTime::Seconds();
    const auto Snapshot = FCkObjectPoolingDebugger_Snapshot::Gather(_WorldModel->Get_SelectedWorld());
    _LastGatherMs = (FPlatformTime::Seconds() - GatherStart) * 1000.0;

    if (const auto Signature = MakeSignature(Snapshot);
        Signature != _LastSignature)
    {
        // pool set changed — rebuild the item objects, prune dead histories, restore selection
        _LastSignature = Signature;

        _Items.Reset(Snapshot.Pools.Num());
        for (const auto& Pool : Snapshot.Pools)
        { _Items.Emplace(MakeShared<FCkObjectPoolingDebugger_PoolRow>(Pool)); }

        auto LiveKeys = TSet<FString>{};
        for (const auto& Item : _Items)
        { LiveKeys.Add(Item->Get_PoolKeyString()); }
        for (auto It = _Histories.CreateIterator(); It; ++It)
        {
            if (NOT LiveKeys.Contains(It.Key()))
            { It.RemoveCurrent(); }
        }

        DoRefresh_VisibleItems();

        if (NOT _SelectedKey.IsEmpty())
        {
            const auto* Restored = _Items.FindByPredicate([this](const ItemPtr& InItem)
            { return InItem->Get_PoolKeyString() == _SelectedKey; });

            _ListView->ClearSelection();
            if (Restored != nullptr)
            { _ListView->SetSelection(*Restored, ESelectInfo::Direct); }
            else
            { OnSelectionChanged(nullptr, ESelectInfo::Direct); }
        }
    }
    else
    {
        // same pools — update stats in place (both are sorted by class+archetype, so indices align)
        const auto Num = FMath::Min(_Items.Num(), Snapshot.Pools.Num());
        for (auto Index = 0; Index < Num; ++Index)
        { *_Items[Index] = Snapshot.Pools[Index]; }

        // re-apply the active metric sort so the order tracks the live values
        DoRefresh_VisibleItems();
    }

    DoSample_Histories(Snapshot, InCurrentTime);
    DoRefresh_OverviewStats(Snapshot);

    if (const auto* SelectedHistory = Get_SelectedHistory();
        SelectedHistory != nullptr && _SelectedItem.IsValid())
    {
        _RateAcquires->SetValue(FText::FromString(ck::Format_UE(TEXT("{:.1f}"), SelectedHistory->AcquiresPerSec)));
        _RateReleases->SetValue(FText::FromString(ck::Format_UE(TEXT("{:.1f}"), SelectedHistory->ReleasesPerSec)));
        _RateHitRate->SetValue(_SelectedItem->Get_TotalAcquires() > 0
            ? FText::FromString(ck::Format_UE(TEXT("{:.1f}%"), _SelectedItem->Get_HitRate() * 100.0f))
            : FText::FromString(TEXT("—")));
        _RateHitRate->SetValueColor(Get_HitRateColor(*_SelectedItem));
    }
    else
    {
        _RateAcquires->SetValue(FText::FromString(TEXT("—")));
        _RateReleases->SetValue(FText::FromString(TEXT("—")));
        _RateHitRate->SetValue(FText::FromString(TEXT("—")));
    }

    _GatherMsText->SetText(FText::FromString(ck::Format_UE(TEXT("gather: {:.2f} ms"), _LastGatherMs)));
}

// ====================================================================================================================
