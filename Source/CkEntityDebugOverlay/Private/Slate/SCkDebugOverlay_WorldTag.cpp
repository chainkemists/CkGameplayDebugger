// Implements SCkDebugOverlay_WorldTag — a tiny one-line world-anchored label.

#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_WorldTag.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"
#include "Rendering/SlateRenderTransform.h"
#include "Layout/Visibility.h"

// ====================================================================================================================

auto
    ck_debugoverlay::
    Advance_WorldTagVisibilityFade(
        FWorldTagVisibilityFadeState& InOutState,
        bool                          InIsInRange,
        double                        InNow)
    -> float
{
    const auto StateIsFinite = NOT InOutState.bInitialized ||
        (FMath::IsFinite(InOutState.StartOpacity) &&
         FMath::IsFinite(InOutState.CurrentOpacity) &&
         FMath::IsFinite(InOutState.TargetOpacity) &&
         FMath::IsFinite(InOutState.TransitionStartTime) &&
         FMath::IsFinite(InOutState.LastUpdateTime));
    if (NOT StateIsFinite)
    { InOutState = FWorldTagVisibilityFadeState{}; }

    const auto LastUpdateTime = InOutState.bInitialized ? InOutState.LastUpdateTime : 0.0;
    const auto SafeNow = FMath::IsFinite(InNow)
        ? FMath::Max(InNow, LastUpdateTime)
        : LastUpdateTime;
    const auto TargetOpacity = InIsInRange ? 1.0f : 0.0f;

    if (NOT InOutState.bInitialized)
    {
        InOutState.bInitialized = true;
        InOutState.StartOpacity = 0.0f;
        InOutState.CurrentOpacity = 0.0f;
        InOutState.TargetOpacity = TargetOpacity;
        InOutState.TransitionStartTime = SafeNow;
    }

    const auto AdvanceTo = [&InOutState](double InSafeNow)
    {
        const auto Elapsed = FMath::Max(0.0, InSafeNow - InOutState.TransitionStartTime);
        const auto Alpha = FMath::Clamp(
            Elapsed / WorldTagVisibilityFadeDurationSeconds,
            0.0,
            1.0);
        InOutState.CurrentOpacity = static_cast<float>(FMath::Lerp(
            static_cast<double>(InOutState.StartOpacity),
            static_cast<double>(InOutState.TargetOpacity),
            Alpha));
        InOutState.CurrentOpacity = FMath::Clamp(InOutState.CurrentOpacity, 0.0f, 1.0f);
    };

    AdvanceTo(SafeNow);
    if (NOT FMath::IsNearlyEqual(InOutState.TargetOpacity, TargetOpacity))
    {
        InOutState.StartOpacity = InOutState.CurrentOpacity;
        InOutState.TargetOpacity = TargetOpacity;
        InOutState.TransitionStartTime = SafeNow;
        AdvanceTo(SafeNow);
    }

    InOutState.LastUpdateTime = SafeNow;
    return InOutState.CurrentOpacity;
}

// ====================================================================================================================

auto
    SCkDebugOverlay_WorldTag::
    Construct(const FArguments& InArgs)
    -> void
{
    SetVisibility(EVisibility::HitTestInvisible);

    SAssignNew(_TextBlock, STextBlock)
        .Text(InArgs._Text)
        .Font_Lambda([]() -> FSlateFontInfo
        {
            return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall());
        })
        .ColorAndOpacity(CkStyle::TextStrong());

    // Dark rounded pill behind the text so it reads against any world background. RowDensity moves
    // the pill's padding by its DELTA — the tag is deliberately denser than any editor row, so the
    // axis' absolutes would blow it up; Comfortable (the default) leaves it exactly as it shipped.
    // Brush and padding are bound so they follow CornerStyle / RowDensity.
    SAssignNew(_Border, SBorder)
        .BorderImage_Static(&CkStyle::GetRoundedBrush)
        .BorderBackgroundColor(CkStyle::OverlayOf(CkStyle::BgRoot(), 0.78f))
        .Padding_Lambda([]() -> FMargin
        {
            return ck::debug_axes::Apply_RowDensity(FMargin{ CkStyle::SpaceS, CkStyle::SpaceXS });
        })
        [
            _TextBlock.ToSharedRef()
        ];

    ChildSlot
    [
        _Border.ToSharedRef()
    ];
}

// ====================================================================================================================

auto
    SCkDebugOverlay_WorldTag::
    Set_Text(const FText& InText)
    -> void
{
    if (_TextBlock.IsValid())
    {
        _TextBlock->SetText(InText);
    }
}

// ====================================================================================================================

auto
    SCkDebugOverlay_WorldTag::
    Set_Scale(float InScale)
    -> void
{
    // Scale: apply via RenderTransform so layout is unaffected (pill stays at its
    // natural size; scale is a visual-only multiplier centred on the widget pivot).
    SetRenderTransform(FSlateRenderTransform(FScale2D(InScale)));
    SetRenderTransformPivot(FVector2D{ 0.5f, 0.5f });
}

// ====================================================================================================================
