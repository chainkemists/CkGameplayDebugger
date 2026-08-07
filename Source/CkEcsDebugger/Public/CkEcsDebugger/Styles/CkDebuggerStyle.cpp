#include "CkDebuggerStyle.h"

#include "CkCore/IO/CkIO_Utils.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkObjective/Objective/CkObjective_Fragment_Data.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr<FSlateStyleSet> FCkDebuggerStyle::StyleInstance = nullptr;
TArray<FName> FCkDebuggerStyle::GeneralIconPool = {};

// -----------------------------------------------------------------------------------------------------------------
auto FCkDebuggerStyle::Get_ObjectiveStatusColor(ECk_ObjectiveStatus InStatus) -> FLinearColor
{
    switch (InStatus)
    {
        case ECk_ObjectiveStatus::NotStarted: return CkStyle::Status_NotStarted();
        case ECk_ObjectiveStatus::Active:     return CkStyle::Status_Active();
        case ECk_ObjectiveStatus::Completed:  return CkStyle::Status_Completed();
        case ECk_ObjectiveStatus::Failed:     return CkStyle::Status_Failed();
        default:                              return CkStyle::Text();
    }
}

// -----------------------------------------------------------------------------------------------------------------
auto FCkDebuggerStyle::Initialize() -> void
{
    if (NOT StyleInstance.IsValid())
    {
        StyleInstance = Create();
        FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
    }
}

auto FCkDebuggerStyle::Shutdown() -> void
{
    if (StyleInstance.IsValid())
    {
        FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
        StyleInstance.Reset();
    }
}

auto FCkDebuggerStyle::Get() -> const ISlateStyle&
{
    return *StyleInstance;
}

auto FCkDebuggerStyle::GetStyleSetName() -> FName
{
    static const FName StyleSetName(TEXT("CkDebuggerStyle"));
    return StyleSetName;
}

auto FCkDebuggerStyle::Create() -> TSharedRef<FSlateStyleSet>
{
    auto Style = MakeShared<FSlateStyleSet>(GetStyleSetName());

    // Content root points at the plugin's Resources/ so icon brushes can resolve
    // Icons/*.svg. Get_PluginsDir joins the plugin FOLDER name (CkGameplayDebugger),
    // not the plugin name (CkDebugger) — and avoids a direct Projects-module dep.
    Style->SetContentRoot(UCk_Utils_IO_UE::Get_PluginsDir(TEXT("CkGameplayDebugger")) / TEXT("Resources"));

    CreateBrushes(Style);
    CreateIconBrushes(Style);
    CreateColors(Style);
    CreateTextStyles(Style);

    return Style;
}

