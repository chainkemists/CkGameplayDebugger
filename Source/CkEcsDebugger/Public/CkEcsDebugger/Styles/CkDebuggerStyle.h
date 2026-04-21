#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateColor.h"

enum class ECk_ObjectiveStatus : uint8;

// ====================================================================================================================
// Brush/text-style set + spacing constants specific to the ECS debugger.
// All colors live in CkDebugStyle (Project Settings → CkGameplayDebugger → GOAP).
// ====================================================================================================================

class FCkDebuggerStyle
{
public:
    static auto Initialize() -> void;
    static auto Shutdown() -> void;
    static auto Get() -> const ISlateStyle&;
    static auto GetStyleSetName() -> FName;

    static auto Get_ObjectiveStatusColor(ECk_ObjectiveStatus InStatus) -> FLinearColor;

    static constexpr float Padding_Small = 4.0f;
    static constexpr float Padding_Medium = 8.0f;
    static constexpr float Padding_Large = 16.0f;

    static constexpr float GraphNode_Width = 180.0f;
    static constexpr float GraphNode_Height = 40.0f;
    static constexpr float GraphNode_AccentWidth = 4.0f;
    static constexpr float GraphNode_CornerRadius = 6.0f;
    static constexpr float GraphNode_BorderThickness = 2.0f;
    static constexpr float GraphNode_BorderThickness_Center = 3.0f;

private:
    static TSharedPtr<FSlateStyleSet> StyleInstance;

    static auto Create() -> TSharedRef<FSlateStyleSet>;
    static auto CreateBrushes(TSharedRef<FSlateStyleSet> InStyle) -> void;
    static auto CreateColors(TSharedRef<FSlateStyleSet> InStyle) -> void;
    static auto CreateTextStyles(TSharedRef<FSlateStyleSet> InStyle) -> void;
};
