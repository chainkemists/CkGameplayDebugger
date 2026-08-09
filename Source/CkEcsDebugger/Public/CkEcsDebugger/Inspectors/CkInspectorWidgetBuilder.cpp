#include "CkInspectorWidgetBuilder.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/String/CkFuzzyMatch_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_MeterBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Sparkline.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace ck_inspector_widget_builder
{
    // RowDensity as a DELTA on the builder's own base padding — same rule as the entity tree
    // (see CkDebuggerWidget_EntityTree.cpp): the absolute metric belongs to the surface, the
    // offset between options belongs to the axis. Comfortable => zero delta => today's rows.
    //
    // Slot padding is a TAttribute, which matters here more than anywhere: the inspector
    // panel's Tick policy forbids rebuilding widget structure, so a revision-poll rebuild was
    // never an option for this surface (CkDebuggerPanel_Inspector.cpp, POLICY comment).
    static auto Apply_RowDensity(const FMargin& InBase) -> FMargin
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

    // ----- Aligned numeric geometry -------------------------------------------
    constexpr auto NumericColumnWidth    = 70.0f;
    constexpr auto NumericColumnWidthMin = 44.0f;
    constexpr auto NumericColumnBudget   = 210.0f;
    constexpr auto NumericSeparator      = TEXT("  ");

    constexpr auto MeterWidth     = 90.0f;
    constexpr auto MeterHeight    = 5.0f;
    constexpr auto SparklineWidth = 110.0f;
    constexpr auto SparklineHeight = 22.0f;

    static auto Format_Numeric(float InValue) -> FText
    {
        auto Options = FNumberFormattingOptions{};
        Options.SetMinimumFractionalDigits(2);
        Options.SetMaximumFractionalDigits(2);
        return FText::AsNumber(InValue, &Options);
    }

    static auto Format_Percent(float InFraction) -> FText
    {
        return FText::AsPercent(FMath::Clamp(InFraction, 0.0f, 1.0f));
    }

    // Ring push gated on the engine frame: the row's value attribute is the sampling pump and Slate
    // may evaluate it several times per frame (desired-size pass plus paint).
    static auto Push_Sample(
        TArray<float>& InRing,
        uint64& InLastFrame,
        float InValue,
        int32 InCapacity) -> void
    {
        if (InLastFrame == GFrameCounter)
        { return; }

        InLastFrame = GFrameCounter;

        InRing.Add(InValue);

        const auto Excess = InRing.Num() - FMath::Max(1, InCapacity);
        if (Excess > 0)
        { InRing.RemoveAt(0, Excess, EAllowShrinking::No); }
    }

    static auto Make_NumericText(const TAttribute<FText>& InText, const FLinearColor& InColor) -> TSharedRef<SWidget>
    {
        return SNew(STextBlock)
            .Text(InText)
            .Font(CkStyle::MonoFont(CkStyle::FontSizeBody()))
            .ColorAndOpacity(FSlateColor{InColor});
    }

    static auto Resolve_Components(const TArray<TAttribute<FText>>& InComponents) -> TArray<FText>
    {
        auto Resolved = TArray<FText>{};
        Resolved.Reserve(InComponents.Num());
        for (const auto& Component : InComponents)
        { Resolved.Add(Component.Get()); }
        return Resolved;
    }
}

auto FCkInspectorWidgetBuilder::SetSelectionModel(TSharedPtr<FCkDebuggerModel_EntitySelection> InModel) -> FCkInspectorWidgetBuilder&
{
    SelectionModel = MoveTemp(InModel);
    return *this;
}

auto FCkInspectorWidgetBuilder::AddRow(
    const FText& InLabel,
    FValueGetter InValueGetter,
    const FLinearColor& InValueColor) -> FCkInspectorWidgetBuilder&
{
    const auto CapturedColor = InValueColor;

    Rows.Add(FRowDefinition
    {
        InLabel,
        MoveTemp(InValueGetter),
        [CapturedColor](const FCk_Handle&) { return CapturedColor; },
        nullptr,
        nullptr,
        false
    });

    return *this;
}

auto FCkInspectorWidgetBuilder::AddConditionalRow(
    const FText& InLabel,
    FValueGetter InValueGetter,
    FColorGetter InColorGetter) -> FCkInspectorWidgetBuilder&
{
    Rows.Add(FRowDefinition
    {
        InLabel,
        MoveTemp(InValueGetter),
        MoveTemp(InColorGetter),
        nullptr,
        nullptr,
        false
    });

    return *this;
}

