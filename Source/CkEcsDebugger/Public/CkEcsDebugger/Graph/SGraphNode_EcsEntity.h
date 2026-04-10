#pragma once

#include "SGraphNode.h"
#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Slate visual for UCkEcsDebugNode_Entity — rounded-rect body with colored accent strip,
// overlay pin for connection geometry, and center-node highlight border.
// --------------------------------------------------------------------------------------------------------------------

class UCkEcsDebugNode_Entity;

// --------------------------------------------------------------------------------------------------------------------

DECLARE_DELEGATE_OneParam(FOnEcsNodeDoubleClicked, UCkEcsDebugNode_Entity*);

class CKECSDEBUGGER_API SGraphNode_EcsEntity : public SGraphNode
{
public:
    SLATE_BEGIN_ARGS(SGraphNode_EcsEntity) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, UCkEcsDebugNode_Entity* InNode) -> void;

    // SGraphNode
    virtual auto UpdateGraphNode() -> void override;
    virtual auto CreatePinWidgets() -> void override;
    virtual auto AddPin(const TSharedRef<SGraphPin>& PinToAdd) -> void override;
    virtual auto OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) -> FReply override;

    // Suppress pin context menus — never report a pin under the cursor
    virtual auto GetHoveredPin(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const -> TSharedPtr<SGraphPin> override { return nullptr; }

    FOnEcsNodeDoubleClicked OnDoubleClicked;

protected:
    auto GetBorderBackgroundColor() const -> FSlateColor;
    auto GetContentBackgroundColor() const -> FSlateColor;
    auto GetAccentColor() const -> FSlateColor;
    auto GetLabelColor() const -> FSlateColor;

private:
    /** Returns 1.0 when matching the active filter, dim alpha when not. */
    auto Get_DimMultiplier() const -> float;

    UCkEcsDebugNode_Entity* _EntityNode = nullptr;
};

// --------------------------------------------------------------------------------------------------------------------
