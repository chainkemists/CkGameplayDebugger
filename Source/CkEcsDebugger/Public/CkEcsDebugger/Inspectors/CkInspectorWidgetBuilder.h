#pragma once

#include "CkEcs/Handle/CkHandle.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

class FCkDebuggerModel_EntitySelection;

class FCkInspectorWidgetBuilder
{
public:
    using FValueGetter = TFunction<FText(const FCk_Handle&)>;
    using FColorGetter = TFunction<FLinearColor(const FCk_Handle&)>;
    using FOnClicked = TFunction<void()>;

    auto SetSelectionModel(TSharedPtr<FCkDebuggerModel_EntitySelection> InModel) -> FCkInspectorWidgetBuilder&;

    auto AddRow(
        const FText& InLabel,
        FValueGetter InValueGetter,
        const FLinearColor& InValueColor = FCkDebuggerStyle::Color_Text_Primary) -> FCkInspectorWidgetBuilder&;

    auto AddConditionalRow(
        const FText& InLabel,
        FValueGetter InValueGetter,
        FColorGetter InColorGetter) -> FCkInspectorWidgetBuilder&;

    auto AddClickableRow(
        const FText& InLabel,
        FValueGetter InValueGetter,
        const FLinearColor& InValueColor,
        FOnClicked InOnClicked) -> FCkInspectorWidgetBuilder&;

    auto AddClickableRow(
        const FText& InLabel,
        FValueGetter InValueGetter,
        FColorGetter InColorGetter,
        FOnClicked InOnClicked) -> FCkInspectorWidgetBuilder&;

    auto AddHeader(const FText& InHeaderText) -> FCkInspectorWidgetBuilder&;

    /** Place a pre-built widget directly in the value column, next to a plain label. */
    auto AddWidgetRow(
        const FText& InLabel,
        TSharedRef<SWidget> InWidget) -> FCkInspectorWidgetBuilder&;

    auto Build(const FCk_Handle& InEntity, const FString& InFilter = FString()) -> TSharedRef<SWidget>;

private:
    struct FRowDefinition
    {
        FText Label;
        FValueGetter ValueGetter;
        FColorGetter ColorGetter;
        FOnClicked OnClicked;
        TSharedPtr<SWidget> CustomWidget;
        bool IsHeader = false;
    };

    TArray<FRowDefinition> Rows;
    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
};
