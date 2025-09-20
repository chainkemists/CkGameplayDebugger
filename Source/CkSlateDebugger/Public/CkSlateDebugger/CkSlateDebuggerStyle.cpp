#include "CkSlateDebuggerStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FCkSlateDebuggerStyle::StyleInstance = nullptr;

void FCkSlateDebuggerStyle::Initialize()
{
    if (NOT FSlateApplication::IsInitialized())
    { return; }

    if (NOT StyleInstance.IsValid())
    {
        StyleInstance = Create();
        FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
    }
}

void FCkSlateDebuggerStyle::Shutdown()
{
    FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
    ensure(StyleInstance.IsUnique());
    StyleInstance.Reset();
}

FName FCkSlateDebuggerStyle::GetStyleSetName()
{
    static FName StyleSetName(TEXT("CkSlateDebuggerStyle"));
    return StyleSetName;
}

const ISlateStyle& FCkSlateDebuggerStyle::Get()
{
    return *StyleInstance;
}

TSharedRef<FSlateStyleSet> FCkSlateDebuggerStyle::Create()
{
    TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet("CkSlateDebuggerStyle"));

    const FTextBlockStyle DefaultText = FTextBlockStyle()
        .SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 10))
        .SetColorAndOpacity(FSlateColor::UseForeground());

    // Window styles
    Style->Set("CkDebugger.Background", new FSlateColorBrush(FLinearColor(0.02f, 0.02f, 0.03f)));
    Style->Set("CkDebugger.Panel", new FSlateColorBrush(FLinearColor(0.04f, 0.04f, 0.05f)));
    Style->Set("CkDebugger.Sidebar", new FSlateColorBrush(FLinearColor(0.03f, 0.03f, 0.04f)));

    // Text styles
    Style->Set("CkDebugger.Text", DefaultText);
    Style->Set("CkDebugger.Text.Bold", FTextBlockStyle(DefaultText)
        .SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 10)));
    Style->Set("CkDebugger.Text.Small", FTextBlockStyle(DefaultText)
        .SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 8)));
    Style->Set("CkDebugger.Text.Title", FTextBlockStyle(DefaultText)
        .SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 14)));

    // Category header style
    Style->Set("CkDebugger.CategoryHeader", FTextBlockStyle(DefaultText)
        .SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 9))
        .SetColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f)));

    // Colors for different value types
    Style->Set("CkDebugger.Color.Vector", FLinearColor(0.764f, 0.909f, 0.552f)); // Green
    Style->Set("CkDebugger.Color.Number", FLinearColor(0.972f, 0.733f, 0.850f)); // Pink
    Style->Set("CkDebugger.Color.Enum", FLinearColor(1.0f, 0.8f, 0.008f)); // Yellow
    Style->Set("CkDebugger.Color.Entity", FLinearColor(0.509f, 0.694f, 1.0f)); // Blue
    Style->Set("CkDebugger.Color.None", FLinearColor(0.4f, 0.4f, 0.4f)); // Gray

    // Button styles
    const FButtonStyle ButtonStyle = FButtonStyle()
        .SetNormal(FSlateColorBrush(FLinearColor(0.06f, 0.06f, 0.07f)))
        .SetHovered(FSlateColorBrush(FLinearColor(0.08f, 0.08f, 0.10f)))
        .SetPressed(FSlateColorBrush(FLinearColor(0.04f, 0.04f, 0.05f)));

    Style->Set("CkDebugger.Button", ButtonStyle);

    // Tree view styles
    Style->Set("CkDebugger.TreeArrow.Collapsed", new IMAGE_BRUSH_SVG("Starship/Common/chevron-right", CoreStyleConstants::Icon16x16));
    Style->Set("CkDebugger.TreeArrow.Expanded", new IMAGE_BRUSH_SVG("Starship/Common/chevron-down", CoreStyleConstants::Icon16x16));

    return Style;
}

void FCkSlateDebuggerStyle::ReloadTextures()
{
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
    }
}