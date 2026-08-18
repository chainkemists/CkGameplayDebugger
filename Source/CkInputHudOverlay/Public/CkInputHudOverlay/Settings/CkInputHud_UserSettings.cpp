#include "CkInputHudOverlay/Settings/CkInputHud_UserSettings.h"

#include "CkCore/Macros/CkMacros.h"

#if WITH_EDITOR
    #include "UObject/UnrealType.h"
#endif

// ====================================================================================================================

namespace ck_input_hud_user_settings
{
    constexpr auto LegacyActiveFillOpacity = 0.18f;
    constexpr auto LegacyActiveGlowOpacity = 0.10f;
    constexpr auto LegacyPanelOpacity      = 0.82f;

    constexpr auto DefaultActiveFillOpacity = 0.92f;
    constexpr auto PreviousActiveGlowOpacity = 0.28f;
    constexpr auto DefaultActiveGlowOpacity = 0.10f;
    constexpr auto DefaultPanelOpacity      = 0.94f;

    constexpr auto LegacyPressPopScale      = 1.08f;
    constexpr auto LegacyPressPopDurationMs = 140.0f;
    constexpr auto PreviousPressPopScale      = 1.12f;
    constexpr auto PreviousPressPopDurationMs = 180.0f;
    constexpr auto DefaultPressPopScale       = 1.20f;
    constexpr auto DefaultPressPopDurationMs  = 250.0f;

    constexpr auto LegacyKeyPaddingX       = 4.0f;
    constexpr auto LegacyKeyPaddingY       = 1.0f;
    constexpr auto LegacyHistoryBrightness = 0.55f;
    constexpr auto LegacyKeyBorderOpacity  = 0.72f;

    constexpr auto DefaultKeyPaddingX       = 5.0f;
    constexpr auto DefaultKeyPaddingY       = 3.0f;
    constexpr auto DefaultHistoryBrightness = 0.82f;
    constexpr auto DefaultKeyBorderOpacity  = 0.50f;

    constexpr auto DefaultPulseScale = 1.0f;

    auto Sanitize(ECk_InputHud_CornerStyle InValue) -> ECk_InputHud_CornerStyle;

    auto Sanitize_NonNegative(float InValue, float InFallback) -> float
    {
        return FMath::IsFinite(InValue) && InValue >= 0.0f
            ? InValue
            : InFallback;
    }

    auto Get_CornerRadiusPreset(ECk_InputHud_CornerStyle InStyle) -> float
    {
        switch (Sanitize(InStyle))
        {
            case ECk_InputHud_CornerStyle::Sharp:   return 0.0f;
            case ECk_InputHud_CornerStyle::Soft:    return 3.0f;
            case ECk_InputHud_CornerStyle::Rounded: return 6.0f;
            default:                                return 3.0f;
        }
    }

    auto Sanitize(ECk_InputHud_Palette InValue) -> ECk_InputHud_Palette
    {
        return InValue <= ECk_InputHud_Palette::TacticalMint
            ? InValue
            : ECk_InputHud_Palette::ArcticSignal;
    }

    auto Sanitize(ECk_InputHud_Density InValue) -> ECk_InputHud_Density
    {
        return InValue <= ECk_InputHud_Density::Readable
            ? InValue
            : ECk_InputHud_Density::Compact;
    }

    auto Sanitize(ECk_InputHud_MetadataMode InValue) -> ECk_InputHud_MetadataMode
    {
        return InValue <= ECk_InputHud_MetadataMode::Full
            ? InValue
            : ECk_InputHud_MetadataMode::Compact;
    }

    auto Sanitize(ECk_InputHud_FrameNotation InValue) -> ECk_InputHud_FrameNotation
    {
        return InValue <= ECk_InputHud_FrameNotation::Range
            ? InValue
            : ECk_InputHud_FrameNotation::Press;
    }

    auto Sanitize(ECk_InputHud_CornerStyle InValue) -> ECk_InputHud_CornerStyle
    {
        return InValue <= ECk_InputHud_CornerStyle::Rounded
            ? InValue
            : ECk_InputHud_CornerStyle::Rounded;
    }

    auto FromRgb(uint8 InRed, uint8 InGreen, uint8 InBlue) -> FLinearColor
    {
        return FLinearColor{FColor{InRed, InGreen, InBlue, 255}};
    }

