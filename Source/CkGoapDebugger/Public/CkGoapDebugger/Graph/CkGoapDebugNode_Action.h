#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "EdGraph/EdGraphNode.h"
#include "CoreMinimal.h"

#include "CkGoapDebugNode_Action.generated.h"

// ====================================================================================================================
// UEdGraphNode subclass — one per Action in the selected ActionSet's catalog.
// Stores a snapshot of FCkGoapDebugger_ActionInfo plus per-render hints
// (plan-step index, selection, failure flag) that the Slate visual reads.
//
// Pin layout:
//   - One input pin per Precondition (named by the gameplay tag).
//   - One output pin per Effect (named by the gameplay tag).
//
// PIE lifetime: Snapshot embeds FCk_Handle_Goap_Action which holds a
// TOptional<FCk_Registry>. The owning UCkGoapDebugGraph clears all nodes
// (and therefore all such handles) on world teardown via Empty()/RebuildFromSnapshot.
// ====================================================================================================================

UCLASS()
class CKGOAPDEBUGGER_API UCkGoapDebugNode_Action : public UEdGraphNode
{
    GENERATED_BODY()

public:
    // UEdGraphNode
    virtual auto AllocateDefaultPins() -> void override;
    virtual auto GetNodeTitle(ENodeTitleType::Type InTitleType) const -> FText override;
    virtual auto CanUserDeleteNode() const -> bool override { return false; }
    virtual auto CanDuplicateNode() const -> bool override { return false; }

    auto PopulateFromActionInfo(const FCkGoapDebugger_ActionInfo& InAction) -> void;

    // Accessors
    auto Get_Snapshot()         const -> const FCkGoapDebugger_ActionInfo& { return _Snapshot; }
    auto Get_ActionHandle()     const -> const FCk_Handle_Goap_Action&     { return _Snapshot.Handle; }
    auto Get_IsInPlan()         const -> bool { return _IsInPlan; }
    auto Get_PlanStepIndex()    const -> int32 { return _PlanStepIndex; }
    auto Get_IsSelected()       const -> bool { return _IsSelected; }
    auto Get_IsFailureBlocked() const -> bool { return _IsFailureBlocked; }
    auto Get_IsComposite()      const -> bool { return _Snapshot.ChildActionHandles.Num() > 0; }

    auto Set_IsInPlan(bool InValue)         -> void { _IsInPlan = InValue; }
    auto Set_PlanStepIndex(int32 InIndex)   -> void { _PlanStepIndex = InIndex; }
    auto Set_IsSelected(bool InValue)       -> void { _IsSelected = InValue; }
    auto Set_IsFailureBlocked(bool InValue) -> void { _IsFailureBlocked = InValue; }

private:
    // Snapshot is NOT a UPROPERTY — it contains plain (non-USTRUCT) types.
    // The graph rebuild path overwrites these on every refresh.
    FCkGoapDebugger_ActionInfo _Snapshot;

    UPROPERTY()
    bool _IsInPlan = false;

    UPROPERTY()
    int32 _PlanStepIndex = 0;  // 1-based; 0 when not in plan

    UPROPERTY()
    bool _IsSelected = false;

    UPROPERTY()
    bool _IsFailureBlocked = false;
};

// ====================================================================================================================
