#include "CkDebuggerStyle.h"

#include "CkCore/IO/CkIO_Utils.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleDefaults.h"

// ====================================================================================================================

TSharedPtr<FSlateStyleSet> FCkDebuggerStyle::StyleInstance = nullptr;
TArray<FName> FCkDebuggerStyle::GeneralIconPool = {};

// ====================================================================================================================

auto
    FCkDebuggerStyle::
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
    FCkDebuggerStyle::
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
    FCkDebuggerStyle::
    Get()
    -> const ISlateStyle&
{
    return *StyleInstance;
}

auto
    FCkDebuggerStyle::
    GetStyleSetName()
    -> FName
{
    static const FName StyleSetName(TEXT("CkDebuggerStyle"));
    return StyleSetName;
}

// ====================================================================================================================

auto
    FCkDebuggerStyle::
    Create()
    -> TSharedRef<FSlateStyleSet>
{
    auto Style = MakeShared<FSlateStyleSet>(GetStyleSetName());

    // Content root points at the plugin's Resources/ so icon brushes can resolve
    // Icons/*.svg. Get_PluginsDir joins the plugin FOLDER name (CkGameplayDebugger),
    // not the plugin name (CkDebugger) — and avoids a direct Projects-module dep.
    // This is a path-only lookup, so it resolves identically from any module in
    // the plugin; the promotion out of CkEcsDebugger does not move the root.
    Style->SetContentRoot(UCk_Utils_IO_UE::Get_PluginsDir(TEXT("CkGameplayDebugger")) / TEXT("Resources"));

    CreateBrushes(Style);
    CreateIconBrushes(Style);
    CreateTextStyles(Style);

    return Style;
}

auto
    FCkDebuggerStyle::
    CreateBrushes(
        TSharedRef<FSlateStyleSet> InStyle)
    -> void
{
    InStyle->Set("CkDebugger.Background.Dark", new FSlateColorBrush(CkStyle::BgRoot()));
    InStyle->Set("CkDebugger.Background.Medium", new FSlateColorBrush(CkStyle::Bg1()));
    InStyle->Set("CkDebugger.Border", new FSlateColorBrush(CkStyle::Border()));

    InStyle->Set("CkDebugger.Panel.Border", new FSlateRoundedBoxBrush(
        CkStyle::Border(),
        2.0f,
        CkStyle::Bg1(),
        1.0f
    ));

    // Alternating row fills — the RowBanding axis' Zebra option.
    InStyle->Set("CkDebugger.Row.Even", new FSlateColorBrush(CkStyle::Bg1()));
    InStyle->Set("CkDebugger.Row.Odd", new FSlateColorBrush(CkStyle::Bg2()));

    InStyle->Set("CkDebugger.Separator", new FSlateColorBrush(CkStyle::Border()));

    InStyle->Set("CkDebugger.Graph.Background", new FSlateColorBrush(CkStyle::Graph_Background()));

    InStyle->Set("CkDebugger.Badge.Rounded", new FSlateRoundedBoxBrush(
        FLinearColor::White, CkStyle::RadiusS()));

    // The two ends of the CornerStyle axis. Rounded is CkStyle's own family, so only square and
    // pill need registering — white, tinted at the use site like everything else here.
    InStyle->Set("CkDebugger.Corner.Square", new FSlateRoundedBoxBrush(
        FLinearColor::White, 0.0f));
    InStyle->Set("CkDebugger.Corner.Pill", new FSlateRoundedBoxBrush(
        FLinearColor::White, CkStyle::RadiusPill()));

    // Ring counterparts of the same three shapes: the fill is transparent at ANY tint (alpha zero
    // multiplies to zero), so the widget's BorderBackgroundColor selects the ring color alone.
    InStyle->Set("CkDebugger.Surface.Outline", new FSlateRoundedBoxBrush(
        FLinearColor::Transparent, 0.0f, FLinearColor::White, CkStyle::RingWidth()));
    InStyle->Set("CkDebugger.Corner.Rounded.Outline", new FSlateRoundedBoxBrush(
        FLinearColor::Transparent, CkStyle::RadiusM(), FLinearColor::White, CkStyle::RingWidth()));
    InStyle->Set("CkDebugger.Corner.Pill.Outline", new FSlateRoundedBoxBrush(
        FLinearColor::Transparent, CkStyle::RadiusPill(), FLinearColor::White, CkStyle::RingWidth()));

    // Tintable icon backdrops — white so BorderBackgroundColor carries the tint.
    InStyle->Set("CkDebugger.Card.IconWell", new FSlateRoundedBoxBrush(
        FLinearColor::White, 4.0f));
    InStyle->Set("CkDebugger.Card.IconRing", new FSlateRoundedBoxBrush(
        FLinearColor::Transparent, 4.0f, FLinearColor::White, CkStyle::RingWidth()));

    // List/tree rows: the engine's selection brush is a saturated fill that drowns row
    // content — replace the four selection states with a translucent accent so text,
    // icons, and pills stay readable on a selected row.
    {
        auto RowStyle = FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");
        RowStyle.SetActiveBrush(FSlateRoundedBoxBrush{CkStyle::Selection().CopyWithNewOpacity(0.22f), 3.0f});
        RowStyle.SetActiveHoveredBrush(FSlateRoundedBoxBrush{CkStyle::Selection().CopyWithNewOpacity(0.30f), 3.0f});
        RowStyle.SetInactiveBrush(FSlateRoundedBoxBrush{CkStyle::Selection().CopyWithNewOpacity(0.14f), 3.0f});
        RowStyle.SetInactiveHoveredBrush(FSlateRoundedBoxBrush{CkStyle::Selection().CopyWithNewOpacity(0.20f), 3.0f});
        InStyle->Set("CkDebugger.TableView.Row", RowStyle);
    }
}

