#pragma once

#include "CkEcs/Handle/CkHandle.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

#include "CkEditorTools/Style/CkStyle.h"
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
        const FLinearColor& InValueColor = CkStyle::Text()) -> FCkInspectorWidgetBuilder&;

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

    /**
     * Same as AddClickableRow, but the value column hosts an arbitrary widget instead of dynamic text.
     * The label column remains a clickable button that fires the supplied delegate.
     */
    auto AddClickableWidgetRow(
        const FText& InLabel,
        TSharedRef<SWidget> InValueWidget,
        FOnClicked InOnClicked) -> FCkInspectorWidgetBuilder&;

    /**
     * Build a clickable entity badge wrap-box. Each handle becomes an
     * SCkDebug_EntityRef pill; clicking opens that entity in the CK ECS
     * Debugger via ck::DebugNav (right-click → copy works too). Used by
     * inspectors that show lists of related entities (probe overlaps,
     * interaction targets, scene-node siblings, etc.).
     */
    static auto MakeBadgeBox(
        const TArray<FCk_Handle>& InHandles) -> TSharedRef<SWrapBox>;

    /** Populate (or repopulate) an existing badge box with the given handles. */
    static auto PopulateBadgeBox(
        SWrapBox& InBox,
        const TArray<FCk_Handle>& InHandles) -> void;

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
