#include "CkInspectorWidgetBuilder.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/String/CkFuzzyMatch_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerCommonStyle.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CountBadge.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_MeterBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NumericEditor.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Sparkline.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Switch.h"

#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
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

    // ----- Interactive rows ---------------------------------------------------

    constexpr auto TextEntryWidth  = 160.0f;
    constexpr auto ActionButtonGap = 6.0f;

    static auto Format_Bool(bool InValue) -> FText
    {
        return FText::FromString(InValue ? TEXT("true") : TEXT("false"));
    }

    static auto Make_ValueLabel(const TAttribute<FText>& InText, const FLinearColor& InColor) -> TSharedRef<SWidget>
    {
        return SNew(STextBlock)
            .Text(InText)
            .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
            .ColorAndOpacity(FSlateColor{InColor});
    }

    // Typing state for the FName / tag entries. Same freeze rule as SCkDebug_NumericEditor: while the
    // user is mid-word the bound live value must not overwrite the text box.
    struct FTextEntryState
    {
        bool  IsEditing = false;       // focus window — what the edit guard defers on
        bool  HasChange = false;       // something was actually typed — what a commit needs
        FText Frozen;
    };

    static auto Make_TextEntry(
        TAttribute<FText> InValue,
        TFunction<void(const FString&)> InCommit,
        TSharedPtr<FCkInspectorEditScope> InScope) -> TSharedRef<SWidget>
    {
        auto State = MakeShared<FTextEntryState>();

        return SNew(SBox)
            .WidthOverride(TextEntryWidth)
            [
                SNew(SEditableTextBox)
                .Text_Lambda([State, InValue]()
                {
                    return State->IsEditing ? State->Frozen : InValue.Get();
                })
                .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
                .SelectAllTextWhenFocused(true)
                .RevertTextOnEscape(true)
                .OnBeginTextEdit_Lambda([State, InScope](const FText& InText)
                {
                    State->IsEditing = true;
                    State->Frozen    = InText;

                    if (InScope.IsValid())
                    { InScope->Set_Active(true); }
                })
                .OnTextChanged_Lambda([State, InScope](const FText& InText)
                {
                    State->IsEditing = true;
                    State->HasChange = true;
                    State->Frozen    = InText;

                    if (InScope.IsValid())
                    { InScope->Set_Active(true); }
                })
                .OnTextCommitted_Lambda([State, InScope, InCommit](const FText& InText, ETextCommit::Type InCommitType)
                {
                    const auto HadChange = State->HasChange;

                    State->IsEditing = false;
                    State->HasChange = false;
                    State->Frozen    = FText::GetEmpty();

                    if (InScope.IsValid())
                    { InScope->Set_Active(false); }

                    // Focus passing through a field nobody typed in is not an edit.
                    if (NOT HadChange)
                    { return; }

                    if (NOT SCkDebug_NumericEditor::Should_Commit(InCommitType))
                    { return; }

                    if (NOT InCommit)
                    { return; }

                    InCommit(InText.ToString());
                })
            ];
    }

    // The GOAP settings drawer's flat verb button, promoted to the shared vocabulary.
    static auto Make_ActionButton(
        const FText& InLabel,
        TAttribute<FText> InTooltip,
        TAttribute<bool> InIsEnabled,
        TFunction<void()> InOnClicked) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(CkStyle::GetRoundedBrush())
            .BorderBackgroundColor(FSlateColor{CkStyle::Border()})
            .Padding(FMargin{1.0f})
            [
                SNew(SButton)
                .ButtonStyle(&FCkDebuggerCommonStyle::Get_FlatButtonStyle())
                .ContentPadding(FMargin{10.0f, 3.0f})
                .ToolTipText(InTooltip)
                .IsEnabled(InIsEnabled)
                .OnClicked_Lambda([InOnClicked]() -> FReply
                {
                    if (InOnClicked)
                    { InOnClicked(); }

                    return FReply::Handled();
                })
                [
                    SNew(STextBlock)
                    .Text(InLabel)
                    .Font(CkStyle::BoldFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(FSlateColor{CkStyle::Text()})
                ]
            ];
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
    return DoAddWidgetValueRow(InLabel, InWidget, nullptr, nullptr);
}

