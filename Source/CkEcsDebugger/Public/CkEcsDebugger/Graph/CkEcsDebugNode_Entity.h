#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "EdGraph/EdGraphNode.h"
#include "CoreMinimal.h"

#include "CkEcsDebugNode_Entity.generated.h"

// --------------------------------------------------------------------------------------------------------------------

enum class ECkEcsDebugEdgeType : uint8
{
    None,
    LifetimeOwner,
    ContextOwner,
    LifetimeDependent,
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS()
class CKECSDEBUGGER_API UCkEcsDebugNode_Entity : public UEdGraphNode
{
    GENERATED_BODY()

public:
    // UEdGraphNode
#if WITH_EDITOR
    virtual auto AllocateDefaultPins() -> void override;
    virtual auto GetNodeTitle(ENodeTitleType::Type InTitleType) const -> FText override;
    virtual auto CanUserDeleteNode() const -> bool override { return false; }
    virtual auto CanDuplicateNode() const -> bool override { return false; }
#endif

    auto
    Populate(
        const FCk_Handle& InEntity,
        const FString& InDisplayName,
        const FLinearColor& InAccentColor,
        bool InIsCenterNode,
        ECkEcsDebugEdgeType InEdgeType) -> void;

    // Accessors
    auto Get_Entity() const -> const FCk_Handle& { return _Entity; }
    auto Get_DisplayName() const -> const FString& { return _DisplayName; }
    auto Get_AccentColor() const -> FLinearColor { return _AccentColor; }
    auto Get_IsCenterNode() const -> bool { return _IsCenterNode; }
    auto Get_EdgeType() const -> ECkEcsDebugEdgeType { return _EdgeType; }

    auto Get_IsFilterMatch() const -> bool { return _IsFilterMatch; }
    auto Set_IsFilterMatch(bool InValue) -> void { _IsFilterMatch = InValue; }

private:
    FCk_Handle _Entity;

    UPROPERTY()
    FString _DisplayName;

    UPROPERTY()
    FLinearColor _AccentColor = FLinearColor::White;

    UPROPERTY()
    bool _IsCenterNode = false;

    UPROPERTY()
    bool _IsFilterMatch = true;

    ECkEcsDebugEdgeType _EdgeType = ECkEcsDebugEdgeType::None;
};

// --------------------------------------------------------------------------------------------------------------------
