#include "CkStyleLabDebugger/Widgets/SCkStyleLab_SamplePane.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Icon.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkEntityDebugOverlay/History/CkDebugOverlay_History.h"
#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_FocusCard.h"
#include "CkEntityDebugOverlay/Style/CkDebugOverlay_RenderStyle.h"

#include "GameplayTagContainer.h"

#include "Styling/AppStyle.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------
// Canned sample content. Every string and number here is authored, never observed — the Lab must
// render the same document with no world loaded (module-unique namespace name: unity builds
// concatenate TUs).
// --------------------------------------------------------------------------------------------------------------------

namespace ck_style_lab_sample
{
    constexpr auto CardWrapWidth      = 520.0f;
    constexpr auto NumericColumnWidth = 96.0f;
    constexpr auto KeyColumnWidth     = 132.0f;

    // Enough rows in one section to overrun the focus card's per-section budget, which is what
    // produces the omission line the sample is supposed to exercise.
    constexpr auto CrowdRowCount = 7;

    auto Get_ProviderTag(const TCHAR* InTagName) -> FGameplayTag
    {
        constexpr auto ErrorIfNotFound = false;
        return FGameplayTag::RequestGameplayTag(FName{InTagName}, ErrorIfNotFound);
    }

    auto Make_Row(
        const TCHAR*                InFieldTag,
        const TCHAR*                InValue,
        ECk_DebugOverlay_Severity   InSeverity,
        int32                       InMergedCount) -> FCk_DebugOverlay_Row
    {
        auto Row         = FCk_DebugOverlay_Row{};
        Row.FieldTag     = Get_ProviderTag(InFieldTag);
        Row.Value        = FText::FromString(FString{InValue});
        Row.Severity     = InSeverity;
        Row.MergedCount  = InMergedCount;
        return Row;
    }

    auto Make_Section(const TCHAR* InProviderTag, uint32 InSourceEntityId, int32 InSourceOrder)
        -> FCk_DebugOverlay_Section
    {
        auto Section           = FCk_DebugOverlay_Section{};
        Section.ProviderTag    = Get_ProviderTag(InProviderTag);
        Section.SourceEntityId = InSourceEntityId;
        Section.SourceOrder    = InSourceOrder;
        return Section;
    }

    // Worst case on purpose: four providers so the legend has something to dedupe, a merged row,
    // a sub-entity source chip, and one over-budget section so the omission summary appears.
    auto Make_CardModel() -> FCk_DebugOverlay_EntityModel
    {
        auto Model   = FCk_DebugOverlay_EntityModel{};
        Model.Header = FText::FromString(TEXT("Guard_Patrol_01"));

        auto Label = Make_Section(TEXT("Ck.OnScreenDebugger.Provider.Label"), 4211, 0);
        Label.Rows.Add(Make_Row(
            TEXT("Ck.OnScreenDebugger.Provider.Label.Value"), TEXT("Guard_Patrol_01"),
            ECk_DebugOverlay_Severity::Normal, 1));
        Model.Sections.Add(Label);

        auto Jolt = Make_Section(TEXT("Ck.OnScreenDebugger.Provider.Jolt"), 4211, 1);
        Jolt.Rows.Add(Make_Row(
            TEXT("Ck.OnScreenDebugger.Provider.Jolt.MotionType"), TEXT("Kinematic"),
            ECk_DebugOverlay_Severity::Normal, 1));
        Jolt.Rows.Add(Make_Row(
            TEXT("Ck.OnScreenDebugger.Provider.Jolt.SleepState"), TEXT("Awake"),
            ECk_DebugOverlay_Severity::Good, 1));
        Model.Sections.Add(Jolt);

        auto Attributes = Make_Section(TEXT("Ck.OnScreenDebugger.Provider.FloatAttributes"), 4213, 2);
        Attributes.SourceName = FText::FromString(TEXT("Health"));
        Attributes.Rows.Add(Make_Row(
            TEXT("Ck.OnScreenDebugger.Provider.FloatAttributes.Value"), TEXT("42.0 / 120.0"),
            ECk_DebugOverlay_Severity::Warn, 1));
        Attributes.Rows.Add(Make_Row(
            TEXT("Ck.OnScreenDebugger.Provider.FloatAttributes.Value"), TEXT("0.0 / 60.0"),
            ECk_DebugOverlay_Severity::Bad, 1));
        Model.Sections.Add(Attributes);

        auto Crowd = Make_Section(TEXT("Ck.OnScreenDebugger.Provider.Crowd"), 4211, 3);
        Crowd.Rows.Add(Make_Row(
            TEXT("Ck.OnScreenDebugger.Provider.Crowd.Status"), TEXT("Following"),
            ECk_DebugOverlay_Severity::Normal, 1));
        Crowd.Rows.Add(Make_Row(
            TEXT("Ck.OnScreenDebugger.Provider.Crowd.Speed"), TEXT("312.4"),
            ECk_DebugOverlay_Severity::Normal, 1));
        // The merged row: twelve lifetime descendants reported the identical neighbour count.
        Crowd.Rows.Add(Make_Row(
            TEXT("Ck.OnScreenDebugger.Provider.Crowd.Neighbors"), TEXT("6"),
            ECk_DebugOverlay_Severity::Normal, 12));
        Crowd.Rows.Add(Make_Row(
            TEXT("Ck.OnScreenDebugger.Provider.Crowd.Goal"), TEXT("(1240.0, -880.0, 92.0)"),
            ECk_DebugOverlay_Severity::Normal, 1));

        for (auto Index = Crowd.Rows.Num(); Index < CrowdRowCount; ++Index)
        {
            Crowd.Rows.Add(Make_Row(
                TEXT("Ck.OnScreenDebugger.Provider.Crowd.Status"), TEXT("(over budget)"),
                ECk_DebugOverlay_Severity::Normal, 1));
        }

        Model.Sections.Add(Crowd);

        return Model;
    }

