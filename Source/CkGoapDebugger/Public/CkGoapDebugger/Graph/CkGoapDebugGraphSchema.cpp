#include "CkGoapDebugGraphSchema.h"

#if WITH_EDITOR
    #include "CkGoapDebugConnectionPolicy.h"
    #include "ConnectionDrawingPolicy.h"
#endif

// ====================================================================================================================

auto
    UCkGoapDebugGraphSchema::
    GetGraphType(
        const UEdGraph* /*InGraph*/) const
    -> EGraphType
{
    return GT_StateMachine;
}

// ====================================================================================================================

auto
    UCkGoapDebugGraphSchema::
    GetGraphContextActions(
        FGraphContextMenuBuilder& /*InContextMenuBuilder*/) const
    -> void
{
    // Intentionally empty — read-only debug graph.
}

// ====================================================================================================================

auto
    UCkGoapDebugGraphSchema::
    GetContextMenuActions(
        UToolMenu* /*InMenu*/,
        UGraphNodeContextMenuContext* /*InContext*/) const
    -> void
{
    // Intentionally not calling Super — the default pin/link actions caused
    // freezes in CkSmDebugger when right-clicked. Per-node copy entries are
    // wired via SCkDebug_NodePill::CopyText where applicable.
}

// ====================================================================================================================

auto
    UCkGoapDebugGraphSchema::
    CanCreateConnection(
        const UEdGraphPin* /*InPinA*/,
        const UEdGraphPin* /*InPinB*/) const
    -> const FPinConnectionResponse
{
    return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Read-only debug graph"));
}

// ====================================================================================================================

auto
    UCkGoapDebugGraphSchema::
    TryCreateConnection(
        UEdGraphPin* /*InPinA*/,
        UEdGraphPin* /*InPinB*/) const
    -> bool
{
    return false;
}

// ====================================================================================================================

auto
    UCkGoapDebugGraphSchema::
    CreateConnectionDrawingPolicy(
        int32 InBackLayerID,
        int32 InFrontLayerID,
        float InZoomFactor,
        const FSlateRect& InClippingRect,
        FSlateWindowElementList& InDrawElements,
        UEdGraph* InGraphObj) const
    -> FConnectionDrawingPolicy*
{
#if WITH_EDITOR
    return new FCkGoapDebugConnectionPolicy(
        InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
#else
    (void)InBackLayerID;
    (void)InFrontLayerID;
    (void)InZoomFactor;
    (void)InClippingRect;
    (void)InDrawElements;
    (void)InGraphObj;
    return nullptr;
#endif
}

// ====================================================================================================================

auto
    UCkGoapDebugGraphSchema::
    GetGraphDisplayInformation(
        const UEdGraph& /*InGraph*/,
        FGraphDisplayInfo& OutDisplayInfo) const
    -> void
{
    OutDisplayInfo.PlainName   = FText::FromString(TEXT("GOAP Action Graph"));
    OutDisplayInfo.DisplayName = FText::FromString(TEXT("GOAP Action Graph"));
}

// ====================================================================================================================
