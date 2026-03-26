#include "CkInspectorWidgetBuilder.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"

#include "Widgets/Input/SButton.h"
#include "Styling/AppStyle.h"

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
        false
    });

    return *this;
}

auto FCkInspectorWidgetBuilder::AddHeader(const FText& InHeaderText) -> FCkInspectorWidgetBuilder&
{
    Rows.Add(FRowDefinition
    {
        InHeaderText,
        nullptr,
        nullptr,
        nullptr,
        true
    });

    return *this;
}

auto FCkInspectorWidgetBuilder::Build(const FCk_Handle& InEntity, const FString& InFilter) -> TSharedRef<SWidget>
{
    auto Grid = SNew(SGridPanel)
        .FillColumn(0, 0.5f)
        .FillColumn(1, 0.5f);

    const auto HasFilter = NOT InFilter.IsEmpty();
    int32 Row = 0;

    for (const auto& RowDef : Rows)
    {
        if (HasFilter && NOT RowDef.Label.ToString().Contains(InFilter, ESearchCase::IgnoreCase))
        { continue; }

        if (RowDef.IsHeader)
        {
            Grid->AddSlot(0, Row)
                .ColumnSpan(2)
                .Padding(FCkDebuggerStyle::Padding_Small, Row > 0 ? FCkDebuggerStyle::Padding_Medium : 0.0f, FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small)
                [
                    SNew(STextBlock)
                    .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Bold"))
                    .Text(RowDef.Label)
                    .ColorAndOpacity(FCkDebuggerStyle::Color_Text_Secondary)
                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                    .ToolTipText(RowDef.Label)
                ];

            Row++;
            continue;
        }

        // Label column -- clickable if OnClicked is set
        if (RowDef.OnClicked)
        {
            const auto OnClicked = RowDef.OnClicked;

            Grid->AddSlot(0, Row)
                .Padding(FCkDebuggerStyle::Padding_Small)
                [
                    SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                    .OnClicked_Lambda([OnClicked]()
                    {
                        OnClicked();
                        return FReply::Handled();
                    })
                    .ContentPadding(0.0f)
                    .ToolTipText(RowDef.Label)
                    [
                        SNew(STextBlock)
                        .Text(RowDef.Label)
                        .ColorAndOpacity(FCkDebuggerStyle::Color_Selection)
                        .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                    ]
                ];
        }
        else
        {
            Grid->AddSlot(0, Row)
                .Padding(FCkDebuggerStyle::Padding_Small)
                [
                    SNew(STextBlock)
                    .Text(RowDef.Label)
                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                    .ToolTipText(RowDef.Label)
                ];
        }

        const auto ValueGetter = RowDef.ValueGetter;
        const auto ColorGetter = RowDef.ColorGetter;

        Grid->AddSlot(1, Row)
            .Padding(FCkDebuggerStyle::Padding_Small)
            [
                SNew(STextBlock)
                .Text(TAttribute<FText>::Create([InEntity, ValueGetter]()
                {
                    if (ck::Is_NOT_Valid(InEntity))
                    { return FText::GetEmpty(); }

                    return ValueGetter(InEntity);
                }))
                .ColorAndOpacity(TAttribute<FSlateColor>::Create([InEntity, ColorGetter]()
                {
                    if (ck::Is_NOT_Valid(InEntity))
                    { return FSlateColor(FCkDebuggerStyle::Color_None); }

                    return FSlateColor(ColorGetter(InEntity));
                }))
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ];

        Row++;
    }

    return Grid;
}
