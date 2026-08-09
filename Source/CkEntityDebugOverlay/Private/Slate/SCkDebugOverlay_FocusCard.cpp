// Implements Ultra-density focus card for the on-screen entity debug overlay.

#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_FocusCard.h"

#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"
#include "CkEntityDebugOverlay/Style/CkDebugOverlay_RenderStyle.h"
#include "CkEntityDebugOverlay/History/CkDebugOverlay_History.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"
#include "CkEntityDebugOverlay/Presentation/CkDebugOverlay_FocusCardBudget.h"
#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"

#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"

#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"

// ====================================================================================================================

namespace FocusCard_Constants
{
    // Breadcrumb separator between history trail entries
    constexpr auto BreadcrumbSeparator = TEXT(" ‹ "); // " ‹ " (U+2039)

    // Flash window in seconds
    constexpr auto FlashDurationSec = 0.4;
}

// ====================================================================================================================
// STYLE AXES — the card's local renderers.
//
// The card rebuilds every slot on every Set_Model, so it needs no attribute plumbing and no
// revision poll: reading the selection here IS live apply.
//
// These deliberately do NOT call the ck::debug_axes::Make_* renderers, and the reason is the
// same in all three cases: those helpers bake a fixed CkStyle font and take an ECk_Tone, while
// this card (a) scales every font by the user's PlateFontScale and (b) colors chips from the
// per-provider HASHED hue, which is not a tone and cannot be reconstructed from one. Routing
// through them would silently drop font scaling and collapse the provider color coding the
// legend pairs against. The OPTION enums remain the single source of truth; only the widget
// construction is local. See the U3 report for the library gap this records.
// ====================================================================================================================

namespace ck_debugoverlay_focuscard
{
    // Dark ink for text sitting on a saturated provider fill.
    constexpr auto ChipInk = FLinearColor{ 0.04f, 0.07f, 0.10f, 1.0f };

    auto Get_Selection() -> const FCkDebuggerStyleSelection&
    {
        return UCkDebuggerStyleSettings::Get_Selection();
    }