auto
    FCkDebuggerStyle::
    CreateIconBrushes(
        TSharedRef<FSlateStyleSet> InStyle)
    -> void
{
    // Everything under Resources/Icons registers as "CkDebugger.Icon.<BaseName>" —
    // monochrome white SVGs, tinted at draw time via SImage.ColorAndOpacity. Resolve
    // through Get_IconBrush. Icons/General/* additionally forms the deterministic
    // assignment pool for archetypes without a bespoke or feature glyph; names are
    // sorted so the hash-pick is stable regardless of filesystem enumeration order.
    const auto RegisterDir = [&InStyle](const FString& InDirectory, TArray<FName>* OutPool) -> void
    {
        auto Files = TArray<FString>{};
        IFileManager::Get().FindFiles(Files, *(InDirectory / TEXT("*.svg")), true, false);
        Files.Sort();

        for (const auto& File : Files)
        {
            const auto IconId = FPaths::GetBaseFilename(File);
            InStyle->Set(
                FName{FString{TEXT("CkDebugger.Icon.")} + IconId},
                new FSlateVectorImageBrush{InDirectory / File, FVector2D{16.0f, 16.0f}});

            if (OutPool != nullptr)
            { OutPool->Add(FName{IconId}); }
        }
    };

    const auto IconsDir = InStyle->GetContentRootDir() / TEXT("Icons");
    GeneralIconPool.Reset();
    RegisterDir(IconsDir, nullptr);
    RegisterDir(IconsDir / TEXT("General"), &GeneralIconPool);
}

auto
    FCkDebuggerStyle::
    Get_GeneralIconPool()
    -> const TArray<FName>&
{
    return GeneralIconPool;
}

auto
    FCkDebuggerStyle::
    Get_IconBrush(
        FName InIconId)
    -> const FSlateBrush*
{
    if (InIconId.IsNone() || NOT StyleInstance.IsValid())
    { return nullptr; }

    return StyleInstance->GetOptionalBrush(
        FName{FString{TEXT("CkDebugger.Icon.")} + InIconId.ToString()});
}

auto
    FCkDebuggerStyle::
    Get_SquareBrush()
    -> const FSlateBrush*
{
    return StyleInstance.IsValid()
        ? StyleInstance->GetBrush(TEXT("CkDebugger.Corner.Square"))
        : FStyleDefaults::GetNoBrush();
}

auto
    FCkDebuggerStyle::
    Get_PillBrush()
    -> const FSlateBrush*
{
    return StyleInstance.IsValid()
        ? StyleInstance->GetBrush(TEXT("CkDebugger.Corner.Pill"))
        : FStyleDefaults::GetNoBrush();
}

