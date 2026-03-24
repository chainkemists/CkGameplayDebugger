#pragma once

#include "EdGraph/EdGraphNode.h"
#include "CoreMinimal.h"

#include "CkSmDebugNode_Entry.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS()
class CKSMDEBUGGER_API UCkSmDebugNode_Entry : public UEdGraphNode
{
    GENERATED_BODY()

public:
    virtual auto AllocateDefaultPins() -> void override;
    virtual auto GetNodeTitle(ENodeTitleType::Type InTitleType) const -> FText override;
    virtual auto CanUserDeleteNode() const -> bool override { return false; }
    virtual auto CanDuplicateNode() const -> bool override { return false; }
};

// --------------------------------------------------------------------------------------------------------------------
