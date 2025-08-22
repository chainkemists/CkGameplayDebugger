#include "CkDebugTools/CkDebugTools_Style.h"

#include <Styling/SlateStyleMacros.h>
#include <Styling/CoreStyle.h>
#include <Engine/Engine.h>

// --------------------------------------------------------------------------------------------------------------------

TSharedPtr<FSlateStyleSet> FCkDebugToolsStyle::StyleInstance = nullptr;

// --------------------------------------------------------------------------------------------------------------------

auto FCkDebugToolsStyle::Initialize() -> void
{
    if (NOT StyleInstance.IsValid())
    {
        StyleInstance = Create();
        FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
    }
}

auto FCkDebugToolsStyle::Shutdown() -> void
{
    FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
    ensure(StyleInstance.IsUnique());
    StyleInstance.Reset();
}

auto FCkDebugToolsStyle::ReloadTextures() -> void
{
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
    }
}

auto FCkDebugToolsStyle::Get_StyleSetName() -> FName
{
    static FName StyleSetName(TEXT("CkDebugToolsStyle"));
    return StyleSetName;
}

auto FCkDebugToolsStyle::Create() -> TSharedRef<FSlateStyleSet>
{
    auto Style = MakeShared<FSlateStyleSet>(Get_StyleSetName());

    // Try to find the plugin, but don't crash if it doesn't exist
    const auto Plugin = IPluginManager::Get().FindPlugin("CkGameplayDebugger");
    if (Plugin.IsValid())
    {
        Style->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
    }
    else
    {
        // Fallback to engine content
        Style->SetContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));
    }

    constexpr auto NormalGray = FLinearColor(0.016f, 0.016f, 0.016f);
    constexpr auto LightGray = FLinearColor(0.035f, 0.035f, 0.035f);
    constexpr auto DarkGray = FLinearColor(0.008f, 0.008f, 0.008f);
    constexpr auto AccentBlue = FLinearColor(0.25f, 0.82f, 0.79f);
    constexpr auto WarningOrange = FLinearColor(0.88f, 0.37f, 0.37f);
    constexpr auto DebugPurple = FLinearColor(0.61f, 0.37f, 0.88f);
    constexpr auto TextWhite = FLinearColor(0.9f, 0.9f, 0.9f);
    constexpr auto TextGray = FLinearColor(0.6f, 0.6f, 0.6f);

    // Color scheme
    Style->Set("CkDebugTools.Color.Primary", TextWhite);
    Style->Set("CkDebugTools.Color.Secondary", TextGray);
    Style->Set("CkDebugTools.Color.Accent", AccentBlue);
    Style->Set("CkDebugTools.Color.Warning", WarningOrange);
    Style->Set("CkDebugTools.Color.Debug", DebugPurple);

    // Entity color coding (from original debugger)
    Style->Set("CkDebugTools.Color.Entity", FLinearColor(0.51f, 0.69f, 1.0f)); // Blue #82b1ff
    Style->Set("CkDebugTools.Color.Vector", FLinearColor(0.76f, 0.91f, 0.55f)); // Green #c3e88d
    Style->Set("CkDebugTools.Color.Number", FLinearColor(0.97f, 0.73f, 0.85f)); // Pink #f8bbd9
    Style->Set("CkDebugTools.Color.Enum", FLinearColor(1.0f, 0.8f, 0.01f)); // Yellow #ffcc02
    Style->Set("CkDebugTools.Color.Unknown", FLinearColor(1.0f, 0.34f, 0.13f)); // Red #ff5722
    Style->Set("CkDebugTools.Color.None", FLinearColor(0.4f, 0.4f, 0.4f)); // Gray #666666

    // Fonts
    const auto DefaultFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);
    const auto BoldFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
    const auto HeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 12);

    Style->Set("CkDebugTools.Font.Regular", DefaultFont);
    Style->Set("CkDebugTools.Font.Bold", BoldFont);
    Style->Set("CkDebugTools.Font.Header", HeaderFont);

    // Button styles
    const auto ButtonNormal = FButtonStyle()
        .SetNormal(FSlateColorBrush(LightGray))
        .SetHovered(FSlateColorBrush(AccentBlue))
        .SetPressed(FSlateColorBrush(AccentBlue * 0.8f))
        .SetDisabled(FSlateColorBrush(DarkGray))
        .SetNormalPadding(FMargin(8, 4))
        .SetPressedPadding(FMargin(8, 4));

    Style->Set("CkDebugTools.Button.Primary", ButtonNormal);

    const auto ButtonSecondary = FButtonStyle(ButtonNormal)
        .SetNormal(FSlateColorBrush(NormalGray))
        .SetHovered(FSlateColorBrush(LightGray))
        .SetPressed(FSlateColorBrush(DarkGray));

    Style->Set("CkDebugTools.Button.Secondary", ButtonSecondary);

    const auto ButtonSmall = FButtonStyle(ButtonNormal)
        .SetNormalPadding(FMargin(4, 2))
        .SetPressedPadding(FMargin(4, 2));

    Style->Set("CkDebugTools.Button.Small", ButtonSmall);

    // Table styles
    const auto TableRowNormal = FTableRowStyle()
        .SetEvenRowBackgroundBrush(FSlateColorBrush(NormalGray))
        .SetOddRowBackgroundBrush(FSlateColorBrush(LightGray))
        .SetSelectorFocusedBrush(FSlateColorBrush(AccentBlue))
        .SetActiveBrush(FSlateColorBrush(AccentBlue * 0.6f))
        .SetActiveHoveredBrush(FSlateColorBrush(AccentBlue * 0.8f))
        .SetInactiveBrush(FSlateColorBrush(LightGray))
        .SetInactiveHoveredBrush(FSlateColorBrush(LightGray * 1.2f))
        .SetTextColor(TextWhite)
        .SetSelectedTextColor(FLinearColor::Black);

    Style->Set("CkDebugTools.TableRow.Normal", TableRowNormal);

    const auto HeaderRowStyle = FHeaderRowStyle()
        .SetBackgroundBrush(FSlateColorBrush(DarkGray))
        .SetForegroundColor(TextWhite);

    Style->Set("CkDebugTools.HeaderRow.Normal", HeaderRowStyle);

    // Search box style
    const auto SearchBoxStyle = FSearchBoxStyle()
        .SetTextBoxStyle(FEditableTextBoxStyle()
            .SetBackgroundImageNormal(FSlateColorBrush(LightGray))
            .SetBackgroundImageHovered(FSlateColorBrush(LightGray * 1.1f))
            .SetBackgroundImageFocused(FSlateColorBrush(LightGray * 1.2f))
            .SetBackgroundImageReadOnly(FSlateColorBrush(DarkGray))
            .SetPadding(FMargin(4.0f))
            .SetFont(DefaultFont)
            .SetForegroundColor(TextWhite))
        .SetUpArrowImage(FSlateNoResource())
        .SetDownArrowImage(FSlateNoResource())
        .SetGlassImage(FSlateNoResource())
        .SetClearImage(FSlateNoResource());

    Style->Set("CkDebugTools.SearchBox.Normal", SearchBoxStyle);

    return Style;
}

// --------------------------------------------------------------------------------------------------------------------