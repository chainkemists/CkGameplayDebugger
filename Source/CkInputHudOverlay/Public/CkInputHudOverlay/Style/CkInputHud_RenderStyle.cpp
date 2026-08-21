#include "CkInputHudOverlay/Style/CkInputHud_RenderStyle.h"

#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Brushes/SlateRoundedBoxBrush.h"

namespace ck_input_hud_render_style
{
    constexpr auto KeyRadiusStep  = 0.25f;
    constexpr auto KeyRadiusCount = 49;

    auto
        Get_RoundedLargeOutlineBrush()
        -> const FSlateBrush*
    {
        static const auto Brush = FSlateRoundedBoxBrush{
            FLinearColor::Transparent, CkStyle::RadiusL(), FLinearColor::White, CkStyle::RingWidth()};
        return &Brush;
    }
}

auto
    ck::input_hud::
    Resolve_Brush(
        ECk_InputHud_BrushShape InShape)
    -> const FSlateBrush*
{
    switch (InShape)
    {
        case ECk_InputHud_BrushShape::Square:       return FCkDebuggerStyle::Get_SquareBrush();
        case ECk_InputHud_BrushShape::Rounded:      return CkStyle::GetRoundedBrush();
        case ECk_InputHud_BrushShape::RoundedLarge: return CkStyle::GetRoundedBrush_Large();
        case ECk_InputHud_BrushShape::Pill:         return CkStyle::GetRoundedBrush_Pill();
        default:                                     return CkStyle::GetRoundedBrush_Large();
    }
}

auto
    ck::input_hud::
    Resolve_KeyBrush(
        float                           InCornerRadius,
        ECk_InputHud_KeyBrushTreatment InTreatment)
    -> const FSlateBrush*
{
    static const auto FillBrushes = []() -> TArray<FSlateRoundedBoxBrush>
    {
        auto Result = TArray<FSlateRoundedBoxBrush>{};
        Result.Reserve(ck_input_hud_render_style::KeyRadiusCount);

        for (auto Index = 0; Index < ck_input_hud_render_style::KeyRadiusCount; ++Index)
        {
            Result.Emplace(
                FLinearColor::White,
                static_cast<float>(Index) * ck_input_hud_render_style::KeyRadiusStep);
        }

        return Result;
    }();

    static const auto OutlineBrushes = []() -> TArray<FSlateRoundedBoxBrush>
    {
        auto Result = TArray<FSlateRoundedBoxBrush>{};
        Result.Reserve(ck_input_hud_render_style::KeyRadiusCount);

        for (auto Index = 0; Index < ck_input_hud_render_style::KeyRadiusCount; ++Index)
        {
            Result.Emplace(
                FLinearColor::Transparent,
                static_cast<float>(Index) * ck_input_hud_render_style::KeyRadiusStep,
                FLinearColor::White,
                CkStyle::RingWidth());
        }

        return Result;
    }();

    const auto RadiusIndex = FMath::Clamp(
        FMath::RoundToInt(InCornerRadius / ck_input_hud_render_style::KeyRadiusStep),
        0,
        ck_input_hud_render_style::KeyRadiusCount - 1);

    return InTreatment == ECk_InputHud_KeyBrushTreatment::Outline
        ? &OutlineBrushes[RadiusIndex]
        : &FillBrushes[RadiusIndex];
}

auto
    ck::input_hud::
    Resolve_OutlineBrush(
        ECk_InputHud_BrushShape InShape)
    -> const FSlateBrush*
{
    switch (InShape)
    {
        case ECk_InputHud_BrushShape::Square: return FCkDebuggerStyle::Get_SurfaceOutlineBrush();
        case ECk_InputHud_BrushShape::Pill:   return FCkDebuggerStyle::Get_PillOutlineBrush();
        case ECk_InputHud_BrushShape::RoundedLarge:
            return ck_input_hud_render_style::Get_RoundedLargeOutlineBrush();
        case ECk_InputHud_BrushShape::Rounded:
        default: return FCkDebuggerStyle::Get_RoundedOutlineBrush();
    }
}

