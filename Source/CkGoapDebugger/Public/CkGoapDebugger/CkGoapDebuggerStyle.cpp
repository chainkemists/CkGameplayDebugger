#include "CkGoapDebuggerStyle.h"

#include "CkCore/Macros/CkMacros.h"

#include "Styling/SlateStyleRegistry.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

// ====================================================================================================================

TSharedPtr<FSlateStyleSet> FCkGoapDebuggerStyle::StyleInstance = nullptr;

// -- Palette --------------------------------------------------------------------
// Hex → linear: FColor::FromHex applies sRGB conversion automatically when
// constructing FLinearColor from FColor, so the resulting tones match the
// mockup as it renders in a browser.

const FLinearColor FCkGoapDebuggerStyle::Color_Bg_Root            = FLinearColor(FColor::FromHex(TEXT("#0a0e17")));
const FLinearColor FCkGoapDebuggerStyle::Color_Bg_Panel           = FLinearColor(FColor::FromHex(TEXT("#0c1018")));
const FLinearColor FCkGoapDebuggerStyle::Color_Bg_Surface         = FLinearColor(FColor::FromHex(TEXT("#0f1520")));
const FLinearColor FCkGoapDebuggerStyle::Color_Bg_Black           = FLinearColor(FColor::FromHex(TEXT("#000000")));

const FLinearColor FCkGoapDebuggerStyle::Color_Border_Subtle      = FLinearColor(FColor::FromHex(TEXT("#1a2332")));
const FLinearColor FCkGoapDebuggerStyle::Color_Border_Strong      = FLinearColor(FColor::FromHex(TEXT("#2a3342")));

const FLinearColor FCkGoapDebuggerStyle::Color_Text_Primary       = FLinearColor(FColor::FromHex(TEXT("#e0e0e0")));
const FLinearColor FCkGoapDebuggerStyle::Color_Text_Secondary     = FLinearColor(FColor::FromHex(TEXT("#aaaaaa")));
const FLinearColor FCkGoapDebuggerStyle::Color_Text_Muted         = FLinearColor(FColor::FromHex(TEXT("#888888")));
const FLinearColor FCkGoapDebuggerStyle::Color_Text_Dim           = FLinearColor(FColor::FromHex(TEXT("#666666")));
const FLinearColor FCkGoapDebuggerStyle::Color_Text_Faint         = FLinearColor(FColor::FromHex(TEXT("#555555")));
const FLinearColor FCkGoapDebuggerStyle::Color_Text_Ghost         = FLinearColor(FColor::FromHex(TEXT("#444444")));

