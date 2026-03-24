#pragma once

#include "SGraphNode.h"
#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Slate visual for UCkSmDebugNode_Entry — small "Entry ▶" label with an overlay output
// pin for drawing the wire to the initial state.
// --------------------------------------------------------------------------------------------------------------------

class UCkSmDebugNode_Entry;

// --------------------------------------------------------------------------------------------------------------------

class CKSMDEBUGGER_API SGraphNode_SmEntry : public SGraphNode
{
public:
    SLATE_BEGIN_ARGS(SGraphNode_SmEntry) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, UCkSmDebugNode_Entry* InNode) -> void;

    // SGraphNode
    virtual auto UpdateGraphNode() -> void override;
    virtual auto CreatePinWidgets() -> void override;
    virtual auto AddPin(const TSharedRef<SGraphPin>& PinToAdd) -> void override;

private:
    UCkSmDebugNode_Entry* _EntryNode = nullptr;
};

// --------------------------------------------------------------------------------------------------------------------
