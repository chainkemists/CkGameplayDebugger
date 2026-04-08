#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphSchema.h"

#include "CkSchedulerDebugGraphSchema.generated.h"

class FConnectionDrawingPolicy;

// --------------------------------------------------------------------------------------------------------------------

UCLASS()
class CKSCHEDULERDEBUGGER_API UCkSchedulerDebugGraphSchema : public UEdGraphSchema
{
	GENERATED_BODY()

public:
	virtual auto GetGraphType(const UEdGraph* InTestEdGraph) const -> EGraphType override;

	virtual auto CanCreateConnection(
		const UEdGraphPin* InPinA,
		const UEdGraphPin* InPinB) const -> const FPinConnectionResponse override;

	virtual auto TryCreateConnection(
		UEdGraphPin* InPinA,
		UEdGraphPin* InPinB) const -> bool override;

	virtual auto GetGraphContextActions(
		FGraphContextMenuBuilder& InContextMenuBuilder) const -> void override;

	virtual auto GetContextMenuActions(
		UToolMenu* InMenu,
		UGraphNodeContextMenuContext* InContext) const -> void override;

	virtual auto CreateConnectionDrawingPolicy(
		int32 InBackLayerID,
		int32 InFrontLayerID,
		float InZoomFactor,
		const FSlateRect& InClippingRect,
		FSlateWindowElementList& InDrawElements,
		UEdGraph* InGraphObj) const -> FConnectionDrawingPolicy* override;
};

// --------------------------------------------------------------------------------------------------------------------