const FLinearColor FCkGoapDebuggerStyle::Color_Status_PlanFound   = FLinearColor(FColor::FromHex(TEXT("#22c55e")));
const FLinearColor FCkGoapDebuggerStyle::Color_Status_Planning    = FLinearColor(FColor::FromHex(TEXT("#60a5fa")));
const FLinearColor FCkGoapDebuggerStyle::Color_Status_PlanningBdr = FLinearColor(FColor::FromHex(TEXT("#3b82f6")));
const FLinearColor FCkGoapDebuggerStyle::Color_Status_Failed      = FLinearColor(FColor::FromHex(TEXT("#ef4444")));
const FLinearColor FCkGoapDebuggerStyle::Color_Status_Selected    = FLinearColor(FColor::FromHex(TEXT("#f59e0b")));
const FLinearColor FCkGoapDebuggerStyle::Color_Status_Composite   = FLinearColor(FColor::FromHex(TEXT("#a855f7")));

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
    // Flat fills -------------------------------------------------------------
    InStyle->Set("CkGoap.Bg.Root",    new FSlateColorBrush(Color_Bg_Root));
    InStyle->Set("CkGoap.Bg.Panel",   new FSlateColorBrush(Color_Bg_Panel));
    InStyle->Set("CkGoap.Bg.Surface", new FSlateColorBrush(Color_Bg_Surface));
    InStyle->Set("CkGoap.Bg.Black",   new FSlateColorBrush(Color_Bg_Black));

    InStyle->Set("CkGoap.Border.Subtle", new FSlateColorBrush(Color_Border_Subtle));
    InStyle->Set("CkGoap.Border.Strong", new FSlateColorBrush(Color_Border_Strong));

    InStyle->Set("CkGoap.Separator", new FSlateColorBrush(Color_Border_Subtle));

    // Sidebar / list rows
    InStyle->Set("CkGoap.Sidebar.Bg",        new FSlateColorBrush(Color_Bg_Panel));
    InStyle->Set("CkGoap.Sidebar.RowHover",  new FSlateColorBrush(FLinearColor(Color_Border_Subtle.R, Color_Border_Subtle.G, Color_Border_Subtle.B, 0.27f)));
    InStyle->Set("CkGoap.Sidebar.RowSelected", new FSlateColorBrush(Color_Border_Subtle));

    // Cards / surfaces
    InStyle->Set("CkGoap.Surface.Rounded", new FSlateRoundedBoxBrush(
        Color_Bg_Surface, CornerRadius_Large,
        Color_Border_Subtle, Border_Standard));

    InStyle->Set("CkGoap.Crumb.Default", new FSlateRoundedBoxBrush(
        Color_Bg_Panel, CornerRadius_Medium,
        Color_Border_Subtle, Border_Thin));

    InStyle->Set("CkGoap.Crumb.Selected", new FSlateRoundedBoxBrush(
        Color_Bg_Panel, CornerRadius_Medium,
        Color_Status_Selected, Border_Standard));

    // Graph nodes
    InStyle->Set("CkGoap.Graph.Background", new FSlateColorBrush(Color_Bg_Root));
    InStyle->Set("CkGoap.Graph.NodeBg", new FSlateRoundedBoxBrush(
        Color_Bg_Surface, CornerRadius_Large,
        Color_Border_Subtle, Border_Standard));
    InStyle->Set("CkGoap.Graph.NodeBg.InPlan", new FSlateRoundedBoxBrush(
        Color_Bg_Surface, CornerRadius_Large,
        Color_Status_PlanningBdr, Border_Strong));
    InStyle->Set("CkGoap.Graph.NodeBg.Selected", new FSlateRoundedBoxBrush(
        Color_Bg_Surface, CornerRadius_Large,
        Color_Status_Selected, Border_Strong));

    // Badges -- background tints (badge text colours come from text style)
    InStyle->Set("CkGoap.Badge.Found", new FSlateRoundedBoxBrush(
        FLinearColor(Color_Status_PlanFound.R, Color_Status_PlanFound.G, Color_Status_PlanFound.B, 0.13f),
        CornerRadius_Badge,
        FLinearColor(Color_Status_PlanFound.R, Color_Status_PlanFound.G, Color_Status_PlanFound.B, 0.27f),
        Border_Thin));

    InStyle->Set("CkGoap.Badge.Planning", new FSlateRoundedBoxBrush(
        FLinearColor(Color_Status_PlanningBdr.R, Color_Status_PlanningBdr.G, Color_Status_PlanningBdr.B, 0.13f),
        CornerRadius_Badge,
        FLinearColor(Color_Status_PlanningBdr.R, Color_Status_PlanningBdr.G, Color_Status_PlanningBdr.B, 0.27f),
        Border_Thin));

    InStyle->Set("CkGoap.Badge.Idle", new FSlateRoundedBoxBrush(
        FLinearColor(Color_Text_Faint.R, Color_Text_Faint.G, Color_Text_Faint.B, 0.13f),
        CornerRadius_Badge,
        FLinearColor(Color_Text_Faint.R, Color_Text_Faint.G, Color_Text_Faint.B, 0.33f),
        Border_Thin));

    InStyle->Set("CkGoap.Badge.Failed", new FSlateRoundedBoxBrush(
        FLinearColor(Color_Status_Failed.R, Color_Status_Failed.G, Color_Status_Failed.B, 0.13f),
        CornerRadius_Badge,
        Color_Status_Failed,
        Border_Thin));

    // WorldState rail — recently-changed row tint. Mockup uses `#f59e0b0a`
    // (amber at ~4% alpha) plus a tiny radius.
    InStyle->Set("CkGoap.WS.RowRecent", new FSlateRoundedBoxBrush(
        FLinearColor(Color_Status_Selected.R, Color_Status_Selected.G, Color_Status_Selected.B, 0.06f),
        CornerRadius_Small));
}