    // RowDensity as a DELTA on the card's own base spacing, never as an absolute: the card is
    // deliberately denser than the editor tree, and Get_RowPadding's absolutes would blow that
    // compactness apart. Comfortable => zero delta => the card as shipped.
    auto Apply_RowDensity(const FMargin& InBase) -> FMargin
    {
        const auto Baseline = ck::debug_axes::Get_RowPadding(FCkDebuggerStyleSelection{});
        const auto Current  = ck::debug_axes::Get_RowPadding(Get_Selection());

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

    // ProviderChipStyle. Tint is the pill exactly as the card shipped.
    auto Make_ProviderChip(
        const FText&        InText,
        const FLinearColor& InProviderColor,
        int32               InFontSize) -> TSharedRef<SWidget>
    {
        const auto Make_Pill = [&](const FLinearColor& InFill, const FLinearColor& InInk) -> TSharedRef<SWidget>
        {
            return SNew(SBorder)
                .BorderImage(CkStyle::GetRoundedBrush())
                .BorderBackgroundColor(InFill)
                .VAlign(VAlign_Center)
                .Padding(FMargin{ CkStyle::SpaceS, 1.0f })
                [
                    SNew(STextBlock)
                        .Text(InText)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", InFontSize))
                        .ColorAndOpacity(InInk)
                ];
        };

        switch (Get_Selection().ProviderChipStyle)
        {
            case ECkDebugAxis_ProviderChipStyle::Tint:
                return Make_Pill(InProviderColor, ChipInk);

            case ECkDebugAxis_ProviderChipStyle::Solid:
                return Make_Pill(InProviderColor, CkStyle::TextStrong());

            // The card's provider label is ALREADY the abbreviation, so this option is the
            // pill-less reading of it rather than a second truncation.
            case ECkDebugAxis_ProviderChipStyle::AbbrevOnly:
                return SNew(STextBlock)
                    .Text(InText)
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", InFontSize))
                    .ColorAndOpacity(InProviderColor);
        }

        return Make_Pill(InProviderColor, ChipInk);
    }

    // ChipStyle, for the field chips. Tint is the chip exactly as the card shipped.
    auto Make_FieldChip(
        const TSharedRef<SWidget>& InContent,
        const FLinearColor&        InProviderColor) -> TSharedRef<SWidget>
    {
        const auto Padding = FMargin{ CkStyle::SpaceS, 1.0f };

        const auto Make_Filled = [&](const FLinearColor& InFill) -> TSharedRef<SWidget>
        {
            return SNew(SBorder)
                .BorderImage(CkStyle::GetRoundedBrush())
                .BorderBackgroundColor(InFill)
                .VAlign(VAlign_Center)
                .Padding(Padding)
                [
                    InContent
                ];
        };

        switch (Get_Selection().ChipStyle)
        {
            case ECkDebugAxis_ChipStyle::Tint:
                return Make_Filled(CkStyle::OverlayOf(InProviderColor, 0.18f));

            case ECkDebugAxis_ChipStyle::Solid:
                return Make_Filled(CkStyle::OverlayOf(InProviderColor, 0.75f));

            case ECkDebugAxis_ChipStyle::Outline:
                return SNew(SBorder)
                    .BorderImage(CkStyle::GetRoundedBrush())
                    .BorderBackgroundColor(InProviderColor)
                    .VAlign(VAlign_Center)
                    .Padding(FMargin{ 1.0f })
                    [
                        SNew(SBorder)
                            .BorderImage(CkStyle::GetRoundedBrush())
                            .BorderBackgroundColor(CkStyle::Bg2())
                            .VAlign(VAlign_Center)
                            .Padding(Padding)
                            [
                                InContent
                            ]
                    ];

            case ECkDebugAxis_ChipStyle::TextOnly:
                return InContent;
        }

        return Make_Filled(CkStyle::OverlayOf(InProviderColor, 0.18f));
    }

    // BadgeStyle, for the merged-row count badge.
    auto Make_Badge(const FText& InText, int32 InFontSize) -> TSharedRef<SWidget>
    {
        const auto Tone = CkStyle::GetToneColor(ECk_Tone::Neutral);

        const auto Make_Label = [&](const FLinearColor& InInk) -> TSharedRef<SWidget>
        {
            return SNew(STextBlock)
                .Text(InText)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", InFontSize))
                .ColorAndOpacity(InInk);
        };

        switch (Get_Selection().BadgeStyle)
        {
            case ECkDebugAxis_BadgeStyle::Solid:
                return SNew(SBorder)
                    .BorderImage(CkStyle::GetRoundedBrush_Small())
                    .BorderBackgroundColor(Tone)
                    .VAlign(VAlign_Center)
                    .Padding(FMargin{ CkStyle::SpaceS, 1.0f })
                    [
                        Make_Label(CkStyle::TextStrong())
                    ];

            case ECkDebugAxis_BadgeStyle::Hollow:
                return SNew(SBorder)
                    .BorderImage(CkStyle::GetRoundedBrush_Small())
                    .BorderBackgroundColor(Tone)
                    .VAlign(VAlign_Center)
                    .Padding(FMargin{ 1.0f })
                    [
                        SNew(SBorder)
                            .BorderImage(CkStyle::GetRoundedBrush_Small())
                            .BorderBackgroundColor(CkStyle::Bg2())
                            .VAlign(VAlign_Center)
                            .Padding(FMargin{ CkStyle::SpaceS, 1.0f })
                            [
                                Make_Label(Tone)
                            ]
                    ];

            case ECkDebugAxis_BadgeStyle::CountOnly:
                return Make_Label(Tone);
        }

        return Make_Label(Tone);
    }

    // MergeCountDisplay. SuffixText reproduces the card's muted-italic xN exactly, including
    // the font scaling the library helper cannot express. Null = render no slot at all.
    auto Make_MergeCount(int32 InCount, int32 InFontSize) -> TSharedPtr<SWidget>
    {
        if (InCount <= 1)
        { return nullptr; }

        switch (Get_Selection().MergeCountDisplay)
        {
            // "x" here is MULTIPLICATION SIGN (U+00D7), same literal-UTF-8 convention as
            // FocusCard_Constants::BreadcrumbSeparator.
            case ECkDebugAxis_MergeCountDisplay::SuffixText:
                return SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("×%d"), InCount)))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", InFontSize))
                    .ColorAndOpacity(CkStyle::TextMute());

