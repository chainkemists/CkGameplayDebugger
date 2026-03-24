#include "CkSmDebugGraphSchema.h"
#include "CkSmDebugConnectionPolicy.h"

#include "CkCore/Macros/CkMacros.h"

#include "ConnectionDrawingPolicy.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugGraphSchema::
    GetGraphType(
        const UEdGraph* InGraph) const
    -> EGraphType
{
    return GT_StateMachine;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugGraphSchema::
    GetGraphContextActions(
        FGraphContextMenuBuilder& InContextMenuBuilder) const
    -> void
{
    // Read-only graph — no context actions
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugGraphSchema::
    CanCreateConnection(
        const UEdGraphPin* InPinA,
        const UEdGraphPin* InPinB) const
    -> const FPinConnectionResponse
{
    return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Read-only debug graph"));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugGraphSchema::
    TryCreateConnection(
        UEdGraphPin* InPinA,
        UEdGraphPin* InPinB) const
    -> bool
{
    return false;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugGraphSchema::
    CreateConnectionDrawingPolicy(
        int32 InBackLayerID,
        int32 InFrontLayerID,
        float InZoomFactor,
        const FSlateRect& InClippingRect,
        FSlateWindowElementList& InDrawElements,
        UEdGraph* InGraphObj) const
    -> FConnectionDrawingPolicy*
{
    return new FCkSmDebugConnectionPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugGraphSchema::
    GetGraphDisplayInformation(
        const UEdGraph& InGraph,
        FGraphDisplayInfo& OutDisplayInfo) const
    -> void
{
    OutDisplayInfo.PlainName = FText::FromString(TEXT("SM Debug"));
    OutDisplayInfo.DisplayName = FText::FromString(TEXT("SM Debug"));
}

// --------------------------------------------------------------------------------------------------------------------
