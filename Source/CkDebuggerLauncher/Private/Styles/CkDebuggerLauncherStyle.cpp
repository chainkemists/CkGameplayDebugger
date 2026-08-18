#include "CkDebuggerLauncherStyle.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"

TSharedPtr<FSlateStyleSet> FCkDebuggerLauncherStyle::_StyleInstance;

// --------------------------------------------------------------------------------------------------------------------

auto FCkDebuggerLauncherStyle::Initialize() -> void
{
    if (_StyleInstance.IsValid())
    { return; }

    _StyleInstance = Create();
    FSlateStyleRegistry::RegisterSlateStyle(*_StyleInstance);
}

auto FCkDebuggerLauncherStyle::Shutdown() -> void
{
    if (NOT _StyleInstance.IsValid())
    { return; }

    FSlateStyleRegistry::UnRegisterSlateStyle(*_StyleInstance);
    _StyleInstance.Reset();
}

auto FCkDebuggerLauncherStyle::Get() -> const ISlateStyle&
{
    return *_StyleInstance;
}

auto FCkDebuggerLauncherStyle::GetStyleSetName() -> FName
{
    static const auto StyleSetName = FName{TEXT("CkDebuggerLauncherStyle")};
    return StyleSetName;
}

auto FCkDebuggerLauncherStyle::Create() -> TSharedRef<FSlateStyleSet>
{
    auto Style = MakeShared<FSlateStyleSet>(GetStyleSetName());
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));

    CK_ENSURE_IF_NOT(Plugin.IsValid(), TEXT("Cannot initialize the CK Debugger Launcher style: CkDebugger plugin not found"))
    { return Style; }

    Style->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
    CreateBrushes(Style);
    return Style;
}

auto FCkDebuggerLauncherStyle::Get_BackgroundBrush() -> const FSlateBrush*
{
    // Suite-shared: same BgRoot fill the rest of the debuggers use. Owned by FCkDebuggerStyle
    // so a palette change lands everywhere at once.
    return FCkDebuggerStyle::Get().GetBrush(TEXT("CkDebugger.Background.Dark"));
}

auto FCkDebuggerLauncherStyle::Get_SeparatorBrush() -> const FSlateBrush*
{
    return FCkDebuggerStyle::Get().GetBrush(TEXT("CkDebugger.Separator"));
}

auto FCkDebuggerLauncherStyle::CreateBrushes(TSharedRef<FSlateStyleSet> InStyle) -> void
{
    // Background + Separator are NOT registered here — they resolve from the promoted
    // suite style set (see Get_BackgroundBrush / Get_SeparatorBrush). Everything below is
    // genuinely launcher-specific: the rail's active marker, the tool button, and the two
    // rail text styles (Bold 7 section caps / foreground-driven tool label).
    InStyle->Set("CkDebuggerLauncher.ActiveMarker", new FSlateRoundedBoxBrush{CkStyle::Selection(), 2.0f});

    InStyle->Set("CkDebuggerLauncher.ToolButton", FButtonStyle{}
        .SetNormal(FSlateRoundedBoxBrush{CkStyle::BgRoot(), 5.0f})
        .SetHovered(FSlateRoundedBoxBrush{CkStyle::Hover(), 5.0f, CkStyle::Border(), 1.0f})
        .SetPressed(FSlateRoundedBoxBrush{CkStyle::Selection().CopyWithNewOpacity(0.22f), 5.0f, CkStyle::Selection(), 1.0f})
        .SetDisabled(FSlateRoundedBoxBrush{CkStyle::BgRoot(), 5.0f})
        .SetNormalForeground(CkStyle::TextDim())
        .SetHoveredForeground(CkStyle::TextStrong())
        .SetPressedForeground(CkStyle::TextStrong())
        .SetDisabledForeground(CkStyle::TextMute())
        .SetNormalPadding(FMargin{0.0f})
        .SetPressedPadding(FMargin{0.0f}));

    InStyle->Set("CkDebuggerLauncher.SectionText", FTextBlockStyle{}
        .SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 7))
        .SetColorAndOpacity(CkStyle::TextMute()));

    InStyle->Set("CkDebuggerLauncher.ToolText", FTextBlockStyle{}
        .SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9))
        .SetColorAndOpacity(FSlateColor::UseForeground()));
}

// --------------------------------------------------------------------------------------------------------------------
