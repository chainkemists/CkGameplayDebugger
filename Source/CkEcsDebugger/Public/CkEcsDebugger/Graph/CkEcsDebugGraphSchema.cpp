#include "CkEcsDebugGraphSchema.h"
#include "CkEcsDebugConnectionPolicy.h"

#include "ConnectionDrawingPolicy.h"
#include "ToolMenu.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkEcsDebugGraphSchema::
    GetGraphType(
        const UEdGraph* InGraph) const
    -> EGraphType
{
    return GT_StateMachine;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkEcsDebugGraphSchema::
    GetGraphContextActions(
        FGraphContextMenuBuilder& InContextMenuBuilder) const
    -> void
{
    // Read-only graph — no context actions
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkEcsDebugGraphSchema::
    GetContextMenuActions(
        UToolMenu* InMenu,
        UGraphNodeContextMenuContext* InContext) const
    -> void
{
    // Read-only graph — no context menu
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkEcsDebugGraphSchema::
    CanCreateConnection(
        const UEdGraphPin* InPinA,
        const UEdGraphPin* InPinB) const
    -> const FPinConnectionResponse
{
    return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Read-only ECS debug graph"));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkEcsDebugGraphSchema::
    TryCreateConnection(
        UEdGraphPin* InPinA,
        UEdGraphPin* InPinB) const
    -> bool
{
    return false;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkEcsDebugGraphSchema::
    CreateConnectionDrawingPolicy(
        int32 InBackLayerID,
        int32 InFrontLayerID,
        float InZoomFactor,
        const FSlateRect& InClippingRect,
        FSlateWindowElementList& InDrawElements,
        UEdGraph* InGraphObj) const
    -> FConnectionDrawingPolicy*
{
    return new FCkEcsDebugConnectionPolicy(
        InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkEcsDebugGraphSchema::
    GetGraphDisplayInformation(
        const UEdGraph& InGraph,
        FGraphDisplayInfo& OutDisplayInfo) const
    -> void
{
    OutDisplayInfo.PlainName = FText::FromString(TEXT("ECS Entity Graph"));
    OutDisplayInfo.DisplayName = FText::FromString(TEXT("ECS Entity Graph"));
}

// --------------------------------------------------------------------------------------------------------------------