auto
    ck::input_hud::
    Get_ActiveRenderStyle()
    -> FCk_InputHud_RenderStyle
{
    auto Result = FCk_InputHud_RenderStyle{};
    Result.Palette            = UCk_InputHud_UserSettings::Get_PaletteSnapshot();
    Result.KeyBorderWidth     = UCk_InputHud_UserSettings::Get_KeyBorderWidth();
    Result.KeyBorderOpacity   = UCk_InputHud_UserSettings::Get_KeyBorderOpacity();
    Result.ActiveFillOpacity  = UCk_InputHud_UserSettings::Get_ActiveFillOpacity();
    Result.ActiveGlowOpacity  = UCk_InputHud_UserSettings::Get_ActiveGlowOpacity();
    Result.PanelOpacity       = UCk_InputHud_UserSettings::Get_PanelOpacity();
    Result.PulseScale         = UCk_InputHud_UserSettings::Get_PulseScale();
    Result.HistoryBrightness  = UCk_InputHud_UserSettings::Get_HistoryBrightness();
    Result.PressPopScale      = UCk_InputHud_UserSettings::Get_PressPopScale();
    Result.PressPopDurationSeconds = UCk_InputHud_UserSettings::Get_PressPopDurationMs() / 1000.0f;
    Result.ReleaseEaseSeconds = UCk_InputHud_UserSettings::Get_ReleaseEaseMs() / 1000.0f;
    switch (UCk_InputHud_UserSettings::Get_CornerStyle())
    {
        case ECk_InputHud_CornerStyle::Sharp:
            Result.PanelBrushShape  = ECk_InputHud_BrushShape::Square;
            Result.ChipBrushShape   = ECk_InputHud_BrushShape::Square;
            break;
        case ECk_InputHud_CornerStyle::Soft:
            Result.PanelBrushShape  = ECk_InputHud_BrushShape::Rounded;
            Result.ChipBrushShape   = ECk_InputHud_BrushShape::Rounded;
            break;
        default: break;
    }
    switch (UCk_InputHud_UserSettings::Get_Density())
    {
        case ECk_InputHud_Density::Standard:
            Result.PanelPaddingX  = 7.0f;
            Result.PanelPaddingY  = 5.0f;
            Result.ChipMinWidth   = 22.0f;
            Result.ChipGap        = 3.0f;
            Result.GlyphRowHeight = 7.0f;
            Result.DeckGap        = 2.0f;
            Result.HoldBarHeight  = 3.0f;
            Result.TapDotSize     = 4.0f;
            Result.PressDotSize   = 5.0f;
            Result.LabelFontSize  = 10;
            break;
        case ECk_InputHud_Density::Readable:
            Result.PanelPaddingX    = 9.0f;
            Result.PanelPaddingY    = 7.0f;
            Result.ChipMinWidth     = 28.0f;
            Result.ChipGap          = 4.0f;
            Result.GlyphRowHeight   = 9.0f;
            Result.DeckGap          = 3.0f;
            Result.HoldBarHeight    = 4.0f;
            Result.TapDotSize       = 4.5f;
            Result.PressDotSize     = 5.5f;
            Result.LabelFontSize    = 11;
            Result.MetadataFontSize = 7;
            break;
        default: break;
    }
    Result.ChipPaddingX    = UCk_InputHud_UserSettings::Get_KeyPaddingX();
    Result.ChipPaddingY    = UCk_InputHud_UserSettings::Get_KeyPaddingY();
    Result.KeyCornerRadius = UCk_InputHud_UserSettings::Get_KeyCornerRadius();
    Result.OverallOpacity  = UCk_InputHud_UserSettings::Get_OverallOpacity();
    Result.AnchorCorner    = UCk_InputHud_UserSettings::Get_AnchorCorner();
    Result.AnchorOffsetX   = UCk_InputHud_UserSettings::Get_AnchorOffsetX();
    Result.AnchorOffsetY   = UCk_InputHud_UserSettings::Get_AnchorOffsetY();
    return Result;
}

auto
    ck::input_hud::
    Get_ValidOverlayScale(
        float InScale)
    -> float
{
    return FMath::IsFinite(InScale) && InScale >= 0.0f ? InScale : 1.0f;
}