    auto Resolve_Palette(ECk_InputHud_Palette InPalette) -> FCk_InputHud_PaletteSnapshot
    {
        switch (Sanitize(InPalette))
        {
            case ECk_InputHud_Palette::EmberTerminal:
            {
                return FCk_InputHud_PaletteSnapshot{
                    FromRgb(27, 16, 12), FromRgb(43, 31, 27), FromRgb(180, 165, 153),
                    FromRgb(224, 218, 210), FromRgb(224, 218, 210), FromRgb(242, 166, 90),
                    FromRgb(242, 166, 90), FromRgb(255, 107, 95), FromRgb(181, 137, 106)};
            }
            case ECk_InputHud_Palette::OrchidSynth:
            {
                return FCk_InputHud_PaletteSnapshot{
                    FromRgb(20, 15, 34), FromRgb(37, 32, 48), FromRgb(171, 166, 184),
                    FromRgb(221, 217, 230), FromRgb(221, 217, 230), FromRgb(179, 140, 255),
                    FromRgb(179, 140, 255), FromRgb(255, 191, 105), FromRgb(157, 137, 198)};
            }
            case ECk_InputHud_Palette::TacticalMint:
            {
                return FCk_InputHud_PaletteSnapshot{
                    FromRgb(8, 23, 20), FromRgb(25, 43, 39), FromRgb(164, 177, 172),
                    FromRgb(213, 225, 220), FromRgb(213, 225, 220), FromRgb(103, 214, 178),
                    FromRgb(103, 214, 178), FromRgb(255, 200, 87), FromRgb(116, 164, 150)};
            }
            case ECk_InputHud_Palette::ArcticSignal:
            default:
            {
                return FCk_InputHud_PaletteSnapshot{
                    FromRgb(13, 18, 25), FromRgb(31, 39, 50), FromRgb(171, 181, 194),
                    FromRgb(84, 87, 91), FromRgb(88, 92, 95), FromRgb(83, 199, 238),
                    FromRgb(83, 199, 238), FromRgb(229, 176, 76), FromRgb(125, 136, 153)};
            }
        }
    }
}

// ====================================================================================================================

UCk_InputHud_UserSettings::UCk_InputHud_UserSettings()
{
    CategoryName = TEXT("Ck");
    SectionName  = TEXT("Input HUD");

    const auto Defaults = ck_input_hud_user_settings::Resolve_Palette(ECk_InputHud_Palette::ArcticSignal);
    CustomKeyBorder = Defaults.KeyBorder;
    CustomContainerOutline = Defaults.ContainerOutline;
    CustomActive    = Defaults.Active;
    CustomResolved  = Defaults.Resolved;
    CustomUnrouted  = Defaults.Unrouted;
}

auto UCk_InputHud_UserSettings::Get_PaletteSnapshot() -> FCk_InputHud_PaletteSnapshot
{
    const auto* Settings = Get();
    auto Result = ck_input_hud_user_settings::Resolve_Palette(Settings->Palette);

    if (Settings->UseCustomColors)
    {
        Result.ContainerOutline = Settings->CustomContainerOutline.GetClamped();
        Result.KeyBorder        = Settings->CustomKeyBorder.GetClamped();
        Result.Active           = Settings->CustomActive.GetClamped();
        Result.Resolved         = Settings->CustomResolved.GetClamped();
        Result.Unrouted         = Settings->CustomUnrouted.GetClamped();
    }

    return Result;
}

auto UCk_InputHud_UserSettings::Get_Density() -> ECk_InputHud_Density
{
    return ck_input_hud_user_settings::Sanitize(Get()->Density);
}

auto UCk_InputHud_UserSettings::Get_MetadataMode() -> ECk_InputHud_MetadataMode
{
    return ck_input_hud_user_settings::Sanitize(Get()->MetadataMode);
}

auto UCk_InputHud_UserSettings::Get_FrameNotation() -> ECk_InputHud_FrameNotation
{
    return ck_input_hud_user_settings::Sanitize(Get()->FrameNotation);
}

auto UCk_InputHud_UserSettings::Get_CornerStyle() -> ECk_InputHud_CornerStyle
{
    return ck_input_hud_user_settings::Sanitize(Get()->CornerStyle);
}

auto UCk_InputHud_UserSettings::Get_KeyPaddingX() -> float
{
    return FMath::Clamp(Get()->KeyPaddingX, 0.0f, 12.0f);
}

