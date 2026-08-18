#include "CkStyleLabDebugger/Widgets/SCkStyleLab_SamplePane.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Icon.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkEntityDebugOverlay/History/CkDebugOverlay_History.h"
#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_FocusCard.h"
#include "CkEntityDebugOverlay/Style/CkDebugOverlay_RenderStyle.h"

#include "CkInputHudOverlay/Model/CkInputHud_Model.h"
#include "CkInputHudOverlay/Widgets/SCkInputHud_Root.h"

#include "GameplayTagContainer.h"

#include "Misc/App.h"
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

    // Released sample events live one year ahead of the process clock. A negative press age
    // is stable history, not a newly pressed key, so they remain fully visible for a
    // long-lived Style Lab session while retaining their authored duration and frame range.
    constexpr auto InputHudPreviewFutureSeconds = 365.0 * 24.0 * 60.0 * 60.0;

    auto Make_InputHudPreviewModel() -> TSharedPtr<FCk_InputHud_Model>
    {
        auto Model = MakeShared<FCk_InputHud_Model>();
        const auto Now = FApp::GetCurrentTime();
        const auto StableReleasedTime = Now + InputHudPreviewFutureSeconds;

        const auto Move = Model->Open_Event(TEXT("W"), TEXT("W"), 2871, Now - 0.72, false);
        Model->Set_EventResolution(Move, TEXT("Move"), true);

        const auto Modifier = Model->Open_Event(TEXT("LShift"), TEXT("LeftShift"), 2874, Now - 0.16, true);
        Model->Set_EventResolution(Modifier, TEXT("Sprint"), true);

        const auto Jump = Model->Open_Event(TEXT("Space"), TEXT("SpaceBar"), 2860, StableReleasedTime - 0.48, false);
        Model->Close_Event(Jump, 2868, StableReleasedTime);
        Model->Set_EventResolution(Jump, TEXT("Jump"), true);

        const auto Unresolved = Model->Open_Event(TEXT("F"), TEXT("F"), 2852, StableReleasedTime - 0.08, false);
        Model->Close_Event(Unresolved, 2853, StableReleasedTime);
        Model->Set_EventResolution(Unresolved, TEXT("Unrouted"), false);

        Model->Set_ActiveInputType(ECommonInputType::MouseAndKeyboard);
        Model->Set_Layers({{10, TEXT("Gameplay 10")}, {20, TEXT("GLOBAL")}});
        return Model;
    }

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
            .Font(ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall()))
            .ColorAndOpacity(FSlateColor{CkStyle::TextDim()});
    }

    // RowBanding, applied the way a real list surface would: Zebra swaps the row's own fill for the
    // alternating band brush, Hairline keeps the fill and adds a rule under the row, Off does
    // neither. The rule widget is always emitted and collapses at zero thickness.
    auto Get_RowFillBrush(const FCkDebuggerStyleSelection& InSelection, int32 InRowIndex) -> const FSlateBrush*
    {
        return InSelection.RowBanding == ECkDebugAxis_RowBanding::Zebra
            ? ck::debug_axes::Get_RowBandingBrush(InRowIndex)
            : ck::debug_axes::Get_SurfaceBrush(2);
    }

    auto Get_RowFillTint(const FCkDebuggerStyleSelection& InSelection) -> FSlateColor
    {
        // The band brushes carry their own color, so they want an identity tint.
        return InSelection.RowBanding == ECkDebugAxis_RowBanding::Zebra
            ? FSlateColor{FLinearColor::White}
            : FSlateColor{ck::debug_axes::Get_SurfaceTint(2)};
    }

    auto Make_RowRule() -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .HeightOverride_Lambda([]() -> FOptionalSize
            {
                return FOptionalSize{ck::debug_axes::Get_RowBandingRuleThickness()};
            })
            .Visibility_Lambda([]()
            {
                return ck::debug_axes::Get_RowBandingRuleThickness() > 0.0f
                    ? EVisibility::Visible
                    : EVisibility::Collapsed;
            })
            [
                SNew(SBorder)
                    .BorderImage(CkStyle::GetFilledBrush())
                    .BorderBackgroundColor(FSlateColor{CkStyle::Border()})
            ];
    }

    // A stand-in graph node: the two roles GraphNodeStyle modulates are exactly what the SM and
    // GOAP node widgets read, so the swatch moves the same way those graphs will.
    auto Make_GraphNode(const FText& InTitle, bool InIsActive) -> TSharedRef<SWidget>
    {
        const auto Fill   = InIsActive ? CkStyle::NodeFill_InPlan()   : CkStyle::NodeFill_Inactive();
        const auto Border = InIsActive ? CkStyle::NodeBorder_InPlan() : CkStyle::NodeBorder_Inactive();

        const auto Opacity = InIsActive ? 1.0f : ck::debug_axes::Get_NodeInactiveOpacity();

        return SNew(SBorder)
            .BorderImage(ck::debug_axes::Get_CardBrush())
            .BorderBackgroundColor(FSlateColor{CkStyle::OverlayOf(Border, Opacity)})
            .Padding(FMargin{ck::debug_axes::Get_NodeBorderThickness()})
            [
                SNew(SBorder)
                    .BorderImage(ck::debug_axes::Get_CardBrush())
                    .BorderBackgroundColor(FSlateColor{CkStyle::OverlayOf(Fill, Opacity)})
                    .Padding(FMargin{CkStyle::SpaceM, CkStyle::SpaceS})
                    [
                        SNew(STextBlock)
                            .Text(InTitle)
                            .Font(ck::debug_axes::ScaledFont("Bold", CkStyle::NodeTitleFontSize()))
                            .ColorAndOpacity(FSlateColor{CkStyle::OverlayOf(CkStyle::Text(), Opacity)})
                    ]
            ];
    }
}

