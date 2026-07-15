#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

class FSlateStyleSet;

// --------------------------------------------------------------------------------------------------------------------

class FCkDebuggerLauncherStyle
{
public:
    static auto Initialize() -> void;
    static auto Shutdown() -> void;

    static auto Get() -> const ISlateStyle&;
    static auto GetStyleSetName() -> FName;
    static auto Get_IconBrush(FName InIconId) -> const FSlateBrush*;

private:
    static auto Create() -> TSharedRef<FSlateStyleSet>;
    static auto CreateBrushes(TSharedRef<FSlateStyleSet> InStyle) -> void;
    static auto CreateIconBrushes(TSharedRef<FSlateStyleSet> InStyle) -> void;

    static TSharedPtr<FSlateStyleSet> _StyleInstance;
};
