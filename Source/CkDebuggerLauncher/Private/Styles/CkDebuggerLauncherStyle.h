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

    // Suite-shared surfaces — these resolve from the promoted FCkDebuggerStyle set rather than
    // from a launcher-local duplicate, so a palette change moves the rail with everything else.
    static auto Get_BackgroundBrush() -> const FSlateBrush*;
    static auto Get_SeparatorBrush() -> const FSlateBrush*;

private:
    static auto Create() -> TSharedRef<FSlateStyleSet>;
    static auto CreateBrushes(TSharedRef<FSlateStyleSet> InStyle) -> void;
    static auto CreateIconBrushes(TSharedRef<FSlateStyleSet> InStyle) -> void;

    static TSharedPtr<FSlateStyleSet> _StyleInstance;
};
