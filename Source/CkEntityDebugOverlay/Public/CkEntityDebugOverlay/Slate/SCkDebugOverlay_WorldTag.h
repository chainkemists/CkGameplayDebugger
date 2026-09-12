#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Math/Vector2D.h"
#include "Internationalization/Text.h"

// ====================================================================================================================
// Per-candidate world-tag payload: stable entity identity, screen position, label text,
// distance-driven scale, and whether the entity is inside the world-tag range.
//
// Candidates may instead carry an ultra-condensed PLATE. Near plates have a name header plus
// colored feature-abbreviation badges (SM / GOAP / INV …); far plates deliberately show only
// those badges, never a behavioral compact-token value or a second entity name.
// ====================================================================================================================

struct FCk_DebugOverlay_WorldTagBadge
{
    FString      Text;
    FLinearColor Color = FLinearColor::White;
};

struct FCk_DebugOverlay_WorldTagInfo
{
    // Full EnTT id (entity number + generation), safe to retain as value-only Slate state.
    uint32    EntityKey  = MAX_uint32;
    FVector2D ScreenPos  = FVector2D::ZeroVector;
    FText     Text       = FText{};
    float     Scale      = 1.0f;   // [MinScale, 1] — applied via Slate RenderTransform

    // Camera→entity distance (cm), retained for distance-aware presentation.
    float     Distance   = 0.0f;
    // Drives the root-owned temporal transition. Out-of-range tags remain in the payload so they
    // can finish fading out; returning tags reverse from their current opacity without a snap.
    bool      bInRange   = true;
    // True for the focus entity's plate — rendered highlighted to match the emphasized diamond.
    bool      bIsFocus   = false;

    // World-plate payload (used when bIsPlate is true; Text is ignored then).
    bool                                  bIsPlate = false;
    bool                                  bShowHeader = true;
    FText                                 Header   = FText{};
    TArray<FCk_DebugOverlay_WorldTagBadge> Badges;
};

namespace ck_debugoverlay
{
    inline constexpr int32 WorldTagPresentationBudget = 16;
    inline constexpr double WorldTagVisibilityFadeDurationSeconds = 0.20;

    struct FWorldTagVisibilityFadeState
    {
        float  StartOpacity       = 0.0f;
        float  CurrentOpacity     = 0.0f;
        float  TargetOpacity      = 0.0f;
        double TransitionStartTime = 0.0;
        double LastUpdateTime      = 0.0;
        bool   bAdmitted           = false;
        bool   bInitialized        = false;
    };

    /** Advances one fixed-duration linear fade, reversing continuously from the current opacity. */
    CKENTITYDEBUGOVERLAY_API auto Advance_WorldTagVisibilityFade(
        FWorldTagVisibilityFadeState& InOutState,
        bool                          InIsInRange,
        double                        InNow) -> float;
}

// ====================================================================================================================
// Tiny single-line label placed at a world-projected screen position.
//
// The text is set imperatively via Set_Text(). Scale is applied via Set_Scale(); the root applies
// transition opacity uniformly to either this pill or a near plate. Hit-test invisible.
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

    // Applies distance-driven scale (B1).
    // Scale is applied as a RenderTransform Scale2D centred on the widget pivot (0.5, 0.5).
    auto Set_Scale(float InScale) -> void;

private:
    TSharedPtr<STextBlock> _TextBlock;
    TSharedPtr<class SBorder> _Border;
};

// ====================================================================================================================
