#include "CkInputHudOverlay/Widgets/SCkInputHud_Ribbon.h"

#include "CkInputHudOverlay/Settings/CkInputHud_Settings.h"

#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_input_hud_ribbon
{
    constexpr auto LabelFontPx    = 10;
    constexpr auto DurationFontPx = 6;
    constexpr auto FrameFontPx    = 6;

    constexpr auto GlyphRowPx     = 7.0f;
    constexpr auto DeckGapPx      = 2.0f;
    constexpr auto ChipGapPx      = 3.0f;
    constexpr auto ChipPadXPx     = 3.0f;
    constexpr auto ChipMinWidthPx = 17.0f;

    constexpr auto DividerGapPx   = 4.0f;
    constexpr auto DividerWidthPx = 1.0f;

    constexpr auto TapDotPx    = 3.0f;
    constexpr auto PressDotPx  = 4.5f;
    constexpr auto BarMinPx    = 4.0f;
    constexpr auto BarHeightPx = 3.0f;

    // How many milliseconds of hold one pixel of bar is worth.
    constexpr auto BarMsPerPx = 120.0f;

    constexpr auto DashLengthPx = 2.0f;
    constexpr auto DashGapPx    = 2.0f;
    constexpr auto BorderPx     = 1.0f;

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_ToneColor(
            bool InResolved)
        -> FLinearColor
    {
        // Resolution is the ONE thing colour says here, which is why the modifier distinction is a shape.
        return InResolved ? CkStyle::Ok() : CkStyle::Warn();
    }

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
            const FLinearColor&      InTint)
        -> void
    {
        constexpr auto Step = DashLengthPx + DashGapPx;

        for (auto X = 0.0f; X < InSize.X; X += Step)
        {
            const auto Length = FMath::Min(DashLengthPx, InSize.X - X);

            Draw_Box(OutDrawElements, InLayerId, InGeometry, InBrush,
                FVector2f{InPos.X + X, InPos.Y}, FVector2f{Length, BorderPx}, InTint);

            Draw_Box(OutDrawElements, InLayerId, InGeometry, InBrush,
                FVector2f{InPos.X + X, InPos.Y + InSize.Y - BorderPx}, FVector2f{Length, BorderPx}, InTint);
        }

        for (auto Y = 0.0f; Y < InSize.Y; Y += Step)
        {
            const auto Length = FMath::Min(DashLengthPx, InSize.Y - Y);

            Draw_Box(OutDrawElements, InLayerId, InGeometry, InBrush,
                FVector2f{InPos.X, InPos.Y + Y}, FVector2f{BorderPx, Length}, InTint);

            Draw_Box(OutDrawElements, InLayerId, InGeometry, InBrush,
                FVector2f{InPos.X + InSize.X - BorderPx, InPos.Y + Y}, FVector2f{BorderPx, Length}, InTint);
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Make_FrameText(
            const FCk_InputHud_Event&    InEvent,
            ECk_InputHud_EventKind       InKind)
        -> FString
    {
        if (InEvent.DownFrame == INDEX_NONE)
        { return {}; }

        const auto IsCompletedHold = InKind == ECk_InputHud_EventKind::HoldRelease && InEvent.UpFrame != INDEX_NONE;

        return IsCompletedHold
            ? FString::Printf(TEXT("%d-%d"), InEvent.DownFrame, InEvent.UpFrame)
            : FString::Printf(TEXT("%d"), InEvent.DownFrame);
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
            { return FString::Printf(TEXT("%.1f"), InDurationSeconds); }
            case ECk_InputHud_EventKind::HoldRelease:
            { return FString::Printf(TEXT("%.1fs"), InDurationSeconds); }
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
    Layout.ShowFrameNumbers = UCk_InputHud_Settings::Get_ShowFrameNumbers();

    if (NOT FSlateApplication::IsInitialized())
    { return Layout; }

    const auto LabelFont    = CkStyle::MonoFont(LabelFontPx);
    const auto DurationFont = CkStyle::MonoFont(DurationFontPx);
    const auto FrameFont    = CkStyle::MonoFont(FrameFontPx);

    const auto FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

    // Row heights come from reference strings rather than per-chip content, so every chip's decks line up even
    // when one of them has no duration readout to draw.
    Layout.LabelRowHeight    = static_cast<float>(FontMeasure->Measure(FString{TEXT("Wg")}, LabelFont).Y);
    Layout.DurationRowHeight = static_cast<float>(FontMeasure->Measure(FString{TEXT("0.0s")}, DurationFont).Y);
    Layout.FrameRowHeight    = Layout.ShowFrameNumbers
        ? static_cast<float>(FontMeasure->Measure(FString{TEXT("0")}, FrameFont).Y)
        : 0.0f;

    Layout.TotalHeight = Layout.LabelRowHeight + DeckGapPx + GlyphRowPx + Layout.DurationRowHeight;

    if (Layout.ShowFrameNumbers)
    { Layout.TotalHeight += DeckGapPx + Layout.FrameRowHeight; }

    const auto Model = _Model.Pin();
    if (NOT Model.IsValid())
    { return Layout; }

    const auto Now          = FApp::GetCurrentTime();
    const auto Threshold    = UCk_InputHud_Settings::Get_TapHoldThresholdMs();
    const auto FadeLifetime = UCk_InputHud_Settings::Get_FadeLifetimeSeconds();
    const auto BarMaxPx     = UCk_InputHud_Settings::Get_HoldBarMaxPx();

    const auto& Events = Model->Get_Events();

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
        Chip.DurationText = Make_DurationText(Kind, Duration);
        Chip.FrameText    = Layout.ShowFrameNumbers ? Make_FrameText(Event, Kind) : FString{};
        Chip.Kind         = Kind;
        Chip.Opacity      = FCk_InputHud_Model::Get_FadeOpacity(Event, Now, FadeLifetime);
        Chip.Resolved     = Event.Resolved;
        Chip.Modifier     = Event.Modifier;
        Chip.Held         = Held;

        Chip.BarWidth = Kind == ECk_InputHud_EventKind::Hold || Kind == ECk_InputHud_EventKind::HoldRelease
            ? FMath::Clamp(BarMinPx + static_cast<float>(Duration * 1000.0) / BarMsPerPx, BarMinPx, BarMaxPx)
            : 0.0f;

        const auto LabelWidth    = static_cast<float>(FontMeasure->Measure(Chip.LabelText, LabelFont).X);
        const auto DurationWidth = static_cast<float>(FontMeasure->Measure(Chip.DurationText, DurationFont).X);
        const auto FrameWidth    = static_cast<float>(FontMeasure->Measure(Chip.FrameText, FrameFont).X);

        const auto ContentWidth = FMath::Max(
            FMath::Max(LabelWidth, DurationWidth),
            FMath::Max(FrameWidth, FMath::Max(Chip.BarWidth, PressDotPx)));

        Chip.Width = FMath::Max(ChipMinWidthPx, ContentWidth + ChipPadXPx * 2.0f);

        if (Held)
        { Layout.Held.Add(MoveTemp(Chip)); }
        else
        { Layout.Released.Insert(MoveTemp(Chip), 0); }
    }

    const auto Accumulate = [](const TArray<FChip>& InChips) -> float
    {
        auto Width = 0.0f;

        for (const auto& Chip : InChips)
        { Width += Chip.Width + ChipGapPx; }

        return Width;
    };

    Layout.TotalWidth = Accumulate(Layout.Held) + Accumulate(Layout.Released);

    // The divider REPLACES the inter-chip gap it lands in, which is why the gap is subtracted back out here.
    if (NOT Layout.Held.IsEmpty() && NOT Layout.Released.IsEmpty())
    { Layout.TotalWidth += DividerWidthPx + DividerGapPx * 2.0f - ChipGapPx; }

    Layout.TotalWidth = FMath::Max(0.0f, Layout.TotalWidth - ChipGapPx);

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

    if (Layout.Held.IsEmpty() && Layout.Released.IsEmpty())
    { return InLayerId; }

    const auto* FillBrush = FAppStyle::GetBrush("WhiteBrush");
    const auto* ChipBrush = FCkDebuggerStyle::Get_SquareBrush();
    const auto* DotBrush  = CkStyle::GetRoundedBrush_Pill();

    const auto LabelFont    = CkStyle::MonoFont(LabelFontPx);
    const auto DurationFont = CkStyle::MonoFont(DurationFontPx);
    const auto FrameFont    = CkStyle::MonoFont(FrameFontPx);

    const auto FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

    const auto FillLayer  = InLayerId;
    const auto GlyphLayer = InLayerId + 1;
    const auto TextLayer  = InLayerId + 2;

    const auto LabelY    = 0.0f;
    const auto GlyphY    = Layout.LabelRowHeight + DeckGapPx;
    const auto DurationY = GlyphY + GlyphRowPx;
    const auto FrameY    = DurationY + Layout.DurationRowHeight + DeckGapPx;

    auto CursorX = 0.0f;

    const auto Draw_Chip = [&](const FChip& InChip) -> void
    {
        const auto Tone = Get_ToneColor(InChip.Resolved);

        if (InChip.Held)
        {
            // Inverted: the accent fills the chip and the label reads out of it — the same vocabulary the device
            // widgets use for a pressed cap.
            Draw_Box(OutDrawElements, FillLayer, InAllottedGeometry, ChipBrush,
                FVector2f{CursorX, LabelY},
                FVector2f{InChip.Width, Layout.TotalHeight},
                Get_Tinted(Tone, InChip.Opacity * CkStyle::AlphaStrong()));
        }

        if (InChip.Modifier)
        {
            Draw_DashedBorder(OutDrawElements, GlyphLayer, InAllottedGeometry, FillBrush,
                FVector2f{CursorX, LabelY},
                FVector2f{InChip.Width, Layout.TotalHeight},
                Get_Tinted(InChip.Held ? CkStyle::BgRoot() : Tone, InChip.Opacity));
        }

        // ---- Deck 1: the key label ----
        {
            const auto LabelWidth = static_cast<float>(FontMeasure->Measure(InChip.LabelText, LabelFont).X);

            const auto LabelTint = InChip.Held
                ? CkStyle::BgRoot()
                : (InChip.Resolved ? CkStyle::Text() : CkStyle::Warn());

            FSlateDrawElement::MakeText(
                OutDrawElements,
                TextLayer,
                InAllottedGeometry.ToPaintGeometry(
                    FVector2f{InChip.Width, Layout.LabelRowHeight},
                    FSlateLayoutTransform{FVector2f{CursorX + (InChip.Width - LabelWidth) * 0.5f, LabelY}}),
                InChip.LabelText,
                LabelFont,
                ESlateDrawEffect::None,
                Get_Tinted(LabelTint, InChip.Opacity));
        }

        // ---- Deck 2: the pulse ----
        {
            const auto GlyphTint = Get_Tinted(InChip.Held ? CkStyle::BgRoot() : Tone, InChip.Opacity);

            switch (InChip.Kind)
            {
                case ECk_InputHud_EventKind::Hold:
                case ECk_InputHud_EventKind::HoldRelease:
                {
                    Draw_Box(OutDrawElements, GlyphLayer, InAllottedGeometry, FillBrush,
                        FVector2f{CursorX + (InChip.Width - InChip.BarWidth) * 0.5f,
                                  GlyphY + (GlyphRowPx - BarHeightPx) * 0.5f},
                        FVector2f{InChip.BarWidth, BarHeightPx},
                        GlyphTint);
                    break;
                }
                case ECk_InputHud_EventKind::Press:
                case ECk_InputHud_EventKind::Tap:
                default:
                {
                    const auto DotSize = InChip.Kind == ECk_InputHud_EventKind::Press ? PressDotPx : TapDotPx;

                    Draw_Box(OutDrawElements, GlyphLayer, InAllottedGeometry, DotBrush,
                        FVector2f{CursorX + (InChip.Width - DotSize) * 0.5f, GlyphY + (GlyphRowPx - DotSize) * 0.5f},
                        FVector2f{DotSize, DotSize},
                        GlyphTint);
                    break;
                }
            }

            if (NOT InChip.DurationText.IsEmpty())
            {
                const auto Width = static_cast<float>(FontMeasure->Measure(InChip.DurationText, DurationFont).X);

                FSlateDrawElement::MakeText(
                    OutDrawElements,
                    TextLayer,
                    InAllottedGeometry.ToPaintGeometry(
                        FVector2f{InChip.Width, Layout.DurationRowHeight},
                        FSlateLayoutTransform{FVector2f{CursorX + (InChip.Width - Width) * 0.5f, DurationY}}),
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

            const auto FrameTint = InChip.Held ? CkStyle::BgRoot() : CkStyle::TextMute();

            FSlateDrawElement::MakeText(
                OutDrawElements,
                TextLayer,
                InAllottedGeometry.ToPaintGeometry(
                    FVector2f{InChip.Width, Layout.FrameRowHeight},
                    FSlateLayoutTransform{FVector2f{CursorX + (InChip.Width - Width) * 0.5f, FrameY}}),
                InChip.FrameText,
                FrameFont,
                ESlateDrawEffect::None,
                Get_Tinted(FrameTint, InChip.Opacity * CkStyle::AlphaSoft()));
        }

        CursorX += InChip.Width + ChipGapPx;
    };

    for (const auto& Chip : Layout.Held)
    { Draw_Chip(Chip); }

    if (NOT Layout.Held.IsEmpty() && NOT Layout.Released.IsEmpty())
    {
        CursorX += DividerGapPx - ChipGapPx;

        Draw_Box(OutDrawElements, GlyphLayer, InAllottedGeometry, FillBrush,
            FVector2f{CursorX, LabelY},
            FVector2f{DividerWidthPx, Layout.TotalHeight},
            Get_Tinted(CkStyle::Border(), CkStyle::AlphaSoft()));

        CursorX += DividerWidthPx + DividerGapPx;
    }

    for (const auto& Chip : Layout.Released)
    { Draw_Chip(Chip); }

    return TextLayer;
}

// --------------------------------------------------------------------------------------------------------------------
