#include "Misc/AutomationTest.h"

#include "CkCore/Format/CkFormat.h"

#include "CkInputHudOverlay/Model/CkInputHud_Model.h"
#include "CkInputHudOverlay/Settings/CkInputHud_UserSettings.h"
#include "CkInputHudOverlay/Style/CkInputHud_RenderStyle.h"
#include "CkInputHudOverlay/Subsystem/CkInputHud_Subsystem.h"

#include <limits>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_input_hud_spec
{
    constexpr auto TapHoldThresholdMs = 300.0f;
    constexpr auto FadeLifetime       = 10.0f;

    // The clock the model reads is wall time, which is never zero at runtime; the tests use a base offset so a
    // press stamped at "t = 0" cannot accidentally read as the never-pressed default.
    constexpr auto T0 = 100.0;

    struct FUserSettingsRestore
    {
        TObjectPtr<UCk_InputHud_UserSettings> Settings = nullptr;
        ECk_InputHud_Palette Palette;
        ECk_InputHud_Density Density;
        ECk_InputHud_MetadataMode MetadataMode;
        ECk_InputHud_FrameNotation FrameNotation;
        ECk_InputHud_CornerStyle CornerStyle;
        float KeyBorderWidth;
        float KeyBorderOpacity;
        float ActiveFillOpacity;
        float ActiveGlowOpacity;
        float PanelOpacity;
        float PulseScale;
        float HistoryBrightness;
        float PressPopScale;
        float PressPopDurationMs;
        float ReleaseEaseMs;
        float KeyPaddingX;
        float KeyPaddingY;
        float KeyCornerRadius;
        float OverallOpacity;
        bool UseCustomColors;
        FLinearColor CustomKeyBorder;
        FLinearColor CustomContainerOutline;
        FLinearColor CustomActive;
        FLinearColor CustomResolved;
        FLinearColor CustomUnrouted;

        FUserSettingsRestore()
            : Settings(UCk_InputHud_UserSettings::Get_Mutable())
            , Palette(Settings->Palette)
            , Density(Settings->Density)
            , MetadataMode(Settings->MetadataMode)
            , FrameNotation(Settings->FrameNotation)
            , CornerStyle(Settings->CornerStyle)
            , KeyBorderWidth(Settings->KeyBorderWidth)
            , KeyBorderOpacity(Settings->KeyBorderOpacity)
            , ActiveFillOpacity(Settings->ActiveFillOpacity)
            , ActiveGlowOpacity(Settings->ActiveGlowOpacity)
            , PanelOpacity(Settings->PanelOpacity)
            , PulseScale(Settings->PulseScale)
            , HistoryBrightness(Settings->HistoryBrightness)
            , PressPopScale(Settings->PressPopScale)
            , PressPopDurationMs(Settings->PressPopDurationMs)
            , ReleaseEaseMs(Settings->ReleaseEaseMs)
            , KeyPaddingX(Settings->KeyPaddingX)
            , KeyPaddingY(Settings->KeyPaddingY)
            , KeyCornerRadius(Settings->KeyCornerRadius)
            , OverallOpacity(Settings->OverallOpacity)
            , UseCustomColors(Settings->UseCustomColors)
            , CustomKeyBorder(Settings->CustomKeyBorder)
            , CustomContainerOutline(Settings->CustomContainerOutline)
            , CustomActive(Settings->CustomActive)
            , CustomResolved(Settings->CustomResolved)
            , CustomUnrouted(Settings->CustomUnrouted)
        {}

        ~FUserSettingsRestore()
        {
            Settings->Palette = Palette;
            Settings->Density = Density;
            Settings->MetadataMode = MetadataMode;
            Settings->FrameNotation = FrameNotation;
            Settings->CornerStyle = CornerStyle;
            Settings->KeyBorderWidth = KeyBorderWidth;
            Settings->KeyBorderOpacity = KeyBorderOpacity;
            Settings->ActiveFillOpacity = ActiveFillOpacity;
            Settings->ActiveGlowOpacity = ActiveGlowOpacity;
            Settings->PanelOpacity = PanelOpacity;
            Settings->PulseScale = PulseScale;
            Settings->HistoryBrightness = HistoryBrightness;
            Settings->PressPopScale = PressPopScale;
            Settings->PressPopDurationMs = PressPopDurationMs;
            Settings->ReleaseEaseMs = ReleaseEaseMs;
            Settings->KeyPaddingX = KeyPaddingX;
            Settings->KeyPaddingY = KeyPaddingY;
            Settings->KeyCornerRadius = KeyCornerRadius;
            Settings->OverallOpacity = OverallOpacity;
            Settings->UseCustomColors = UseCustomColors;
            Settings->CustomKeyBorder = CustomKeyBorder;
            Settings->CustomContainerOutline = CustomContainerOutline;
            Settings->CustomActive = CustomActive;
            Settings->CustomResolved = CustomResolved;
            Settings->CustomUnrouted = CustomUnrouted;
            Settings->SaveConfig();
            Settings->NotifyChanged();
        }
    };

    auto
        Get_Label(
            const FCk_InputHud_Model& InModel,
            int32                     InIndex)
        -> FString
    {
        const auto& Events = InModel.Get_Events();

        return Events.IsValidIndex(InIndex) ? Events[InIndex].KeyLabel : FString{};
    }

    auto
        Get_Kind(
            const FCk_InputHud_Model& InModel,
            int32                     InIndex,
            double                    InNow)
        -> ECk_InputHud_EventKind
    {
        return FCk_InputHud_Model::Get_EventKind(InModel.Get_Events()[InIndex], InNow, TapHoldThresholdMs);
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInputHud_RenderStyle_Test,
    "Ck.InputHud.RenderStyle.SignalStripTuning",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInputHud_RenderStyle_Test::RunTest(const FString&)
{
    using namespace ck::input_hud;
    using namespace ck_input_hud_spec;

    const auto Restore = FUserSettingsRestore{};
    auto* Settings = UCk_InputHud_UserSettings::Get_Mutable();
    Settings->Reset_VisualTuning();
    Settings->Reset_ReadoutTuning();

    const auto Default = Get_ActiveRenderStyle();
    TestNotNull(TEXT("Signal Strip resolves a panel brush"), Resolve_Brush(Default.PanelBrushShape));
    TestNotNull(TEXT("Signal Strip resolves a cap brush"), Resolve_Brush(Default.ChipBrushShape));
    TestNotNull(TEXT("Signal Strip resolves a shape-matched panel outline"), Resolve_OutlineBrush(Default.PanelBrushShape));
    TestNotNull(TEXT("Signal Strip resolves a shape-matched cap outline"), Resolve_OutlineBrush(Default.ChipBrushShape));
    TestNotEqual(TEXT("large rounded surfaces keep their own outline radius"),
        Resolve_OutlineBrush(ECk_InputHud_BrushShape::RoundedLarge),
        Resolve_OutlineBrush(ECk_InputHud_BrushShape::Rounded));
    TestNotNull(TEXT("numeric key radius resolves a fill brush"),
        Resolve_KeyBrush(Default.KeyCornerRadius, ECk_InputHud_KeyBrushTreatment::Fill));
    TestNotNull(TEXT("numeric key radius resolves an outline brush"),
        Resolve_KeyBrush(Default.KeyCornerRadius, ECk_InputHud_KeyBrushTreatment::Outline));
    TestNotNull(TEXT("compact signal dot resolves a radius-matched foreground brush"),
        Resolve_KeyBrush(Default.TapDotSize * 0.5f, ECk_InputHud_KeyBrushTreatment::Fill));
    TestTrue(TEXT("compact signal dot keeps a visible pixel footprint"), Default.TapDotSize >= 4.0f);
    TestTrue(TEXT("numeric key brush resolution is stable"),
        Resolve_KeyBrush(3.0f, ECk_InputHud_KeyBrushTreatment::Fill) ==
        Resolve_KeyBrush(3.0f, ECk_InputHud_KeyBrushTreatment::Fill));
    TestTrue(TEXT("distinct numeric radii resolve distinct cached brushes"),
        Resolve_KeyBrush(3.0f, ECk_InputHud_KeyBrushTreatment::Fill) !=
        Resolve_KeyBrush(6.0f, ECk_InputHud_KeyBrushTreatment::Fill));
    TestTrue(TEXT("Signal Strip has a thin key border"), FMath::IsNearlyEqual(Default.KeyBorderWidth, 1.0f));
    TestTrue(TEXT("Signal Strip uses the mockup's strong active fill"), FMath::IsNearlyEqual(Default.ActiveFillOpacity, 0.92f));
    TestTrue(TEXT("Signal Strip uses the accepted restrained active glow"), FMath::IsNearlyEqual(Default.ActiveGlowOpacity, 0.10f));
    TestTrue(TEXT("Signal Strip uses the accepted press pop"),
        FMath::IsNearlyEqual(Default.PressPopScale, 1.20f) &&
        FMath::IsNearlyEqual(Default.PressPopDurationSeconds, 0.25f));
    TestTrue(TEXT("Signal Strip is compact by default"), Default.ChipMinWidth <= 18.0f && Default.LabelFontSize <= 9);
    TestTrue(TEXT("Compact uses the mockup padding and radius defaults"),
        FMath::IsNearlyEqual(Default.ChipPaddingX, 5.0f) &&
        FMath::IsNearlyEqual(Default.ChipPaddingY, 3.0f) &&
        FMath::IsNearlyEqual(Default.KeyCornerRadius, 3.0f));
    TestTrue(TEXT("Compact keeps visible gutters between keys"), FMath::IsNearlyEqual(Default.ChipGap, 3.0f));
    TestTrue(TEXT("history text remains legible without becoming white"),
        Default.Palette.HistoryText.GetLuminance() > Default.Palette.HistoryFill.GetLuminance() &&
        Default.Palette.HistoryText.GetLuminance() < FLinearColor::White.GetLuminance());
    TestTrue(TEXT("history uses a restrained border and bright-muted text"),
        FMath::IsNearlyEqual(Default.KeyBorderOpacity, 0.50f) &&
        FMath::IsNearlyEqual(Default.HistoryBrightness, 0.82f));
    TestTrue(TEXT("overall opacity defaults to fully opaque"),
        FMath::IsNearlyEqual(Default.OverallOpacity, 1.0f));
    TestFalse(TEXT("released keys remain distinct from the panel"), Default.Palette.HistoryFill.Equals(Default.Palette.Panel));
    TestTrue(TEXT("Arctic bound signal uses the palette's cyan family"),
        Default.Palette.Resolved.Equals(Default.Palette.Active));
    TestTrue(TEXT("Arctic container outline matches the accepted Style Lab value"),
        Default.Palette.ContainerOutline.Equals(FLinearColor{FColor{84, 87, 91, 255}}));
    TestTrue(TEXT("Arctic key outline matches the accepted Style Lab value"),
        Default.Palette.KeyBorder.Equals(FLinearColor{FColor{88, 92, 95, 255}}));
    TestEqual(TEXT("accepted readout defaults to full metadata"),
        UCk_InputHud_UserSettings::Get_MetadataMode(), ECk_InputHud_MetadataMode::Full);
    TestEqual(TEXT("accepted readout defaults to delta frames"),
        UCk_InputHud_UserSettings::Get_FrameNotation(), ECk_InputHud_FrameNotation::Delta);
    TestEqual(TEXT("accepted visual defaults to rounded surfaces"),
        UCk_InputHud_UserSettings::Get_CornerStyle(), ECk_InputHud_CornerStyle::Rounded);
    TestFalse(TEXT("unrouted keys remain distinct from active keys"), Default.Palette.Unrouted.Equals(Default.Palette.Active));

    const auto KeysDecks = Get_DeckVisibility(ECk_InputHud_MetadataMode::Keys, true, true);
    const auto CompactTapDecks = Get_DeckVisibility(ECk_InputHud_MetadataMode::Compact, false, true);
    const auto CompactHoldDecks = Get_DeckVisibility(ECk_InputHud_MetadataMode::Compact, true, true);
    const auto FullTapDecks = Get_DeckVisibility(ECk_InputHud_MetadataMode::Full, false, true);
    TestFalse(TEXT("Keys mode never reserves a duration deck"), KeysDecks.ShowDuration);
    TestFalse(TEXT("Keys mode never reserves a frame deck"), KeysDecks.ShowFrameNumbers);
    TestFalse(TEXT("Compact tap history does not reserve an empty duration deck"), CompactTapDecks.ShowDuration);
    TestTrue(TEXT("Compact reveals duration only when a hold supplies content"), CompactHoldDecks.ShowDuration);
    TestFalse(TEXT("Compact never reserves frame metadata"), CompactHoldDecks.ShowFrameNumbers);
    TestFalse(TEXT("Full tap history does not reserve an empty duration deck"), FullTapDecks.ShowDuration);
    TestTrue(TEXT("Full reveals frame metadata when a frame exists"), FullTapDecks.ShowFrameNumbers);

    Settings->Set_CornerStyle(ECk_InputHud_CornerStyle::Sharp);
    const auto SharpCorners = Get_ActiveRenderStyle();
    Settings->Set_CornerStyle(ECk_InputHud_CornerStyle::Soft);
    const auto SoftCorners = Get_ActiveRenderStyle();
    Settings->Set_CornerStyle(ECk_InputHud_CornerStyle::Rounded);
    const auto RoundedCorners = Get_ActiveRenderStyle();
    TestEqual(TEXT("Sharp keys resolve to square caps"),
        SharpCorners.ChipBrushShape, ECk_InputHud_BrushShape::Square);
    TestEqual(TEXT("Soft keys resolve to the small rounded family"),
        SoftCorners.ChipBrushShape, ECk_InputHud_BrushShape::Rounded);
    TestEqual(TEXT("Rounded keys resolve to the visibly larger radius"),
        RoundedCorners.ChipBrushShape, ECk_InputHud_BrushShape::RoundedLarge);
    TestEqual(TEXT("Sharp preset sets a zero key radius"), SharpCorners.KeyCornerRadius, 0.0f, 0.001f);
    TestEqual(TEXT("Soft preset sets the mockup key radius"), SoftCorners.KeyCornerRadius, 3.0f, 0.001f);
    TestEqual(TEXT("Rounded preset sets the larger key radius"), RoundedCorners.KeyCornerRadius, 6.0f, 0.001f);

    for (const auto Palette : {ECk_InputHud_Palette::ArcticSignal, ECk_InputHud_Palette::EmberTerminal,
        ECk_InputHud_Palette::OrchidSynth, ECk_InputHud_Palette::TacticalMint})
    {
        Settings->Set_Palette(Palette);
        const auto Snapshot = UCk_InputHud_UserSettings::Get_PaletteSnapshot();
        TestFalse(TEXT("palette has distinct active and unrouted colors"), Snapshot.Active.Equals(Snapshot.Unrouted));
        TestTrue(TEXT("palette bound signal follows its active theme family"),
            Snapshot.Resolved.Equals(Snapshot.Active));
        TestFalse(TEXT("palette history remains neutral instead of using the active color"),
            Snapshot.HistoryFill.Equals(Snapshot.Active));
        TestTrue(TEXT("palette history text is brighter than its surface"),
            Snapshot.HistoryText.GetLuminance() > Snapshot.HistoryFill.GetLuminance());
    }

    Settings->Set_Density(ECk_InputHud_Density::Compact);
    const auto Compact = Get_ActiveRenderStyle();
    Settings->Set_Density(ECk_InputHud_Density::Standard);
    const auto Standard = Get_ActiveRenderStyle();
    Settings->Set_Density(ECk_InputHud_Density::Readable);
    const auto Readable = Get_ActiveRenderStyle();
    TestTrue(TEXT("density geometry is monotonic"),
        Compact.ChipMinWidth < Standard.ChipMinWidth && Standard.ChipMinWidth < Readable.ChipMinWidth);

    TestEqual(TEXT("short keys use height as their square width"),
        Get_ChipWidth(10.0f, 32.0f, 4.0f, 18.0f), 32.0f, 0.001f);
    TestEqual(TEXT("long metadata expands beyond the square width"),
        Get_ChipWidth(40.0f, 32.0f, 4.0f, 18.0f), 48.0f, 0.001f);
    TestEqual(TEXT("horizontal padding independently grows content-driven width"),
        Get_ChipWidth(40.0f, 32.0f, 7.0f, 18.0f), 54.0f, 0.001f);
    TestEqual(TEXT("a taller vertically padded key also grows its square width"),
        Get_ChipWidth(10.0f, 36.0f, 4.0f, 18.0f), 36.0f, 0.001f);

    const auto CustomKeyBorder = FLinearColor{0.18f, 0.29f, 0.40f, 1.0f};
    const auto CustomContainerOutline = FLinearColor{0.74f, 0.81f, 0.92f, 1.0f};
    const auto CustomActive    = FLinearColor{0.91f, 0.18f, 0.41f, 1.0f};
    const auto CustomResolved  = FLinearColor{0.21f, 0.81f, 0.38f, 1.0f};
    const auto CustomUnrouted  = FLinearColor{0.95f, 0.55f, 0.12f, 1.0f};
    Settings->Set_CustomColor(ECk_InputHud_ColorRole::Active, CustomActive);
    TestTrue(TEXT("first custom edit snapshots and enables the complete semantic override set"), Settings->UseCustomColors);
    TestTrue(TEXT("custom active color is retained"), UCk_InputHud_UserSettings::Get_PaletteSnapshot().Active.Equals(Settings->CustomActive));
    TestTrue(TEXT("first custom edit preserves the palette's other semantic colors"),
        UCk_InputHud_UserSettings::Get_PaletteSnapshot().Unrouted.Equals(Settings->CustomUnrouted));
    Settings->Set_CustomColor(ECk_InputHud_ColorRole::KeyBorder, CustomKeyBorder);
    Settings->Set_CustomColor(ECk_InputHud_ColorRole::ContainerOutline, CustomContainerOutline);
    Settings->Set_CustomColor(ECk_InputHud_ColorRole::Resolved, CustomResolved);
    Settings->Set_CustomColor(ECk_InputHud_ColorRole::Unrouted, CustomUnrouted);
    const auto CustomSnapshot = UCk_InputHud_UserSettings::Get_PaletteSnapshot();
    TestTrue(TEXT("custom key border color tunes every key outline"),
        CustomSnapshot.KeyBorder.Equals(CustomKeyBorder));
    TestTrue(TEXT("custom container outline remains independent from key outlines"),
        CustomSnapshot.ContainerOutline.Equals(CustomContainerOutline) &&
        NOT CustomSnapshot.ContainerOutline.Equals(CustomSnapshot.KeyBorder));
    TestTrue(TEXT("custom resolved color is retained"), CustomSnapshot.Resolved.Equals(CustomResolved));
    TestTrue(TEXT("custom unrouted color is retained"), CustomSnapshot.Unrouted.Equals(CustomUnrouted));

    Settings->Set_Palette(ECk_InputHud_Palette::ArcticSignal);
    TestFalse(TEXT("palette selection clears custom overrides"), Settings->UseCustomColors);

    Settings->Set_KeyBorderWidth(99.0f);
    TestTrue(TEXT("border width getter is total and clamped"), UCk_InputHud_UserSettings::Get_KeyBorderWidth() <= 2.0f);
    Settings->Set_KeyBorderOpacity(99.0f);
    Settings->Set_ActiveFillOpacity(-1.0f);
    Settings->Set_ActiveGlowOpacity(99.0f);
    Settings->Set_PanelOpacity(-1.0f);
    TestTrue(TEXT("border opacity is clamped"), UCk_InputHud_UserSettings::Get_KeyBorderOpacity() <= 1.0f);
    TestTrue(TEXT("active fill opacity is clamped"), UCk_InputHud_UserSettings::Get_ActiveFillOpacity() >= 0.0f);
    TestTrue(TEXT("active glow opacity is clamped"), UCk_InputHud_UserSettings::Get_ActiveGlowOpacity() <= 1.0f);
    TestTrue(TEXT("panel opacity preserves the legibility floor"), UCk_InputHud_UserSettings::Get_PanelOpacity() >= 0.15f);

    Settings->Set_HistoryBrightness(-1.0f);
    Settings->Set_PressPopScale(99.0f);
    Settings->Set_PressPopDurationMs(9999.0f);
    Settings->Set_ReleaseEaseMs(-1.0f);
    TestTrue(TEXT("history brightness preserves its legibility floor"),
        UCk_InputHud_UserSettings::Get_HistoryBrightness() >= 0.15f);
    TestEqual(TEXT("press pop scale has no arbitrary upper limit"),
        UCk_InputHud_UserSettings::Get_PressPopScale(), 99.0f, 0.001f);
    TestTrue(TEXT("press pop duration is clamped"), UCk_InputHud_UserSettings::Get_PressPopDurationMs() <= 500.0f);
    TestTrue(TEXT("release easing is clamped"), UCk_InputHud_UserSettings::Get_ReleaseEaseMs() >= 0.0f);

    Settings->Set_KeyPaddingX(99.0f);
    Settings->Set_KeyPaddingY(-1.0f);
    Settings->Set_KeyCornerRadius(99.0f);
    Settings->Set_OverallOpacity(-1.0f);
    TestTrue(TEXT("horizontal key padding is clamped"), UCk_InputHud_UserSettings::Get_KeyPaddingX() <= 12.0f);
    TestTrue(TEXT("vertical key padding is clamped"), UCk_InputHud_UserSettings::Get_KeyPaddingY() >= 0.0f);
    TestTrue(TEXT("numeric key radius is clamped"), UCk_InputHud_UserSettings::Get_KeyCornerRadius() <= 12.0f);
    TestTrue(TEXT("overall opacity is clamped"), UCk_InputHud_UserSettings::Get_OverallOpacity() >= 0.0f);

    TestEqual(TEXT("overlay opacity multiplies the idle fade, the cvar, and the user setting"),
        Get_ComposedOverlayOpacity(0.5f, 0.8f, 0.5f), 0.2f, 0.001f);
    TestEqual(TEXT("composed overlay opacity clamps malformed inputs"),
        Get_ComposedOverlayOpacity(2.0f, 2.0f, -1.0f), 0.0f, 0.001f);

    // The cvar keeps the legibility floor it has always had, so a stray `ck.InputOverlay.Opacity 0` cannot make the
    // overlay invisible and unreportable...
    TestEqual(TEXT("the cvar keeps its legibility floor"),
        Get_ComposedOverlayOpacity(1.0f, 0.0f, 1.0f), 0.15f, 0.001f);

    // ...but that floor must NOT leak onto the explicit, persisted user setting, or Overall opacity 0 would leave a
    // ghost of the strip on screen with no way to clear it.
    TestEqual(TEXT("an explicit overall opacity of zero is fully transparent"),
        Get_ComposedOverlayOpacity(1.0f, 1.0f, 0.0f), 0.0f, 0.001f);

    Settings->Reset_VisualTuning();
    const auto AnimationStyle = Get_ActiveRenderStyle();
    const auto HeldStart = Get_EventAnimation(T0, 0.0, T0, AnimationStyle);
    const auto HeldMid = Get_EventAnimation(
        T0, 0.0, T0 + AnimationStyle.PressPopDurationSeconds * 0.5, AnimationStyle);
    const auto HeldEnd = Get_EventAnimation(
        T0, 0.0, T0 + AnimationStyle.PressPopDurationSeconds, AnimationStyle);
    TestEqual(TEXT("a press starts at the tuned pop scale"), HeldStart.Scale, AnimationStyle.PressPopScale, 0.001f);
    TestTrue(TEXT("press pop eases continuously toward one"),
        HeldMid.Scale < HeldStart.Scale && HeldMid.Scale > 1.0f);
    TestEqual(TEXT("press pop settles at one"), HeldEnd.Scale, 1.0f, 0.001f);
    TestEqual(TEXT("held keys remain fully active after the pop"), HeldEnd.ActiveBlend, 1.0f, 0.001f);

    constexpr auto ReleaseAt = T0 + 0.2;
    const auto ReleaseStart = Get_EventAnimation(T0, ReleaseAt, ReleaseAt, AnimationStyle);
    const auto ReleaseMid = Get_EventAnimation(
        T0, ReleaseAt, ReleaseAt + AnimationStyle.ReleaseEaseSeconds * 0.5, AnimationStyle);
    const auto ReleaseEnd = Get_EventAnimation(
        T0, ReleaseAt, ReleaseAt + AnimationStyle.ReleaseEaseSeconds, AnimationStyle);
    TestEqual(TEXT("release begins from the active treatment"), ReleaseStart.ActiveBlend, 1.0f, 0.001f);
    TestTrue(TEXT("release eases continuously into history"),
        ReleaseMid.ActiveBlend < 1.0f && ReleaseMid.ActiveBlend > 0.0f);
    TestEqual(TEXT("release settles into the semantic history treatment"), ReleaseEnd.ActiveBlend, 0.0f, 0.001f);

    const auto FutureFixture = Get_EventAnimation(T0 + 1000.0, T0 + 1000.1, T0, AnimationStyle);
    TestEqual(TEXT("future-authored Style Lab history does not freeze in the pop"), FutureFixture.Scale, 1.0f, 0.001f);
    TestEqual(TEXT("future-authored Style Lab history is released, not active"), FutureFixture.ActiveBlend, 0.0f, 0.001f);

    Settings->Set_Density(ECk_InputHud_Density::Compact);
    Settings->Set_PulseScale(2.0f);
    const auto MaxPulseStyle = Get_ActiveRenderStyle();
    TestTrue(TEXT("max pulse scale expands the glyph row instead of overlapping adjacent metadata"),
        Get_EffectiveGlyphRowHeight(MaxPulseStyle) >= MaxPulseStyle.PressDotSize * MaxPulseStyle.PulseScale &&
        Get_EffectiveGlyphRowHeight(MaxPulseStyle) >= MaxPulseStyle.HoldBarHeight * MaxPulseStyle.PulseScale);

    Settings->Set_PulseScale(-1.0f);
    TestEqual(TEXT("invalid negative pulse scale fails closed to its default"),
        UCk_InputHud_UserSettings::Get_PulseScale(), 1.0f, 0.001f);
    Settings->Set_PulseScale(37.0f);
    TestEqual(TEXT("pulse scale has no arbitrary upper limit"),
        UCk_InputHud_UserSettings::Get_PulseScale(), 37.0f, 0.001f);
    TestEqual(TEXT("whole-overlay scale accepts values above the old cap"), Get_ValidOverlayScale(12.5f), 12.5f, 0.001f);
    TestEqual(TEXT("whole-overlay scale accepts zero"), Get_ValidOverlayScale(0.0f), 0.0f, 0.001f);
    TestEqual(TEXT("negative whole-overlay scale fails closed"), Get_ValidOverlayScale(-1.0f), 1.0f, 0.001f);
    TestEqual(TEXT("non-finite whole-overlay scale fails closed"),
        Get_ValidOverlayScale(std::numeric_limits<float>::infinity()), 1.0f, 0.001f);
    Settings->Set_MetadataMode(ECk_InputHud_MetadataMode::Keys);
    TestEqual(TEXT("keys metadata mode persists"), static_cast<uint8>(UCk_InputHud_UserSettings::Get_MetadataMode()), static_cast<uint8>(ECk_InputHud_MetadataMode::Keys));
    Settings->Set_FrameNotation(ECk_InputHud_FrameNotation::Range);
    TestEqual(TEXT("frame notation persists"), static_cast<uint8>(UCk_InputHud_UserSettings::Get_FrameNotation()), static_cast<uint8>(ECk_InputHud_FrameNotation::Range));

    Settings->Density = static_cast<ECk_InputHud_Density>(255);
    Settings->MetadataMode = static_cast<ECk_InputHud_MetadataMode>(255);
    Settings->FrameNotation = static_cast<ECk_InputHud_FrameNotation>(255);
    Settings->CornerStyle = static_cast<ECk_InputHud_CornerStyle>(255);
    TestEqual(TEXT("unknown density fails closed to Compact"),
        UCk_InputHud_UserSettings::Get_Density(), ECk_InputHud_Density::Compact);
    TestEqual(TEXT("unknown metadata fails closed to Compact"),
        UCk_InputHud_UserSettings::Get_MetadataMode(), ECk_InputHud_MetadataMode::Compact);
    TestEqual(TEXT("unknown frame notation fails closed to Press"),
        UCk_InputHud_UserSettings::Get_FrameNotation(), ECk_InputHud_FrameNotation::Press);
    TestEqual(TEXT("unknown corner style fails closed to Rounded"),
        UCk_InputHud_UserSettings::Get_CornerStyle(), ECk_InputHud_CornerStyle::Rounded);

    Settings->Reset_VisualTuning();
    Settings->Reset_ReadoutTuning();
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInputHud_DefaultEnabled_Test,
    "Ck.InputHud.Config.DefaultEnabled",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInputHud_DefaultEnabled_Test::RunTest(const FString&)
{
    TestEqual(TEXT("editor and non-shipping builds default the keyboard overlay on"),
        ck::input_hud::DefaultOverlayMode, 1);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInputHud_EventLifecycle_Test,
    "Ck.InputHud.Model.EventLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInputHud_EventLifecycle_Test::RunTest(const FString&)
{
    using namespace ck_input_hud_spec;

    auto Model = FCk_InputHud_Model{};

    TestEqual(TEXT("empty model has no events"), Model.Get_Events().Num(), 0);
    TestEqual(TEXT("nothing is open"), Model.TryGet_OpenEvent(TEXT("E")), INDEX_NONE);

    const auto First = Model.Open_Event(TEXT("E"), FName(TEXT("E")), 10, T0, false);

    TestEqual(TEXT("the press is addressable"), Model.TryGet_OpenEvent(TEXT("E")), First);
    TestEqual(TEXT("an open press counts as held"), Model.Get_HeldNum(), 1);
    TestEqual(TEXT("and not as history"), Model.Get_ReleasedNum(), 0);

    Model.Set_EventResolution(First, TEXT("Dash"), true);

    TestEqual(TEXT("the intent lands"), Model.Get_Events()[First].IntentLabel, FString(TEXT("Dash")));
    TestTrue(TEXT("and marks the chip resolved"), Model.Get_Events()[First].Resolved);

    Model.Close_Event(First, 14, T0 + 0.1);

    TestEqual(TEXT("a released press stops being held"), Model.Get_HeldNum(), 0);
    TestEqual(TEXT("and becomes history"), Model.Get_ReleasedNum(), 1);
    TestEqual(TEXT("the up frame is recorded"), Model.Get_Events()[First].UpFrame, 14);
    TestEqual(TEXT("a closed press is no longer open"), Model.TryGet_OpenEvent(TEXT("E")), INDEX_NONE);

    // ---- Repeated presses of one key are INDEPENDENT chips ----
    const auto Second = Model.Open_Event(TEXT("E"), FName(TEXT("E")), 20, T0 + 1.0, false);

    TestEqual(TEXT("a second press appends rather than reopening"), Model.Get_Events().Num(), 2);
    TestEqual(TEXT("and it is the one a release would close"), Model.TryGet_OpenEvent(TEXT("E")), Second);

    // ---- A release resolves to the NEWEST open press of that key ----
    const auto Third = Model.Open_Event(TEXT("E"), FName(TEXT("E")), 25, T0 + 1.2, false);
    TestEqual(TEXT("the newest open press wins the address"), Model.TryGet_OpenEvent(TEXT("E")), Third);

    Model.Close_Event(Third, 26, T0 + 1.3);
    TestEqual(TEXT("closing it exposes the older one again"), Model.TryGet_OpenEvent(TEXT("E")), Second);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInputHud_EventKind_Test,
    "Ck.InputHud.Model.EventKind",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInputHud_EventKind_Test::RunTest(const FString&)
{
    using namespace ck_input_hud_spec;

    auto Model = FCk_InputHud_Model{};

    const auto Live = Model.Open_Event(TEXT("W"), FName(TEXT("W")), 1, T0, false);

    TestEqual(TEXT("a fresh press is Press"),
        static_cast<int32>(Get_Kind(Model, Live, T0 + 0.05)),
        static_cast<int32>(ECk_InputHud_EventKind::Press));

    TestEqual(TEXT("held past the threshold it is Hold"),
        static_cast<int32>(Get_Kind(Model, Live, T0 + 0.5)),
        static_cast<int32>(ECk_InputHud_EventKind::Hold));

    TestEqual(TEXT("a live hold measures against NOW"),
        FCk_InputHud_Model::Get_DurationSeconds(Model.Get_Events()[Live], T0 + 0.5), 0.5);

    const auto Quick = Model.Open_Event(TEXT("Q"), FName(TEXT("Q")), 2, T0, false);
    Model.Close_Event(Quick, 3, T0 + 0.1);

    TestEqual(TEXT("released under the threshold is Tap"),
        static_cast<int32>(Get_Kind(Model, Quick, T0 + 5.0)),
        static_cast<int32>(ECk_InputHud_EventKind::Tap));

    const auto Long = Model.Open_Event(TEXT("R"), FName(TEXT("R")), 4, T0, false);
    Model.Close_Event(Long, 40, T0 + 2.6);

    TestEqual(TEXT("released past the threshold is HoldRelease"),
        static_cast<int32>(Get_Kind(Model, Long, T0 + 5.0)),
        static_cast<int32>(ECk_InputHud_EventKind::HoldRelease));

    TestEqual(TEXT("a closed hold freezes its duration"),
        FCk_InputHud_Model::Get_DurationSeconds(Model.Get_Events()[Long], T0 + 30.0), 2.6);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInputHud_HistoryCap_Test,
    "Ck.InputHud.Model.HistoryCap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInputHud_HistoryCap_Test::RunTest(const FString&)
{
    using namespace ck_input_hud_spec;

    constexpr auto Cap = 3;

    auto Model = FCk_InputHud_Model{};

    // One pinned hold, then more releases than the cap allows.
    Model.Open_Event(TEXT("Shift"), FName(TEXT("Shift")), 0, T0, true);

    for (auto Index = 0; Index < 6; ++Index)
    {
        const auto Label = ck::Format_UE(TEXT("K{}"), Index);
        const auto Opened = Model.Open_Event(Label, FName{*Label}, Index + 1, T0, false);
        Model.Close_Event(Opened, Index + 2, T0 + 0.05);
    }

    Model.Enforce_HistoryCap(Cap);

    TestEqual(TEXT("released entries are capped"), Model.Get_ReleasedNum(), Cap);
    TestEqual(TEXT("the held entry is never evicted"), Model.Get_HeldNum(), 1);
    TestEqual(TEXT("and it survives at the front"), Get_Label(Model, 0), FString(TEXT("Shift")));
    TestEqual(TEXT("the OLDEST releases went"), Get_Label(Model, 1), FString(TEXT("K3")));
    TestEqual(TEXT("newest release kept"), Get_Label(Model, 3), FString(TEXT("K5")));

    // A cap smaller than the number of holds still evicts nothing that is held.
    Model.Enforce_HistoryCap(0);
    TestEqual(TEXT("a zero cap clears history only"), Model.Get_ReleasedNum(), 0);
    TestEqual(TEXT("the hold is still pinned"), Model.Get_HeldNum(), 1);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInputHud_Fade_Test,
    "Ck.InputHud.Model.Fade",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInputHud_Fade_Test::RunTest(const FString&)
{
    using namespace ck_input_hud_spec;

    auto Model = FCk_InputHud_Model{};

    const auto Held = Model.Open_Event(TEXT("Shift"), FName(TEXT("Shift")), 0, T0, true);

    const auto Released = Model.Open_Event(TEXT("E"), FName(TEXT("E")), 1, T0, false);
    Model.Close_Event(Released, 2, T0 + 0.05);

    const auto ReleaseTime = Model.Get_Events()[Released].UpTimeSeconds;

    const auto Fade = [&](double InAtSeconds) -> float
    {
        return FCk_InputHud_Model::Get_FadeOpacity(Model.Get_Events()[Released], InAtSeconds, FadeLifetime);
    };

    TestEqual(TEXT("full at the moment of release"), Fade(ReleaseTime), 1.0f);
    TestEqual(TEXT("fade is proportional one-third through the lifetime"),
        Fade(ReleaseTime + 3.0), 0.7f, 0.001f);
    TestEqual(TEXT("fade is half opacity halfway through the lifetime"),
        Fade(ReleaseTime + 5.0), 0.5f, 0.001f);

    TestEqual(TEXT("gone at the end of the lifetime"), Fade(ReleaseTime + FadeLifetime), 0.0f);

    TestEqual(TEXT("a held entry never fades"),
        FCk_InputHud_Model::Get_FadeOpacity(Model.Get_Events()[Held], T0 + 1000.0, FadeLifetime), 1.0f);

    // ---- Pruning follows the same math ----
    Model.Prune_FadedEvents(ReleaseTime + 5.0, FadeLifetime);
    TestEqual(TEXT("a fading entry is kept"), Model.Get_Events().Num(), 2);

    Model.Prune_FadedEvents(ReleaseTime + FadeLifetime, FadeLifetime);
    TestEqual(TEXT("a spent entry is pruned"), Model.Get_ReleasedNum(), 0);
    TestEqual(TEXT("the hold outlives every lifetime"), Model.Get_HeldNum(), 1);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInputHud_LayerLine_Test,
    "Ck.InputHud.Model.LayerLineRebuild",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInputHud_LayerLine_Test::RunTest(const FString&)
{
    auto Model = FCk_InputHud_Model{};

    auto Layers = TArray<TPair<int32, FString>>{};
    Layers.Emplace(100, TEXT("Modal"));
    Layers.Emplace(10,  TEXT("Player"));

    TestTrue(TEXT("first set rebuilds"), Model.Set_Layers(Layers));
    TestEqual(TEXT("line joins top-down with the compact footer separator"),
        Model.Get_LayerLine(), FString(TEXT("Modal · Player")));
    TestEqual(TEXT("the leading layer is separately tintable"), Model.Get_LayerPrimary(), FString(TEXT("Modal")));
    TestEqual(TEXT("the remainder is separately tintable"), Model.Get_LayerRemainder(), FString(TEXT("Player")));

    TestFalse(TEXT("an unchanged list does not rebuild"), Model.Set_Layers(Layers));

    // Same names, different priority — the compare key carries BOTH, so this is a real change.
    Layers[1] = TPair<int32, FString>{20, TEXT("Player")};
    TestTrue(TEXT("a priority move rebuilds"), Model.Set_Layers(Layers));

    Layers.RemoveAt(0);
    TestTrue(TEXT("a popped layer rebuilds"), Model.Set_Layers(Layers));
    TestEqual(TEXT("line follows the pop"), Model.Get_LayerLine(), FString(TEXT("Player")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInputHud_Reset_Test,
    "Ck.InputHud.Model.Reset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInputHud_Reset_Test::RunTest(const FString&)
{
    using namespace ck_input_hud_spec;

    auto Model = FCk_InputHud_Model{};

    Model.Open_Event(TEXT("E"), FName(TEXT("E")), 0, T0, false);
    Model.Set_HasSource(true);
    Model.Set_LastSeenSamplerFrame(42);

    auto Layers = TArray<TPair<int32, FString>>{};
    Layers.Emplace(1, TEXT("Player"));
    Model.Set_Layers(Layers);

    // Losing the source must not leave a key pinned forever: the release will never arrive.
    Model.Reset_VolatileState(T0 + 1.0);

    TestEqual(TEXT("open entries are closed, not dropped"), Model.Get_HeldNum(), 0);
    TestEqual(TEXT("and become history"), Model.Get_ReleasedNum(), 1);
    TestEqual(TEXT("layer line cleared"), Model.Get_LayerLine(), FString{});
    TestEqual(TEXT("sampler cursor cleared"), Model.Get_LastSeenSamplerFrame(), INDEX_NONE);

    Model.Reset();

    TestEqual(TEXT("reset empties the stream"), Model.Get_Events().Num(), 0);
    TestFalse(TEXT("source cleared"), Model.Get_HasSource());

    // Reset must also clear the layer COMPARE key, or the first post-reset set would be seen as unchanged and the
    // line would stay empty forever.
    TestTrue(TEXT("the first set after reset rebuilds"), Model.Set_Layers(Layers));
    TestEqual(TEXT("line rebuilt"), Model.Get_LayerLine(), FString(TEXT("Player")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInputHud_LivenessSweep_Test,
    "Ck.InputHud.Model.LivenessSweep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInputHud_LivenessSweep_Test::RunTest(const FString&)
{
    using namespace ck_input_hud_spec;

    auto Model = FCk_InputHud_Model{};

    // A genuinely held key, and an ORPHAN whose routed release never arrived (focus-loss flush, or a rapid
    // click whose within-frame ordering paired as release-then-press).
    const auto StillHeld = Model.Open_Event(TEXT("W"), FName(TEXT("W")), 1, T0, false);
    const auto Orphan    = Model.Open_Event(TEXT("LMB"), FName(TEXT("LeftMouseButton")), 2, T0 + 0.1, false);

    auto PhysicallyHeld = TSet<FName>{};
    PhysicallyHeld.Add(FName(TEXT("W")));

    Model.Close_OpenEventsNotHeld(
        [&](const FName& InKeyName) { return PhysicallyHeld.Contains(InKeyName); },
        T0 + 1.0);

    TestEqual(TEXT("the physically held key stays pinned"), Model.TryGet_OpenEvent(TEXT("W")), StillHeld);
    TestEqual(TEXT("the orphan is closed"), Model.TryGet_OpenEvent(TEXT("LMB")), INDEX_NONE);
    TestFalse(TEXT("closed, not dropped — it lands in history"),
        FCk_InputHud_Model::Get_IsHeld(Model.Get_Events()[Orphan]));
    TestEqual(TEXT("with no up-frame to name"), Model.Get_Events()[Orphan].UpFrame, INDEX_NONE);
    TestEqual(TEXT("and only history counts changed"), Model.Get_HeldNum(), 1);

    // The sweep is idempotent: a second pass with the same truth closes nothing further.
    Model.Close_OpenEventsNotHeld(
        [&](const FName& InKeyName) { return PhysicallyHeld.Contains(InKeyName); },
        T0 + 2.0);
    TestEqual(TEXT("a second sweep is a no-op"), Model.Get_HeldNum(), 1);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
