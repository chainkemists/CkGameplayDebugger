#include "CkGoapDebuggerStyle.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Styling/SlateStyleRegistry.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

// ====================================================================================================================

TSharedPtr<FSlateStyleSet> FCkGoapDebuggerStyle::StyleInstance = nullptr;

// ====================================================================================================================

auto
    FCkGoapDebuggerStyle::
    Initialize()
    -> void
{
    if (NOT StyleInstance.IsValid())
    {
        StyleInstance = Create();
        FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
    }
}

auto
    FCkGoapDebuggerStyle::
    Shutdown()
    -> void
{
    if (StyleInstance.IsValid())
    {
        FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
        StyleInstance.Reset();
    }
}

auto
    FCkGoapDebuggerStyle::
    Get()
    -> const ISlateStyle&
{
    return *StyleInstance;
}

auto
    FCkGoapDebuggerStyle::
    GetStyleSetName()
    -> FName
{
    static const FName StyleSetName(TEXT("CkGoapDebuggerStyle"));
    return StyleSetName;
}

// ====================================================================================================================

auto
    FCkGoapDebuggerStyle::
    Create()
    -> TSharedRef<FSlateStyleSet>
{
    auto Style = MakeShared<FSlateStyleSet>(GetStyleSetName());

    CreateBrushes(Style);
    CreateTextStyles(Style);

    return Style;
}

auto
    FCkGoapDebuggerStyle::
    CreateBrushes(TSharedRef<FSlateStyleSet> InStyle)
    -> void
{
    const auto Muted = CkStyle::OverlayOf(CkStyle::TextMute(), 0.75f);

    // Flat fills -------------------------------------------------------------
    InStyle->Set("CkGoap.Bg.Root",    new FSlateColorBrush(CkStyle::Bg1()));
    InStyle->Set("CkGoap.Bg.Panel",   new FSlateColorBrush(CkStyle::Bg3()));
    InStyle->Set("CkGoap.Bg.Surface", new FSlateColorBrush(CkStyle::Bg2()));
    InStyle->Set("CkGoap.Bg.Black",   new FSlateColorBrush(CkStyle::BgRoot()));

    InStyle->Set("CkGoap.Border.Subtle", new FSlateColorBrush(CkStyle::Border()));
    InStyle->Set("CkGoap.Border.Strong", new FSlateColorBrush(CkStyle::BorderStrong()));

    InStyle->Set("CkGoap.Separator", new FSlateColorBrush(CkStyle::Border()));

    // Sidebar / list rows
    InStyle->Set("CkGoap.Sidebar.Bg",        new FSlateColorBrush(CkStyle::Bg3()));
    InStyle->Set("CkGoap.Sidebar.RowHover",  new FSlateColorBrush(CkStyle::OverlayOf(CkStyle::Border(), 0.27f)));
    InStyle->Set("CkGoap.Sidebar.RowSelected", new FSlateColorBrush(CkStyle::Border()));

    // Cards / surfaces
    InStyle->Set("CkGoap.Surface.Rounded", new FSlateRoundedBoxBrush(
        CkStyle::Bg2(), CornerRadius_Large,
        CkStyle::Border(), Border_Standard));

    InStyle->Set("CkGoap.Crumb.Default", new FSlateRoundedBoxBrush(
        CkStyle::Bg3(), CornerRadius_Medium,
        CkStyle::Border(), Border_Thin));

    InStyle->Set("CkGoap.Crumb.Selected", new FSlateRoundedBoxBrush(
        CkStyle::Bg3(), CornerRadius_Medium,
        CkStyle::Warn(), Border_Standard));

    // Graph nodes
    InStyle->Set("CkGoap.Graph.Background", new FSlateColorBrush(CkStyle::Bg1()));
    InStyle->Set("CkGoap.Graph.NodeBg", new FSlateRoundedBoxBrush(
        CkStyle::Bg2(), CornerRadius_Large,
        CkStyle::Border(), Border_Standard));
    InStyle->Set("CkGoap.Graph.NodeBg.InPlan", new FSlateRoundedBoxBrush(
        CkStyle::Bg2(), CornerRadius_Large,
        CkStyle::Info(), Border_Strong));
    InStyle->Set("CkGoap.Graph.NodeBg.Selected", new FSlateRoundedBoxBrush(
        CkStyle::Bg2(), CornerRadius_Large,
        CkStyle::Warn(), Border_Strong));

    // Badges -- background tints (badge text colours come from text style)
    InStyle->Set("CkGoap.Badge.Found", new FSlateRoundedBoxBrush(
        CkStyle::OverlayOf(CkStyle::Ok(), 0.13f),
        CornerRadius_Badge,
        CkStyle::OverlayOf(CkStyle::Ok(), 0.27f),
        Border_Thin));

    InStyle->Set("CkGoap.Badge.Planning", new FSlateRoundedBoxBrush(
        CkStyle::OverlayOf(CkStyle::Info(), 0.13f),
        CornerRadius_Badge,
        CkStyle::OverlayOf(CkStyle::Info(), 0.27f),
        Border_Thin));

    InStyle->Set("CkGoap.Badge.Idle", new FSlateRoundedBoxBrush(
        CkStyle::OverlayOf(Muted, 0.13f),
        CornerRadius_Badge,
        CkStyle::OverlayOf(Muted, 0.33f),
        Border_Thin));

    InStyle->Set("CkGoap.Badge.Failed", new FSlateRoundedBoxBrush(
        CkStyle::OverlayOf(CkStyle::Err(), 0.13f),
        CornerRadius_Badge,
        CkStyle::Err(),
        Border_Thin));

    // WorldState rail — recently-changed row tint. Mockup uses `#f59e0b0a`
    // (amber at ~4% alpha) plus a tiny radius.
    InStyle->Set("CkGoap.WS.RowRecent", new FSlateRoundedBoxBrush(
        CkStyle::OverlayOf(CkStyle::Warn(), 0.06f),
        CornerRadius_Small));
}