auto UCk_InputHud_UserSettings::Get_KeyPaddingY() -> float
{
    return FMath::Clamp(Get()->KeyPaddingY, 0.0f, 6.0f);
}

auto UCk_InputHud_UserSettings::Get_KeyCornerRadius() -> float
{
    return FMath::Clamp(Get()->KeyCornerRadius, 0.0f, 12.0f);
}

auto UCk_InputHud_UserSettings::Get_KeyOpacity() -> float
{
    return FMath::Clamp(Get()->KeyOpacity, 0.0f, 1.0f);
}

auto UCk_InputHud_UserSettings::Get_KeyBorderWidth() -> float
{
    return FMath::Clamp(Get()->KeyBorderWidth, 0.0f, 2.0f);
}

auto UCk_InputHud_UserSettings::Get_KeyBorderOpacity() -> float
{
    return FMath::Clamp(Get()->KeyBorderOpacity, 0.0f, 1.0f);
}

auto UCk_InputHud_UserSettings::Get_ActiveFillOpacity() -> float
{
    return FMath::Clamp(Get()->ActiveFillOpacity, 0.0f, 1.0f);
}

auto UCk_InputHud_UserSettings::Get_ActiveGlowOpacity() -> float
{
    return FMath::Clamp(Get()->ActiveGlowOpacity, 0.0f, 1.0f);
}

auto UCk_InputHud_UserSettings::Get_PanelOpacity() -> float
{
    return FMath::Clamp(Get()->PanelOpacity, 0.15f, 1.0f);
}

auto UCk_InputHud_UserSettings::Get_PulseScale() -> float
{
    return ck_input_hud_user_settings::Sanitize_NonNegative(Get()->PulseScale, ck_input_hud_user_settings::DefaultPulseScale);
}

auto UCk_InputHud_UserSettings::Get_HistoryBrightness() -> float
{
    return FMath::Clamp(Get()->HistoryBrightness, 0.15f, 1.0f);
}

auto UCk_InputHud_UserSettings::Get_PressPopScale() -> float
{
    return ck_input_hud_user_settings::Sanitize_NonNegative(Get()->PressPopScale, ck_input_hud_user_settings::DefaultPressPopScale);
}

auto UCk_InputHud_UserSettings::Get_PressPopDurationMs() -> float
{
    return FMath::Clamp(Get()->PressPopDurationMs, 0.0f, 500.0f);
}

auto UCk_InputHud_UserSettings::Get_ReleaseEaseMs() -> float
{
    return FMath::Clamp(Get()->ReleaseEaseMs, 0.0f, 1000.0f);
}

auto UCk_InputHud_UserSettings::Set_Palette(ECk_InputHud_Palette InValue) -> void
{
    const auto Sanitized = ck_input_hud_user_settings::Sanitize(InValue);
    const auto Changed = Palette != Sanitized || UseCustomColors;
    Palette = Sanitized;
    UseCustomColors = false;
    Commit_IfChanged(Changed);
}

auto UCk_InputHud_UserSettings::Set_Density(ECk_InputHud_Density InValue) -> void
{
    const auto Sanitized = ck_input_hud_user_settings::Sanitize(InValue);
    const auto Changed = Density != Sanitized;
    Density = Sanitized;
    Commit_IfChanged(Changed);
}

auto UCk_InputHud_UserSettings::Set_MetadataMode(ECk_InputHud_MetadataMode InValue) -> void
{
    const auto Sanitized = ck_input_hud_user_settings::Sanitize(InValue);
    const auto Changed = MetadataMode != Sanitized;
    MetadataMode = Sanitized;
    Commit_IfChanged(Changed);
}

auto UCk_InputHud_UserSettings::Set_FrameNotation(ECk_InputHud_FrameNotation InValue) -> void
{
    const auto Sanitized = ck_input_hud_user_settings::Sanitize(InValue);
    const auto Changed = FrameNotation != Sanitized;
    FrameNotation = Sanitized;
    Commit_IfChanged(Changed);
}

auto UCk_InputHud_UserSettings::Set_CornerStyle(ECk_InputHud_CornerStyle InValue) -> void
{
    const auto Sanitized = ck_input_hud_user_settings::Sanitize(InValue);
    const auto CornerRadius = ck_input_hud_user_settings::Get_CornerRadiusPreset(Sanitized);
    const auto Changed = CornerStyle != Sanitized || NOT FMath::IsNearlyEqual(KeyCornerRadius, CornerRadius);
    CornerStyle = Sanitized;
    KeyCornerRadius = CornerRadius;
    Commit_IfChanged(Changed);
}

