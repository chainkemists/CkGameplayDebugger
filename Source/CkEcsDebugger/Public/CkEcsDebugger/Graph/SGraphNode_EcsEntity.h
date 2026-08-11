#pragma once

#if WITH_EDITOR

#include "SGraphNode.h"
#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

// Slate visual for UCkEcsDebugNode_Entity. Wraps the common SCkDebug_NodePill
// and feeds it the node's accent color + edge-type border override, plus a
// per-frame dim tint driven by the inspector filter.
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

private:
    /** Returns 1.0 when matching the active filter, dim alpha when not. */
    auto Get_DimMultiplier() const -> float;
    auto Get_DimTint() const -> FLinearColor;

    UCkEcsDebugNode_Entity* _EntityNode = nullptr;
};

// --------------------------------------------------------------------------------------------------------------------

#endif // WITH_EDITOR