auto
    ck::input_hud::
    Get_EffectiveGlyphRowHeight(
        const FCk_InputHud_RenderStyle& InStyle)
    -> float
{
    return FMath::Max3(
        InStyle.GlyphRowHeight,
        InStyle.PressDotSize * InStyle.PulseScale,
        InStyle.HoldBarHeight * InStyle.PulseScale);
}

auto
    ck::input_hud::
    Get_ChipWidth(
        float InContentWidth,
        float InChipHeight,
        float InPaddingX,
        float InMinWidth)
    -> float
{
    return FMath::Max3(
        FMath::Max(0.0f, InChipHeight),
        FMath::Max(0.0f, InMinWidth),
        FMath::Max(0.0f, InContentWidth) + FMath::Max(0.0f, InPaddingX) * 2.0f);
}

auto
    ck::input_hud::
    Get_ComposedOverlayOpacity(
        float InIdleFade,
        float InCVarOpacity,
        float InOverallOpacity)
    -> float
{
    // The cvar keeps the legibility floor it has always had: a stray `ck.InputOverlay.Opacity 0` must not make the
    // overlay silently invisible and unreportable. The Overall opacity setting is an explicit, discoverable, and
    // persisted choice, so it is allowed all the way to zero — the floor must NOT leak onto it.
    return FMath::Clamp(InIdleFade, 0.0f, 1.0f) *
           FMath::Clamp(InCVarOpacity, 0.15f, 1.0f) *
           FMath::Clamp(InOverallOpacity, 0.0f, 1.0f);
}

auto
    ck::input_hud::
    Get_DeckVisibility(
        ECk_InputHud_MetadataMode InMetadataMode,
        bool                      InHasDuration,
        bool                      InHasFrameNumbers)
    -> FCk_InputHud_DeckVisibility
{
    switch (InMetadataMode)
    {
        case ECk_InputHud_MetadataMode::Keys:
            return FCk_InputHud_DeckVisibility{};
        case ECk_InputHud_MetadataMode::Compact:
            return FCk_InputHud_DeckVisibility{InHasDuration, false};
        case ECk_InputHud_MetadataMode::Full:
            return FCk_InputHud_DeckVisibility{InHasDuration, InHasFrameNumbers};
        default:
            return FCk_InputHud_DeckVisibility{};
    }
}

auto
    ck::input_hud::
    Get_EventAnimation(
        double                           InDownTimeSeconds,
        double                           InUpTimeSeconds,
        double                           InNowSeconds,
        const FCk_InputHud_RenderStyle& InStyle)
    -> FCk_InputHud_EventAnimation
{
    auto Result = FCk_InputHud_EventAnimation{};
    const auto IsHeld = InUpTimeSeconds <= 0.0;
    Result.ActiveBlend = IsHeld ? 1.0f : 0.0f;

    // Style Lab uses future-authored timestamps for stable released samples. They are presentation fixtures, not
    // presses that should sit forever at the first frame of the pop animation.
    if (InDownTimeSeconds > 0.0 && InNowSeconds >= InDownTimeSeconds && InStyle.PressPopDurationSeconds > 0.0f)
    {
        const auto PressAge = static_cast<float>(InNowSeconds - InDownTimeSeconds);
        const auto Alpha = FMath::Clamp(PressAge / InStyle.PressPopDurationSeconds, 0.0f, 1.0f);
        const auto Ease = FMath::SmoothStep(0.0f, 1.0f, Alpha);
        Result.Scale = FMath::Lerp(InStyle.PressPopScale, 1.0f, Ease);
    }

    if (NOT IsHeld && InUpTimeSeconds > 0.0 && InNowSeconds >= InUpTimeSeconds && InStyle.ReleaseEaseSeconds > 0.0f)
    {
        const auto ReleaseAge = static_cast<float>(InNowSeconds - InUpTimeSeconds);
        const auto Alpha = FMath::Clamp(ReleaseAge / InStyle.ReleaseEaseSeconds, 0.0f, 1.0f);
        Result.ActiveBlend = 1.0f - FMath::SmoothStep(0.0f, 1.0f, Alpha);
    }

    return Result;
}
