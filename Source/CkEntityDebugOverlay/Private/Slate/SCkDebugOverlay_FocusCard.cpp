// Implements Ultra-density focus card for the on-screen entity debug overlay.

#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_FocusCard.h"

#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"
#include "CkEntityDebugOverlay/Style/CkDebugOverlay_RenderStyle.h"
#include "CkEntityDebugOverlay/History/CkDebugOverlay_History.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
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
            .BorderImage(CkDebugStyle::GetRoundedBrush())
            .BorderBackgroundColor(FLinearColor::Transparent)   // no ring unless locked
            .Padding(FMargin{ 2.0f })                            // ring thickness
            [
                SNew(SBorder)
                    .BorderImage(CkDebugStyle::GetRoundedBrush())
                    .BorderBackgroundColor(CkDebugStyle::OverlayOf(CkDebugStyle::BgRoot(), 0.9f))
                    .Padding(FMargin{ CkDebugStyle::SpaceM })
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
        bool                                bIsLocked)
    -> void
{
    if (NOT _ContentBox.IsValid())
    {
        return;
    }

    // Update the lock ring color.
    if (_LockFrame.IsValid())
    {
        _LockFrame->SetBorderBackgroundColor(
            bIsLocked ? FLinearColor{ 1.0f, 0.82f, 0.0f, 1.0f }   // amber/yellow lock ring
                      : FLinearColor::Transparent);
    }

    _ContentBox->ClearChildren();

    // ---- Header row — SCkDebug_EntityRef pill (entity identity, consistent with debugger style) ----
    // The overlay is HitTestInvisible, so the EntityRef click is a no-op — but we still get
    // the canonical entity rendering (ID, version, debug name).
    _ContentBox->AddSlot()
        .AutoHeight()
        .Padding(FMargin{ 0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceXS })
        [
            SNew(SCkDebug_EntityRef)
                .Entity(InModel.Entity)
                .ShowName(true)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeH3()))
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
        const auto ProviderColor = Get_ProviderColor(Section.ProviderTag);
        const auto ProviderName  = FText::FromString(
            ck_debugoverlay::Get_LeafName(Section.ProviderTag).ToUpper());

        // One color-grouped flow line per section:
        //   [PROVIDER]  [KEY value]  [KEY trail‹ value] ...
        auto SectionRow = SNew(SWrapBox)
            .UseAllottedSize(true);

        // Provider chip — solid provider-colored pill with dark text.
        SectionRow->AddSlot()
            .VAlign(VAlign_Center)
            .Padding(FMargin{ 0.0f, 0.0f, CkDebugStyle::SpaceXS, CkDebugStyle::SpaceXS })
            [
                SNew(SBorder)
                    .BorderImage(CkDebugStyle::GetRoundedBrush())
                    .BorderBackgroundColor(ProviderColor)
                    .VAlign(VAlign_Center)
                    .Padding(FMargin{ CkDebugStyle::SpaceS, 1.0f })
                    [
                        SNew(STextBlock)
                            .Text(ProviderName)
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeMicro()))
                            .ColorAndOpacity(FLinearColor{ 0.04f, 0.07f, 0.10f, 1.0f })
                    ]
            ];

        // Field chips share a provider-tinted background + key color.
        const auto FieldChipTint = CkDebugStyle::OverlayOf(ProviderColor, 0.18f);
        const auto FieldKeyColor = CkDebugStyle::OverlayOf(ProviderColor, 0.95f);

        for (const auto& Row : Section.Rows)
        {
            const FCk_DebugOverlay_HistoryKey HistKey{ EntityId, Row.FieldTag };

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
                ? CkDebugStyle::TextStrong()
                : CkDebugStyle::GetToneColor(Severity_To_Tone(Row.Severity));

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
                .Padding(FMargin{ 0.0f, 0.0f, CkDebugStyle::SpaceXS, 0.0f })
                [
                    SNew(STextBlock)
                        .Text(FieldLabel)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeMicro()))
                        .ColorAndOpacity(FieldKeyColor)
                ];

            if (NOT TrailText.IsEmpty())
            {
                ChipInner->AddSlot()
                    .AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TrailText + FocusCard_Constants::BreadcrumbSeparator))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeSmall()))
                            .ColorAndOpacity(CkDebugStyle::TextMute())
                    ];
            }

            ChipInner->AddSlot()
                .AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(Row.Value)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeSmall()))
                        .ColorAndOpacity(ValueColor)
                ];

            SectionRow->AddSlot()
                .VAlign(VAlign_Center)
                .Padding(FMargin{ 0.0f, 0.0f, CkDebugStyle::SpaceS, CkDebugStyle::SpaceXS })
                [
                    SNew(SBorder)
                        .BorderImage(CkDebugStyle::GetRoundedBrush())
                        .BorderBackgroundColor(FieldChipTint)
                        .VAlign(VAlign_Center)
                        .Padding(FMargin{ CkDebugStyle::SpaceS, 1.0f })
                        [
                            ChipInner
                        ]
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
