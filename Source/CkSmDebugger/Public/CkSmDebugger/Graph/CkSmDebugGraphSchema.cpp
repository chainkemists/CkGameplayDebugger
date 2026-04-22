#include "CkSmDebugGraphSchema.h"
#include "CkSmDebugConnectionPolicy.h"
#include "CkSmDebugGraph.h"
#include "CkSmDebugNode_Compound.h"
#include "CkSmDebugNode_State.h"

#include "ConnectionDrawingPolicy.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "ToolMenu.h"

#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"

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
    // Read-only graph — no context actions for empty space
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugGraphSchema::
    GetContextMenuActions(
        UToolMenu* InMenu,
        UGraphNodeContextMenuContext* InContext) const
    -> void
{
    // Intentionally not calling Super — the default pin/link actions caused the
    // graph to freeze after right-click. Only the Copy Text entry is exposed here;
    // all other actions live in the detail panel.
    if (InMenu == nullptr || InContext == nullptr || InContext->Node == nullptr)
    { return; }

    // Compound (SubSmTask) group: aggregate every child state's display name so
    // the user can right-click the group and grab its full contents in one shot.
    if (const auto* Compound = Cast<UCkSmDebugNode_Compound>(InContext->Node))
    {
        const auto& GroupLabel = Compound->Get_CompoundLabel();
        const auto& ChildIndices = Compound->Get_SubSmStateIndices();

        auto NameDepth = 1;
        if (const auto* Graph = Cast<UCkSmDebugGraph>(Compound->GetGraph()))
        { NameDepth = Graph->LayoutParams.NameDepth; }

        // Resolve child indices to the real state nodes on the graph so we can
        // pull their display names through the same shortener the renderer uses.
        auto ChildDisplayLines = TArray<FString>{};
        auto ChildClassLines   = TArray<FString>{};
        if (const auto* OwningGraph = Compound->GetGraph())
        {
            for (const auto& Other : OwningGraph->Nodes)
            {
                const auto* State = Cast<UCkSmDebugNode_State>(Other.Get());
                if (State == nullptr || NOT ChildIndices.Contains(State->Get_StateIndex()))
                { continue; }

                const auto& Full = State->Get_StateName();
                ChildDisplayLines.Add(FCkSmLayoutParams::ComputeDisplayName(Full, NameDepth));
                ChildClassLines.Add(Full);
            }
        }

        const auto JoinedDisplay = FString::Join(ChildDisplayLines, TEXT("\n"));
        const auto JoinedClasses = FString::Join(ChildClassLines, TEXT("\n"));
        const auto AllStr = FString::Printf(TEXT("%s\nStates:\n%s"),
            *GroupLabel,
            ChildDisplayLines.Num() > 0 ? *JoinedDisplay : TEXT("  (none)"));

        ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
            TEXT("CopyGroupLabel"),
            FText::FromString(TEXT("Copy Group Label")),
            FText::FromString(TEXT("Copy this group's title")),
            GroupLabel);

        if (ChildDisplayLines.Num() > 0)
        {
            ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
                TEXT("CopyGroupChildNames"),
                FText::FromString(TEXT("Copy Child State Names")),
                FText::FromString(TEXT("Copy each child state's visible label, one per line")),
                JoinedDisplay);

            ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
                TEXT("CopyGroupChildClasses"),
                FText::FromString(TEXT("Copy Child State Class Names")),
                FText::FromString(TEXT("Copy each child state's full class name, one per line")),
                JoinedClasses);
        }

        ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
            TEXT("CopyGroupAll"),
            FText::FromString(TEXT("Copy All")),
            FText::FromString(TEXT("Copy the group label plus every child state name")),
            AllStr);

        return;
    }

    const auto* Node = Cast<UCkSmDebugNode_State>(InContext->Node);
    if (Node == nullptr)
    { return; }

    // Match what the Slate node widget actually shows: the full _StateName run
    // through the graph's name-depth shortener. Without this we'd hand the user
    // the long internal class name (e.g. "Ck_SmTest_Hier_Parent_Engage") even
    // though "Engage" is what they see on the node.
    const auto& FullName = Node->Get_StateName();
    auto NameDepth = 1;
    if (const auto* Graph = Cast<UCkSmDebugGraph>(Node->GetGraph()))
    { NameDepth = Graph->LayoutParams.NameDepth; }
    const auto DisplayName = FCkSmLayoutParams::ComputeDisplayName(FullName, NameDepth);

    ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
        TEXT("CopyDisplayName"),
        FText::FromString(TEXT("Copy Display Name")),
        FText::FromString(TEXT("Copy the visible state label (shortened by NameDepth)")),
        DisplayName);

    ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
        TEXT("CopyClassName"),
        FText::FromString(TEXT("Copy State Class Name")),
        FText::FromString(TEXT("Copy the full underlying state class name")),
        FullName);
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
