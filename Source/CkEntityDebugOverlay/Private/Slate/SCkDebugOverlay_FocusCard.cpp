// Implements Ultra-density focus card for the on-screen entity debug overlay.

#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_FocusCard.h"

#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"
#include "CkEntityDebugOverlay/Style/CkDebugOverlay_RenderStyle.h"
#include "CkEntityDebugOverlay/History/CkDebugOverlay_History.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

#include "CkEditorTools/Style/CkStyle.h"
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
        .Padding(FMargin{ 0.0f, 0.0f, 0.0f, CkStyle::SpaceXS })
        [
            HeaderRow
        ];

    // ---- Sections (sorted by SortPriority, then source order) ----
    // Copy the sections array so we can sort without mutating the model. The SourceOrder
    // tie-break is load-bearing: subtree aggregation emits one section per source for the
    // same provider (equal priorities), and an unstable sort over equal keys reorders the
    // card every tick.
    auto SortedSections = InModel.Sections;
    SortedSections.Sort([](const FCk_DebugOverlay_Section& A, const FCk_DebugOverlay_Section& B)
    {
        if (A.SortPriority != B.SortPriority)
        { return A.SortPriority < B.SortPriority; }
        return A.SourceOrder < B.SourceOrder;
    });

    // Derive a stable entity id for the history key.
    // FCk_Entity::Get_EntityNumber() returns the entt entity index (uint32-compatible).
    const auto EntityId = static_cast<uint32>(InModel.Entity.Get_Entity().Get_EntityNumber());

    // Legend entries collected for the providers actually rendered:
    // colored abbrev pill + muted expanded name.
    struct FLegendEntry
    {
        FString      Abbrev;
        FString      FullName;
        FLinearColor Color = FLinearColor::White;
    };
    auto LegendEntries = TArray<FLegendEntry>{};

    for (const auto& Section : SortedSections)
    {
        // A section whose provider contributed no rows renders as a lone chip /
        // blank-looking line — skip it entirely.
        if (Section.Rows.IsEmpty())
        { continue; }

        const auto ProviderColor = Get_ProviderColor(Section.ProviderTag);
        const auto ProviderLeaf  = ck_debugoverlay::Get_LeafName(Section.ProviderTag);
        const auto ProviderAbbr  = ck_debugoverlay::Get_ProviderAbbrev(ProviderLeaf);
        const auto ProviderName  = FText::FromString(ProviderAbbr);

        LegendEntries.Add(FLegendEntry{ ProviderAbbr, ProviderLeaf, ProviderColor });

        // One color-grouped flow line per section:
        //   [PROVIDER]  [KEY value]  [KEY trail‹ value] ...
        // Explicit PreferredSize, NOT UseAllottedSize: the wrap boxes are recreated
        // every Set_Model call, so the allotted-size sync (which happens in Tick)
        // never runs — desired size would wrap at the 100px default and reserve
        // phantom empty lines under each section.
        auto SectionRow = SNew(SWrapBox)
            .PreferredSize(_WrapWidth);

        // Provider chip — solid provider-colored pill with dark text.
        SectionRow->AddSlot()
            .VAlign(VAlign_Center)
            .Padding(FMargin{ 0.0f, 0.0f, CkStyle::SpaceXS, CkStyle::SpaceXS })
            [
                SNew(SBorder)
                    .BorderImage(CkStyle::GetRoundedBrush())
                    .BorderBackgroundColor(ProviderColor)
                    .VAlign(VAlign_Center)
                    .Padding(FMargin{ CkStyle::SpaceS, 1.0f })
                    [
                        SNew(STextBlock)
                            .Text(ProviderName)
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", ScaledFont(CkStyle::FontSizeMicro())))
                            .ColorAndOpacity(FLinearColor{ 0.04f, 0.07f, 0.10f, 1.0f })
                    ]
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

        // Field chips share a provider-tinted background + key color.
        const auto FieldChipTint = CkStyle::OverlayOf(ProviderColor, 0.18f);
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

            SectionRow->AddSlot()
                .VAlign(VAlign_Center)
                .Padding(FMargin{ 0.0f, 0.0f, CkStyle::SpaceS, CkStyle::SpaceXS })
                [
                    SNew(SBorder)
                        .BorderImage(CkStyle::GetRoundedBrush())
                        .BorderBackgroundColor(FieldChipTint)
                        .VAlign(VAlign_Center)
                        .Padding(FMargin{ CkStyle::SpaceS, 1.0f })
                        [
                            ChipInner
                        ]
                ];
        }

        _ContentBox->AddSlot()
            .AutoHeight()
            .Padding(FMargin{ 0.0f, 0.0f, 0.0f, CkStyle::SpaceXS })
            [
                SectionRow
            ];
    }

    // ---- Legend — colored abbrev pill + muted expanded name, one pair per provider ----
    if (LegendEntries.Num() > 0)
    {
        auto LegendRow = SNew(SWrapBox)
            .PreferredSize(_WrapWidth);

        for (const auto& Entry : LegendEntries)
        {
            auto Pair = SNew(SHorizontalBox);

            Pair->AddSlot()
                .AutoWidth().VAlign(VAlign_Center)
                .Padding(FMargin{ 0.0f, 0.0f, 2.0f, 0.0f })
                [
                    SNew(SBorder)
                        .BorderImage(CkStyle::GetRoundedBrush())
                        .BorderBackgroundColor(Entry.Color)
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
