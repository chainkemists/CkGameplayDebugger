// Implements Ultra-density focus card for the on-screen entity debug overlay.

#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_FocusCard.h"

#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"
#include "CkEntityDebugOverlay/Style/CkDebugOverlay_RenderStyle.h"
#include "CkEntityDebugOverlay/History/CkDebugOverlay_History.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "Widgets/Layout/SWrapBox.h"
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

    ChildSlot
    [
        _ContentBox.ToSharedRef()
    ];
}

// ====================================================================================================================

auto
    SCkDebugOverlay_FocusCard::
    Set_Model(
        const FCk_DebugOverlay_EntityModel& InModel,
        const FCk_DebugOverlay_RenderStyle& InStyle,
        const FCk_DebugOverlay_History&     InHistory,
        double                              InNow)
    -> void
{
    if (NOT _ContentBox.IsValid())
    {
        return;
    }

    _ContentBox->ClearChildren();

    // ---- Header row ----
    _ContentBox->AddSlot()
        .AutoHeight()
        .Padding(FMargin{ 0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceXS })
        [
            SNew(STextBlock)
                .Text(InModel.Header)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeH3()))
                .ColorAndOpacity(CkDebugStyle::TextStrong())
        ];

    // ---- Sections (sorted by SortPriority) ----
    // Copy the sections array so we can sort without mutating the model.
    auto SortedSections = InModel.Sections;
    SortedSections.Sort([](const FCk_DebugOverlay_Section& A, const FCk_DebugOverlay_Section& B)
    {
        return A.SortPriority < B.SortPriority;
    });

    // Derive a stable entity id for the history key.
    // FCk_Entity::Get_EntityNumber() returns the entt entity index (uint32-compatible).
    const auto EntityId = static_cast<uint32>(InModel.Entity.Get_Entity().Get_EntityNumber());

    for (const auto& Section : SortedSections)
    {
        // Provider chip label
        const auto ProviderName = FText::FromString(
            ck_debugoverlay::Get_LeafName(Section.ProviderTag));

        // One horizontal line per section: [provider chip] [field chip] [field chip] ...
        auto SectionRow = SNew(SWrapBox)
            .UseAllottedSize(true);

        // Provider chip — small pill showing the section/provider name
        SectionRow->AddSlot()
            .Padding(FMargin{ 0.0f, 0.0f, CkDebugStyle::SpaceXS, CkDebugStyle::SpaceXS })
            [
                SNew(SCkDebug_StatusPill)
                    .Text(ProviderName)
                    .Tone(ECkDebug_Tone::Accent)
                    .ShowDot(false)
            ];

        // Field chips
        for (const auto& Row : Section.Rows)
        {
            const FCk_DebugOverlay_HistoryKey HistKey{ EntityId, Row.FieldTag };

            // Build the breadcrumb trail string
            FString TrailText;
            if (Row.ExplicitHistory.Num() > 0)
            {
                // Explicit history is authoritative — join in order (oldest first)
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

            // Choose color from severity
            const auto Tone = Severity_To_Tone(Row.Severity);
            auto FieldColor = CkDebugStyle::GetToneColor(Tone);

            // Apply flash tint if enabled
            if (InStyle.bFlashOnChange)
            {
                const auto Alpha = Get_FlashAlpha(InHistory.Get_LastChangedTime(HistKey), InNow);
                FieldColor.A = FMath::Clamp(FieldColor.A * Alpha, 0.0f, 1.0f);
            }

            const auto FieldLabel = FText::FromString(
                ck_debugoverlay::Get_LeafName(Row.FieldTag));

            // Compose the field chip: "[FieldLabel]  trail ‹ value"
            FString ChipText = FieldLabel.ToString();
            ChipText += TEXT(": ");
            if (NOT TrailText.IsEmpty())
            {
                ChipText += TrailText;
                ChipText += FocusCard_Constants::BreadcrumbSeparator;
            }
            ChipText += Row.Value.ToString();

            SectionRow->AddSlot()
                .Padding(FMargin{ 0.0f, 0.0f, CkDebugStyle::SpaceS, CkDebugStyle::SpaceXS })
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(ChipText))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeSmall()))
                        .ColorAndOpacity(FieldColor)
                ];
        }

        _ContentBox->AddSlot()
            .AutoHeight()
            .Padding(FMargin{ 0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceXS })
            [
                SectionRow
            ];
    }
}

// ====================================================================================================================

auto
    SCkDebugOverlay_FocusCard::
    Severity_To_Tone(ECk_DebugOverlay_Severity InSeverity)
    -> ECkDebug_Tone
{
    switch (InSeverity)
    {
        case ECk_DebugOverlay_Severity::Good:   return ECkDebug_Tone::Ok;
        case ECk_DebugOverlay_Severity::Warn:   return ECkDebug_Tone::Warn;
        case ECk_DebugOverlay_Severity::Bad:    return ECkDebug_Tone::Err;
        case ECk_DebugOverlay_Severity::Normal:
        default:                                return ECkDebug_Tone::Neutral;
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
