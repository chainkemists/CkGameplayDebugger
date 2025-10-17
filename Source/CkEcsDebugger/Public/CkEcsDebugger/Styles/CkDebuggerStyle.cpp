#include "CkDebuggerStyle.h"

#include "Styling/SlateStyleRegistry.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr<FSlateStyleSet> FCkDebuggerStyle::StyleInstance = nullptr;

const FLinearColor FCkDebuggerStyle::Color_Background_Dark = FLinearColor(0.01f, 0.01f, 0.01f);
const FLinearColor FCkDebuggerStyle::Color_Background_Medium = FLinearColor(0.025f, 0.025f, 0.025f);
const FLinearColor FCkDebuggerStyle::Color_Background_Light = FLinearColor(0.04f, 0.04f, 0.04f);
const FLinearColor FCkDebuggerStyle::Color_Border = FLinearColor(0.08f, 0.08f, 0.08f);
const FLinearColor FCkDebuggerStyle::Color_Selection = FLinearColor(0.2f, 0.4f, 0.8f);
const FLinearColor FCkDebuggerStyle::Color_SelectionInactive = FLinearColor(0.15f, 0.15f, 0.2f);
const FLinearColor FCkDebuggerStyle::Color_Hover = FLinearColor(0.06f, 0.06f, 0.08f);

const FLinearColor FCkDebuggerStyle::Color_Text_Primary = FLinearColor(0.85f, 0.85f, 0.85f);
const FLinearColor FCkDebuggerStyle::Color_Text_Secondary = FLinearColor(0.6f, 0.6f, 0.6f);
const FLinearColor FCkDebuggerStyle::Color_Text_Muted = FLinearColor(0.35f, 0.35f, 0.35f);
const FLinearColor FCkDebuggerStyle::Color_Text_Highlight = FLinearColor(0.95f, 0.95f, 0.95f);

const FLinearColor FCkDebuggerStyle::Color_Entity_ID = FLinearColor(0.4f, 0.55f, 0.85f);
const FLinearColor FCkDebuggerStyle::Color_Transform = FLinearColor(0.6f, 0.75f, 0.45f);
const FLinearColor FCkDebuggerStyle::Color_Network = FLinearColor(0.85f, 0.65f, 0.01f);
const FLinearColor FCkDebuggerStyle::Color_Relationship = FLinearColor(0.8f, 0.6f, 0.7f);
const FLinearColor FCkDebuggerStyle::Color_Error = FLinearColor(0.85f, 0.25f, 0.1f);
const FLinearColor FCkDebuggerStyle::Color_Warning = FLinearColor(0.85f, 0.65f, 0.01f);
const FLinearColor FCkDebuggerStyle::Color_Success = FLinearColor(0.25f, 0.75f, 0.25f);

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

    CreateBrushes(Style);
    CreateColors(Style);
    CreateTextStyles(Style);

    return Style;
}

auto FCkDebuggerStyle::CreateBrushes(TSharedRef<FSlateStyleSet> InStyle) -> void
{
    InStyle->Set("CkDebugger.Background.Dark", new FSlateColorBrush(Color_Background_Dark));
    InStyle->Set("CkDebugger.Background.Medium", new FSlateColorBrush(Color_Background_Medium));
    InStyle->Set("CkDebugger.Background.Light", new FSlateColorBrush(Color_Background_Light));
    InStyle->Set("CkDebugger.Border", new FSlateColorBrush(Color_Border));
    InStyle->Set("CkDebugger.Selection", new FSlateColorBrush(Color_Selection));
    InStyle->Set("CkDebugger.Hover", new FSlateColorBrush(Color_Hover));

    InStyle->Set("CkDebugger.Panel.Background", new FSlateColorBrush(Color_Background_Medium));
    InStyle->Set("CkDebugger.Panel.Border", new FSlateRoundedBoxBrush(
        Color_Border,
        2.0f,
        Color_Background_Medium,
        1.0f
    ));

    InStyle->Set("CkDebugger.Row.Even", new FSlateColorBrush(Color_Background_Medium));
    InStyle->Set("CkDebugger.Row.Odd", new FSlateColorBrush(Color_Background_Light));

    InStyle->Set("CkDebugger.Separator", new FSlateColorBrush(Color_Border));
}

auto FCkDebuggerStyle::CreateColors(TSharedRef<FSlateStyleSet> InStyle) -> void
{
    InStyle->Set("CkDebugger.Color.Text.Primary", Color_Text_Primary);
    InStyle->Set("CkDebugger.Color.Text.Secondary", Color_Text_Secondary);
    InStyle->Set("CkDebugger.Color.Text.Muted", Color_Text_Muted);
    InStyle->Set("CkDebugger.Color.Text.Highlight", Color_Text_Highlight);

    InStyle->Set("CkDebugger.Color.EntityID", Color_Entity_ID);
    InStyle->Set("CkDebugger.Color.Transform", Color_Transform);
    InStyle->Set("CkDebugger.Color.Network", Color_Network);
    InStyle->Set("CkDebugger.Color.Relationship", Color_Relationship);

    InStyle->Set("CkDebugger.Color.Error", Color_Error);
    InStyle->Set("CkDebugger.Color.Warning", Color_Warning);
    InStyle->Set("CkDebugger.Color.Success", Color_Success);
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
        .SetColorAndOpacity(Color_Text_Primary));

    InStyle->Set("CkDebugger.Text.Bold", FTextBlockStyle()
        .SetFont(BoldFont)
        .SetColorAndOpacity(Color_Text_Primary));

    InStyle->Set("CkDebugger.Text.Monospace", FTextBlockStyle()
        .SetFont(MonospaceFont)
        .SetColorAndOpacity(Color_Text_Secondary));

    InStyle->Set("CkDebugger.Text.Header", FTextBlockStyle()
        .SetFont(HeaderFont)
        .SetColorAndOpacity(Color_Text_Highlight));

    InStyle->Set("CkDebugger.Text.LargeHeader", FTextBlockStyle()
        .SetFont(LargeHeaderFont)
        .SetColorAndOpacity(Color_Text_Highlight));

    InStyle->Set("CkDebugger.Text.Muted", FTextBlockStyle()
        .SetFont(DefaultFont)
        .SetColorAndOpacity(Color_Text_Muted));
}