auto FCkInspectorWidgetBuilder::AddWidgetRow(
    const FText& InLabel,
    TSharedRef<SWidget> InWidget,
    FFilterValueGetter InFilterValueGetter) -> FCkInspectorWidgetBuilder&
{
    return DoAddWidgetValueRow(InLabel, InWidget, MoveTemp(InFilterValueGetter), nullptr);
}

auto FCkInspectorWidgetBuilder::AddClickableWidgetRow(
    const FText& InLabel,
    TSharedRef<SWidget> InValueWidget,
    FOnClicked InOnClicked) -> FCkInspectorWidgetBuilder&
{
    return DoAddWidgetValueRow(InLabel, InValueWidget, nullptr, MoveTemp(InOnClicked));
}

auto FCkInspectorWidgetBuilder::AddClickableWidgetRow(
    const FText& InLabel,
    TSharedRef<SWidget> InValueWidget,
    FOnClicked InOnClicked,
    FFilterValueGetter InFilterValueGetter) -> FCkInspectorWidgetBuilder&
{
    return DoAddWidgetValueRow(InLabel, InValueWidget, MoveTemp(InFilterValueGetter), MoveTemp(InOnClicked));
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
    FFilterValueGetter InFilterValueGetter,
    FOnClicked InOnClicked) -> FCkInspectorWidgetBuilder&
{
    Rows.Add(FRowDefinition
    {
        InLabel,
        nullptr,
        nullptr,
        MoveTemp(InOnClicked),
        InValueWidget.ToSharedPtr(),
        false,
        MoveTemp(InFilterValueGetter)
    });

    return *this;
}

auto FCkInspectorWidgetBuilder::AddMeterRow(
    const FText& InLabel,
    TAttribute<float> InFraction,
    TAttribute<ECk_Tone> InTone,
    TAttribute<FText> InValueText,
    FOnClicked InOnClicked) -> FCkInspectorWidgetBuilder&
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
                .FillColor_Lambda([InTone]() { return CkStyle::GetToneColor(InTone.Get()); })
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

    return DoAddWidgetValueRow(InLabel, Value, [Text]() { return Text.Get().ToString(); }, MoveTemp(InOnClicked));
}

auto FCkInspectorWidgetBuilder::AddSparklineRow(
    const FText& InLabel,
    TAttribute<float> InSample,
    TAttribute<ECk_Tone> InTone,
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
            .Color_Lambda([InTone]() { return CkStyle::GetToneColor(InTone.Get()); })
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
    TAttribute<ECk_Tone> InTone,
    FOnClicked InOnClicked) -> FCkInspectorWidgetBuilder&
{
    // SCkDebug_StatusPill is visual-only (no internal button), so it never eats the click that the
    // label button owns.
    auto Value = SNew(SCkDebug_StatusPill)
        .Text(InText)
        .Tone(InTone);

    return DoAddWidgetValueRow(InLabel, Value, [InText]() { return InText.Get().ToString(); }, MoveTemp(InOnClicked));
}

