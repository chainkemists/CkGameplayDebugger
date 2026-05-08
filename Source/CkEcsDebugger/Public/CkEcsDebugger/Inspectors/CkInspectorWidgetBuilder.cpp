#include "CkInspectorWidgetBuilder.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/String/CkFuzzyMatch_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"

#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

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

auto FCkInspectorWidgetBuilder::Build(const FCk_Handle& InEntity, const FString& InFilter) -> TSharedRef<SWidget>
{
    auto Column = SNew(SVerticalBox);

    const auto HasFilter = NOT InFilter.IsEmpty();
    auto FirstRowInSection = true;
    auto HasAnyRowBefore = false;

    for (const auto& RowDef : Rows)
    {
        if (HasFilter && NOT ck::fuzzy::Match(InFilter, RowDef.Label.ToString(), {}).Get_IsMatch())
        { continue; }

        if (RowDef.IsHeader)
        {
            Column->AddSlot()
                .AutoHeight()
                .Padding(0.0f, HasAnyRowBefore ? CkDebugStyle::SpaceL : 0.0f, 0.0f, 2.0f)
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
                if (ck::Is_NOT_Valid(CapturedEntity)) { return CkDebugStyle::None(); }
                if (NOT ColorGetter) { return CkDebugStyle::Text(); }
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
            .Padding(0.0f, 1.0f)
            [
                RowWidget
            ];

        FirstRowInSection = false;
        HasAnyRowBefore = true;
    }

    return Column;
}