            case ECkDebugAxis_MergeCountDisplay::CountBadge:
                return Make_Badge(FText::AsNumber(InCount), InFontSize);

            case ECkDebugAxis_MergeCountDisplay::Hidden:
                return nullptr;
        }

        return SNew(STextBlock)
            .Text(FText::FromString(FString::Printf(TEXT("×%d"), InCount)))
            .Font(FCoreStyle::GetDefaultFontStyle("Italic", InFontSize))
            .ColorAndOpacity(CkStyle::TextMute());
    }
}

// ====================================================================================================================

auto
    SCkDebugOverlay_FocusCard::
    Construct(const FArguments& InArgs)
    -> void
{
    SetVisibility(EVisibility::HitTestInvisible);

    SAssignNew(_ContentBox, SVerticalBox);

    // Dark, rounded, translucent card panel behind the content — turns the
    // floating chips/text into a readable "card" matching the design mockup.
    // Wrapped in an outer SBorder (_LockFrame) that renders a 2px amber ring
    // when the focus is locked (transparent otherwise).
    ChildSlot
    [
        SAssignNew(_LockFrame, SBorder)
            .BorderImage(CkStyle::GetRoundedBrush())
            .BorderBackgroundColor(FLinearColor::Transparent)   // no ring unless locked
            .Padding(FMargin{ 2.0f })                            // ring thickness
            [
                SNew(SBorder)
                    .BorderImage(CkStyle::GetRoundedBrush())
                    .BorderBackgroundColor(CkStyle::OverlayOf(CkStyle::BgRoot(), 0.9f))
                    .Padding(FMargin{ CkStyle::SpaceM })
                    [
                        _ContentBox.ToSharedRef()
                    ]
            ]
    ];
}

// ====================================================================================================================