auto FCkInspectorWidgetBuilder::AddClickableRow(
    const FText& InLabel,
    FValueGetter InValueGetter,
    const FLinearColor& InValueColor,
    FOnClicked InOnClicked) -> FCkInspectorWidgetBuilder&
{
    const auto CapturedColor = InValueColor;

    Rows.Add(FRowDefinition
    {
        InLabel,
        MoveTemp(InValueGetter),
        [CapturedColor](const FCk_Handle&) { return CapturedColor; },
        MoveTemp(InOnClicked),
        nullptr,
        false
    });

    return *this;
}

auto FCkInspectorWidgetBuilder::AddClickableRow(
    const FText& InLabel,
    FValueGetter InValueGetter,
    FColorGetter InColorGetter,
    FOnClicked InOnClicked) -> FCkInspectorWidgetBuilder&
{
    Rows.Add(FRowDefinition
    {
        InLabel,
        MoveTemp(InValueGetter),
        MoveTemp(InColorGetter),
        MoveTemp(InOnClicked),
        nullptr,
        false
    });

    return *this;
}

auto FCkInspectorWidgetBuilder::AddWidgetRow(
    const FText& InLabel,
    TSharedRef<SWidget> InWidget) -> FCkInspectorWidgetBuilder&
{
    Rows.Add(FRowDefinition{
        InLabel,
        nullptr,
        nullptr,
        nullptr,
        InWidget.ToSharedPtr(),
        false
    });
    return *this;
}

auto FCkInspectorWidgetBuilder::AddClickableWidgetRow(
    const FText& InLabel,
    TSharedRef<SWidget> InValueWidget,
    FOnClicked InOnClicked) -> FCkInspectorWidgetBuilder&
{
    Rows.Add(FRowDefinition{
        InLabel,
        nullptr,
        nullptr,
        MoveTemp(InOnClicked),
        InValueWidget.ToSharedPtr(),
        false
    });
    return *this;
}

auto FCkInspectorWidgetBuilder::MakeBadgeBox(
    const TArray<FCk_Handle>& InHandles) -> TSharedRef<SWrapBox>
{
    auto Box = SNew(SWrapBox).UseAllottedSize(true);
    PopulateBadgeBox(*Box, InHandles);
    return Box;
}

auto FCkInspectorWidgetBuilder::PopulateBadgeBox(
    SWrapBox& InBox,
    const TArray<FCk_Handle>& InHandles) -> void
{
    InBox.ClearChildren();

    // Clicks route through the global ck::DebugNav navigator (registered by
    // the ECS debugger module), which sets selection on whichever ECS Debugger
    // window is open — same behaviour as SCkDebug_EntityRef everywhere else.
    for (const auto& Handle : InHandles)
    {
        if (ck::Is_NOT_Valid(Handle)) { continue; }

        InBox.AddSlot()
            .Padding(FMargin(0.0f, 0.0f, 2.0f, 2.0f))
            [
                SNew(SCkDebug_EntityRef)
                    .Entity(Handle)
                    .ShowName(true)
            ];
    }
}

auto FCkInspectorWidgetBuilder::AddHeader(const FText& InHeaderText) -> FCkInspectorWidgetBuilder&
{
    Rows.Add(FRowDefinition
    {
        InHeaderText,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        true
    });

    return *this;
}

auto FCkInspectorWidgetBuilder::DoAddWidgetValueRow(
    const FText& InLabel,
    TSharedRef<SWidget> InValueWidget,
    FFilterValueGetter InFilterValueGetter) -> FCkInspectorWidgetBuilder&
{
    Rows.Add(FRowDefinition
    {
        InLabel,
        nullptr,
        nullptr,
        nullptr,
        InValueWidget.ToSharedPtr(),
        false,
        MoveTemp(InFilterValueGetter)
    });

    return *this;
}

