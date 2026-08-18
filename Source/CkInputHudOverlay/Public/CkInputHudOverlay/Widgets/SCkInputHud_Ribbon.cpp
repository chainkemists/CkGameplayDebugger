#include "CkInputHudOverlay/Widgets/SCkInputHud_Ribbon.h"

#include "CkInputHudOverlay/Settings/CkInputHud_Settings.h"
#include "CkInputHudOverlay/Style/CkInputHud_RenderStyle.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"
#include "Rendering/DrawElements.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_input_hud_ribbon
{
    constexpr auto DividerGapPx   = 4.0f;
    constexpr auto DividerWidthPx = 1.0f;

    constexpr auto BarMinPx = 4.0f;

    // How many milliseconds of hold one pixel of bar is worth.
    constexpr auto BarMsPerPx = 120.0f;

    constexpr auto DashLengthPx = 2.0f;
    constexpr auto DashGapPx    = 2.0f;

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_Tinted(
            FLinearColor InColor,
            float        InOpacity)
        -> FLinearColor
    {
        InColor.A *= InOpacity;
        return InColor;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Draw_Box(
            FSlateWindowElementList& OutDrawElements,
            int32                    InLayerId,
            const FGeometry&         InGeometry,
            const FSlateBrush*       InBrush,
            const FVector2f&         InPos,
            const FVector2f&         InSize,
            const FLinearColor&      InTint)
        -> void
    {
        if (InSize.X <= 0.0f || InSize.Y <= 0.0f)
        { return; }

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            InLayerId,
            InGeometry.ToPaintGeometry(InSize, FSlateLayoutTransform{InPos}),
            InBrush,
            ESlateDrawEffect::None,
            InTint);
    }

    // A dashed rectangle, drawn as short filled segments. FCkDebuggerStyle's outline brushes are solid rings, and a
    // solid ring is exactly what the modifier distinction must NOT look like — the resolution ring already uses one.
    auto
        Draw_DashedBorder(
            FSlateWindowElementList& OutDrawElements,
            int32                    InLayerId,
            const FGeometry&         InGeometry,
            const FSlateBrush*       InBrush,
            const FVector2f&         InPos,
            const FVector2f&         InSize,
            float                    InThickness,
            float                    InCornerInset,
            const FLinearColor&      InTint)
        -> void
    {
        if (InThickness <= 0.0f)
        { return; }

        const auto Thickness = FMath::Min(InThickness, FMath::Min(InSize.X, InSize.Y) * 0.5f);
        constexpr auto Step = DashLengthPx + DashGapPx;

        const auto HorizontalEnd = FMath::Max(InCornerInset, InSize.X - InCornerInset);
        const auto VerticalEnd   = FMath::Max(InCornerInset, InSize.Y - InCornerInset);

        for (auto X = InCornerInset; X < HorizontalEnd; X += Step)
        {
            const auto Length = FMath::Min(DashLengthPx, HorizontalEnd - X);

            Draw_Box(OutDrawElements, InLayerId, InGeometry, InBrush,
                FVector2f{InPos.X + X, InPos.Y}, FVector2f{Length, Thickness}, InTint);

            Draw_Box(OutDrawElements, InLayerId, InGeometry, InBrush,
                FVector2f{InPos.X + X, InPos.Y + InSize.Y - Thickness}, FVector2f{Length, Thickness}, InTint);
        }

        for (auto Y = InCornerInset; Y < VerticalEnd; Y += Step)
        {
            const auto Length = FMath::Min(DashLengthPx, VerticalEnd - Y);

            Draw_Box(OutDrawElements, InLayerId, InGeometry, InBrush,
                FVector2f{InPos.X, InPos.Y + Y}, FVector2f{Thickness, Length}, InTint);

            Draw_Box(OutDrawElements, InLayerId, InGeometry, InBrush,
                FVector2f{InPos.X + InSize.X - Thickness, InPos.Y + Y}, FVector2f{Thickness, Length}, InTint);
        }
    }

    auto
        Make_FrameText(
            const FCk_InputHud_Event&  InEvent,
            ECk_InputHud_FrameNotation InNotation)
        -> FString
    {
        if (InEvent.DownFrame == INDEX_NONE)
        { return {}; }

        const auto HasReleaseFrame = InEvent.UpFrame != INDEX_NONE;

        switch (InNotation)
        {
            case ECk_InputHud_FrameNotation::Delta:
                return HasReleaseFrame
                    ? ck::Format_UE(TEXT("{}+{}"), InEvent.DownFrame, InEvent.UpFrame - InEvent.DownFrame)
                    : ck::Format_UE(TEXT("{}"), InEvent.DownFrame);
            case ECk_InputHud_FrameNotation::Range:
                return HasReleaseFrame
                    ? ck::Format_UE(TEXT("{}-{}"), InEvent.DownFrame, InEvent.UpFrame)
                    : ck::Format_UE(TEXT("{}"), InEvent.DownFrame);
            case ECk_InputHud_FrameNotation::Press:
            default: return ck::Format_UE(TEXT("{}"), InEvent.DownFrame);
        }
    }

    auto
        Make_DurationText(
            ECk_InputHud_EventKind InKind,
            double                 InDurationSeconds)
        -> FString
    {
        switch (InKind)
        {
            case ECk_InputHud_EventKind::Hold:
            { return ck::Format_UE(TEXT("{:.1f}"), InDurationSeconds); }
            case ECk_InputHud_EventKind::HoldRelease:
            { return ck::Format_UE(TEXT("{:.1f}s"), InDurationSeconds); }
            case ECk_InputHud_EventKind::Press:
            case ECk_InputHud_EventKind::Tap:
            default:
            { return {}; }
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputHud_Ribbon::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _Model = InArgs._Model;

    SetVisibility(EVisibility::HitTestInvisible);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputHud_Ribbon::
    Build_Layout() const
    -> FLayout
{
    using namespace ck_input_hud_ribbon;

    auto Layout = FLayout{};
    const auto RenderStyle = ck::input_hud::Get_ActiveRenderStyle();
    const auto MetadataMode = UCk_InputHud_UserSettings::Get_MetadataMode();

    if (NOT FSlateApplication::IsInitialized())
    { return Layout; }

    const auto LabelFont    = CkStyle::MonoFont(RenderStyle.LabelFontSize);
    const auto DurationFont = CkStyle::MonoFont(RenderStyle.MetadataFontSize);
    const auto FrameFont    = CkStyle::MonoFont(RenderStyle.MetadataFontSize);

    const auto FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

    const auto Model = _Model.Pin();
    if (NOT Model.IsValid())
    { return Layout; }

    const auto Now          = FApp::GetCurrentTime();
    const auto Threshold    = UCk_InputHud_Settings::Get_TapHoldThresholdMs();
    const auto FadeLifetime = UCk_InputHud_Settings::Get_FadeLifetimeSeconds();
    const auto BarMaxPx     = UCk_InputHud_Settings::Get_HoldBarMaxPx();

    const auto& Events = Model->Get_Events();
    auto Chips = TArray<FChip>{};
    Chips.Reserve(Events.Num());
    auto HasDurationText = false;
    auto HasFrameText    = false;

    // Released chips are newest-first; held chips keep press order so a two-key hold reads left-to-right in the
    // order the player pressed them.
    for (auto Index = 0; Index < Events.Num(); ++Index)
    {
        const auto& Event = Events[Index];

        const auto Held     = FCk_InputHud_Model::Get_IsHeld(Event);
        const auto Kind     = FCk_InputHud_Model::Get_EventKind(Event, Now, Threshold);
        const auto Duration = FCk_InputHud_Model::Get_DurationSeconds(Event, Now);

        auto Chip = FChip{};
        Chip.LabelText    = Event.KeyLabel;
        Chip.DurationText = MetadataMode != ECk_InputHud_MetadataMode::Keys
            ? Make_DurationText(Kind, Duration)
            : FString{};
        Chip.FrameText    = MetadataMode == ECk_InputHud_MetadataMode::Full &&
                            UCk_InputHud_Settings::Get_ShowFrameNumbers()
            ? Make_FrameText(Event, UCk_InputHud_UserSettings::Get_FrameNotation())
            : FString{};
        Chip.Kind         = Kind;
        Chip.Opacity      = ck::input_hud::Get_ComposedKeyOpacity(
            FCk_InputHud_Model::Get_FadeOpacity(Event, Now, FadeLifetime),
            RenderStyle.KeyOpacity);
        Chip.Resolved     = Event.Resolved;
        Chip.Modifier     = Event.Modifier;
        Chip.Held         = Held;
        Chip.DownTimeSeconds = Event.DownTimeSeconds;
        Chip.UpTimeSeconds   = Event.UpTimeSeconds;

        const auto PulseScale = RenderStyle.PulseScale;
        Chip.BarWidth = Kind == ECk_InputHud_EventKind::Hold || Kind == ECk_InputHud_EventKind::HoldRelease
            ? FMath::Clamp(
                BarMinPx * PulseScale + static_cast<float>(Duration * 1000.0) / BarMsPerPx * PulseScale,
                BarMinPx * PulseScale,
                BarMaxPx * PulseScale)
            : 0.0f;

        HasDurationText |= NOT Chip.DurationText.IsEmpty();
        HasFrameText    |= NOT Chip.FrameText.IsEmpty();
        Chips.Add(MoveTemp(Chip));
    }

    const auto DeckVisibility = ck::input_hud::Get_DeckVisibility(
        MetadataMode, HasDurationText, HasFrameText);
    Layout.ShowDuration     = DeckVisibility.ShowDuration;
    Layout.ShowFrameNumbers = DeckVisibility.ShowFrameNumbers;

    // Deck presence follows the content currently on screen, but every visible chip shares one height so the row
    // remains stable. Empty duration/frame decks must not turn a compact tap stream into tall blank controls.
    Layout.LabelRowHeight = static_cast<float>(FontMeasure->Measure(FString{TEXT("Wg")}, LabelFont).Y);
    Layout.GlyphRowHeight = ck::input_hud::Get_EffectiveGlyphRowHeight(RenderStyle);
    Layout.DurationRowHeight = Layout.ShowDuration
        ? static_cast<float>(FontMeasure->Measure(FString{TEXT("0.0s")}, DurationFont).Y)
        : 0.0f;
    Layout.FrameRowHeight = Layout.ShowFrameNumbers
        ? static_cast<float>(FontMeasure->Measure(FString{TEXT("0")}, FrameFont).Y)
        : 0.0f;

    Layout.TotalHeight = RenderStyle.ChipPaddingY * 2.0f + Layout.LabelRowHeight + RenderStyle.DeckGap +
        Layout.GlyphRowHeight;

    if (Layout.ShowDuration)
    { Layout.TotalHeight += RenderStyle.DeckGap + Layout.DurationRowHeight; }

    if (Layout.ShowFrameNumbers)
    { Layout.TotalHeight += RenderStyle.DeckGap + Layout.FrameRowHeight; }

    for (auto& Chip : Chips)
    {
        const auto LabelWidth = static_cast<float>(FontMeasure->Measure(Chip.LabelText, LabelFont).X);
        const auto DurationWidth = Layout.ShowDuration
            ? static_cast<float>(FontMeasure->Measure(Chip.DurationText, DurationFont).X)
            : 0.0f;
        const auto FrameWidth = Layout.ShowFrameNumbers
            ? static_cast<float>(FontMeasure->Measure(Chip.FrameText, FrameFont).X)
            : 0.0f;

        const auto ContentWidth = FMath::Max(
            FMath::Max(LabelWidth, DurationWidth),
            FMath::Max(FrameWidth, FMath::Max(Chip.BarWidth, RenderStyle.PressDotSize)));

        Chip.Width = ck::input_hud::Get_ChipWidth(
            ContentWidth,
            Layout.TotalHeight,
            RenderStyle.ChipPaddingX,
            RenderStyle.ChipMinWidth);

        if (Chip.Held)
        { Layout.Held.Add(MoveTemp(Chip)); }
        else
        { Layout.Released.Insert(MoveTemp(Chip), 0); }
    }

    const auto Accumulate = [ChipGap = RenderStyle.ChipGap](const TArray<FChip>& InChips) -> float
    {
        auto Width = 0.0f;

        for (const auto& Chip : InChips)
        { Width += Chip.Width + ChipGap; }

        return Width;
    };

    Layout.TotalWidth = Accumulate(Layout.Held) + Accumulate(Layout.Released);

    // The divider REPLACES the inter-chip gap it lands in, which is why the gap is subtracted back out here.
    if (NOT Layout.Held.IsEmpty() && NOT Layout.Released.IsEmpty())
    { Layout.TotalWidth += DividerWidthPx + DividerGapPx * 2.0f - RenderStyle.ChipGap; }

    Layout.TotalWidth = FMath::Max(0.0f, Layout.TotalWidth - RenderStyle.ChipGap);

    return Layout;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputHud_Ribbon::
    ComputeDesiredSize(
        float InLayoutScaleMultiplier) const
    -> FVector2D
{
    const auto Layout = Build_Layout();

    return FVector2D{Layout.TotalWidth, Layout.TotalHeight};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkInputHud_Ribbon::
    OnPaint(
        const FPaintArgs&        InArgs,
        const FGeometry&         InAllottedGeometry,
        const FSlateRect&        InCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32                    InLayerId,
        const FWidgetStyle&      InWidgetStyle,
        bool                     InParentEnabled) const
    -> int32
{
    using namespace ck_input_hud_ribbon;

    const auto Layout = Build_Layout();
    const auto RenderStyle = ck::input_hud::Get_ActiveRenderStyle();

    if (Layout.Held.IsEmpty() && Layout.Released.IsEmpty())
    { return InLayerId; }

    const auto* FillBrush    = CkStyle::GetFilledBrush();
    const auto* ChipBrush = ck::input_hud::Resolve_KeyBrush(
        RenderStyle.KeyCornerRadius, ECk_InputHud_KeyBrushTreatment::Fill);

    const auto LabelFont    = CkStyle::MonoFont(RenderStyle.LabelFontSize);
    const auto DurationFont = CkStyle::MonoFont(RenderStyle.MetadataFontSize);
    const auto FrameFont    = CkStyle::MonoFont(RenderStyle.MetadataFontSize);

    const auto FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

    const auto FillLayer  = InLayerId;
    const auto GlyphLayer = InLayerId + 1;
    const auto TextLayer  = InLayerId + 2;

    const auto LabelY    = RenderStyle.ChipPaddingY;
    const auto GlyphY    = LabelY + Layout.LabelRowHeight + RenderStyle.DeckGap;
    const auto DurationY = GlyphY + Layout.GlyphRowHeight + RenderStyle.DeckGap;
    const auto FrameY    = Layout.ShowDuration
        ? DurationY + Layout.DurationRowHeight + RenderStyle.DeckGap
        : DurationY;

    auto CursorX = 0.0f;

    const auto Draw_Chip = [&](const FChip& InChip) -> void
    {
        const auto Tone = InChip.Resolved ? RenderStyle.Palette.Resolved : RenderStyle.Palette.Unrouted;

        const auto Now = FApp::GetCurrentTime();
        const auto Animation = ck::input_hud::Get_EventAnimation(
            InChip.DownTimeSeconds, InChip.UpTimeSeconds, Now, RenderStyle);

        const auto ChipGeometry = InAllottedGeometry.MakeChild(
            FVector2f{InChip.Width, Layout.TotalHeight},
            FSlateLayoutTransform{FVector2f{CursorX, 0.0f}},
            FSlateRenderTransform{Animation.Scale},
            FVector2f{0.5f, 0.5f});

        const auto DimHistoryColor = [&](const FLinearColor& InColor) -> FLinearColor
        {
            return FMath::Lerp(RenderStyle.Palette.Panel, InColor, RenderStyle.HistoryBrightness);
        };

        const auto BorderColor = RenderStyle.Palette.KeyBorder;
        const auto HistoryFillColor = DimHistoryColor(RenderStyle.Palette.HistoryFill);
        const auto FillColor = FMath::Lerp(HistoryFillColor, RenderStyle.Palette.Active, Animation.ActiveBlend);
        const auto FillOpacity = FMath::Lerp(1.0f, RenderStyle.ActiveFillOpacity, Animation.ActiveBlend);
        const auto BorderOpacity = InChip.Opacity * RenderStyle.KeyBorderOpacity;
        const auto ChipSize = FVector2f{InChip.Width, Layout.TotalHeight};

        if (Animation.ActiveBlend > 0.0f && RenderStyle.ActiveGlowOpacity > 0.0f)
        {
            // As with the panel, use a tintable filled shell rather than the rounded brush's baked-white outline.
            // The cap drawn immediately afterward covers the center, leaving only the active halo visible.
            Draw_Box(OutDrawElements, FillLayer, ChipGeometry, ChipBrush,
                FVector2f{-2.0f, -2.0f}, ChipSize + FVector2f{4.0f, 4.0f},
                Get_Tinted(RenderStyle.Palette.Active,
                    InChip.Opacity * RenderStyle.ActiveGlowOpacity * Animation.ActiveBlend));
        }

        if (InChip.Modifier)
        {
            Draw_Box(OutDrawElements, FillLayer, ChipGeometry, ChipBrush,
                FVector2f::ZeroVector, ChipSize, Get_Tinted(FillColor, InChip.Opacity * FillOpacity));

            const auto CornerInset = FMath::Min(RenderStyle.KeyCornerRadius, 2.5f);
            Draw_DashedBorder(OutDrawElements, GlyphLayer, ChipGeometry, FillBrush,
                FVector2f::ZeroVector, ChipSize,
                RenderStyle.KeyBorderWidth,
                CornerInset,
                Get_Tinted(BorderColor, BorderOpacity));
        }
        else
        {
            const auto BorderWidth = FMath::Clamp(
                RenderStyle.KeyBorderWidth, 0.0f, FMath::Min(ChipSize.X, ChipSize.Y) * 0.5f);

            if (BorderWidth > 0.0f)
            {
                Draw_Box(OutDrawElements, FillLayer, ChipGeometry, ChipBrush,
                    FVector2f::ZeroVector, ChipSize, Get_Tinted(BorderColor, BorderOpacity));

                const auto InnerSize = ChipSize - FVector2f{BorderWidth * 2.0f, BorderWidth * 2.0f};
                Draw_Box(OutDrawElements, FillLayer, ChipGeometry, ChipBrush,
                    FVector2f{BorderWidth, BorderWidth}, InnerSize,
                    Get_Tinted(FillColor, InChip.Opacity * FillOpacity));
            }
            else
            {
                Draw_Box(OutDrawElements, FillLayer, ChipGeometry, ChipBrush,
                    FVector2f::ZeroVector, ChipSize, Get_Tinted(FillColor, InChip.Opacity * FillOpacity));
            }
        }

        // ---- Deck 1: the key label ----
        {
            const auto LabelWidth = static_cast<float>(FontMeasure->Measure(InChip.LabelText, LabelFont).X);

            const auto HistoryLabelTint = InChip.Resolved
                ? DimHistoryColor(RenderStyle.Palette.HistoryText)
                : RenderStyle.Palette.Unrouted;
            const auto LabelTint = FMath::Lerp(
                HistoryLabelTint, RenderStyle.Palette.Panel, Animation.ActiveBlend);

            FSlateDrawElement::MakeText(
                OutDrawElements,
                TextLayer,
                ChipGeometry.ToPaintGeometry(
                    FVector2f{InChip.Width, Layout.LabelRowHeight},
                    FSlateLayoutTransform{FVector2f{(InChip.Width - LabelWidth) * 0.5f, LabelY}}),
                InChip.LabelText,
                LabelFont,
                ESlateDrawEffect::None,
                Get_Tinted(LabelTint, InChip.Opacity));
        }

        // ---- Deck 2: the pulse ----
        {
            // The neutral history treatment belongs to the cap surface, not the signal. The centered marker remains
            // the immediate semantic read: bound/resolved uses the palette signal; unbound/unrouted uses warning.
            const auto HistoryGlyphColor = Tone;
            const auto GlyphColor = FMath::Lerp(
                HistoryGlyphColor, RenderStyle.Palette.Panel, Animation.ActiveBlend);
            const auto GlyphTint = Get_Tinted(GlyphColor, InChip.Opacity);

            switch (InChip.Kind)
            {
                case ECk_InputHud_EventKind::Hold:
                case ECk_InputHud_EventKind::HoldRelease:
                {
                    Draw_Box(OutDrawElements, GlyphLayer, ChipGeometry, FillBrush,
                        FVector2f{(InChip.Width - InChip.BarWidth) * 0.5f,
                                   GlyphY + (Layout.GlyphRowHeight - RenderStyle.HoldBarHeight * RenderStyle.PulseScale) * 0.5f},
                        FVector2f{InChip.BarWidth, RenderStyle.HoldBarHeight * RenderStyle.PulseScale},
                        GlyphTint);
                    break;
                }
                case ECk_InputHud_EventKind::Press:
                case ECk_InputHud_EventKind::Tap:
                default:
                {
                    const auto DotSize = (InChip.Kind == ECk_InputHud_EventKind::Press
                        ? RenderStyle.PressDotSize : RenderStyle.TapDotSize) * RenderStyle.PulseScale;
                    const auto* SignalBrush = ck::input_hud::Resolve_KeyBrush(
                        DotSize * 0.5f, ECk_InputHud_KeyBrushTreatment::Fill);

                    // The generic pill brush has a 99px radius and can anti-alias away inside a 3-4px box. Match the
                    // brush radius to the actual marker so the centered signal always produces foreground pixels.
                    Draw_Box(OutDrawElements, TextLayer, ChipGeometry, SignalBrush,
                        FVector2f{(InChip.Width - DotSize) * 0.5f, GlyphY + (Layout.GlyphRowHeight - DotSize) * 0.5f},
                        FVector2f{DotSize, DotSize},
                        GlyphTint);
                    break;
                }
            }

            if (Layout.ShowDuration && NOT InChip.DurationText.IsEmpty())
            {
                const auto Width = static_cast<float>(FontMeasure->Measure(InChip.DurationText, DurationFont).X);

                FSlateDrawElement::MakeText(
                    OutDrawElements,
                    TextLayer,
                    ChipGeometry.ToPaintGeometry(
                        FVector2f{InChip.Width, Layout.DurationRowHeight},
                        FSlateLayoutTransform{FVector2f{(InChip.Width - Width) * 0.5f, DurationY}}),
                    InChip.DurationText,
                    DurationFont,
                    ESlateDrawEffect::None,
                    GlyphTint);
            }
        }

        // ---- Deck 3: the frames ----
        if (Layout.ShowFrameNumbers && NOT InChip.FrameText.IsEmpty())
        {
            const auto Width = static_cast<float>(FontMeasure->Measure(InChip.FrameText, FrameFont).X);

            const auto HistoryFrameColor = InChip.Resolved
                ? DimHistoryColor(RenderStyle.Palette.Metadata)
                : RenderStyle.Palette.Unrouted;
            const auto FrameColor = FMath::Lerp(
                HistoryFrameColor, RenderStyle.Palette.Panel, Animation.ActiveBlend);

            FSlateDrawElement::MakeText(
                OutDrawElements,
                TextLayer,
                ChipGeometry.ToPaintGeometry(
                    FVector2f{InChip.Width, Layout.FrameRowHeight},
                    FSlateLayoutTransform{FVector2f{(InChip.Width - Width) * 0.5f, FrameY}}),
                InChip.FrameText,
                FrameFont,
                ESlateDrawEffect::None,
                Get_Tinted(FrameColor, InChip.Opacity * (InChip.Resolved ? CkStyle::AlphaSoft() : 1.0f)));
        }

        CursorX += InChip.Width + RenderStyle.ChipGap;
    };

    for (const auto& Chip : Layout.Held)
    { Draw_Chip(Chip); }

    if (NOT Layout.Held.IsEmpty() && NOT Layout.Released.IsEmpty())
    {
        CursorX += DividerGapPx - RenderStyle.ChipGap;

        Draw_Box(OutDrawElements, GlyphLayer, InAllottedGeometry, FillBrush,
            FVector2f{CursorX, 0.0f},
            FVector2f{DividerWidthPx, Layout.TotalHeight},
            Get_Tinted(CkStyle::Border(), CkStyle::AlphaSoft()));

        CursorX += DividerWidthPx + DividerGapPx;
    }

    for (const auto& Chip : Layout.Released)
    { Draw_Chip(Chip); }

    return TextLayer;
}

// --------------------------------------------------------------------------------------------------------------------