auto
    SCkDebugOverlay_FocusCard::
    Set_Model(
        const FCk_DebugOverlay_EntityModel& InModel,
        const FCk_DebugOverlay_RenderStyle& InStyle,
        const FCk_DebugOverlay_History&     InHistory,
        double                              InNow,
        bool                                bIsLocked,
        bool                                bIsPinned,
        int32                               InCoLocatedIndex,
        int32                               InCoLocatedCount)
    -> void
{
    if (NOT _ContentBox.IsValid())
    {
        return;
    }

    // Ring color: pinned (cyan) takes precedence over focus-lock (amber); else none.
    if (_LockFrame.IsValid())
    {
        const auto RingColor =
            bIsPinned ? FLinearColor{ 0.0f, 0.80f, 0.95f, 1.0f }
          : bIsLocked ? FLinearColor{ 1.0f, 0.82f, 0.0f, 1.0f }
          :             FLinearColor::Transparent;
        _LockFrame->SetBorderBackgroundColor(RingColor);
    }

    _ContentBox->ClearChildren();

    // Settings-driven font scaling (PlateFontScale × layout FontScale), floored so
    // text never becomes unreadable.
    const auto FontScale  = FMath::Clamp(InStyle.FontScale, 0.5f, 3.0f);
    const auto ScaledFont = [FontScale](int32 InBaseSize) -> int32
    {
        return FMath::Max(6, FMath::RoundToInt(InBaseSize * FontScale));
    };

    // ---- Header row — optional "i/N" co-located badge + SCkDebug_EntityRef pill ----
    // The overlay is HitTestInvisible, so the EntityRef click is a no-op — but we still get
    // the canonical entity rendering (ID, version, debug name).
    auto HeaderRow = SNew(SHorizontalBox);

    if (InCoLocatedCount > 1)
    {
        const auto IndexDisplay = InCoLocatedIndex >= 0 ? InCoLocatedIndex + 1 : 0;
        const auto IndexText    = FString::Printf(TEXT("%d/%d"), IndexDisplay, InCoLocatedCount);

        HeaderRow->AddSlot()
            .AutoWidth().VAlign(VAlign_Center)
            .Padding(FMargin{ 0.0f, 0.0f, CkStyle::SpaceXS, 0.0f })
            [
                SNew(SBorder)
                    .BorderImage(CkStyle::GetRoundedBrush())
                    .BorderBackgroundColor(FLinearColor{ 0.0f, 0.80f, 0.95f, 1.0f })
                    .VAlign(VAlign_Center)
                    .Padding(FMargin{ CkStyle::SpaceS, 1.0f })
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(IndexText))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", ScaledFont(CkStyle::FontSizeMicro())))
                            .ColorAndOpacity(FLinearColor{ 0.04f, 0.07f, 0.10f, 1.0f })
                    ]
            ];
    }

    HeaderRow->AddSlot()
        .AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SCkDebug_EntityRef)
                .Entity(InModel.Entity)
                .ShowName(true)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", ScaledFont(CkStyle::FontSizeH3())))
        ];

    _ContentBox->AddSlot()
        .AutoHeight()
        .Padding(ck_debugoverlay_focuscard::Apply_RowDensity(
            FMargin{ 0.0f, 0.0f, 0.0f, CkStyle::SpaceXS }))
        [
            HeaderRow
        ];

    // ---- Sections (sorted by SortPriority, then source order) ----
    // Copy the sections array so we can sort without mutating the model. The SourceOrder
    // tie-break is load-bearing: subtree aggregation emits one section per source for the
    // same provider (equal priorities), and an unstable sort over equal keys reorders the
    // card every tick.
    const auto* Settings = GetDefault<UCk_DebugOverlay_Settings>();
    const auto BudgetedModel = ck_debugoverlay::Apply_FocusCardBudget(InModel,
        { Settings ? Settings->FocusCardMaxRowsPerSection : 4, Settings ? Settings->FocusCardMaxRows : 18 });
    const auto& SortedSections = BudgetedModel.Sections;

    // Derive a stable entity id for the history key.
    // FCk_Entity::Get_EntityNumber() returns the entt entity index (uint32-compatible).
    const auto EntityId = static_cast<uint32>(InModel.Entity.Get_Entity().Get_EntityNumber());

    // LegendMode. Deduped (the default) keeps P0's one-entry-per-PROVIDER strip; PerSection
    // restores the pre-P0 reading where every rendered section gets its own entry; Off skips
    // the strip entirely and builds nothing.
    const auto& Selection = ck_debugoverlay_focuscard::Get_Selection();

    const auto LegendEntries = [&]() -> TArray<FCk_DebugOverlay_LegendEntry>
    {
        if (NOT ck::debug_axes::Legend_IsVisible(Selection))
        { return {}; }

        return ck::debug_axes::Legend_IsDeduped(Selection)
            ? Build_LegendEntries(SortedSections)
            : Build_LegendEntries_PerSection(SortedSections);
    }();

    auto InputRowCount = 0;
    for (const auto& Section : InModel.Sections)
    { InputRowCount += Section.Rows.Num(); }
    auto RenderedRowCount = 0;

    for (const auto& Section : SortedSections)
    {
        RenderedRowCount += Section.Rows.Num();
        // A section whose provider contributed no rows renders as a lone chip /
        // blank-looking line — skip it entirely.
        if (Section.Rows.IsEmpty() && Section.OmittedRowCount == 0)
        { continue; }

        const auto ProviderColor = Get_ProviderColor(Section.ProviderTag);
        const auto ProviderLeaf  = ck_debugoverlay::Get_LeafName(Section.ProviderTag);
        const auto ProviderAbbr  = ck_debugoverlay::Get_ProviderAbbrev(ProviderLeaf);
        const auto ProviderName  = FText::FromString(ProviderAbbr);

        // One color-grouped flow line per section:
        //   [PROVIDER]  [KEY value]  [KEY trail‹ value] ...
        // Explicit PreferredSize, NOT UseAllottedSize: the wrap boxes are recreated
        // every Set_Model call, so the allotted-size sync (which happens in Tick)
        // never runs — desired size would wrap at the 100px default and reserve
        // phantom empty lines under each section.
        auto SectionRow = SNew(SWrapBox)
            .PreferredSize(_WrapWidth);

        // Provider chip — ProviderChipStyle axis; Tint is the shipped provider-colored pill.
        SectionRow->AddSlot()
            .VAlign(VAlign_Center)
            .Padding(FMargin{ 0.0f, 0.0f, CkStyle::SpaceXS, CkStyle::SpaceXS })
            [
                ck_debugoverlay_focuscard::Make_ProviderChip(
                    ProviderName, ProviderColor, ScaledFont(CkStyle::FontSizeMicro()))
            ];

        // Source chip — dim sub-entity name when the section came from a descendant
        // (subtree aggregation), so the reader can tell WHOSE SM/attribute this is.
        if (NOT Section.SourceName.IsEmpty())
        {
            SectionRow->AddSlot()
                .VAlign(VAlign_Center)
                .Padding(FMargin{ 0.0f, 0.0f, CkStyle::SpaceXS, CkStyle::SpaceXS })
                [
                    SNew(STextBlock)
                        .Text(Section.SourceName)
                        .Font(FCoreStyle::GetDefaultFontStyle("Italic", ScaledFont(CkStyle::FontSizeMicro())))
                        .ColorAndOpacity(CkStyle::TextMute())
                ];
        }

        // Field chips share a provider-derived key color; the chip's own fill is the
        // ChipStyle axis' business (Make_FieldChip below).
        const auto FieldKeyColor = CkStyle::OverlayOf(ProviderColor, 0.95f);

        for (const auto& Row : Section.Rows)
        {
            // Rows with no value and no explicit history render as an empty chip —
            // skip them (providers sometimes emit placeholders for absent data).
            if (Row.Value.IsEmpty() && Row.ExplicitHistory.IsEmpty())
            { continue; }

            // Subtree aggregation: history buckets by the section's SOURCE entity (the
            // same field on two sub-entities must not share a trail). 0 = legacy model
            // built without source info — fall back to the focus entity id.
            const auto RowEntityId = Section.SourceEntityId != 0 ? Section.SourceEntityId : EntityId;
            const FCk_DebugOverlay_HistoryKey HistKey{ RowEntityId, Row.FieldTag };

            // Breadcrumb trail (explicit history wins; else diff-tracked ring).
            FString TrailText;
            if (Row.ExplicitHistory.Num() > 0)
            {
                for (auto Idx = 0; Idx < Row.ExplicitHistory.Num(); ++Idx)
                {
                    if (Idx > 0) { TrailText += FocusCard_Constants::BreadcrumbSeparator; }
                    TrailText += Row.ExplicitHistory[Idx].ToString();
                }
            }
            else if (InStyle.HistoryDepth > 0)
            {
                const auto Trail = InHistory.Get_Trail(HistKey, InStyle.HistoryDepth);
                for (auto Idx = 0; Idx < Trail.Num(); ++Idx)
                {
                    if (Idx > 0) { TrailText += FocusCard_Constants::BreadcrumbSeparator; }
                    TrailText += Trail[Idx];
                }
            }

            // Value color from severity; Normal uses the strong text color.
            auto ValueColor = (Row.Severity == ECk_DebugOverlay_Severity::Normal)
                ? CkStyle::TextStrong()
                : CkStyle::GetToneColor(Severity_To_Tone(Row.Severity));

            if (InStyle.bFlashOnChange)
            {
                const auto Alpha = Get_FlashAlpha(InHistory.Get_LastChangedTime(HistKey), InNow);
                ValueColor.A = FMath::Clamp(ValueColor.A * Alpha, 0.3f, 1.0f);
            }

            const auto FieldLabel = FText::FromString(
                ck_debugoverlay::Get_LeafName(Row.FieldTag).ToUpper());

            // Inner chip layout: [KEY] [faint trail ‹] [value]
            auto ChipInner = SNew(SHorizontalBox);

            ChipInner->AddSlot()
                .AutoWidth().VAlign(VAlign_Center)
                .Padding(FMargin{ 0.0f, 0.0f, CkStyle::SpaceXS, 0.0f })
                [
                    SNew(STextBlock)
                        .Text(FieldLabel)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", ScaledFont(CkStyle::FontSizeMicro())))
                        .ColorAndOpacity(FieldKeyColor)
                ];

            if (NOT TrailText.IsEmpty())
            {
                ChipInner->AddSlot()
                    .AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TrailText + FocusCard_Constants::BreadcrumbSeparator))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", ScaledFont(CkStyle::FontSizeSmall())))
                            .ColorAndOpacity(CkStyle::TextMute())
                    ];
            }

            ChipInner->AddSlot()
                .AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(Row.Value)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", ScaledFont(CkStyle::FontSizeSmall())))
                        .ColorAndOpacity(ValueColor)
                ];

            // Merge multiplier — this row stands for N identical rows across the lifetime
            // subtree. MergeCountDisplay axis; SuffixText keeps the muted/italic treatment
            // shared with the "+N more" overflow line, so it reads as annotation not data.
            // Null means the axis renders nothing — skip the slot rather than pad an empty one.
            if (const auto MergeCount = ck_debugoverlay_focuscard::Make_MergeCount(
                    Row.MergedCount, ScaledFont(CkStyle::FontSizeMicro()));
                MergeCount.IsValid())
            {
                ChipInner->AddSlot()
                    .AutoWidth().VAlign(VAlign_Center)
                    .Padding(FMargin{ CkStyle::SpaceXS, 0.0f, 0.0f, 0.0f })
                    [
                        MergeCount.ToSharedRef()
                    ];
            }

            SectionRow->AddSlot()
                .VAlign(VAlign_Center)
                .Padding(FMargin{ 0.0f, 0.0f, CkStyle::SpaceS, CkStyle::SpaceXS })
                [
                    ck_debugoverlay_focuscard::Make_FieldChip(ChipInner, ProviderColor)
                ];
        }

        if (Section.OmittedRowCount > 0)
        {
            SectionRow->AddSlot().VAlign(VAlign_Center).Padding(FMargin{ 0.0f, 0.0f, CkStyle::SpaceS, CkStyle::SpaceXS })
            [ SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("+%d more"), Section.OmittedRowCount)))
                .Font(FCoreStyle::GetDefaultFontStyle("Italic", ScaledFont(CkStyle::FontSizeMicro())))
                .ColorAndOpacity(CkStyle::TextMute()) ];
        }

        _ContentBox->AddSlot()
            .AutoHeight()
            .Padding(ck_debugoverlay_focuscard::Apply_RowDensity(
                FMargin{ 0.0f, 0.0f, 0.0f, CkStyle::SpaceXS }))
            [
                SectionRow
            ];
    }

    const auto TotalOmittedRows = FMath::Max(0, InputRowCount - RenderedRowCount);
    if (TotalOmittedRows > 0)
    {
        _ContentBox->AddSlot().AutoHeight().Padding(FMargin{ 0.0f, 0.0f, 0.0f, CkStyle::SpaceXS })
        [ SNew(STextBlock)
            .Text(FText::FromString(FString::Printf(TEXT("+%d focus-card rows omitted by budget"), TotalOmittedRows)))
            .Font(FCoreStyle::GetDefaultFontStyle("Italic", ScaledFont(CkStyle::FontSizeMicro())))
            .ColorAndOpacity(CkStyle::TextMute()) ];
    }

    // ---- Legend — colored abbrev pill + muted expanded name, one pair per provider ----
    if (LegendEntries.Num() > 0)
    {
        auto LegendRow = SNew(SWrapBox)
            .PreferredSize(_WrapWidth);

        for (const auto& Entry : LegendEntries)
        {
            auto Pair = SNew(SHorizontalBox);
            const auto LegendPillColor = CkStyle::OverlayOf(Entry.Color, 0.55f);

            Pair->AddSlot()
                .AutoWidth().VAlign(VAlign_Center)
                .Padding(FMargin{ 0.0f, 0.0f, 2.0f, 0.0f })
                [
                    SNew(SBorder)
                        .BorderImage(CkStyle::GetRoundedBrush())
                        .BorderBackgroundColor(LegendPillColor)
                        .VAlign(VAlign_Center)
                        .Padding(FMargin{ 3.0f, 0.0f })
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(Entry.Abbrev))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", ScaledFont(CkStyle::FontSizeMicro())))
                                .ColorAndOpacity(FLinearColor{ 0.04f, 0.07f, 0.10f, 1.0f })
                        ]
                ];

            Pair->AddSlot()
                .AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(Entry.FullName))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", ScaledFont(CkStyle::FontSizeMicro())))
                        .ColorAndOpacity(CkStyle::TextMute())
                ];

            // Same "×N" annotation as a merged row: how many sections this provider owns
            // (one per contributing sub-entity), now that the legend lists it only once.
            if (Entry.SectionCount > 1)
            {
                Pair->AddSlot()
                    .AutoWidth().VAlign(VAlign_Center)
                    .Padding(FMargin{ CkStyle::SpaceXS, 0.0f, 0.0f, 0.0f })
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(FString::Printf(TEXT("×%d"), Entry.SectionCount)))
                            .Font(FCoreStyle::GetDefaultFontStyle("Italic", ScaledFont(CkStyle::FontSizeMicro())))
                            .ColorAndOpacity(CkStyle::TextMute())
                    ];
            }

            LegendRow->AddSlot()
                .VAlign(VAlign_Center)
                .Padding(FMargin{ 0.0f, 0.0f, CkStyle::SpaceM, 0.0f })
                [
                    Pair
                ];
        }

        _ContentBox->AddSlot()
            .AutoHeight()
            .Padding(FMargin{ 0.0f, CkStyle::SpaceXS, 0.0f, 0.0f })
            [
                LegendRow
            ];
    }
}