auto FCkInspectorWidgetBuilder::AddMeterRow(
    const FText& InLabel,
    TAttribute<float> InFraction,
    ECk_Tone InTone,
    TAttribute<FText> InValueText) -> FCkInspectorWidgetBuilder&
{
    namespace builder = ck_inspector_widget_builder;

    auto Value = SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(SBox)
            .WidthOverride(builder::MeterWidth)
            [
                SNew(SCkDebug_MeterBar)
                .Fraction(InFraction)
                .FillColor_Lambda([InTone]() { return CkStyle::GetToneColor(InTone); })
                .DesiredSize(FVector2D{builder::MeterWidth, builder::MeterHeight})
            ]
        ];

    const auto Text = InValueText.IsSet()
        ? InValueText
        : TAttribute<FText>::Create([InFraction]() { return ck_inspector_widget_builder::Format_Percent(InFraction.Get()); });

    Value->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
        [
            builder::Make_NumericText(Text, CkStyle::Text())
        ];

    return DoAddWidgetValueRow(InLabel, Value, [Text]() { return Text.Get().ToString(); });
}

auto FCkInspectorWidgetBuilder::AddSparklineRow(
    const FText& InLabel,
    TAttribute<float> InSample,
    ECk_Tone InTone,
    TAttribute<FText> InValueText,
    int32 InHistoryLength) -> FCkInspectorWidgetBuilder&
{
    namespace builder = ck_inspector_widget_builder;

    auto Samples = MakeShared<TArray<float>>();
    auto LastSampledFrame = MakeShared<uint64>(0);

    const auto Text = TAttribute<FText>::Create(
        [Samples, LastSampledFrame, InSample, InValueText, InHistoryLength]()
        {
            const auto Current = InSample.Get();
            ck_inspector_widget_builder::Push_Sample(*Samples, *LastSampledFrame, Current, InHistoryLength);
            return InValueText.IsSet() ? InValueText.Get() : ck_inspector_widget_builder::Format_Numeric(Current);
        });

    auto Value = SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(SCkDebug_Sparkline)
            .Samples(Samples)
            .Color(CkStyle::GetToneColor(InTone))
            .DesiredSize(FVector2D{builder::SparklineWidth, builder::SparklineHeight})
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
        [
            builder::Make_NumericText(Text, CkStyle::Text())
        ];

    // The pump must not run from the filter pass — a filtered-out row would still sample.
    return DoAddWidgetValueRow(InLabel, Value,
        [InSample, InValueText]()
        {
            return InValueText.IsSet()
                ? InValueText.Get().ToString()
                : ck_inspector_widget_builder::Format_Numeric(InSample.Get()).ToString();
        });
}

auto FCkInspectorWidgetBuilder::AddStatusPillRow(
    const FText& InLabel,
    TAttribute<FText> InText,
    TAttribute<ECk_Tone> InTone) -> FCkInspectorWidgetBuilder&
{
    auto Value = SNew(SCkDebug_StatusPill)
        .Text(InText)
        .Tone(InTone);

    return DoAddWidgetValueRow(InLabel, Value, [InText]() { return InText.Get().ToString(); });
}

auto FCkInspectorWidgetBuilder::AddChipsRow(
    const FText& InLabel,
    const TArray<FCkInspector_Chip>& InChips) -> FCkInspectorWidgetBuilder&
{
    auto Box = SNew(SWrapBox).UseAllottedSize(true);

    for (const auto& Chip : InChips)
    {
        Box->AddSlot()
            .Padding(FMargin{0.0f, 0.0f, CkStyle::SpaceXS, CkStyle::SpaceXS})
            [
                ck::debug_axes::Make_Chip(UCkDebuggerStyleSettings::Get_Selection(), Chip.Text, Chip.Tone)
            ];
    }

    auto FilterValue = FString{};
    for (const auto& Chip : InChips)
    {
        if (NOT FilterValue.IsEmpty()) { FilterValue.AppendChar(TEXT(' ')); }
        FilterValue.Append(Chip.Text.ToString());
    }

    return DoAddWidgetValueRow(InLabel, Box, [FilterValue]() { return FilterValue; });
}

auto FCkInspectorWidgetBuilder::AddTimelineRow(
    const FText& InLabel,
    const TArray<FString>& InLaneLabels,
    const FCkInspector_TimelineContent& InContent,
    float InDesiredHeight) -> FCkInspectorWidgetBuilder&
{
    auto Timeline = SNew(SCkDebug_EventTimeline)
        .LaneLabels(InLaneLabels)
        .DesiredHeight(InDesiredHeight);

    Timeline->Set_Content(InContent.TimeMin, InContent.TimeMax, InContent.Events, InContent.Spans);

    auto FilterValue = FString::Join(InLaneLabels, TEXT(" "));
    for (const auto& Event : InContent.Events)
    {
        if (Event.Tooltip.IsEmpty()) { continue; }
        FilterValue.AppendChar(TEXT(' '));
        FilterValue.Append(Event.Tooltip);
    }

    return DoAddWidgetValueRow(InLabel, Timeline, [FilterValue]() { return FilterValue; });
}

