#include "CkEcsDebugGraphSchema.h"
#include "CkEcsDebugNode_Entity.h"

#include "EdGraph/EdGraphNode.h"

#if WITH_EDITOR
    #include "CkEcsDebugConnectionPolicy.h"
    #include "ConnectionDrawingPolicy.h"
    #include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#endif

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
#if WITH_EDITOR
    if (InMenu == nullptr || InContext == nullptr || InContext->Node == nullptr)
    { return; }

    const auto* EntityNode = Cast<UCkEcsDebugNode_Entity>(InContext->Node);
    if (EntityNode == nullptr)
    { return; }

    const auto NodeText = EntityNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString();

    ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
        TEXT("CopyText"),
        FText::FromString(TEXT("Copy Text")),
        FText::FromString(TEXT("Copy this node's display text to the clipboard")),
        NodeText);
#endif
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
#if WITH_EDITOR
    return new FCkEcsDebugConnectionPolicy(
        InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
#else
    return nullptr;
#endif
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
