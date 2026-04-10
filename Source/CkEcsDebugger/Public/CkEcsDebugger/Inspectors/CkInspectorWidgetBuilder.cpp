#include "CkInspectorWidgetBuilder.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
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
    const TArray<FCk_Handle>& InHandles,
    TWeakPtr<FCkDebuggerModel_EntitySelection> InWeakModel) -> TSharedRef<SWrapBox>
{
    auto Box = SNew(SWrapBox).UseAllottedSize(true);
    PopulateBadgeBox(*Box, InHandles, InWeakModel);
    return Box;
}

auto FCkInspectorWidgetBuilder::PopulateBadgeBox(
    SWrapBox& InBox,
    const TArray<FCk_Handle>& InHandles,
    TWeakPtr<FCkDebuggerModel_EntitySelection> InWeakModel) -> void
{
    InBox.ClearChildren();

    for (const auto& Handle : InHandles)
    {
        if (ck::Is_NOT_Valid(Handle)) { continue; }

        const auto DebugName = UCk_Utils_Handle_UE::Get_DebugName(Handle).ToString();
        const auto CapturedHandle = Handle;

        InBox.AddSlot()
            .Padding(FMargin(0.0f, 0.0f, 2.0f, 2.0f))
            [
                SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                    .ContentPadding(FMargin(4.0f, 1.0f))
                    .OnClicked_Lambda([InWeakModel, CapturedHandle]()
                    {
                        if (const auto Model = InWeakModel.Pin(); Model.IsValid())
                        {
                            Model->Set_SelectedEntities({ CapturedHandle });
                        }
                        return FReply::Handled();
                    })
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(DebugName))
                            .ColorAndOpacity(FCkDebuggerStyle::Color_Selection)
                    ]
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

        // ---- Label column: clickable button if OnClicked is set, otherwise plain text ----
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

        // ---- Value column: pre-built widget if supplied, otherwise dynamic text ----
        if (RowDef.CustomWidget.IsValid())
        {
            Grid->AddSlot(1, Row)
                .Padding(FCkDebuggerStyle::Padding_Small)
                [
                    RowDef.CustomWidget.ToSharedRef()
                ];
        }
        else
        {
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

                        if (NOT ValueGetter)
                        { return FText::GetEmpty(); }

                        return ValueGetter(InEntity);
                    }))
                    .ColorAndOpacity(TAttribute<FSlateColor>::Create([InEntity, ColorGetter]()
                    {
                        if (ck::Is_NOT_Valid(InEntity))
                        { return FSlateColor(FCkDebuggerStyle::Color_None); }

                        if (NOT ColorGetter)
                        { return FSlateColor(FCkDebuggerStyle::Color_Text_Primary); }

                        return FSlateColor(ColorGetter(InEntity));
                    }))
                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                ];
        }

        Row++;
    }

    return Grid;
}