auto FCkInspectorWidgetBuilder::AddAlignedNumericRow(
    const FText& InLabel,
    const TArray<TAttribute<FText>>& InComponents) -> FCkInspectorWidgetBuilder&
{
    namespace builder = ck_inspector_widget_builder;

    const auto Layout = Get_NumericLayout(UCkDebuggerStyleSettings::Get_Selection().ValueAlignment);
    const auto ColumnWidth = Get_AlignedColumnWidth(InComponents.Num());

    const auto Components = InComponents;
    const auto FilterValue = FFilterValueGetter{[Components]()
    {
        return Join_NumericComponents(ck_inspector_widget_builder::Resolve_Components(Components)).ToString();
    }};

    if (Layout == ECkInspector_NumericLayout::AlignedColumns)
    {
        auto Value = SNew(SHorizontalBox);

        for (auto Index = 0; Index < InComponents.Num(); ++Index)
        {
            Value->AddSlot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(ColumnWidth)
                    .HAlign(HAlign_Right)
                    [
                        builder::Make_NumericText(InComponents[Index], Get_AxisColor(Index))
                    ]
                ];
        }

        return DoAddWidgetValueRow(InLabel, Value, FilterValue);
    }

    const auto Joined = TAttribute<FText>::Create([Components]()
    {
        return Join_NumericComponents(ck_inspector_widget_builder::Resolve_Components(Components));
    });

    const auto IsLeft = Layout == ECkInspector_NumericLayout::SingleLine_Left;

    auto Value = SNew(SBox)
        .HAlign(IsLeft ? HAlign_Left : HAlign_Right)
        .MinDesiredWidth(IsLeft ? ColumnWidth * static_cast<float>(InComponents.Num()) : 0.0f)
        [
            builder::Make_NumericText(Joined, CkStyle::Text())
        ];

    return DoAddWidgetValueRow(InLabel, Value, FilterValue);
}

// ====================================================================================================================
// Pure helpers

auto FCkInspectorWidgetBuilder::Matches_Filter(
    const FString& InFilter,
    const FString& InLabel,
    const FString& InValue) -> bool
{
    if (InFilter.IsEmpty())
    { return true; }

    if (ck::fuzzy::Match(InFilter, InLabel, {}).Get_IsMatch())
    { return true; }

    return NOT InValue.IsEmpty() && ck::fuzzy::Match(InFilter, InValue, {}).Get_IsMatch();
}

auto FCkInspectorWidgetBuilder::Get_NumericLayout(ECkDebugAxis_ValueAlignment InAlignment) -> ECkInspector_NumericLayout
{
    switch (InAlignment)
    {
        case ECkDebugAxis_ValueAlignment::Left:           return ECkInspector_NumericLayout::SingleLine_Left;
        case ECkDebugAxis_ValueAlignment::Right:          return ECkInspector_NumericLayout::SingleLine_Right;
        case ECkDebugAxis_ValueAlignment::AlignedColumns: return ECkInspector_NumericLayout::AlignedColumns;
    }

    return ECkInspector_NumericLayout::SingleLine_Left;
}

auto FCkInspectorWidgetBuilder::Get_AlignedColumnWidth(int32 InComponentCount) -> float
{
    namespace builder = ck_inspector_widget_builder;

    if (InComponentCount <= 0)
    { return builder::NumericColumnWidth; }

    return FMath::Clamp(
        builder::NumericColumnBudget / static_cast<float>(InComponentCount),
        builder::NumericColumnWidthMin,
        builder::NumericColumnWidth);
}

auto FCkInspectorWidgetBuilder::Get_AxisColor(int32 InComponentIndex) -> FLinearColor
{
    switch (InComponentIndex)
    {
        case 0:  return CkStyle::AxisX();
        case 1:  return CkStyle::AxisY();
        case 2:  return CkStyle::AxisZ();
        default: return CkStyle::Text();
    }
}

auto FCkInspectorWidgetBuilder::Join_NumericComponents(const TArray<FText>& InComponents) -> FText
{
    auto Joined = FString{};

    for (const auto& Component : InComponents)
    {
        if (NOT Joined.IsEmpty())
        { Joined.Append(ck_inspector_widget_builder::NumericSeparator); }

        Joined.Append(Component.ToString());
    }

    return FText::FromString(Joined);
}