auto
    FCkGoapDebuggerStyle::
    CreateTextStyles(TSharedRef<FSlateStyleSet> InStyle)
    -> void
{
    const auto Regular     = FCoreStyle::GetDefaultFontStyle("Regular", 9);
    const auto Bold        = FCoreStyle::GetDefaultFontStyle("Bold",    9);
    const auto Mono        = FCoreStyle::GetDefaultFontStyle("Mono",    9);
    const auto MonoSmall   = FCoreStyle::GetDefaultFontStyle("Mono",    8);
    const auto Header      = FCoreStyle::GetDefaultFontStyle("Bold",   10);
    const auto LargeHeader = FCoreStyle::GetDefaultFontStyle("Bold",   12);
    const auto TitleHeader = FCoreStyle::GetDefaultFontStyle("Bold",   15);

    InStyle->Set("CkGoap.Text.Normal",    FTextBlockStyle().SetFont(Regular).SetColorAndOpacity(Color_Text_Primary));
    InStyle->Set("CkGoap.Text.Secondary", FTextBlockStyle().SetFont(Regular).SetColorAndOpacity(Color_Text_Secondary));
    InStyle->Set("CkGoap.Text.Muted",     FTextBlockStyle().SetFont(Regular).SetColorAndOpacity(Color_Text_Muted));
    InStyle->Set("CkGoap.Text.Dim",       FTextBlockStyle().SetFont(Regular).SetColorAndOpacity(Color_Text_Dim));
    InStyle->Set("CkGoap.Text.Faint",     FTextBlockStyle().SetFont(Regular).SetColorAndOpacity(Color_Text_Faint));

    InStyle->Set("CkGoap.Text.Bold",      FTextBlockStyle().SetFont(Bold).SetColorAndOpacity(Color_Text_Primary));

    InStyle->Set("CkGoap.Text.Mono",      FTextBlockStyle().SetFont(Mono).SetColorAndOpacity(Color_Text_Dim));
    InStyle->Set("CkGoap.Text.Mono.Tiny", FTextBlockStyle().SetFont(MonoSmall).SetColorAndOpacity(Color_Text_Faint));

    InStyle->Set("CkGoap.Text.Header",      FTextBlockStyle().SetFont(Header).SetColorAndOpacity(Color_Text_Muted));
    InStyle->Set("CkGoap.Text.LargeHeader", FTextBlockStyle().SetFont(LargeHeader).SetColorAndOpacity(Color_Text_Primary));

    InStyle->Set("CkGoap.Text.Title",
        FTextBlockStyle().SetFont(TitleHeader).SetColorAndOpacity(Color_Status_Selected));

    // Status accents
    InStyle->Set("CkGoap.Text.Status.PlanFound", FTextBlockStyle().SetFont(Bold).SetColorAndOpacity(Color_Status_PlanFound));
    InStyle->Set("CkGoap.Text.Status.Planning",  FTextBlockStyle().SetFont(Bold).SetColorAndOpacity(Color_Status_Planning));
    InStyle->Set("CkGoap.Text.Status.Failed",    FTextBlockStyle().SetFont(Bold).SetColorAndOpacity(Color_Status_Failed));
    InStyle->Set("CkGoap.Text.Status.Selected",  FTextBlockStyle().SetFont(Bold).SetColorAndOpacity(Color_Status_Selected));
    InStyle->Set("CkGoap.Text.Status.Composite", FTextBlockStyle().SetFont(Bold).SetColorAndOpacity(Color_Status_Composite));
}
