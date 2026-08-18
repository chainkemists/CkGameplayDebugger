#include "CkDebuggerCommonStyle.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Interfaces/IPluginManager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleDefaults.h"
#include "Brushes/SlateBoxBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"

// ====================================================================================================================

TSharedPtr<FSlateStyleSet> FCkDebuggerCommonStyle::StyleInstance = nullptr;

namespace ck_debugger_common_style
{
    // 9-slice margins: fraction of the image that is corner. Chosen so the
    // slice boundary sits just past the (core inset + corner radius) band of
    // the generated glow PNGs — the stretched center is then uniform alpha.
    constexpr auto GlowSoft_Margin  = 0.47f;   // 64px image, core inset 22 + r8
    constexpr auto GlowTight_Margin = 0.46f;   // 48px image, core inset 16 + r6
}

// ====================================================================================================================

auto
    FCkDebuggerCommonStyle::
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
    FCkDebuggerCommonStyle::
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
    FCkDebuggerCommonStyle::
    Get()
    -> const ISlateStyle&
{
    return *StyleInstance;
}

auto
    FCkDebuggerCommonStyle::
    GetStyleSetName()
    -> FName
{
    static const FName StyleSetName(TEXT("CkDebuggerCommonStyle"));
    return StyleSetName;
}

// ====================================================================================================================

auto
    FCkDebuggerCommonStyle::
    Get_GlowSoftBrush()
    -> const FSlateBrush*
{
    return StyleInstance.IsValid() ? StyleInstance->GetBrush(TEXT("CkCommon.Glow.Soft")) : FStyleDefaults::GetNoBrush();
}

auto
    FCkDebuggerCommonStyle::
    Get_GlowTightBrush()
    -> const FSlateBrush*
{
    return StyleInstance.IsValid() ? StyleInstance->GetBrush(TEXT("CkCommon.Glow.Tight")) : FStyleDefaults::GetNoBrush();
}

auto
    FCkDebuggerCommonStyle::
    Get_FlatButtonStyle()
    -> const FButtonStyle&
{
    return Get().GetWidgetStyle<FButtonStyle>(TEXT("CkCommon.FlatButton"));
}

auto
    FCkDebuggerCommonStyle::
    Get_IconToggleStyle()
    -> const FCheckBoxStyle&
{
    return Get().GetWidgetStyle<FCheckBoxStyle>(TEXT("CkCommon.IconToggle"));
}

// ====================================================================================================================

auto
    FCkDebuggerCommonStyle::
    Create()
    -> TSharedRef<FSlateStyleSet>
{
    auto Style = MakeShared<FSlateStyleSet>(GetStyleSetName());

    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    const auto PluginIsValid = Plugin.IsValid();
    CK_ENSURE_IF_NOT(PluginIsValid, TEXT("CkDebugger plugin not found — common debugger brushes will be missing"))
    {}
    if (NOT PluginIsValid)
    { return Style; }

    Style->SetContentRoot(Plugin->GetBaseDir() / TEXT("Source/CkDebuggerCommon/Resources"));

    // ---- Glow halos (white, tint at use site) ----------------------------------------------------------------------
    Style->Set("CkCommon.Glow.Soft", new FSlateBoxBrush(
        Style->RootToContentDir(TEXT("Common/Glow_Soft"), TEXT(".png")),
        ck_debugger_common_style::GlowSoft_Margin));

    Style->Set("CkCommon.Glow.Tight", new FSlateBoxBrush(
        Style->RootToContentDir(TEXT("Common/Glow_Tight"), TEXT(".png")),
        ck_debugger_common_style::GlowTight_Margin));

    // ---- Flat button (underline tabs, text-that-clicks) ------------------------------------------------------------
    Style->Set("CkCommon.FlatButton", FButtonStyle()
        .SetNormal(FSlateRoundedBoxBrush{FLinearColor::Transparent, 4.0f})
        .SetHovered(FSlateRoundedBoxBrush{CkStyle::Hover(), 4.0f})
        .SetPressed(FSlateRoundedBoxBrush{CkStyle::Hover(), 4.0f})
        .SetDisabled(FSlateRoundedBoxBrush{FLinearColor::Transparent, 4.0f})
        .SetNormalPadding(FMargin{0.0f})
        .SetPressedPadding(FMargin{0.0f}));

    // ---- Icon toggle (toolbar + contextual boolean controls) ------------------------------------------------------
    Style->Set("CkCommon.IconToggle", FCheckBoxStyle()
        .SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
        .SetUncheckedImage(FSlateRoundedBoxBrush{CkStyle::Bg2(), 4.0f, CkStyle::Border(), 1.0f})
        .SetUncheckedHoveredImage(FSlateRoundedBoxBrush{CkStyle::Hover(), 4.0f, CkStyle::TextMute(), 1.0f})
        .SetUncheckedPressedImage(FSlateRoundedBoxBrush{CkStyle::Hover(), 4.0f, CkStyle::TextDim(), 1.0f})
        .SetCheckedImage(FSlateRoundedBoxBrush{CkStyle::Selection().CopyWithNewOpacity(0.30f), 4.0f, CkStyle::Selection(), 1.0f})
        .SetCheckedHoveredImage(FSlateRoundedBoxBrush{CkStyle::Selection().CopyWithNewOpacity(0.45f), 4.0f, CkStyle::Selection(), 1.0f})
        .SetCheckedPressedImage(FSlateRoundedBoxBrush{CkStyle::Selection().CopyWithNewOpacity(0.30f), 4.0f, CkStyle::Selection(), 1.0f})
        .SetForegroundColor(CkStyle::TextDim())
        .SetHoveredForegroundColor(CkStyle::Text())
        .SetPressedForegroundColor(CkStyle::Text())
        .SetCheckedForegroundColor(CkStyle::TextStrong())
        .SetCheckedHoveredForegroundColor(CkStyle::TextStrong())
        .SetCheckedPressedForegroundColor(CkStyle::TextStrong())
        .SetPadding(FMargin{6.0f, 4.0f}));

    return Style;
}

// ====================================================================================================================
