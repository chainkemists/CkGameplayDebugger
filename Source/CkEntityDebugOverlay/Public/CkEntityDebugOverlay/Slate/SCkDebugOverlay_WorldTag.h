#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Math/Vector2D.h"
#include "Internationalization/Text.h"

// ====================================================================================================================
// Per-candidate world-tag payload: screen position, label text, and distance-driven
// scale / opacity computed in Push_ToRoot (B1 — distance-scaled world pills).
//
// Near candidates may instead carry an ultra-condensed PLATE: a name header plus a
// row of colored feature-abbreviation badges (SM / GOAP / INV …) rendered under it.
// ====================================================================================================================

struct FCk_DebugOverlay_WorldTagBadge
{
    FString      Text;
    FLinearColor Color = FLinearColor::White;
};

struct FCk_DebugOverlay_WorldTagInfo
{
    FVector2D ScreenPos  = FVector2D::ZeroVector;
    FText     Text       = FText{};
    float     Scale      = 1.0f;   // [MinScale, 1] — applied via Slate RenderTransform
    float     Opacity    = 1.0f;   // [0.15, 1] — applied to ColorAndOpacity alpha

    // Near-plate payload (used when bIsPlate is true; Text is ignored then).
    bool                                  bIsPlate = false;
    FText                                 Header   = FText{};
    TArray<FCk_DebugOverlay_WorldTagBadge> Badges;
};

// ====================================================================================================================
// Tiny single-line label placed at a world-projected screen position.
//
// The text is set imperatively via Set_Text(). Scale + opacity are applied via
// Set_Style(). Hit-test invisible. Positioned by SCkDebugOverlay_Root via SConstraintCanvas.
// ====================================================================================================================

class CKENTITYDEBUGOVERLAY_API SCkDebugOverlay_WorldTag : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebugOverlay_WorldTag)
        : _Text(FText::GetEmpty())
    {}
        SLATE_ARGUMENT(FText, Text)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    // Updates the displayed text. Safe to call every tick.
    auto Set_Text(const FText& InText) -> void;

    // Applies distance-driven scale and opacity (B1).
    // Scale is applied as a RenderTransform Scale2D centred on the widget pivot (0.5, 0.5).
    // Opacity sets the text + border color alpha.
    auto Set_Style(float InScale, float InOpacity) -> void;

private:
    TSharedPtr<STextBlock> _TextBlock;
    TSharedPtr<class SBorder> _Border;
};

// ====================================================================================================================