auto FCkInspectorWidgetBuilder::AddCountBadgeRow(
    const FText& InLabel,
    TAttribute<int32> InCount,
    ECk_Tone InTone,
    const FText& InSuffix,
    FOnClicked InOnClicked) -> FCkInspectorWidgetBuilder&
{
    const auto ToneColor = CkStyle::GetToneColor(InTone);

    const auto Text = TAttribute<FText>::Create([InCount]() { return Format_Count(InCount.Get()); });

    auto Value = SNew(SCkDebug_CountBadge)
        .ValueText(Text)
        .SuffixText(InSuffix)
        .ValueColor(ToneColor)
        .SuffixColor(CkStyle::TextDim())
        .BorderColor(ToneColor.CopyWithNewOpacity(0.4f));

    const auto Suffix = InSuffix;

    return DoAddWidgetValueRow(InLabel, Value, [Text, Suffix]()
    {
        return Suffix.IsEmpty()
            ? Text.Get().ToString()
            : Text.Get().ToString() + TEXT(" ") + Suffix.ToString();
    },
    MoveTemp(InOnClicked));
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
// Interactive vocabulary

auto FCkInspectorWidgetBuilder::SetEditGuard(TSharedPtr<FCkInspectorEditGuard> InGuard) -> FCkInspectorWidgetBuilder&
{
    _EditGuard = MoveTemp(InGuard);
    return *this;
}

auto FCkInspectorWidgetBuilder::DoMakeEditScope() const -> TSharedPtr<FCkInspectorEditScope>
{
    return MakeShared<FCkInspectorEditScope>(_EditGuard);
}

auto FCkInspectorWidgetBuilder::DoMakeGateEnabled(ECk_DebugRequest_Requirement InRequirement) const -> TAttribute<bool>
{
    const auto Entity = _GateEntity;

    return TAttribute<bool>::Create([Entity, InRequirement]()
    {
        return ck::DebugRequestGate::Evaluate(*Entity, InRequirement).IsEnabled;
    });
}

auto FCkInspectorWidgetBuilder::DoMakeGateTooltip(
    TAttribute<FText> InOwnTooltip,
    ECk_DebugRequest_Requirement InRequirement) const -> TAttribute<FText>
{
    const auto Entity = _GateEntity;

    return TAttribute<FText>::Create([Entity, InRequirement, InOwnTooltip]()
    {
        const auto Verdict = ck::DebugRequestGate::Evaluate(*Entity, InRequirement);
        const auto Own = InOwnTooltip.IsSet() ? InOwnTooltip.Get() : FText::GetEmpty();

        if (Verdict.Reason.IsEmpty())
        { return Own; }

        if (Own.IsEmpty())
        { return Verdict.Reason; }

        return FText::FromString(ck::Format_UE(TEXT("{}\n\n{}"), Own.ToString(), Verdict.Reason.ToString()));
    });
}

auto FCkInspectorWidgetBuilder::DoApplyGate(
    TSharedRef<SWidget> InControl,
    TAttribute<FText> InOwnTooltip,
    ECk_DebugRequest_Requirement InRequirement) const -> TSharedRef<SWidget>
{
    // Enabled-ness propagates to children, so gating the outermost wrapper greys the whole control;
    // the reason rides along as its tooltip, which is the entire point of gating instead of hiding.
    InControl->SetEnabled(DoMakeGateEnabled(InRequirement));
    InControl->SetToolTipText(DoMakeGateTooltip(MoveTemp(InOwnTooltip), InRequirement));

    return InControl;
}

auto FCkInspectorWidgetBuilder::DoComposeEditControl(
    TSharedRef<SWidget> InControl,
    TAttribute<FText> InReadOnlyText) -> TSharedRef<SWidget>
{
    if (NOT ck::debug_axes::EditControls_RevealOnHover(UCkDebuggerStyleSettings::Get_Selection()))
    { return InControl; }

    auto Host = SNew(SBox);
    const auto HostWeak = TWeakPtr<SBox>{Host};

    const auto Get_IsHovered = [HostWeak]()
    {
        const auto Pinned = HostWeak.Pin();
        return Pinned.IsValid() && Pinned->IsHovered();
    };

    // Hidden, never Collapsed: both children keep their footprint, so revealing the control on hover
    // cannot re-flow the row out from under the cursor.
    auto ReadOnly = ck_inspector_widget_builder::Make_ValueLabel(MoveTemp(InReadOnlyText), CkStyle::TextDim());

    ReadOnly->SetVisibility(TAttribute<EVisibility>::Create([Get_IsHovered]()
    {
        return Get_IsHovered() ? EVisibility::Hidden : EVisibility::Visible;
    }));

    InControl->SetVisibility(TAttribute<EVisibility>::Create([Get_IsHovered]()
    {
        return Get_IsHovered() ? EVisibility::Visible : EVisibility::Hidden;
    }));

    Host->SetContent(
        SNew(SOverlay)

        + SOverlay::Slot()
        .VAlign(VAlign_Center)
        [
            ReadOnly
        ]

        + SOverlay::Slot()
        .VAlign(VAlign_Center)
        [
            InControl
        ]);

    return Host;
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkInspectorWidgetBuilder::AddActionRow(
    const FText& InLabel,
    const TArray<FCkInspector_Action>& InActions) -> FCkInspectorWidgetBuilder&
{
    namespace builder = ck_inspector_widget_builder;

    // An action row has no read-only form: Hidden restores the inspector that existed before this
    // vocabulary, which simply had no verbs.
    if (NOT ck::debug_axes::EditControls_AreVisible(UCkDebuggerStyleSettings::Get_Selection()))
    { return *this; }

    auto Buttons = SNew(SHorizontalBox);
    auto FilterValue = FString{};

    for (const auto& Action : InActions)
    {
        Buttons->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, builder::ActionButtonGap, 0.0f)
            [
                builder::Make_ActionButton(
                    Action.ButtonLabel,
                    DoMakeGateTooltip(Action.Tooltip, Action.Requirement),
                    DoMakeGateEnabled(Action.Requirement),
                    Action.OnClicked)
            ];

        if (NOT FilterValue.IsEmpty())
        { FilterValue.AppendChar(TEXT(' ')); }

        FilterValue.Append(Action.ButtonLabel.ToString());
    }

    const auto ReadOnlyText = FText::FromString(FilterValue);

    return DoAddWidgetValueRow(
        InLabel,
        DoComposeEditControl(Buttons, ReadOnlyText),
        [FilterValue]() { return FilterValue; });
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkInspectorWidgetBuilder::AddToggleRow(
    const FText& InLabel,
    TAttribute<bool> InGet,
    TFunction<void(bool)> InSet,
    ECk_DebugRequest_Requirement InRequirement) -> FCkInspectorWidgetBuilder&
{
    const auto ReadOnlyText = TAttribute<FText>::Create([InGet]()
    {
        return ck_inspector_widget_builder::Format_Bool(InGet.Get());
    });

    if (NOT ck::debug_axes::EditControls_AreVisible(UCkDebuggerStyleSettings::Get_Selection()))
    {
        return AddRow(InLabel,
            [InGet](const FCk_Handle&) { return ck_inspector_widget_builder::Format_Bool(InGet.Get()); },
            CkStyle::Text());
    }

    // A switch flip is ATOMIC — there is no in-flight window a rebuild could eat, so it takes no
    // edit-guard scope. Only the type-in rows below do.
    // HAlign_Left keeps the 30x17 switch its natural size; a fill-width slot stretches it.
    auto Switch = SNew(SBox)
        .HAlign(HAlign_Left)
        [
            SNew(SCkDebug_Switch)
            .IsOn(InGet)
            .OnStateChanged_Lambda([InSet](bool InNewState)
            {
                if (InSet)
                { InSet(InNewState); }
            })
        ];

    DoApplyGate(Switch, TAttribute<FText>{}, InRequirement);

    return DoAddWidgetValueRow(
        InLabel,
        DoComposeEditControl(Switch, ReadOnlyText),
        [ReadOnlyText]() { return ReadOnlyText.Get().ToString(); });
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkInspectorWidgetBuilder::AddNumericRow(
    const FText& InLabel,
    TAttribute<float> InGet,
    TFunction<void(float)> InSet,
    TOptional<float> InMin,
    TOptional<float> InMax,
    ECk_DebugRequest_Requirement InRequirement) -> FCkInspectorWidgetBuilder&
{
    const auto ReadOnlyText = TAttribute<FText>::Create([InGet]()
    {
        return ck_inspector_widget_builder::Format_Numeric(InGet.Get());
    });

    if (NOT ck::debug_axes::EditControls_AreVisible(UCkDebuggerStyleSettings::Get_Selection()))
    {
        return AddRow(InLabel,
            [InGet](const FCk_Handle&) { return ck_inspector_widget_builder::Format_Numeric(InGet.Get()); },
            CkStyle::Text());
    }

    auto Scope = DoMakeEditScope();

    auto Editor = SNew(SCkDebug_NumericEditor)
        .Value_Lambda([InGet]() { return static_cast<double>(InGet.Get()); })
        .Kind(ECkDebug_NumericKind::Float)
        .MinValue(InMin.IsSet() ? TOptional<double>{static_cast<double>(InMin.GetValue())} : TOptional<double>{})
        .MaxValue(InMax.IsSet() ? TOptional<double>{static_cast<double>(InMax.GetValue())} : TOptional<double>{})
        .Width(Get_AlignedColumnWidth(1))
        .OnValueCommitted_Lambda([InSet](double InValue)
        {
            if (InSet)
            { InSet(static_cast<float>(InValue)); }
        })
        .OnEditStateChanged_Lambda([Scope](bool InIsEditing)
        {
            if (Scope.IsValid())
            { Scope->Set_Active(InIsEditing); }
        });

    DoApplyGate(Editor, TAttribute<FText>{}, InRequirement);

    return DoAddWidgetValueRow(
        InLabel,
        DoComposeEditControl(Editor, ReadOnlyText),
        [ReadOnlyText]() { return ReadOnlyText.Get().ToString(); });
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkInspectorWidgetBuilder::AddIntegerRow(
    const FText& InLabel,
    TAttribute<int32> InGet,
    TFunction<void(int32)> InSet,
    TOptional<int32> InMin,
    TOptional<int32> InMax,
    ECk_DebugRequest_Requirement InRequirement) -> FCkInspectorWidgetBuilder&
{
    const auto ReadOnlyText = TAttribute<FText>::Create([InGet]()
    {
        return FText::AsNumber(InGet.Get());
    });

    if (NOT ck::debug_axes::EditControls_AreVisible(UCkDebuggerStyleSettings::Get_Selection()))
    {
        return AddRow(InLabel,
            [InGet](const FCk_Handle&) { return FText::AsNumber(InGet.Get()); },
            CkStyle::Text());
    }

    auto Scope = DoMakeEditScope();

    auto Editor = SNew(SCkDebug_NumericEditor)
        .Value_Lambda([InGet]() { return static_cast<double>(InGet.Get()); })
        .Kind(ECkDebug_NumericKind::Integer)
        .MinValue(InMin.IsSet() ? TOptional<double>{static_cast<double>(InMin.GetValue())} : TOptional<double>{})
        .MaxValue(InMax.IsSet() ? TOptional<double>{static_cast<double>(InMax.GetValue())} : TOptional<double>{})
        .Width(Get_AlignedColumnWidth(1))
        .OnValueCommitted_Lambda([InSet](double InValue)
        {
            if (InSet)
            { InSet(static_cast<int32>(FMath::RoundToDouble(InValue))); }
        })
        .OnEditStateChanged_Lambda([Scope](bool InIsEditing)
        {
            if (Scope.IsValid())
            { Scope->Set_Active(InIsEditing); }
        });

    DoApplyGate(Editor, TAttribute<FText>{}, InRequirement);

    return DoAddWidgetValueRow(
        InLabel,
        DoComposeEditControl(Editor, ReadOnlyText),
        [ReadOnlyText]() { return ReadOnlyText.Get().ToString(); });
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkInspectorWidgetBuilder::AddVectorRow(
    const FText& InLabel,
    TAttribute<FVector> InGet,
    TFunction<void(const FVector&)> InSet,
    ECk_DebugRequest_Requirement InRequirement) -> FCkInspectorWidgetBuilder&
{
    constexpr auto ComponentCount = 3;

    // Read-only projections double as the Hidden fallback, the OnHover text, and the filter value.
    auto Components = TArray<TAttribute<FText>>{};
    Components.Reserve(ComponentCount);

    for (auto Axis = 0; Axis < ComponentCount; ++Axis)
    {
        Components.Emplace(TAttribute<FText>::Create([InGet, Axis]()
        {
            return ck_inspector_widget_builder::Format_Numeric(static_cast<float>(InGet.Get()[Axis]));
        }));
    }

    if (NOT ck::debug_axes::EditControls_AreVisible(UCkDebuggerStyleSettings::Get_Selection()))
    { return AddAlignedNumericRow(InLabel, Components); }

    const auto ColumnWidth = Get_AlignedColumnWidth(ComponentCount);

    auto Row = SNew(SHorizontalBox);

    for (auto Axis = 0; Axis < ComponentCount; ++Axis)
    {
        // One scope PER COMPONENT: tabbing X -> Y interleaves the two editors' begin/end callbacks,
        // and a shared scope would let X's end cancel the edit Y just started.
        auto Scope = DoMakeEditScope();

        Row->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f)
            [
                SNew(SCkDebug_NumericEditor)
                .Value_Lambda([InGet, Axis]() { return InGet.Get()[Axis]; })
                .Kind(ECkDebug_NumericKind::Float)
                .Width(ColumnWidth)
                .ForegroundColor(Get_AxisColor(Axis))
                // Read-modify-write the WHOLE vector: the setter is a public Request_*/Set_* taking a
                // vector, and the other two components may have moved since this row was composed.
                .OnValueCommitted_Lambda([InGet, InSet, Axis](double InValue)
                {
                    if (NOT InSet)
                    { return; }

                    auto Current = InGet.Get();
                    Current[Axis] = InValue;
                    InSet(Current);
                })
                .OnEditStateChanged_Lambda([Scope](bool InIsEditing)
                {
                    if (Scope.IsValid())
                    { Scope->Set_Active(InIsEditing); }
                })
            ];
    }

    DoApplyGate(Row, TAttribute<FText>{}, InRequirement);

    const auto ReadOnlyText = TAttribute<FText>::Create([Components]()
    {
        return Join_NumericComponents(ck_inspector_widget_builder::Resolve_Components(Components));
    });

    return DoAddWidgetValueRow(
        InLabel,
        DoComposeEditControl(Row, ReadOnlyText),
        [ReadOnlyText]() { return ReadOnlyText.Get().ToString(); });
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkInspectorWidgetBuilder::AddRotatorRow(
    const FText& InLabel,
    TAttribute<FRotator> InGet,
    TFunction<void(const FRotator&)> InSet,
    ECk_DebugRequest_Requirement InRequirement) -> FCkInspectorWidgetBuilder&
{
    // (Roll, Pitch, Yaw) so component 0/1/2 colours match the axis each angle turns about — the
    // convention CkInspector_Transform's read-only "Rotation (R,P,Y)" row already uses.
    return AddVectorRow(
        InLabel,
        TAttribute<FVector>::Create([InGet]()
        {
            const auto Rotator = InGet.Get();
            return FVector{Rotator.Roll, Rotator.Pitch, Rotator.Yaw};
        }),
        [InSet](const FVector& InComponents)
        {
            if (NOT InSet)
            { return; }

            // FRotator's constructor order is (Pitch, Yaw, Roll).
            InSet(FRotator{InComponents.Y, InComponents.Z, InComponents.X});
        },
        InRequirement);
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkInspectorWidgetBuilder::AddEnumDropdownRow(
    const FText& InLabel,
    const TArray<FText>& InOptions,
    TAttribute<int32> InGetSelectedIndex,
    TFunction<void(int32)> InSetSelectedIndex,
    ECk_DebugRequest_Requirement InRequirement) -> FCkInspectorWidgetBuilder&
{
    const auto Options = InOptions;

    const auto ReadOnlyText = TAttribute<FText>::Create([Options, InGetSelectedIndex]()
    {
        const auto Index = InGetSelectedIndex.Get();
        return Options.IsValidIndex(Index) ? Options[Index] : FText::GetEmpty();
    });

    if (Options.IsEmpty() || NOT ck::debug_axes::EditControls_AreVisible(UCkDebuggerStyleSettings::Get_Selection()))
    {
        return AddRow(InLabel,
            [ReadOnlyText](const FCk_Handle&) { return ReadOnlyText.Get(); },
            CkStyle::Text());
    }

    // OptionsSource stores a RAW `const TArray<OptionType>*` (SLATE_ITEMS_SOURCE_ARGUMENT) and hands
    // it to the combo's inner list view, so the array must outlive the widget. GOAP's combo solves
    // that with a function-static array; these options are per-row, so the array is a shared one that
    // the combo's own delegates and content attribute capture BY VALUE — its lifetime is the combo's.
    const auto Items = MakeShared<TArray<TSharedPtr<int32>>>();
    for (auto Index = 0; Index < Options.Num(); ++Index)
    { Items->Add(MakeShared<int32>(Index)); }

    const auto InitialIndex = FMath::Clamp(InGetSelectedIndex.Get(), 0, Options.Num() - 1);

    // A dropdown selection is atomic like a switch: the menu is a popup that closes before any other
    // interaction can trigger a rebuild, so it takes no edit-guard scope (and cannot wedge one open).
    auto Combo = SNew(SComboBox<TSharedPtr<int32>>)
        // &Items.Get(): TSharedRef::Get() yields a REFERENCE, and only the pointer overload exists.
        .OptionsSource(&Items.Get())
        .InitiallySelectedItem((*Items)[InitialIndex])
        .OnGenerateWidget_Lambda([Items, Options](TSharedPtr<int32> InItem) -> TSharedRef<SWidget>
        {
            const auto Index = InItem.IsValid() ? *InItem : INDEX_NONE;

            return SNew(STextBlock)
                .Text(Options.IsValidIndex(Index) ? Options[Index] : FText::GetEmpty())
                .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()));
        })
        .OnSelectionChanged_Lambda([Items, InSetSelectedIndex](TSharedPtr<int32> InItem, ESelectInfo::Type InSelectInfo)
        {
            // Programmatic restores echo back as Direct — never a user intent, never a request.
            if (InSelectInfo == ESelectInfo::Direct)
            { return; }

            if (NOT InItem.IsValid() || NOT InSetSelectedIndex)
            { return; }

            InSetSelectedIndex(*InItem);
        })
        [
            SNew(STextBlock)
            // Items rides along here too, so the backing array's lifetime does not hinge on any one
            // delegate surviving a later edit of this row.
            .Text_Lambda([Items, ReadOnlyText]() { return ReadOnlyText.Get(); })
            .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
        ];

    DoApplyGate(Combo, TAttribute<FText>{}, InRequirement);

    return DoAddWidgetValueRow(
        InLabel,
        DoComposeEditControl(Combo, ReadOnlyText),
        [ReadOnlyText]() { return ReadOnlyText.Get().ToString(); });
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkInspectorWidgetBuilder::AddNameEntryRow(
    const FText& InLabel,
    TAttribute<FText> InGet,
    TFunction<void(FName)> InCommit,
    ECk_DebugRequest_Requirement InRequirement) -> FCkInspectorWidgetBuilder&
{
    namespace builder = ck_inspector_widget_builder;

    if (NOT ck::debug_axes::EditControls_AreVisible(UCkDebuggerStyleSettings::Get_Selection()))
    {
        return AddRow(InLabel,
            [InGet](const FCk_Handle&) { return InGet.Get(); },
            CkStyle::Text());
    }

    auto Scope = DoMakeEditScope();

    auto Entry = builder::Make_TextEntry(InGet,
        [InCommit](const FString& InText)
        {
            if (NOT InCommit)
            { return; }

            InCommit(FName{*InText.TrimStartAndEnd()});
        },
        Scope);

    DoApplyGate(Entry, TAttribute<FText>{}, InRequirement);

    return DoAddWidgetValueRow(
        InLabel,
        DoComposeEditControl(Entry, InGet),
        [InGet]() { return InGet.Get().ToString(); });
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkInspectorWidgetBuilder::AddTagEntryRow(
    const FText& InLabel,
    TAttribute<FText> InGet,
    TFunction<void(FGameplayTag)> InCommit,
    ECk_DebugRequest_Requirement InRequirement) -> FCkInspectorWidgetBuilder&
{
    namespace builder = ck_inspector_widget_builder;

    if (NOT ck::debug_axes::EditControls_AreVisible(UCkDebuggerStyleSettings::Get_Selection()))
    {
        return AddRow(InLabel,
            [InGet](const FCk_Handle&) { return InGet.Get(); },
            CkStyle::Value_Tag());
    }

    auto Scope = DoMakeEditScope();

    auto Entry = builder::Make_TextEntry(InGet,
        [InCommit](const FString& InText)
        {
            if (NOT InCommit)
            { return; }

            const auto Trimmed = InText.TrimStartAndEnd();

            if (Trimmed.IsEmpty())
            { return; }

            // A tag that does not exist is a typo, not a request to clear the caller's tag —
            // ErrorIfNotFound=false so the debugger never ensures on the user's spelling.
            constexpr auto ErrorIfNotFound = false;
            const auto Tag = FGameplayTag::RequestGameplayTag(FName{*Trimmed}, ErrorIfNotFound);

            if (NOT Tag.IsValid())
            { return; }

            InCommit(Tag);
        },
        Scope);

    DoApplyGate(Entry, TAttribute<FText>{}, InRequirement);

    return DoAddWidgetValueRow(
        InLabel,
        DoComposeEditControl(Entry, InGet),
        [InGet]() { return InGet.Get().ToString(); });
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

auto FCkInspectorWidgetBuilder::Format_Count(int32 InCount) -> FText
{
    auto Options = FNumberFormattingOptions{};
    Options.SetUseGrouping(true);
    Options.SetMinimumFractionalDigits(0);
    Options.SetMaximumFractionalDigits(0);

    return FText::AsNumber(InCount, &Options);
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
    // Interactive rows were composed before the entity was known; they read it live out of this box,
    // which is what keeps the request gate honest when the inspected world changes underneath them.
    *_GateEntity = InEntity;

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
