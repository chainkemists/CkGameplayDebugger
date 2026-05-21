#include "CkGoapDebugGraph.h"

#include "CkGoapDebugNode_Action.h"
#include "CkGoapDebugNode_Goal.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

// ====================================================================================================================
// Layout constants — chosen to match the mockup F v2 spacing.
// ====================================================================================================================

namespace ck_goap_graph
{
    constexpr float NodeWidth     = 180.0f;
    constexpr float NodeHeight    = 110.0f;   // visual estimate for layout stacking
    constexpr float HorzGap       = 70.0f;
    constexpr float VertGap       = 14.0f;
    constexpr float NodeMinX      = 40.0f;
    constexpr float NodeMinY      = 40.0f;
    constexpr float GoalGap       = 90.0f;    // extra horizontal gap before goal anchor
    constexpr float GoalNodeWidth = 200.0f;
}

// ====================================================================================================================

auto
    UCkGoapDebugGraph::
    ForceClear()
    -> void
{
    _GoalNode = nullptr;
    Nodes.Empty();
}

// ====================================================================================================================

auto
    UCkGoapDebugGraph::
    Get_ActionCount() const
    -> int32
{
    auto Count = 0;
    for (UEdGraphNode* Node : Nodes)
    {
        if (Cast<UCkGoapDebugNode_Action>(Node) != nullptr) { ++Count; }
    }
    return Count;
}

// ====================================================================================================================

auto
    UCkGoapDebugGraph::
    Get_EdgeCount() const
    -> int32
{
    auto Count = 0;
    for (UEdGraphNode* Node : Nodes)
    {
        if (NOT Node) { continue; }
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->Direction == EGPD_Output)
            { Count += Pin->LinkedTo.Num(); }
        }
    }
    return Count;
}

// ====================================================================================================================

auto
    UCkGoapDebugGraph::
    FindActionNode(
        const FCk_Handle_Goap_Action& InHandle) const
    -> UCkGoapDebugNode_Action*
{
    if (ck::Is_NOT_Valid(InHandle)) { return nullptr; }

    for (UEdGraphNode* Node : Nodes)
    {
        if (auto* ActionNode = Cast<UCkGoapDebugNode_Action>(Node))
        {
            if (ActionNode->Get_ActionHandle() == InHandle)
            { return ActionNode; }
        }
    }
    return nullptr;
}

// ====================================================================================================================