auto FCkDebuggerStyle::CreateBrushes(TSharedRef<FSlateStyleSet> InStyle) -> void
{
    InStyle->Set("CkDebugger.Background.Dark", new FSlateColorBrush(CkStyle::BgRoot()));
    InStyle->Set("CkDebugger.Background.Medium", new FSlateColorBrush(CkStyle::Bg1()));
    InStyle->Set("CkDebugger.Background.Light", new FSlateColorBrush(CkStyle::Bg2()));
    InStyle->Set("CkDebugger.Border", new FSlateColorBrush(CkStyle::Border()));
    InStyle->Set("CkDebugger.Selection", new FSlateColorBrush(CkStyle::Selection()));
    InStyle->Set("CkDebugger.Hover", new FSlateColorBrush(CkStyle::Hover()));

    InStyle->Set("CkDebugger.Panel.Background", new FSlateColorBrush(CkStyle::Bg1()));
    InStyle->Set("CkDebugger.Panel.Border", new FSlateRoundedBoxBrush(
        CkStyle::Border(),
        2.0f,
        CkStyle::Bg1(),
        1.0f
    ));

    InStyle->Set("CkDebugger.Row.Even", new FSlateColorBrush(CkStyle::Bg1()));
    InStyle->Set("CkDebugger.Row.Odd", new FSlateColorBrush(CkStyle::Bg2()));

    InStyle->Set("CkDebugger.Separator", new FSlateColorBrush(CkStyle::Border()));

    InStyle->Set("CkDebugger.Graph.Background", new FSlateColorBrush(CkStyle::Graph_Background()));
    InStyle->Set("CkDebugger.Graph.NodeBackground", new FSlateRoundedBoxBrush(
        CkStyle::Graph_Node_Default(), GraphNode_CornerRadius));
    InStyle->Set("CkDebugger.Graph.NodeBackground.Hover", new FSlateRoundedBoxBrush(
        CkStyle::Graph_Node_Center(), GraphNode_CornerRadius));
    InStyle->Set("CkDebugger.Graph.NodeBackground.Center", new FSlateRoundedBoxBrush(
        CkStyle::Graph_Node_Center(), GraphNode_CornerRadius));

    InStyle->Set("CkDebugger.Badge.Rounded", new FSlateRoundedBoxBrush(
        FLinearColor::White, 3.0f));

    // Tintable icon backdrop for cards — white so BorderBackgroundColor carries the tint.
    InStyle->Set("CkDebugger.Card.IconWell", new FSlateRoundedBoxBrush(
        FLinearColor::White, 4.0f));

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

auto FCkDebuggerStyle::CreateIconBrushes(TSharedRef<FSlateStyleSet> InStyle) -> void
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

auto FCkDebuggerStyle::Get_GeneralIconPool() -> const TArray<FName>&
{
    return GeneralIconPool;
}

auto FCkDebuggerStyle::Get_IconBrush(FName InIconId) -> const FSlateBrush*
{
    if (InIconId.IsNone() || NOT StyleInstance.IsValid())
    { return nullptr; }

    return StyleInstance->GetOptionalBrush(
        FName{FString{TEXT("CkDebugger.Icon.")} + InIconId.ToString()});
}

auto FCkDebuggerStyle::CreateColors(TSharedRef<FSlateStyleSet> InStyle) -> void
{
    InStyle->Set("CkDebugger.Color.Text.Primary", CkStyle::Text());
    InStyle->Set("CkDebugger.Color.Text.Secondary", CkStyle::TextDim());
    InStyle->Set("CkDebugger.Color.Text.Muted", CkStyle::TextMute());
    InStyle->Set("CkDebugger.Color.Text.Highlight", CkStyle::TextStrong());

    InStyle->Set("CkDebugger.Color.EntityID", CkStyle::EntityId());
    InStyle->Set("CkDebugger.Color.Transform", CkStyle::Transform());
    InStyle->Set("CkDebugger.Color.Network", CkStyle::Network());
    InStyle->Set("CkDebugger.Color.Relationship", CkStyle::Relationship());
    InStyle->Set("CkDebugger.Color.Attribute", CkStyle::Attribute());
    InStyle->Set("CkDebugger.Color.Reference", CkStyle::Reference());
    InStyle->Set("CkDebugger.Color.None", CkStyle::None());

    InStyle->Set("CkDebugger.Color.Error", CkStyle::Err());
    InStyle->Set("CkDebugger.Color.Warning", CkStyle::Warn());
    InStyle->Set("CkDebugger.Color.Success", CkStyle::Ok());
}

auto FCkDebuggerStyle::CreateTextStyles(TSharedRef<FSlateStyleSet> InStyle) -> void
{
    const auto DefaultFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);
    const auto BoldFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
    const auto MonospaceFont = FCoreStyle::GetDefaultFontStyle("Mono", 9);
    const auto HeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
    const auto LargeHeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 12);

    InStyle->Set("CkDebugger.Text.Normal", FTextBlockStyle()
        .SetFont(DefaultFont)
        .SetColorAndOpacity(CkStyle::Text()));

    InStyle->Set("CkDebugger.Text.Bold", FTextBlockStyle()
        .SetFont(BoldFont)
        .SetColorAndOpacity(CkStyle::Text()));

    InStyle->Set("CkDebugger.Text.Monospace", FTextBlockStyle()
        .SetFont(MonospaceFont)
        .SetColorAndOpacity(CkStyle::TextDim()));

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