auto
    FCkGoapDebuggerStyle::
    CreateTextStyles(TSharedRef<FSlateStyleSet> InStyle)
    -> void
{
    // Sizes come from CkStyle roles, resolved here at Initialize() (module startup, past static
    // init), so an Editor Preferences typography edit moves this style set with the rest of the
    // suite. They deliberately do NOT compose TextScale: an FTextBlockStyle bakes its font at
    // style-set creation, so a registered style cannot follow a live axis (see CkDebuggerAxes.h,
    // "Typography"). Text that must track TextScale composes through ck::debug_axes::ScaledFont at
    // the call site instead — which is what every GOAP panel now does.
    const auto Regular     = FCoreStyle::GetDefaultFontStyle("Regular", CkStyle::FontSizeBody());
    const auto Bold        = FCoreStyle::GetDefaultFontStyle("Bold",    CkStyle::FontSizeBody());
    const auto Mono        = FCoreStyle::GetDefaultFontStyle("Mono",    CkStyle::FontSizeBody());
    const auto MonoSmall   = FCoreStyle::GetDefaultFontStyle("Mono",    CkStyle::FontSizeMicro());
    const auto Header      = FCoreStyle::GetDefaultFontStyle("Bold",    CkStyle::FontSizeH3());
    const auto LargeHeader = FCoreStyle::GetDefaultFontStyle("Bold",    CkStyle::FontSizeH2());
    // Deliberate outlier: the Mission Control product title is a stop ABOVE the H2 role and has no
    // role of its own. Kept literal rather than inventing an H1 nobody else would use.
    const auto TitleHeader = FCoreStyle::GetDefaultFontStyle("Bold",    15);

    const auto Faint = CkStyle::OverlayOf(CkStyle::TextMute(), 0.75f);

    InStyle->Set("CkGoap.Text.Normal",    FTextBlockStyle().SetFont(Regular).SetColorAndOpacity(CkStyle::Text()));
    InStyle->Set("CkGoap.Text.Secondary", FTextBlockStyle().SetFont(Regular).SetColorAndOpacity(CkStyle::TextDim()));
    InStyle->Set("CkGoap.Text.Muted",     FTextBlockStyle().SetFont(Regular).SetColorAndOpacity(CkStyle::TextDim()));
    InStyle->Set("CkGoap.Text.Dim",       FTextBlockStyle().SetFont(Regular).SetColorAndOpacity(CkStyle::TextMute()));
    InStyle->Set("CkGoap.Text.Faint",     FTextBlockStyle().SetFont(Regular).SetColorAndOpacity(Faint));

    InStyle->Set("CkGoap.Text.Bold",      FTextBlockStyle().SetFont(Bold).SetColorAndOpacity(CkStyle::Text()));

    InStyle->Set("CkGoap.Text.Mono",      FTextBlockStyle().SetFont(Mono).SetColorAndOpacity(CkStyle::TextMute()));
    InStyle->Set("CkGoap.Text.Mono.Tiny", FTextBlockStyle().SetFont(MonoSmall).SetColorAndOpacity(Faint));

    InStyle->Set("CkGoap.Text.Header",      FTextBlockStyle().SetFont(Header).SetColorAndOpacity(CkStyle::TextDim()));
    InStyle->Set("CkGoap.Text.LargeHeader", FTextBlockStyle().SetFont(LargeHeader).SetColorAndOpacity(CkStyle::Text()));

    InStyle->Set("CkGoap.Text.Title",
        FTextBlockStyle().SetFont(TitleHeader).SetColorAndOpacity(CkStyle::Warn()));

    // Status accents
    InStyle->Set("CkGoap.Text.Status.PlanFound", FTextBlockStyle().SetFont(Bold).SetColorAndOpacity(CkStyle::Ok()));
    InStyle->Set("CkGoap.Text.Status.Planning",  FTextBlockStyle().SetFont(Bold).SetColorAndOpacity(CkStyle::Accent()));
    InStyle->Set("CkGoap.Text.Status.Failed",    FTextBlockStyle().SetFont(Bold).SetColorAndOpacity(CkStyle::Err()));
    InStyle->Set("CkGoap.Text.Status.Selected",  FTextBlockStyle().SetFont(Bold).SetColorAndOpacity(CkStyle::Warn()));
    // Composite (Action + Planner) is a TIER distinction, not a severity — the categorical purple
    // role, not a tone. CategoryAge is the palette's purple slot (also index 4 of Get_CategoricalColor).
    InStyle->Set("CkGoap.Text.Status.Composite", FTextBlockStyle().SetFont(Bold).SetColorAndOpacity(CkStyle::CategoryAge()));
}
