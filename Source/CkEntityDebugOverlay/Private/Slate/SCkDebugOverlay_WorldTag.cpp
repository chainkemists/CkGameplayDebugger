// Implements SCkDebugOverlay_WorldTag — a tiny one-line world-anchored label.

#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_WorldTag.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"
#include "Rendering/SlateRenderTransform.h"
#include "Layout/Visibility.h"

// ====================================================================================================================

auto
    SCkDebugOverlay_WorldTag::
    Construct(const FArguments& InArgs)
    -> void
{
    SetVisibility(EVisibility::HitTestInvisible);

    SAssignNew(_TextBlock, STextBlock)
        .Text(InArgs._Text)
        .Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeSmall()))
        .ColorAndOpacity(CkDebugStyle::TextStrong());

    // Dark rounded pill behind the text so it reads against any world background.
    SAssignNew(_Border, SBorder)
        .BorderImage(CkDebugStyle::GetRoundedBrush())
        .BorderBackgroundColor(CkDebugStyle::OverlayOf(CkDebugStyle::BgRoot(), 0.78f))
        .Padding(FMargin{ CkDebugStyle::SpaceS, CkDebugStyle::SpaceXS })
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
    Set_Style(float InScale, float InOpacity)
    -> void
{
    // Scale: apply via RenderTransform so layout is unaffected (pill stays at its
    // natural size; scale is a visual-only multiplier centred on the widget pivot).
    SetRenderTransform(FSlateRenderTransform(FScale2D(InScale)));
    SetRenderTransformPivot(FVector2D{ 0.5f, 0.5f });

    // Opacity: modulate text and border background alpha together.
    if (_TextBlock.IsValid())
    {
        _TextBlock->SetColorAndOpacity(
            FSlateColor{ FLinearColor{ 1.0f, 1.0f, 1.0f, InOpacity } });
    }
    if (_Border.IsValid())
    {
        // Modulate the background alpha by InOpacity so the pill fades with the label.
        auto BgColor = CkDebugStyle::OverlayOf(CkDebugStyle::BgRoot(), 0.78f);
        BgColor.A    = FMath::Clamp(BgColor.A * InOpacity, 0.0f, 1.0f);
        _Border->SetBorderBackgroundColor(FSlateColor{ BgColor });
    }
}

// ====================================================================================================================