auto UCk_InputHud_UserSettings::Set_KeyPaddingX(float InValue) -> void
{
    const auto Sanitized = FMath::Clamp(InValue, 0.0f, 12.0f);
    const auto Changed = NOT FMath::IsNearlyEqual(KeyPaddingX, Sanitized);
    KeyPaddingX = Sanitized;
    Commit_IfChanged(Changed);
}

auto UCk_InputHud_UserSettings::Set_KeyPaddingY(float InValue) -> void
{
    const auto Sanitized = FMath::Clamp(InValue, 0.0f, 6.0f);
    const auto Changed = NOT FMath::IsNearlyEqual(KeyPaddingY, Sanitized);
    KeyPaddingY = Sanitized;
    Commit_IfChanged(Changed);
}

auto UCk_InputHud_UserSettings::Set_KeyCornerRadius(float InValue) -> void
{
    const auto Sanitized = FMath::Clamp(InValue, 0.0f, 12.0f);
    const auto Changed = NOT FMath::IsNearlyEqual(KeyCornerRadius, Sanitized);
    KeyCornerRadius = Sanitized;
    Commit_IfChanged(Changed);
}

auto UCk_InputHud_UserSettings::Set_KeyOpacity(float InValue) -> void
{
    const auto Sanitized = FMath::Clamp(InValue, 0.0f, 1.0f);
    const auto Changed = NOT FMath::IsNearlyEqual(KeyOpacity, Sanitized);
    KeyOpacity = Sanitized;
    Commit_IfChanged(Changed);
}

auto UCk_InputHud_UserSettings::Set_KeyBorderWidth(float InValue) -> void
{
    const auto Sanitized = FMath::Clamp(InValue, 0.0f, 2.0f);
    const auto Changed = NOT FMath::IsNearlyEqual(KeyBorderWidth, Sanitized);
    KeyBorderWidth = Sanitized;
    Commit_IfChanged(Changed);
}

auto UCk_InputHud_UserSettings::Set_KeyBorderOpacity(float InValue) -> void
{
    const auto Sanitized = FMath::Clamp(InValue, 0.0f, 1.0f);
    const auto Changed = NOT FMath::IsNearlyEqual(KeyBorderOpacity, Sanitized);
    KeyBorderOpacity = Sanitized;
    Commit_IfChanged(Changed);
}

auto UCk_InputHud_UserSettings::Set_ActiveFillOpacity(float InValue) -> void
{
    const auto Sanitized = FMath::Clamp(InValue, 0.0f, 1.0f);
    const auto Changed = NOT FMath::IsNearlyEqual(ActiveFillOpacity, Sanitized);
    ActiveFillOpacity = Sanitized;
    Commit_IfChanged(Changed);
}

auto UCk_InputHud_UserSettings::Set_ActiveGlowOpacity(float InValue) -> void
{
    const auto Sanitized = FMath::Clamp(InValue, 0.0f, 1.0f);
    const auto Changed = NOT FMath::IsNearlyEqual(ActiveGlowOpacity, Sanitized);
    ActiveGlowOpacity = Sanitized;
    Commit_IfChanged(Changed);
}

auto UCk_InputHud_UserSettings::Set_PanelOpacity(float InValue) -> void
{
    const auto Sanitized = FMath::Clamp(InValue, 0.15f, 1.0f);
    const auto Changed = NOT FMath::IsNearlyEqual(PanelOpacity, Sanitized);
    PanelOpacity = Sanitized;
    Commit_IfChanged(Changed);
}

auto UCk_InputHud_UserSettings::Set_PulseScale(float InValue) -> void
{
    const auto Sanitized = ck_input_hud_user_settings::Sanitize_NonNegative(InValue, ck_input_hud_user_settings::DefaultPulseScale);
    const auto Changed = NOT FMath::IsNearlyEqual(PulseScale, Sanitized);
    PulseScale = Sanitized;
    Commit_IfChanged(Changed);
}

auto UCk_InputHud_UserSettings::Set_HistoryBrightness(float InValue) -> void
{
    const auto Sanitized = FMath::Clamp(InValue, 0.15f, 1.0f);
    const auto Changed = NOT FMath::IsNearlyEqual(HistoryBrightness, Sanitized);
    HistoryBrightness = Sanitized;
    Commit_IfChanged(Changed);
}

