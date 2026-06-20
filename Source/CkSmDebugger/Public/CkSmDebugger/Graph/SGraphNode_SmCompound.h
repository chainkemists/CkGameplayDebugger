#pragma once

#include "SGraphNode.h"
#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Slate visual for UCkSmDebugNode_Compound — a subtle rounded-rect container that
// groups sub-state-machine states. Nearly transparent background with a thin border
// and a small label. Renders behind the sub-SM state nodes.
// --------------------------------------------------------------------------------------------------------------------

class UCkSmDebugNode_Compound;

// --------------------------------------------------------------------------------------------------------------------

class CKSMDEBUGGER_API SGraphNode_SmCompound : public SGraphNode
{
public:
    SLATE_BEGIN_ARGS(SGraphNode_SmCompound) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, UCkSmDebugNode_Compound* InNode) -> void;

    // SGraphNode
    virtual auto UpdateGraphNode() -> void override;
    virtual auto CreatePinWidgets() -> void override;
    virtual auto AddPin(const TSharedRef<SGraphPin>& PinToAdd) -> void override;
    virtual auto OnPaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const -> int32 override;

    // Dragging the compound translates every grouped child by the same delta
    virtual auto MoveTo(
        const FVector2f& InNewPosition,
        FNodeSet& InNodeFilter,
        bool bMarkDirty = true) -> void override;

    // SWidget — drives automatic resize when children move
    virtual auto Tick(
        const FGeometry& AllottedGeometry,
        const double InCurrentTime,
        const float InDeltaTime) -> void override;

private:
    auto RecomputeBoundsFromChildren() -> bool;

    UCkSmDebugNode_Compound* _CompoundNode = nullptr;

    // Last anchored position — used by MoveTo to compute translation delta for children
    FVector2D _LastKnownPosition = FVector2D::ZeroVector;
    bool _HasLastKnownPosition = false;

    // Bubble border color fade: 0 = grey (inactive), 1 = blue (holds the active
    // sub-state). Driven toward HasActiveSubState in Tick; read in OnPaint.
    float _BubbleGlowAlpha = 0.0f;
};

// --------------------------------------------------------------------------------------------------------------------