    struct FSampleTreeRow
    {
        FString CleanName;
        FString IdText;
        FString FoldChipText;
        FString BadgeText;
        ECk_Tone Tone = ECk_Tone::Neutral;
    };

    auto Get_TreeRows() -> const TArray<FSampleTreeRow>&
    {
        static const auto Rows = []() -> TArray<FSampleTreeRow>
        {
            auto Result = TArray<FSampleTreeRow>{};
            Result.Add(FSampleTreeRow{TEXT("Guard_Patrol_01"), TEXT("412|3(198)"), {}, {}, ECk_Tone::Neutral});
            Result.Add(FSampleTreeRow{{}, TEXT("577|1(577)"), {}, {}, ECk_Tone::Neutral});
            Result.Add(FSampleTreeRow{TEXT("Probe_Interaction_Sphere"), TEXT("613|2(357)"), {}, TEXT("3"), ECk_Tone::Info});
            Result.Add(FSampleTreeRow{TEXT("Attribute_Float"), TEXT("704|1(704)"), TEXT("+3"), TEXT("12"), ECk_Tone::Accent});
            return Result;
        }();

        return Rows;
    }

    struct FSampleValueRow
    {
        FString Key;
        FString Value;
    };

    auto Get_TransformRows() -> const TArray<FSampleValueRow>&
    {
        static const auto Rows = []() -> TArray<FSampleValueRow>
        {
            auto Result = TArray<FSampleValueRow>{};
            Result.Add(FSampleValueRow{TEXT("Location"), TEXT("1240.000")});
            Result.Add(FSampleValueRow{TEXT("Rotation"), TEXT("-88.250")});
            Result.Add(FSampleValueRow{TEXT("Scale"), TEXT("1.000")});
            return Result;
        }();

        return Rows;
    }

    auto Get_NetworkRows() -> const TArray<FSampleValueRow>&
    {
        static const auto Rows = []() -> TArray<FSampleValueRow>
        {
            auto Result = TArray<FSampleValueRow>{};
            Result.Add(FSampleValueRow{TEXT("Net Role"), TEXT("Authority")});
            Result.Add(FSampleValueRow{TEXT("Replication"), TEXT("Enabled")});
            Result.Add(FSampleValueRow{TEXT("Pending Reps"), TEXT("4")});
            return Result;
        }();

        return Rows;
    }

    struct FSampleTone
    {
        ECk_Tone Tone = ECk_Tone::Neutral;
        FString  Name;

        // Off by default the chip row shows only the tones the real inspector badge boxes use
        // most; the window's "All Tones" action widens it to the full semantic set.
        bool     IsCommon = false;
    };

    auto Get_ToneChips() -> const TArray<FSampleTone>&
    {
        static const auto Tones = []() -> TArray<FSampleTone>
        {
            auto Result = TArray<FSampleTone>{};
            Result.Add(FSampleTone{ECk_Tone::Neutral, TEXT("Neutral"), true});
            Result.Add(FSampleTone{ECk_Tone::Info,    TEXT("Info"),    false});
            Result.Add(FSampleTone{ECk_Tone::Ok,      TEXT("Ok"),      true});
            Result.Add(FSampleTone{ECk_Tone::Warn,    TEXT("Warn"),    true});
            Result.Add(FSampleTone{ECk_Tone::Err,     TEXT("Err"),     false});
            Result.Add(FSampleTone{ECk_Tone::Accent,  TEXT("Accent"),  false});
            return Result;
        }();

        return Tones;
    }