auto UCk_InputHud_UserSettings::Set_PressPopScale(float InValue) -> void
{
    const auto Sanitized = ck_input_hud_user_settings::Sanitize_NonNegative(InValue, ck_input_hud_user_settings::DefaultPressPopScale);
    const auto Changed = NOT FMath::IsNearlyEqual(PressPopScale, Sanitized);
    PressPopScale = Sanitized;
    Commit_IfChanged(Changed);
}

auto UCk_InputHud_UserSettings::Set_PressPopDurationMs(float InValue) -> void
{
    const auto Sanitized = FMath::Clamp(InValue, 0.0f, 500.0f);
    const auto Changed = NOT FMath::IsNearlyEqual(PressPopDurationMs, Sanitized);
    PressPopDurationMs = Sanitized;
    Commit_IfChanged(Changed);
}

auto UCk_InputHud_UserSettings::Set_ReleaseEaseMs(float InValue) -> void
{
    const auto Sanitized = FMath::Clamp(InValue, 0.0f, 1000.0f);
    const auto Changed = NOT FMath::IsNearlyEqual(ReleaseEaseMs, Sanitized);
    ReleaseEaseMs = Sanitized;
    Commit_IfChanged(Changed);
}

auto UCk_InputHud_UserSettings::Set_CustomColor(
    ECk_InputHud_ColorRole InRole,
    FLinearColor InValue) -> void
{
    const auto Enabled = Snapshot_CustomColorsIfNeeded();
    const auto Sanitized = InValue.GetClamped();

    auto* Target = &CustomKeyBorder;
    switch (InRole)
    {
        case ECk_InputHud_ColorRole::ContainerOutline: Target = &CustomContainerOutline; break;
        case ECk_InputHud_ColorRole::Active:   Target = &CustomActive; break;
        case ECk_InputHud_ColorRole::Resolved: Target = &CustomResolved; break;
        case ECk_InputHud_ColorRole::Unrouted: Target = &CustomUnrouted; break;
        case ECk_InputHud_ColorRole::KeyBorder:
        default:                               break;
    }

    const auto Changed = NOT Target->Equals(Sanitized);
    *Target = Sanitized;
    Commit_IfChanged(Enabled || Changed);
}

auto UCk_InputHud_UserSettings::Reset_CustomColors() -> void
{
    const auto Changed = UseCustomColors;
    UseCustomColors = false;
    Commit_IfChanged(Changed);
}

auto UCk_InputHud_UserSettings::Reset_VisualTuning() -> void
{
    const auto Changed = Palette != ECk_InputHud_Palette::ArcticSignal ||
        Density != ECk_InputHud_Density::Compact ||
        CornerStyle != ECk_InputHud_CornerStyle::Rounded ||
        UseCustomColors ||
        NOT FMath::IsNearlyEqual(KeyPaddingX, ck_input_hud_user_settings::DefaultKeyPaddingX) ||
        NOT FMath::IsNearlyEqual(KeyPaddingY, ck_input_hud_user_settings::DefaultKeyPaddingY) ||
        NOT FMath::IsNearlyEqual(KeyCornerRadius, 3.0f) ||
        NOT FMath::IsNearlyEqual(KeyOpacity, 1.0f) ||
        NOT FMath::IsNearlyEqual(KeyBorderWidth, 1.0f) ||
        NOT FMath::IsNearlyEqual(KeyBorderOpacity, ck_input_hud_user_settings::DefaultKeyBorderOpacity) ||
        NOT FMath::IsNearlyEqual(ActiveFillOpacity, ck_input_hud_user_settings::DefaultActiveFillOpacity) ||
        NOT FMath::IsNearlyEqual(ActiveGlowOpacity, ck_input_hud_user_settings::DefaultActiveGlowOpacity) ||
        NOT FMath::IsNearlyEqual(PanelOpacity, ck_input_hud_user_settings::DefaultPanelOpacity) ||
        NOT FMath::IsNearlyEqual(PulseScale, 1.0f) ||
        NOT FMath::IsNearlyEqual(HistoryBrightness, ck_input_hud_user_settings::DefaultHistoryBrightness) ||
        NOT FMath::IsNearlyEqual(PressPopScale, ck_input_hud_user_settings::DefaultPressPopScale) ||
        NOT FMath::IsNearlyEqual(PressPopDurationMs, ck_input_hud_user_settings::DefaultPressPopDurationMs) ||
        NOT FMath::IsNearlyEqual(ReleaseEaseMs, 320.0f);

    Palette           = ECk_InputHud_Palette::ArcticSignal;
    Density           = ECk_InputHud_Density::Compact;
    CornerStyle       = ECk_InputHud_CornerStyle::Rounded;
    UseCustomColors   = false;
    KeyPaddingX       = ck_input_hud_user_settings::DefaultKeyPaddingX;
    KeyPaddingY       = ck_input_hud_user_settings::DefaultKeyPaddingY;
    KeyCornerRadius   = 3.0f;
    KeyOpacity        = 1.0f;
    KeyBorderWidth    = 1.0f;
    KeyBorderOpacity  = ck_input_hud_user_settings::DefaultKeyBorderOpacity;
    ActiveFillOpacity = ck_input_hud_user_settings::DefaultActiveFillOpacity;
    ActiveGlowOpacity = ck_input_hud_user_settings::DefaultActiveGlowOpacity;
    PanelOpacity      = ck_input_hud_user_settings::DefaultPanelOpacity;
    PulseScale        = 1.0f;
    HistoryBrightness = ck_input_hud_user_settings::DefaultHistoryBrightness;
    PressPopScale     = ck_input_hud_user_settings::DefaultPressPopScale;
    PressPopDurationMs = ck_input_hud_user_settings::DefaultPressPopDurationMs;
    ReleaseEaseMs      = 320.0f;
    Commit_IfChanged(Changed);
}