// ====================================================================================================================

auto
    SCkStyleLab_SamplePane::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _InputHudPreviewModel = ck_style_lab_sample::Make_InputHudPreviewModel();

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
                Build_InputHudPreview(Selection)
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
            ]

        + SVerticalBox::Slot().AutoHeight()
            [
                Build_Separator(Selection)
            ]

        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
            [
                Build_ShapesAndSurfaces(Selection)
            ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkStyleLab_SamplePane::
    Build_InputHudPreview(
        const FCkDebuggerStyleSelection& InSelection) const
    -> TSharedRef<SWidget>
{
    return SNew(SVerticalBox)

        + SVerticalBox::Slot().AutoHeight()
            [
                ck::debug_axes::Make_SectionHeader(
                    InSelection, FText::FromString(TEXT("Input HUD")), ECk_Tone::Accent)
            ]

        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
            [
                SNew(SBorder)
                    .BorderImage(ck::debug_axes::Get_SurfaceBrush(2))
                    .BorderBackgroundColor(FSlateColor{ck::debug_axes::Get_SurfaceTint(2)})
                    .Padding(FMargin{CkStyle::SpaceM})
                    [
                        SNew(SCkInputHud_Root)
                            .Model(_InputHudPreviewModel)
                            .Corner(0)
                            .Scale(1.0f)
                            .Mode(2)
                            .Opacity(1.0f)
                    ]
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

    auto RowIndex = 0;

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
                    .Accent(CkStyle::GetToneColor(Row.Tone))
                    .Size(FVector2D{IconSize, IconSize})
            ];

        // The shipping widget, in preview mode: the Lab has no world, and an invalid handle would
        // collapse all four EntityRefStyle treatments onto the same muted "None". The explicit
        // tooltip replaces the navigator's "click to open" promise, which no preview can keep.
        RowContent->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_EntityRef)
                    .PreviewName(Row.CleanName)
                    .PreviewIdText(Row.IdText)
                    .Tooltip(FText::FromString(TEXT("Style Lab preview — not a live entity")))
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
                    .BorderImage(ck_style_lab_sample::Get_RowFillBrush(InSelection, RowIndex))
                    .BorderBackgroundColor(ck_style_lab_sample::Get_RowFillTint(InSelection))
                    .Padding(RowPadding)
                    [
                        RowContent
                    ]
            ];

        Rows->AddSlot()
            .AutoHeight()
            [
                ck_style_lab_sample::Make_RowRule()
            ];

        ++RowIndex;
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
            .Font(ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeSmall()))
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
            .BorderImage(ck::debug_axes::Get_SurfaceBrush(2))
            .BorderBackgroundColor(FSlateColor{ck::debug_axes::Get_SurfaceTint(2)})
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
    Build_ShapesAndSurfaces(
        const FCkDebuggerStyleSelection& InSelection) const
    -> TSharedRef<SWidget>
{
    auto Body = SNew(SVerticalBox);

    Body->AddSlot().AutoHeight()
        [
            ck::debug_axes::Make_SectionHeader(
                InSelection, FText::FromString(TEXT("Shapes and surfaces")), ECk_Tone::Accent)
        ];

    // ---- CornerStyle: the three brush selectors, side by side -----------------
    const auto MakeCornerSwatch = [](const FSlateBrush* InBrush, const TCHAR* InLabel) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(InBrush)
            .BorderBackgroundColor(FSlateColor{CkStyle::AccentDim()})
            .Padding(FMargin{CkStyle::SpaceM, CkStyle::SpaceS})
            [
                SNew(STextBlock)
                    .Text(FText::FromString(FString{InLabel}))
                    .Font(ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(FSlateColor{CkStyle::Accent()})
            ];
    };

    auto CornerRow = SNew(SHorizontalBox);

    CornerRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [ MakeCornerSwatch(ck::debug_axes::Get_ChipBrush(), TEXT("chip")) ];
    CornerRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [ MakeCornerSwatch(ck::debug_axes::Get_BadgeBrush(), TEXT("badge")) ];
    CornerRow->AddSlot().AutoWidth().VAlign(VAlign_Center)
        [ MakeCornerSwatch(ck::debug_axes::Get_CardBrush(), TEXT("card")) ];

    Body->AddSlot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
        [
            CornerRow
        ];

    // ---- IconTreatment: the same glyph on three feature accents ---------------
    auto IconRow = SNew(SHorizontalBox);

    const auto IconTones = TArray<ECk_Tone>{ECk_Tone::Accent, ECk_Tone::Ok, ECk_Tone::Warn};
    for (const auto Tone : IconTones)
    {
        IconRow->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
            [
                SNew(SCkDebug_Icon)
                    .Brush(FAppStyle::GetBrush("Icons.FilledCircle"))
                    .Meaning(FText::FromString(TEXT("Icon treatment preview")))
                    .ColorAndOpacity(FSlateColor{CkStyle::GetToneColor(Tone)})
                    .Accent(CkStyle::GetToneColor(Tone))
            ];
    }

    IconRow->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            ck_style_lab_sample::Make_KeyLabel(TEXT("icon treatment"))
        ];

    Body->AddSlot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
        [
            IconRow
        ];

    // ---- SurfaceElevation: a depth-1 strip wrapping a depth-2 body ------------
    Body->AddSlot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
        [
            SNew(SBorder)
                .BorderImage(ck::debug_axes::Get_SurfaceBrush(1))
                .BorderBackgroundColor(FSlateColor{ck::debug_axes::Get_SurfaceTint(1)})
                .Padding(FMargin{CkStyle::SpaceM, CkStyle::SpaceS})
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight()
                        [
                            ck_style_lab_sample::Make_KeyLabel(TEXT("depth 1 — strip"))
                        ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
                        [
                            SNew(SBorder)
                                .BorderImage(ck::debug_axes::Get_SurfaceBrush(2))
                                .BorderBackgroundColor(FSlateColor{ck::debug_axes::Get_SurfaceTint(2)})
                                .Padding(FMargin{CkStyle::SpaceM, CkStyle::SpaceS})
                                [
                                    ck_style_lab_sample::Make_KeyLabel(TEXT("depth 2 — body"))
                                ]
                        ]
                ]
        ];

    // ---- GraphNodeStyle: an active node beside a faded inactive one -----------
    constexpr auto NodeIsActive = true;

    Body->AddSlot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    ck_style_lab_sample::Make_GraphNode(
                        FText::FromString(TEXT("Reach_Target")), NodeIsActive)
                ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    ck_style_lab_sample::Make_GraphNode(
                        FText::FromString(TEXT("Gather_Wood")), NOT NodeIsActive)
                ]
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
