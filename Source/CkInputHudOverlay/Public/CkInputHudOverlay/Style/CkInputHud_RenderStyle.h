#pragma once

#include "CkInputHudOverlay/Settings/CkInputHud_UserSettings.h"

struct FSlateBrush;

enum class ECk_InputHud_BrushShape : uint8
{
    Square,
    Rounded,
    RoundedLarge,
    Pill,
};

enum class ECk_InputHud_KeyBrushTreatment : uint8
{
    Fill,
    Outline,
};

struct FCk_InputHud_RenderStyle
{
    ECk_InputHud_BrushShape PanelBrushShape    = ECk_InputHud_BrushShape::RoundedLarge;
    ECk_InputHud_BrushShape ChipBrushShape     = ECk_InputHud_BrushShape::RoundedLarge;
    FCk_InputHud_PaletteSnapshot Palette;

    float PanelPaddingX     = 6.0f;
    float PanelPaddingY     = 4.0f;
    float PanelOutlineWidth = 1.0f;
    float ChipPaddingX     = 5.0f;
    float ChipPaddingY     = 3.0f;
    float KeyCornerRadius  = 3.0f;
    float OverallOpacity   = 1.0f;
    float ChipMinWidth     = 18.0f;
    float ChipGap          = 3.0f;
    float GlyphRowHeight   = 6.0f;
    float DeckGap          = 1.0f;
    float KeyBorderWidth   = 1.0f;
    float HoldBarHeight    = 2.5f;
    float TapDotSize       = 4.0f;
    float PressDotSize     = 5.0f;
    int32 LabelFontSize    = 9;
    int32 MetadataFontSize = 6;
    float ActiveFillOpacity = 0.92f;
    float ActiveGlowOpacity = 0.10f;
    float PanelOpacity = 0.94f;
    float KeyBorderOpacity = 0.50f;
    float PulseScale = 1.0f;
    float HistoryBrightness = 0.82f;
    float PressPopScale = 1.20f;
    float PressPopDurationSeconds = 0.25f;
    float ReleaseEaseSeconds = 0.32f;
};

struct FCk_InputHud_EventAnimation
{
    float Scale       = 1.0f;
    float ActiveBlend = 0.0f;
};

struct FCk_InputHud_DeckVisibility
{
    bool ShowDuration     = false;
    bool ShowFrameNumbers = false;
};

namespace ck::input_hud
{
    CKINPUTHUDOVERLAY_API auto Resolve_Brush(ECk_InputHud_BrushShape InShape) -> const FSlateBrush*;
    CKINPUTHUDOVERLAY_API auto Resolve_OutlineBrush(ECk_InputHud_BrushShape InShape) -> const FSlateBrush*;
    CKINPUTHUDOVERLAY_API auto
    Resolve_KeyBrush(
        float                           InCornerRadius,
        ECk_InputHud_KeyBrushTreatment InTreatment) -> const FSlateBrush*;
    CKINPUTHUDOVERLAY_API auto Get_ActiveRenderStyle() -> FCk_InputHud_RenderStyle;

    CKINPUTHUDOVERLAY_API auto Get_ValidOverlayScale(float InScale) -> float;

    CKINPUTHUDOVERLAY_API auto
    Get_EffectiveGlyphRowHeight(const FCk_InputHud_RenderStyle& InStyle) -> float;

    CKINPUTHUDOVERLAY_API auto
    Get_ChipWidth(
        float InContentWidth,
        float InChipHeight,
        float InPaddingX,
        float InMinWidth) -> float;

    // The overlay composites as ONE surface. The idle fade-out, the ck.InputOverlay.Opacity cvar, and the user's
    // Overall opacity all multiply into the panel's RenderOpacity, which Slate blends down into every descendant
    // (including the ribbon's draw elements). Per-element tint alpha cannot express this: a key drawn at reduced
    // alpha blends toward the panel fill behind it, which reads as dimming rather than transparency.
    // The cvar keeps its own legibility floor; an explicitly authored Overall opacity is allowed to reach zero.
    CKINPUTHUDOVERLAY_API auto
    Get_ComposedOverlayOpacity(
        float InIdleFade,
        float InCVarOpacity,
        float InOverallOpacity) -> float;

    CKINPUTHUDOVERLAY_API auto
    Get_DeckVisibility(
        ECk_InputHud_MetadataMode InMetadataMode,
        bool                      InHasDuration,
        bool                      InHasFrameNumbers) -> FCk_InputHud_DeckVisibility;

    CKINPUTHUDOVERLAY_API auto
    Get_EventAnimation(
        double                           InDownTimeSeconds,
        double                           InUpTimeSeconds,
        double                           InNowSeconds,
        const FCk_InputHud_RenderStyle& InStyle) -> FCk_InputHud_EventAnimation;
}