auto UCk_InputHud_UserSettings::Reset_ReadoutTuning() -> void
{
    const auto Changed = MetadataMode != ECk_InputHud_MetadataMode::Full ||
        FrameNotation != ECk_InputHud_FrameNotation::Delta;

    MetadataMode  = ECk_InputHud_MetadataMode::Full;
    FrameNotation = ECk_InputHud_FrameNotation::Delta;
    Commit_IfChanged(Changed);
}

auto UCk_InputHud_UserSettings::Migrate_VisualSettingsIfNeeded() -> void
{
    if (VisualSchemaVersion >= CurrentVisualSchemaVersion)
    { return; }

    if (VisualSchemaVersion < 2)
    {
        if (ActiveFillOpacity == ck_input_hud_user_settings::LegacyActiveFillOpacity)
        { ActiveFillOpacity = ck_input_hud_user_settings::DefaultActiveFillOpacity; }

        if (ActiveGlowOpacity == ck_input_hud_user_settings::LegacyActiveGlowOpacity)
        { ActiveGlowOpacity = ck_input_hud_user_settings::DefaultActiveGlowOpacity; }

        if (PanelOpacity == ck_input_hud_user_settings::LegacyPanelOpacity)
        { PanelOpacity = ck_input_hud_user_settings::DefaultPanelOpacity; }
    }

    if (VisualSchemaVersion < 3)
    {
        if (PressPopScale == ck_input_hud_user_settings::LegacyPressPopScale)
        { PressPopScale = ck_input_hud_user_settings::DefaultPressPopScale; }

        if (PressPopDurationMs == ck_input_hud_user_settings::LegacyPressPopDurationMs)
        { PressPopDurationMs = ck_input_hud_user_settings::DefaultPressPopDurationMs; }

        // The old Rounded enum was the default, so it cannot distinguish an intentional choice from an untouched
        // config. We choose the new Soft/radius-3 default for that ambiguous value; Sharp and Soft translate exactly.
        switch (ck_input_hud_user_settings::Sanitize(CornerStyle))
        {
            case ECk_InputHud_CornerStyle::Sharp:
                CornerStyle = ECk_InputHud_CornerStyle::Sharp;
                KeyCornerRadius = 0.0f;
                break;
            case ECk_InputHud_CornerStyle::Soft:
                CornerStyle = ECk_InputHud_CornerStyle::Soft;
                KeyCornerRadius = 3.0f;
                break;
            case ECk_InputHud_CornerStyle::Rounded:
            default:
                CornerStyle = ECk_InputHud_CornerStyle::Soft;
                KeyCornerRadius = 3.0f;
                break;
        }
    }

    if (VisualSchemaVersion < 4)
    {
        if (KeyPaddingX == ck_input_hud_user_settings::LegacyKeyPaddingX)
        { KeyPaddingX = ck_input_hud_user_settings::DefaultKeyPaddingX; }

        if (KeyPaddingY == ck_input_hud_user_settings::LegacyKeyPaddingY)
        { KeyPaddingY = ck_input_hud_user_settings::DefaultKeyPaddingY; }

        if (HistoryBrightness == ck_input_hud_user_settings::LegacyHistoryBrightness)
        { HistoryBrightness = ck_input_hud_user_settings::DefaultHistoryBrightness; }

        if (KeyBorderOpacity == ck_input_hud_user_settings::LegacyKeyBorderOpacity)
        { KeyBorderOpacity = ck_input_hud_user_settings::DefaultKeyBorderOpacity; }
    }

    if (VisualSchemaVersion < 5 && UseCustomColors)
    {
        // Before v5 one custom border drove both outlines. Preserve that authored key color at both sites.
        CustomContainerOutline = CustomKeyBorder;
    }

    if (VisualSchemaVersion < 6)
    {
        // These were the exact v5 defaults. Equality is intentionally narrow: non-default authored tuning survives.
        if (ActiveGlowOpacity == ck_input_hud_user_settings::PreviousActiveGlowOpacity)
        { ActiveGlowOpacity = ck_input_hud_user_settings::DefaultActiveGlowOpacity; }

        if (PressPopScale == ck_input_hud_user_settings::PreviousPressPopScale)
        { PressPopScale = ck_input_hud_user_settings::DefaultPressPopScale; }

        if (PressPopDurationMs == ck_input_hud_user_settings::PreviousPressPopDurationMs)
        { PressPopDurationMs = ck_input_hud_user_settings::DefaultPressPopDurationMs; }

        if (MetadataMode == ECk_InputHud_MetadataMode::Compact)
        { MetadataMode = ECk_InputHud_MetadataMode::Full; }

        if (FrameNotation == ECk_InputHud_FrameNotation::Press)
        { FrameNotation = ECk_InputHud_FrameNotation::Delta; }

        if (CornerStyle == ECk_InputHud_CornerStyle::Soft && FMath::IsNearlyEqual(KeyCornerRadius, 3.0f))
        { CornerStyle = ECk_InputHud_CornerStyle::Rounded; }
    }

    VisualSchemaVersion = CurrentVisualSchemaVersion;
    NotifyChanged();
    SaveConfig();
}