// ====================================================================================================================

auto
    SCkDebugOverlay_FocusCard::
    Build_LegendEntries(
        const TArray<FCk_DebugOverlay_Section>& InSections)
    -> TArray<FCk_DebugOverlay_LegendEntry>
{
    auto Entries = TArray<FCk_DebugOverlay_LegendEntry>{};

    // ProviderTag -> index into Entries. Subtree aggregation emits one section per
    // (provider × source), so an un-deduped legend repeated "[LABL] Label" once per
    // lifetime descendant.
    auto IndexOf = TMap<FGameplayTag, int32>{};

    for (const auto& Section : InSections)
    {
        // Must match the render loop's skip exactly — a section that draws nothing
        // must not claim a legend slot.
        if (Section.Rows.IsEmpty() && Section.OmittedRowCount == 0)
        { continue; }

        if (const auto* Existing = IndexOf.Find(Section.ProviderTag))
        {
            ++Entries[*Existing].SectionCount;
            continue;
        }

        const auto ProviderLeaf = ck_debugoverlay::Get_LeafName(Section.ProviderTag);

        IndexOf.Add(Section.ProviderTag, Entries.Num());
        Entries.Add(FCk_DebugOverlay_LegendEntry{
            ck_debugoverlay::Get_ProviderAbbrev(ProviderLeaf),
            ProviderLeaf,
            Get_ProviderColor(Section.ProviderTag),
            1 });
    }

    return Entries;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebugOverlay_FocusCard::
    Build_LegendEntries_PerSection(
        const TArray<FCk_DebugOverlay_Section>& InSections)
    -> TArray<FCk_DebugOverlay_LegendEntry>
{
    auto Entries = TArray<FCk_DebugOverlay_LegendEntry>{};

    for (const auto& Section : InSections)
    {
        // Same skip as the render loop and as the deduped builder — a section that draws
        // nothing must not claim a legend slot under any LegendMode.
        if (Section.Rows.IsEmpty() && Section.OmittedRowCount == 0)
        { continue; }

        const auto ProviderLeaf = ck_debugoverlay::Get_LeafName(Section.ProviderTag);

        // SectionCount stays 1 by construction: with one entry per section there is nothing
        // to count, so the xN annotation never renders in this mode.
        Entries.Add(FCk_DebugOverlay_LegendEntry{
            ck_debugoverlay::Get_ProviderAbbrev(ProviderLeaf),
            ProviderLeaf,
            Get_ProviderColor(Section.ProviderTag),
            1 });
    }

    return Entries;
}

// ====================================================================================================================

auto
    SCkDebugOverlay_FocusCard::
    Severity_To_Tone(ECk_DebugOverlay_Severity InSeverity)
    -> ECk_Tone
{
    switch (InSeverity)
    {
        case ECk_DebugOverlay_Severity::Good:   return ECk_Tone::Ok;
        case ECk_DebugOverlay_Severity::Warn:   return ECk_Tone::Warn;
        case ECk_DebugOverlay_Severity::Bad:    return ECk_Tone::Err;
        case ECk_DebugOverlay_Severity::Normal:
        default:                                return ECk_Tone::Neutral;
    }
}

// ====================================================================================================================

auto
    SCkDebugOverlay_FocusCard::
    Get_FlashAlpha(double InLastChanged, double InNow)
    -> float
{
    if (InLastChanged <= 0.0)
    {
        // Never changed — no flash
        return 1.0f;
    }

    const auto Elapsed = InNow - InLastChanged;
    if (Elapsed >= FocusCard_Constants::FlashDurationSec)
    {
        return 1.0f;
    }

    // Fade from 1 to 0 as elapsed approaches FlashDurationSec,
    // then back to 1 (we actually want to HIGHLIGHT on change —
    // full alpha = visible = 1.0 when just changed, normal when settled).
    // Design: alpha ramps from 1.0 at t=0 down to ~0.3 at t=FlashDuration.
    // We keep it above 0.3 so the text is always readable.
    const auto T = static_cast<float>(Elapsed / FocusCard_Constants::FlashDurationSec); // [0,1)
    return FMath::Lerp(1.0f, 0.3f, T);
}

// ====================================================================================================================

auto
    SCkDebugOverlay_FocusCard::
    Get_ProviderColor(const FGameplayTag& InProviderTag)
    -> FLinearColor
{
    // Hash the provider leaf to a stable hue so each provider gets a distinct,
    // consistent color across frames (SM / GOAP / Physics / ... differ).
    const auto Leaf = ck_debugoverlay::Get_LeafName(InProviderTag);
    const auto Hue  = static_cast<uint8>(GetTypeHash(Leaf) % 256);
    return FLinearColor::MakeFromHSV8(Hue, 150, 205);
}

// ====================================================================================================================