auto
    FCkDebuggerStyle::
    Get_SurfaceOutlineBrush()
    -> const FSlateBrush*
{
    return StyleInstance.IsValid()
        ? StyleInstance->GetBrush(TEXT("CkDebugger.Surface.Outline"))
        : FStyleDefaults::GetNoBrush();
}

auto
    FCkDebuggerStyle::
    Get_RoundedOutlineBrush()
    -> const FSlateBrush*
{
    return StyleInstance.IsValid()
        ? StyleInstance->GetBrush(TEXT("CkDebugger.Corner.Rounded.Outline"))
        : FStyleDefaults::GetNoBrush();
}

auto
    FCkDebuggerStyle::
    Get_PillOutlineBrush()
    -> const FSlateBrush*
{
    return StyleInstance.IsValid()
        ? StyleInstance->GetBrush(TEXT("CkDebugger.Corner.Pill.Outline"))
        : FStyleDefaults::GetNoBrush();
}

auto
    FCkDebuggerStyle::
    Get_IconWellBrush()
    -> const FSlateBrush*
{
    return StyleInstance.IsValid()
        ? StyleInstance->GetBrush(TEXT("CkDebugger.Card.IconWell"))
        : FStyleDefaults::GetNoBrush();
}

auto
    FCkDebuggerStyle::
    Get_IconRingBrush()
    -> const FSlateBrush*
{
    return StyleInstance.IsValid()
        ? StyleInstance->GetBrush(TEXT("CkDebugger.Card.IconRing"))
        : FStyleDefaults::GetNoBrush();
}

auto
    FCkDebuggerStyle::
    Get_RowBandBrush(
        bool InIsOddRow)
    -> const FSlateBrush*
{
    if (NOT StyleInstance.IsValid())
    { return FStyleDefaults::GetNoBrush(); }

    return StyleInstance->GetBrush(InIsOddRow ? TEXT("CkDebugger.Row.Odd") : TEXT("CkDebugger.Row.Even"));
}

auto
    FCkDebuggerStyle::
    Get_SeparatorBrush()
    -> const FSlateBrush*
{
    return StyleInstance.IsValid()
        ? StyleInstance->GetBrush(TEXT("CkDebugger.Separator"))
        : FStyleDefaults::GetNoBrush();
}

auto
    FCkDebuggerStyle::
    CreateTextStyles(
        TSharedRef<FSlateStyleSet> InStyle)
    -> void
{
    // Sizes come from the CkStyle roles rather than literals, so an Editor Preferences typography
    // edit moves them. A registered FTextBlockStyle is construct-baked by nature — the TextScale
    // axis cannot reach through one, and only reaches text composed via ck::debug_axes::ScaledFont.
    const auto DefaultFont = FCoreStyle::GetDefaultFontStyle("Regular", CkStyle::FontSizeBody());
    const auto BoldFont = FCoreStyle::GetDefaultFontStyle("Bold", CkStyle::FontSizeBody());
    const auto HeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", CkStyle::FontSizeH3());
    const auto LargeHeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", CkStyle::FontSizeH2());

    InStyle->Set("CkDebugger.Text.Normal", FTextBlockStyle()
        .SetFont(DefaultFont)
        .SetColorAndOpacity(CkStyle::Text()));

    InStyle->Set("CkDebugger.Text.Bold", FTextBlockStyle()
        .SetFont(BoldFont)
        .SetColorAndOpacity(CkStyle::Text()));

    InStyle->Set("CkDebugger.Text.Header", FTextBlockStyle()
        .SetFont(HeaderFont)
        .SetColorAndOpacity(CkStyle::TextStrong()));

    InStyle->Set("CkDebugger.Text.LargeHeader", FTextBlockStyle()
        .SetFont(LargeHeaderFont)
        .SetColorAndOpacity(CkStyle::TextStrong()));

    InStyle->Set("CkDebugger.Text.Muted", FTextBlockStyle()
        .SetFont(DefaultFont)
        .SetColorAndOpacity(CkStyle::TextMute()));
}

// ====================================================================================================================