// ====================================================================================================================

auto FCkInspectorWidgetBuilder::Build(const FCk_Handle& InEntity, const FString& InFilter) -> TSharedRef<SWidget>
{
    auto Column = SNew(SVerticalBox);

    const auto HasFilter = NOT InFilter.IsEmpty();
    auto FirstRowInSection = true;
    auto HasAnyRowBefore = false;

    for (const auto& RowDef : Rows)
    {
        if (HasFilter)
        {
            auto RowValue = FString{};

            if (RowDef.ValueGetter && ck::IsValid(InEntity))
            { RowValue = RowDef.ValueGetter(InEntity).ToString(); }
            else if (RowDef.FilterValueGetter)
            { RowValue = RowDef.FilterValueGetter(); }

            if (NOT Matches_Filter(InFilter, RowDef.Label.ToString(), RowValue))
            { continue; }
        }

        if (RowDef.IsHeader)
        {
            Column->AddSlot()
                .AutoHeight()
                .Padding(0.0f, HasAnyRowBefore ? CkStyle::SpaceL : 0.0f, 0.0f, 2.0f)
                [
                    SNew(SCkDebug_SectionHeader)
                    .Label(RowDef.Label)
                ];
            FirstRowInSection = true;
            HasAnyRowBefore = true;
            continue;
        }

        // ---- Build the row. CustomWidget wins over dynamic value text. ----
        const auto HasCustom = RowDef.CustomWidget.IsValid();
        const auto HasOnClicked = static_cast<bool>(RowDef.OnClicked);
        const auto OnClicked = RowDef.OnClicked;

        TSharedRef<SWidget> RowWidget = SNullWidget::NullWidget;

        if (HasCustom)
        {
            if (HasOnClicked)
            {
                RowWidget = SNew(SCkDebug_KeyValueRow)
                    .KeyText(RowDef.Label)
                    .Tone(ECkDebug_KeyValueTone::Custom)
                    .OnKeyClicked_Lambda([OnClicked]() { OnClicked(); })
                    .ValueWidget()
                    [
                        RowDef.CustomWidget.ToSharedRef()
                    ];
            }
            else
            {
                RowWidget = SNew(SCkDebug_KeyValueRow)
                    .KeyText(RowDef.Label)
                    .Tone(ECkDebug_KeyValueTone::Custom)
                    .ValueWidget()
                    [
                        RowDef.CustomWidget.ToSharedRef()
                    ];
            }
        }
        else
        {
            const auto ValueGetter = RowDef.ValueGetter;
            const auto ColorGetter = RowDef.ColorGetter;
            const auto CapturedEntity = InEntity;

            auto ValueAttr = TAttribute<FText>::Create([CapturedEntity, ValueGetter]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity)) { return FText::GetEmpty(); }
                if (NOT ValueGetter) { return FText::GetEmpty(); }
                return ValueGetter(CapturedEntity);
            });

            auto ColorAttr = TAttribute<FLinearColor>::Create([CapturedEntity, ColorGetter]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity)) { return CkStyle::None(); }
                if (NOT ColorGetter) { return CkStyle::Text(); }
                return ColorGetter(CapturedEntity);
            });

            if (HasOnClicked)
            {
                RowWidget = SNew(SCkDebug_KeyValueRow)
                    .KeyText(RowDef.Label)
                    .Tone(ECkDebug_KeyValueTone::Custom)
                    .ValueText(ValueAttr)
                    .CustomValueColor(ColorAttr)
                    .OnKeyClicked_Lambda([OnClicked]() { OnClicked(); });
            }
            else
            {
                RowWidget = SNew(SCkDebug_KeyValueRow)
                    .KeyText(RowDef.Label)
                    .Tone(ECkDebug_KeyValueTone::Custom)
                    .ValueText(ValueAttr)
                    .CustomValueColor(ColorAttr);
            }
        }

        Column->AddSlot()
            .AutoHeight()
            .Padding(TAttribute<FMargin>::CreateLambda([]()
            {
                return ck_inspector_widget_builder::Apply_RowDensity(FMargin{0.0f, 1.0f});
            }))
            [
                RowWidget
            ];

        FirstRowInSection = false;
        HasAnyRowBefore = true;
    }

    return Column;
}
