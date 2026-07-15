#include "CkDebuggerLauncherStyle.h"

#include "CkCore/Ensure/CkEnsure.h"
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

auto FCkDebuggerLauncherStyle::Get_IconBrush(FName InIconId) -> const FSlateBrush*
{
    if (InIconId.IsNone() || NOT _StyleInstance.IsValid())
    { return nullptr; }

    return _StyleInstance->GetOptionalBrush(
        FName{FString{TEXT("CkDebuggerLauncher.Icon.")} + InIconId.ToString()});
}

auto FCkDebuggerLauncherStyle::Create() -> TSharedRef<FSlateStyleSet>
{
    auto Style = MakeShared<FSlateStyleSet>(GetStyleSetName());
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));

    CK_ENSURE_IF_NOT(Plugin.IsValid(), TEXT("Cannot initialize the CK Debugger Launcher style: CkDebugger plugin not found"))
    { return Style; }

    Style->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
    CreateBrushes(Style);
    CreateIconBrushes(Style);
    return Style;
}

auto FCkDebuggerLauncherStyle::CreateBrushes(TSharedRef<FSlateStyleSet> InStyle) -> void
{
    InStyle->Set("CkDebuggerLauncher.Background", new FSlateColorBrush{CkStyle::BgRoot()});
    InStyle->Set("CkDebuggerLauncher.Separator", new FSlateColorBrush{CkStyle::Border()});
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

auto FCkDebuggerLauncherStyle::CreateIconBrushes(TSharedRef<FSlateStyleSet> InStyle) -> void
{
    const auto RegisterDirectory = [&InStyle](const FString& InDirectory) -> void
    {
        auto Files = TArray<FString>{};
        IFileManager::Get().FindFiles(Files, *(InDirectory / TEXT("*.svg")), true, false);
        Files.Sort();

        for (const auto& File : Files)
        {
            const auto IconId = FPaths::GetBaseFilename(File);
            InStyle->Set(
                FName{FString{TEXT("CkDebuggerLauncher.Icon.")} + IconId},
                new FSlateVectorImageBrush{InDirectory / File, FVector2D{24.0f, 24.0f}});
        }
    };

    const auto IconsDirectory = InStyle->GetContentRootDir() / TEXT("Icons");
    RegisterDirectory(IconsDirectory);
    RegisterDirectory(IconsDirectory / TEXT("General"));
}

// --------------------------------------------------------------------------------------------------------------------
