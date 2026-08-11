#pragma once

#include "EdGraph/EdGraphNode.h"
#include "CoreMinimal.h"

#include "CkSmDebugNode_Compound.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS()
class CKSMDEBUGGER_API UCkSmDebugNode_Compound : public UEdGraphNode
{
    GENERATED_BODY()

public:
#if WITH_EDITOR
    virtual auto AllocateDefaultPins() -> void override;
    virtual auto GetNodeTitle(ENodeTitleType::Type InTitleType) const -> FText override;
    virtual auto CanUserDeleteNode() const -> bool override { return false; }
    virtual auto CanDuplicateNode() const -> bool override { return false; }
#endif

    auto
    Populate(
        int32 InParentStateIndex,
        const FString& InLabel,
        float InWidth,
        float InHeight,
        const TArray<int32>& InSubSmStateIndices) -> void;

    auto Get_ParentStateIndex() const -> int32 { return _ParentStateIndex; }
    auto Get_CompoundLabel() const -> const FString& { return _CompoundLabel; }
    auto Get_CompoundWidth() const -> float { return _CompoundWidth; }
    auto Get_CompoundHeight() const -> float { return _CompoundHeight; }
    auto Get_SubSmStateIndices() const -> const TArray<int32>& { return _SubSmStateIndices; }

    auto Get_HasActiveSubState() const -> bool { return _HasActiveSubState; }
    auto Set_HasActiveSubState(bool InValue) -> void { _HasActiveSubState = InValue; }

    auto Get_IsParentStateActive() const -> bool { return _IsParentStateActive; }
    auto Set_IsParentStateActive(bool InValue) -> void { _IsParentStateActive = InValue; }

private:
    UPROPERTY()
    int32 _ParentStateIndex = -1;

    UPROPERTY()
    FString _CompoundLabel;

    UPROPERTY()
    float _CompoundWidth = 200.0f;

    UPROPERTY()
    float _CompoundHeight = 150.0f;

    TArray<int32> _SubSmStateIndices;

    bool _HasActiveSubState = false;
    bool _IsParentStateActive = false;
};

// --------------------------------------------------------------------------------------------------------------------