    auto Make_KeyLabel(const FString& InKey) -> TSharedRef<SWidget>
    {
        return SNew(STextBlock)
            .Text(FText::FromString(InKey))
            .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
            .ColorAndOpacity(FSlateColor{CkStyle::TextDim()});
    }
}

// ====================================================================================================================

auto
    SCkStyleLab_SamplePane::
    Construct(
        const FArguments& InArgs)
    -> void
{
    ChildSlot
    [
        SAssignNew(_Root, SBorder)
            .BorderImage(CkStyle::GetRoundedBrush_Large())
            .BorderBackgroundColor(FSlateColor{CkStyle::Bg1()})
            .Padding(FMargin{CkStyle::SpaceL})
            [
                Build_Body()
            ]
    ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkStyleLab_SamplePane::
    RequestRebuild()
    -> void
{
    if (NOT _Root.IsValid())
    { return; }

    _Root->SetContent(Build_Body());
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkStyleLab_SamplePane::
    Set_ShowAllTones(
        bool InShowAllTones)
    -> void
{
    if (_ShowAllTones == InShowAllTones)
    { return; }

    _ShowAllTones = InShowAllTones;
    RequestRebuild();
}

// ====================================================================================================================

auto
    SCkStyleLab_SamplePane::
    Build_Body() const
    -> TSharedRef<SWidget>
{
    const auto& Selection = UCkDebuggerStyleSettings::Get_Selection();

    return SNew(SVerticalBox)

        + SVerticalBox::Slot().AutoHeight()
            [
                ck::debug_axes::Make_SectionHeader(
                    Selection, FText::FromString(TEXT("On-screen focus card")), ECk_Tone::Accent)
            ]

        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
            [
                Build_FocusCard()
            ]

        + SVerticalBox::Slot().AutoHeight()
            [
                Build_Separator(Selection)
            ]

        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
            [
                ck::debug_axes::Make_SectionHeader(
                    Selection, FText::FromString(TEXT("Entity tree")), ECk_Tone::Accent)
            ]

        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
            [
                Build_TreeRows(Selection)
            ]

        + SVerticalBox::Slot().AutoHeight()
            [
                Build_Separator(Selection)
            ]

        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
            [
                Build_Inspector(Selection)
            ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkStyleLab_SamplePane::
    Build_FocusCard() const
    -> TSharedRef<SWidget>
{
    // The shipping widget, fed a canned model — so whatever U3 wires into the card converges here
    // for free. The card is HitTestInvisible by design; in the Lab that only means it is inert.
    auto Card = SNew(SCkDebugOverlay_FocusCard);
    Card->Set_WrapWidth(ck_style_lab_sample::CardWrapWidth);

    const auto Model   = ck_style_lab_sample::Make_CardModel();
    const auto Style   = FCk_DebugOverlay_RenderStyle{};
    const auto History = FCk_DebugOverlay_History{};

    constexpr auto Now      = 0.0;
    constexpr auto IsLocked = false;
    constexpr auto IsPinned = true;

    Card->Set_Model(Model, Style, History, Now, IsLocked, IsPinned);

    return SNew(SBox)
        .MaxDesiredWidth(ck_style_lab_sample::CardWrapWidth + CkStyle::SpaceXXL)
        .HAlign(HAlign_Left)
        [
            Card
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkStyleLab_SamplePane::
    Build_TreeRows(
        const FCkDebuggerStyleSelection& InSelection) const
    -> TSharedRef<SWidget>
{
    const auto RowPadding = ck::debug_axes::Get_RowPadding(InSelection);
    const auto IconSize   = ck::debug_axes::Get_IconSize(InSelection);

    auto Rows = SNew(SVerticalBox);

    for (const auto& Row : ck_style_lab_sample::Get_TreeRows())
    {
        auto RowContent = SNew(SHorizontalBox);

        RowContent->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SCkDebug_Icon)
                    .Brush(FAppStyle::GetBrush("Icons.FilledCircle"))
                    .Meaning(FText::FromString(TEXT("Entity is alive")))
                    .ColorAndOpacity(FSlateColor{CkStyle::GetToneColor(Row.Tone)})
                    .Size(FVector2D{IconSize, IconSize})
            ];

        RowContent->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_SelectableLabel)
                    .Text(ck::debug_axes::Make_EntityIdText(InSelection, Row.CleanName, Row.IdText))
                    .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(FSlateColor{
                        Row.CleanName.IsEmpty() ? CkStyle::TextMute() : CkStyle::Text()})
            ];

        if (NOT Row.FoldChipText.IsEmpty())
        {
            RowContent->AddSlot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
                [
                    ck::debug_axes::Make_FoldChip(
                        InSelection, FText::FromString(Row.FoldChipText), ECk_Tone::Neutral)
                ];
        }

        if (NOT Row.BadgeText.IsEmpty())
        {
            RowContent->AddSlot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
                [
                    ck::debug_axes::Make_Badge(
                        InSelection, FText::FromString(Row.BadgeText), Row.Tone)
                ];
        }

        Rows->AddSlot()
            .AutoHeight()
            [
                SNew(SBorder)
                    .BorderImage(CkStyle::GetFilledBrush())
                    .BorderBackgroundColor(FSlateColor{CkStyle::Bg2()})
                    .Padding(RowPadding)
                    [
                        RowContent
                    ]
            ];
    }

    return Rows;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkStyleLab_SamplePane::
    Build_Inspector(
        const FCkDebuggerStyleSelection& InSelection) const
    -> TSharedRef<SWidget>
{
    const auto RowPadding      = ck::debug_axes::Get_RowPadding(InSelection);
    const auto UseAlignedCols  = ck::debug_axes::Values_UseAlignedColumns(InSelection);
    const auto AlignRight      = ck::debug_axes::Values_AlignRight(InSelection);

    const auto MakeValueRow = [&](const ck_style_lab_sample::FSampleValueRow& InRow) -> TSharedRef<SWidget>
    {
        auto Value = SNew(SCkDebug_SelectableLabel)
            .Text(FText::FromString(InRow.Value))
            .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
            .ColorAndOpacity(FSlateColor{CkStyle::Value_Numeric()});

        // AlignedColumns is the one option that needs geometry, not just alignment: a fixed-width
        // right-aligned box is what makes decimal points line up down the column.
        auto ValueSlotContent = UseAlignedCols
            ? StaticCastSharedRef<SWidget>(
                SNew(SBox)
                    .WidthOverride(ck_style_lab_sample::NumericColumnWidth)
                    .HAlign(HAlign_Right)
                    [
                        Value
                    ])
            : StaticCastSharedRef<SWidget>(Value);

        const auto ValueAlignment = (UseAlignedCols || AlignRight) ? HAlign_Right : HAlign_Left;

        return SNew(SBorder)
            .BorderImage(CkStyle::GetFilledBrush())
            .BorderBackgroundColor(FSlateColor{CkStyle::Bg2()})
            .Padding(RowPadding)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(SBox)
                            .WidthOverride(ck_style_lab_sample::KeyColumnWidth)
                            [
                                ck_style_lab_sample::Make_KeyLabel(InRow.Key)
                            ]
                    ]

                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).HAlign(ValueAlignment)
                    [
                        ValueSlotContent
                    ]
            ];
    };

    auto Body = SNew(SVerticalBox);

    Body->AddSlot().AutoHeight()
        [
            ck::debug_axes::Make_SectionHeader(
                InSelection, FText::FromString(TEXT("Transform")), ECk_Tone::Info)
        ];

    for (const auto& Row : ck_style_lab_sample::Get_TransformRows())
    { Body->AddSlot().AutoHeight()[MakeValueRow(Row)]; }

    Body->AddSlot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
        [
            Build_Separator(InSelection)
        ];

    Body->AddSlot().AutoHeight()
        [
            ck::debug_axes::Make_SectionHeader(
                InSelection, FText::FromString(TEXT("Network")), ECk_Tone::Info)
        ];

    for (const auto& Row : ck_style_lab_sample::Get_NetworkRows())
    { Body->AddSlot().AutoHeight()[MakeValueRow(Row)]; }

    Body->AddSlot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
        [
            Build_Separator(InSelection)
        ];

    Body->AddSlot().AutoHeight()
        [
            ck::debug_axes::Make_SectionHeader(
                InSelection, FText::FromString(TEXT("Features")), ECk_Tone::Info)
        ];

    auto BadgeBox = SNew(SWrapBox).UseAllottedSize(true);

    for (const auto& ToneChip : ck_style_lab_sample::Get_ToneChips())
    {
        if (NOT _ShowAllTones && NOT ToneChip.IsCommon)
        { continue; }

        BadgeBox->AddSlot()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, CkStyle::SpaceXS)
            [
                ck::debug_axes::Make_Chip(InSelection, FText::FromString(ToneChip.Name), ToneChip.Tone)
            ];
    }

    Body->AddSlot().AutoHeight().Padding(RowPadding)
        [
            BadgeBox
        ];

    return Body;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkStyleLab_SamplePane::
    Build_Separator(
        const FCkDebuggerStyleSelection& InSelection) const
    -> TSharedRef<SWidget>
{
    const auto Thickness = ck::debug_axes::Get_SeparatorThickness(InSelection);

    // Zero means "draw nothing" — an SSeparator with 0 thickness would still occupy its slot.
    if (Thickness <= 0.0f)
    { return SNullWidget::NullWidget; }

    return SNew(SSeparator).Thickness(Thickness);
}

// ====================================================================================================================