auto UCk_InputHud_UserSettings::NotifyChanged() -> void
{
    ++_Revision;
}

auto UCk_InputHud_UserSettings::Commit_IfChanged(bool InChanged) -> void
{
    if (NOT InChanged)
    { return; }

    NotifyChanged();
    SaveConfig();
}

auto UCk_InputHud_UserSettings::Snapshot_CustomColorsIfNeeded() -> bool
{
    if (UseCustomColors)
    { return false; }

    const auto Snapshot = ck_input_hud_user_settings::Resolve_Palette(Palette);
    CustomKeyBorder = Snapshot.KeyBorder;
    CustomContainerOutline = Snapshot.ContainerOutline;
    CustomActive    = Snapshot.Active;
    CustomResolved  = Snapshot.Resolved;
    CustomUnrouted  = Snapshot.Unrouted;
    UseCustomColors = true;
    return true;
}

#if WITH_EDITOR
auto UCk_InputHud_UserSettings::PostEditChangeProperty(
    FPropertyChangedEvent& InPropertyChangedEvent) -> void
{
    const auto PropertyName = InPropertyChangedEvent.GetPropertyName();
    if (PropertyName == GET_MEMBER_NAME_CHECKED(UCk_InputHud_UserSettings, Palette))
    { UseCustomColors = false; }
    else if (PropertyName == GET_MEMBER_NAME_CHECKED(UCk_InputHud_UserSettings, CornerStyle))
    { KeyCornerRadius = ck_input_hud_user_settings::Get_CornerRadiusPreset(CornerStyle); }
    else if (PropertyName == GET_MEMBER_NAME_CHECKED(UCk_InputHud_UserSettings, UseCustomColors) && UseCustomColors)
    {
        UseCustomColors = false;
        Snapshot_CustomColorsIfNeeded();
    }

    Super::PostEditChangeProperty(InPropertyChangedEvent);
    NotifyChanged();
}
#endif

// ====================================================================================================================
