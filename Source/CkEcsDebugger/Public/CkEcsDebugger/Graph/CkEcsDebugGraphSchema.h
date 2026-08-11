#pragma once

#include "EdGraph/EdGraphSchema.h"
#include "CoreMinimal.h"

#include "CkEcsDebugGraphSchema.generated.h"

class FConnectionDrawingPolicy;

// --------------------------------------------------------------------------------------------------------------------

UCLASS()
class CKECSDEBUGGER_API UCkEcsDebugGraphSchema : public UEdGraphSchema
{
    GENERATED_BODY()

public:
    // UEdGraphSchema
    virtual auto GetGraphType(const UEdGraph* InGraph) const -> EGraphType override;

    virtual auto GetGraphContextActions(
        FGraphContextMenuBuilder& InContextMenuBuilder) const -> void override;

    virtual auto GetContextMenuActions(
        UToolMenu* InMenu,
        UGraphNodeContextMenuContext* InContext) const -> void override;

    virtual auto CanCreateConnection(
        const UEdGraphPin* InPinA,
        const UEdGraphPin* InPinB) const -> const FPinConnectionResponse override;

    virtual auto TryCreateConnection(
        UEdGraphPin* InPinA,
        UEdGraphPin* InPinB) const -> bool override;

    virtual auto CreateConnectionDrawingPolicy(
        int32 InBackLayerID,
        int32 InFrontLayerID,
        float InZoomFactor,
        const FSlateRect& InClippingRect,
        FSlateWindowElementList& InDrawElements,
        UEdGraph* InGraphObj) const -> FConnectionDrawingPolicy* override;

    virtual auto GetGraphDisplayInformation(
        const UEdGraph& InGraph,
        FGraphDisplayInfo& OutDisplayInfo) const -> void override;
};

// --------------------------------------------------------------------------------------------------------------------