auto
    UCkGoapDebugGraph::
    RebuildFromSnapshot(
        const FCkGoapDebugger_ActionSetInfo& InActionSet,
        const FCk_Handle_Goap_Action& InSelectedActionHandle)
    -> void
{
    using namespace ck_goap_graph;

    SetSuppressNotifications(true);

    _GoalNode = nullptr;
    Nodes.Empty();

    const auto& Catalog = InActionSet.Catalog;
    const auto ActionCount = Catalog.Num();

    // ----------------------------------------------------------------------------------------------------------------
    // Phase 1: Create one node per Catalog entry. Defer pin allocation until
    //          after we've populated the snapshot so AllocateDefaultPins can
    //          read the precondition/effect arrays.
    // ----------------------------------------------------------------------------------------------------------------

    auto ActionNodes = TArray<UCkGoapDebugNode_Action*>{};
    ActionNodes.Reserve(ActionCount);

    // Build effect-key → producing-action-index map for fast wiring later.
    auto EffectKeyToProducer = TMap<FGameplayTag, TArray<int32>>{};

    // Compute plan membership / step index from the ActiveChain.
    // ChainHandles ordered root→leaf; the leaf is the "current plan tip"
    // and we number plan steps from 1.
    auto PlanIndexByHandle = TMap<FCk_Handle_Goap_Action, int32>{};
    {
        auto Step = 1;
        for (const auto& H : InActionSet.ActiveChainHandles)
        {
            PlanIndexByHandle.Add(H, Step++);
        }
    }

    for (auto i = 0; i < ActionCount; ++i)
    {
        const auto& Action = Catalog[i];

        auto* Node = NewObject<UCkGoapDebugNode_Action>(this);
        Node->PopulateFromActionInfo(Action);
        Node->AllocateDefaultPins();
        Node->SetFlags(RF_Transactional);

        const auto IsInChain = Action.IsInActiveChain;
        Node->Set_IsInPlan(IsInChain);

        if (auto* StepPtr = PlanIndexByHandle.Find(Action.Handle))
        { Node->Set_PlanStepIndex(*StepPtr); }
        else
        { Node->Set_PlanStepIndex(0); }

        Node->Set_IsSelected(ck::IsValid(InSelectedActionHandle) && Action.Handle == InSelectedActionHandle);

        // Defer failure-blocked classification — we only have a flag when
        // PlanStatus == Failed and the action is part of the failing chain.
        // Mark the leaf of a failed plan as failure-blocked for now.
        const auto ActionFailed =
            Action.PlanStatus == ECk_GoapPlanStatus::PlanFailed
            && (Action.Role == ECkGoapDebugger_ActionRole::Leaf
                || Action.Role == ECkGoapDebugger_ActionRole::Mid);
        Node->Set_IsFailureBlocked(ActionFailed);

        AddNode(Node, /*bFromUI=*/ false, /*bSelectNewNode=*/ false);
        ActionNodes.Add(Node);

        for (const auto& Eff : Action.Effects)
        {
            EffectKeyToProducer.FindOrAdd(Eff.Key).Add(i);
        }
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Phase 2: Compute per-node layer via memoized DFS over precondition->producer edges.
    //          Cycle-safe: nodes currently being visited use a sentinel layer of -2,
    //          so re-entry is detected and we fall through to layer 0 for that node.
    // ----------------------------------------------------------------------------------------------------------------

    auto Layers = TArray<int32>{};
    Layers.Init(-1, ActionCount);

    auto HasCycle = false;

    // Lambda-friendly iterative resolver: we use the call-stack-free pattern
    // with an explicit visit stack to avoid hitting the default 1MB stack on
    // deep dependency graphs.
    auto ResolveLayer = [&](auto& InSelf, int32 InIndex) -> int32
    {
        if (Layers[InIndex] >= 0) { return Layers[InIndex]; }
        if (Layers[InIndex] == -2) { HasCycle = true; return 0; }  // cycle — break

        Layers[InIndex] = -2;  // visiting sentinel

        auto BestLayer = 0;
        const auto& Action = Catalog[InIndex];
        for (const auto& Pre : Action.Preconditions)
        {
            const auto* Producers = EffectKeyToProducer.Find(Pre.Key);
            if (NOT Producers) { continue; }

            for (auto ProducerIdx : *Producers)
            {
                if (ProducerIdx == InIndex) { continue; }   // self-loop
                const auto ProducerLayer = InSelf(InSelf, ProducerIdx);
                BestLayer = FMath::Max(BestLayer, ProducerLayer + 1);
            }
        }

        Layers[InIndex] = BestLayer;
        return BestLayer;
    };

    for (auto i = 0; i < ActionCount; ++i)
    { ResolveLayer(ResolveLayer, i); }

    // Cycle fallback — if a cycle was detected, place every node on layer
    // by Catalog index in a single row. The DAG visualisation falls back
    // gracefully and the user still sees all nodes.
    if (HasCycle)
    {
        for (auto i = 0; i < ActionCount; ++i)
        { Layers[i] = i; }
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Phase 3: Group by layer and stack within each layer with a fixed vertical gap.
    //          Layer ordering is stable: catalog order within a layer.
    // ----------------------------------------------------------------------------------------------------------------

    auto MaxLayer = -1;
    for (auto L : Layers) { MaxLayer = FMath::Max(MaxLayer, L); }

    auto NodesPerLayer = TArray<int32>{};
    NodesPerLayer.SetNumZeroed(MaxLayer + 1);
    for (auto L : Layers) { ++NodesPerLayer[L]; }

    auto SlotCounter = TArray<int32>{};
    SlotCounter.SetNumZeroed(MaxLayer + 1);

    for (auto i = 0; i < ActionCount; ++i)
    {
        const auto L = Layers[i];
        const auto Slot = SlotCounter[L]++;
        const auto SlotCount = NodesPerLayer[L];

        const auto X = NodeMinX + L * (NodeWidth + HorzGap);

        // Vertically centre the layer's nodes around 0 so layers visually balance.
        const auto LayerColumnHeight = (SlotCount - 1) * (NodeHeight + VertGap);
        const auto Y = NodeMinY + Slot * (NodeHeight + VertGap) - LayerColumnHeight / 2.0f;

        ActionNodes[i]->NodePosX = static_cast<int32>(X);
        ActionNodes[i]->NodePosY = static_cast<int32>(Y);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Phase 4: Wire pins. For each Action's precondition pin, link from every
    //          matching effect pin on producer Actions. Pins are matched by name.
    // ----------------------------------------------------------------------------------------------------------------

    auto FindEffectPin = [](UCkGoapDebugNode_Action* InNode, const FGameplayTag& InKey) -> UEdGraphPin*
    {
        const auto Target = FName(*InKey.ToString());
        for (auto* Pin : InNode->Pins)
        {
            if (Pin && Pin->Direction == EGPD_Output && Pin->PinName == Target)
            { return Pin; }
        }
        return nullptr;
    };

    auto FindPrePin = [](UCkGoapDebugNode_Action* InNode, const FGameplayTag& InKey) -> UEdGraphPin*
    {
        const auto Target = FName(*InKey.ToString());
        for (auto* Pin : InNode->Pins)
        {
            if (Pin && Pin->Direction == EGPD_Input && Pin->PinName == Target)
            { return Pin; }
        }
        return nullptr;
    };

    for (auto i = 0; i < ActionCount; ++i)
    {
        const auto& Action = Catalog[i];
        for (const auto& Pre : Action.Preconditions)
        {
            auto* PrePin = FindPrePin(ActionNodes[i], Pre.Key);
            if (NOT PrePin) { continue; }

            const auto* Producers = EffectKeyToProducer.Find(Pre.Key);
            if (NOT Producers) { continue; }

            for (auto ProducerIdx : *Producers)
            {
                if (ProducerIdx == i) { continue; }  // skip self
                if (auto* EffPin = FindEffectPin(ActionNodes[ProducerIdx], Pre.Key))
                { EffPin->MakeLinkTo(PrePin); }
            }
        }
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Phase 5: Goal anchor. Use selected Action's Goal when selection is valid,
    //          otherwise fall back to the root Action's Goal. Place beyond the
    //          last action layer.
    // ----------------------------------------------------------------------------------------------------------------

    const FCkGoapDebugger_ActionInfo* GoalSource = nullptr;
    auto GoalOwnerName = FString{};

    if (ck::IsValid(InSelectedActionHandle))
    {
        for (const auto& A : Catalog)
        {
            if (A.Handle == InSelectedActionHandle) { GoalSource = &A; GoalOwnerName = A.ClassName; break; }
        }
    }

    if (NOT GoalSource && ck::IsValid(InActionSet.RootActionHandle))
    {
        for (const auto& A : Catalog)
        {
            if (A.Handle == InActionSet.RootActionHandle) { GoalSource = &A; GoalOwnerName = A.ClassName; break; }
        }
    }

    if (GoalSource != nullptr)
    {
        auto* GoalNode = NewObject<UCkGoapDebugNode_Goal>(this);
        GoalNode->PopulateFromGoal(GoalOwnerName, GoalSource->Goal);
        GoalNode->AllocateDefaultPins();
        GoalNode->SetFlags(RF_Transactional);

        const auto GoalLayer = (MaxLayer >= 0) ? (MaxLayer + 1) : 0;
        GoalNode->NodePosX = static_cast<int32>(NodeMinX + GoalLayer * (NodeWidth + HorzGap) + GoalGap);
        GoalNode->NodePosY = static_cast<int32>(NodeMinY);

        AddNode(GoalNode, /*bFromUI=*/ false, /*bSelectNewNode=*/ false);
        _GoalNode = GoalNode;

        // Wire effect-producers into matching goal-condition pins.
        for (const auto& Cond : GoalSource->Goal)
        {
            auto* GoalPin = static_cast<UEdGraphPin*>(nullptr);
            const auto Target = FName(*Cond.Key.ToString());
            for (auto* Pin : GoalNode->Pins)
            {
                if (Pin && Pin->Direction == EGPD_Input && Pin->PinName == Target)
                { GoalPin = Pin; break; }
            }
            if (NOT GoalPin) { continue; }

            const auto* Producers = EffectKeyToProducer.Find(Cond.Key);
            if (NOT Producers) { continue; }

            for (auto ProducerIdx : *Producers)
            {
                if (auto* EffPin = FindEffectPin(ActionNodes[ProducerIdx], Cond.Key))
                { EffPin->MakeLinkTo(GoalPin); }
            }
        }
    }

    SetSuppressNotifications(false);
    NotifyGraphChanged();
}

// ====================================================================================================================
