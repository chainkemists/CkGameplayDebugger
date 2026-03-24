#include "CkSmDebugGraphSchema.h"
#include "CkSmDebugConnectionPolicy.h"
#include "CkSmDebugGraph.h"
#include "CkSmDebugNode_State.h"
#include "CkSmDebugNode_Transition.h"

#include "CkCore/Macros/CkMacros.h"

#include "ConnectionDrawingPolicy.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "GraphEditorModule.h"

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
    // Don't call Super — suppresses default pin/link actions ("Break This Link", etc.)

    if (NOT InContext || NOT InContext->Node)
    { return; }

    auto* Graph = Cast<UCkSmDebugGraph>(InContext->Graph);

    // ---- State node context menu ----
    if (auto* StateNode = Cast<UCkSmDebugNode_State>(InContext->Node))
    {
        auto StateIdx = StateNode->Get_StateIndex();
        auto bHasEntryBP = StateNode->Get_HasEntryBreakpoint();
        auto bHasExitBP  = StateNode->Get_HasExitBreakpoint();

        auto& Section = InMenu->AddSection("Breakpoints", FText::FromString(TEXT("Breakpoints")));

        Section.AddMenuEntry(
            FName("ToggleEntryBP"),
            FText::FromString(bHasEntryBP ? TEXT("\x2612 Entry Breakpoint") : TEXT("\x2610 Entry Breakpoint")),
            FText::FromString(TEXT("Break when this state is entered")),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateLambda([StateIdx, Graph]()
            {
                if (Graph && Graph->OnIssueCommand)
                {
                    auto Cmd = FCkSmDebugger_Command{};
                    Cmd.Type = FCkSmDebugger_Command::EType::ToggleStateEntryBreakpoint;
                    Cmd.StateIndex = StateIdx;
                    Graph->OnIssueCommand(Cmd);
                }
            })));

        Section.AddMenuEntry(
            FName("ToggleExitBP"),
            FText::FromString(bHasExitBP ? TEXT("\x2612 Exit Breakpoint") : TEXT("\x2610 Exit Breakpoint")),
            FText::FromString(TEXT("Break when this state is exited")),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateLambda([StateIdx, Graph]()
            {
                if (Graph && Graph->OnIssueCommand)
                {
                    auto Cmd = FCkSmDebugger_Command{};
                    Cmd.Type = FCkSmDebugger_Command::EType::ToggleStateExitBreakpoint;
                    Cmd.StateIndex = StateIdx;
                    Graph->OnIssueCommand(Cmd);
                }
            })));
    }

    // ---- Transition node context menu ----
    else if (auto* TransNode = Cast<UCkSmDebugNode_Transition>(InContext->Node))
    {
        auto TransIdx = TransNode->Get_TransitionIndex();

        auto& Section = InMenu->AddSection("Breakpoints", FText::FromString(TEXT("Breakpoints")));

        Section.AddMenuEntry(
            FName("ToggleTransBP"),
            FText::FromString(TEXT("Toggle Transition Breakpoint")),
            FText::FromString(TEXT("Break when this transition fires")),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateLambda([TransIdx, Graph]()
            {
                if (Graph && Graph->OnIssueCommand)
                {
                    auto Cmd = FCkSmDebugger_Command{};
                    Cmd.Type = FCkSmDebugger_Command::EType::ToggleTransitionBreakpoint;
                    Cmd.TransitionIndex = TransIdx;
                    Graph->OnIssueCommand(Cmd);
                }
            })));
    }
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
