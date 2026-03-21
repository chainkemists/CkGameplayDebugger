#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateColor.h"

class FCkDebuggerStyle
{
public:
    static auto Initialize() -> void;
    static auto Shutdown() -> void;
    static auto Get() -> const ISlateStyle&;
    static auto GetStyleSetName() -> FName;

    static constexpr float Padding_Small = 4.0f;
    static constexpr float Padding_Medium = 8.0f;
    static constexpr float Padding_Large = 16.0f;

    static const FLinearColor Color_Background_Dark;
    static const FLinearColor Color_Background_Medium;
    static const FLinearColor Color_Background_Light;
    static const FLinearColor Color_Border;
    static const FLinearColor Color_Selection;
    static const FLinearColor Color_SelectionInactive;
    static const FLinearColor Color_Hover;
    
    static const FLinearColor Color_Text_Primary;
    static const FLinearColor Color_Text_Secondary;
    static const FLinearColor Color_Text_Muted;
    static const FLinearColor Color_Text_Highlight;
    
    static const FLinearColor Color_Entity_ID;
    static const FLinearColor Color_Transform;
    static const FLinearColor Color_Network;
    static const FLinearColor Color_Relationship;
    static const FLinearColor Color_Attribute;
    static const FLinearColor Color_Reference;
    static const FLinearColor Color_None;
    static const FLinearColor Color_Error;
    static const FLinearColor Color_Warning;
    static const FLinearColor Color_Success;

private:
    static TSharedPtr<FSlateStyleSet> StyleInstance;
    
    static auto Create() -> TSharedRef<FSlateStyleSet>;
    static auto CreateBrushes(TSharedRef<FSlateStyleSet> InStyle) -> void;
    static auto CreateColors(TSharedRef<FSlateStyleSet> InStyle) -> void;
    static auto CreateTextStyles(TSharedRef<FSlateStyleSet> InStyle) -> void;
};