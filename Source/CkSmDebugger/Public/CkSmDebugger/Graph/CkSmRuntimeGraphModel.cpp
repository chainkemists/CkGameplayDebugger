#include "CkSmDebugger/Graph/CkSmRuntimeGraphModel.h"

#include "CkDebuggerCommon/Graph/CkDebugGraphLayout.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"

namespace
{
    constexpr auto StateIdBase = 0x100000000ull;
    constexpr auto TransitionIdBase = 0x200000000ull;
    constexpr auto CompoundIdBase = 0x300000000ull;
    constexpr auto LayoutSpacingX = 350.0f;
    constexpr auto LayoutSpacingY = 120.0f;

    auto GetFallbackPosition(const int32 InIndex) -> FVector2D
    {
        return FVector2D{static_cast<float>(InIndex) * LayoutSpacingX, 120.0f};
    }

    auto CkSmRuntimeGraph_HasOverride(const FCkSmDebugger_StateInfo& InState) -> bool
    {
        return IsValid(InState.ScriptClass) && IsValid(InState.RequestedScriptClass) &&
               InState.RequestedScriptClass != InState.ScriptClass;
    }

    auto CkSmRuntimeGraph_IsFullyEventDriven(const FCkSmDebugger_SmInfo& InInfo,
                                              const int32 InStateIndex) -> bool
    {
        if (NOT InInfo.States.IsValidIndex(InStateIndex) ||
            NOT IsValid(InInfo.States[InStateIndex].ScriptClass))
        {
            return false;
        }

        const auto& State = InInfo.States[InStateIndex];
        if (State.Tasks.ContainsByPredicate(
                [](const FCkSmDebugger_TaskInfo& InTask)
                {
                    return InTask.Mode == ECk_SmTaskMode::Tick;
                }))
        {
            return false;
        }
        for (const auto& Transition : InInfo.Transitions)
        {
            if (Transition.SourceStateIndex != InStateIndex)
            {
                continue;
            }
            if (Transition.Conditions.ContainsByPredicate(
                    [](const FCkSmDebugger_ConditionInfo& InCondition)
                    {
                        return InCondition.Mode != ECk_SmConditionMode::EventDriven;
                    }))
            {
                return false;
            }
        }
        return true;
    }
} // namespace

auto FCkSmRuntimeGraphModel::GetStateId(const int32 InStateIndex) -> uint64
{
    return StateIdBase + static_cast<uint32>(InStateIndex);
}
auto FCkSmRuntimeGraphModel::GetTransitionId(const int32 InTransitionIndex) -> uint64
{
    return TransitionIdBase + static_cast<uint32>(InTransitionIndex);
}
auto FCkSmRuntimeGraphModel::GetCompoundId(const int32 InParentStateIndex) -> uint64
{
    return CompoundIdBase + static_cast<uint32>(InParentStateIndex);
}

auto FCkSmRuntimeGraphModel::FindNodeById(const uint64 InNodeId) const
    -> const FCkSmRuntimeGraphNode*
{
    return _Scene.Nodes.FindByPredicate(
        [InNodeId](const FCkSmRuntimeGraphNode& InNode)
        {
            return InNode.Id == InNodeId;
        });
}

auto FCkSmRuntimeGraphModel::BuildCopyPayload(const uint64 InNodeId) const
    -> TOptional<FCkSmRuntimeGraphCopyPayload>
{
    const auto* Node = FindNodeById(InNodeId);
    if (Node == nullptr)
    {
        return {};
    }
    if (Node->Kind == ECkSmRuntimeGraphNodeKind::State && Node->State)
    {
        auto Payload = FCkSmRuntimeGraphCopyPayload{};
        Payload.Target = ECkSmRuntimeGraphCopyTarget::State;
        Payload.DisplayName = Node->Label;
        Payload.ClassName = Node->State->StateName;
        return Payload;
    }
    if (Node->Kind != ECkSmRuntimeGraphNodeKind::Compound)
    {
        return {};
    }

    auto Payload = FCkSmRuntimeGraphCopyPayload{};
    Payload.Target = ECkSmRuntimeGraphCopyTarget::Compound;
    Payload.GroupLabel = Node->Label;
    for (const auto& Child : _Scene.Nodes)
    {
        if (Child.Kind == ECkSmRuntimeGraphNodeKind::State && Child.State &&
            Child.State->IsSubSmNode && Child.State->SubSmParentStateIndex == Node->StateIndex)
        {
            Payload.ChildDisplayNames.Add(Child.Label);
            Payload.ChildClassNames.Add(Child.State->StateName);
        }
    }
    const auto JoinedDisplay = FString::Join(Payload.ChildDisplayNames, TEXT("\n"));
    Payload.All = FString::Printf(TEXT("%s\nStates:\n%s"),
                                  *Payload.GroupLabel,
                                  Payload.ChildDisplayNames.IsEmpty() ? TEXT("  (none)")
                                                                      : *JoinedDisplay);
    return Payload;
}

auto FCkSmRuntimeGraphModel::ApplyScrubHighlight(const int32 InActiveStateIndex,
                                                 const int32 InExitedStateIndex) -> void
{
    for (auto& Node : _Scene.Nodes)
    {
        Node.bScrubActive = Node.Kind == ECkSmRuntimeGraphNodeKind::State &&
                            Node.StateIndex == InActiveStateIndex;
        Node.bScrubExited = Node.Kind == ECkSmRuntimeGraphNodeKind::State &&
                            Node.StateIndex == InExitedStateIndex;
        Node.bPrevious = false;
    }
    for (auto& Edge : _Scene.Edges)
    {
        Edge.bScrubHighlighted = Edge.SourceId == GetStateId(InExitedStateIndex) &&
                                 Edge.TargetId == GetStateId(InActiveStateIndex);
    }
}

auto FCkSmRuntimeGraphModel::ClearPresentation() -> void
{
    for (auto& Node : _Scene.Nodes)
    {
        Node.bScrubActive = false;
        Node.bScrubExited = false;
        Node.bPrevious = false;
    }
    for (auto& Edge : _Scene.Edges)
    {
        Edge.bScrubHighlighted = false;
        Edge.LiveFlashAlpha = 0.0f;
    }
}

auto FCkSmRuntimeGraphModel::TickLivePresentation(const float InDeltaTime,
                                                  const int32 InPreviousStateIndex,
                                                  const int32 InCurrentStateIndex,
                                                  const TSet<FString>& InPreviousStateNames) -> void
{
    for (auto& Node : _Scene.Nodes)
    {
        Node.bScrubActive = false;
        Node.bScrubExited = false;
        Node.bPrevious = Node.Kind == ECkSmRuntimeGraphNodeKind::State && Node.Label.Len() > 0 &&
                          InPreviousStateNames.Contains(Node.State ? Node.State->StateName
                                                                   : Node.Label) &&
                          NOT Node.bCurrent;
        if (Node.Kind == ECkSmRuntimeGraphNodeKind::State)
        {
            const auto bEntering = Node.StateIndex == InCurrentStateIndex &&
                                   InPreviousStateIndex >= 0 &&
                                   InPreviousStateIndex != InCurrentStateIndex;
            if (bEntering)
            {
                Node.EntryPulseAlpha = 1.0f;
            }
            Node.EntryPulseAlpha = FMath::Max(0.0f, Node.EntryPulseAlpha - InDeltaTime * 2.5f);
            Node.BorderGlowAlpha = FMath::FInterpTo(Node.BorderGlowAlpha,
                                                     Node.bCurrent ? 1.0f : 0.0f,
                                                     InDeltaTime,
                                                     7.0f);
            // Match the editor: a previously/currently visited card remains readable;
            // its current-only border is the signal that fades out.
            if (Node.bCurrent || Node.bPrevious)
            {
                Node.CellGlowAlpha = FMath::FInterpTo(Node.CellGlowAlpha, 1.0f, InDeltaTime, 7.0f);
            }
        }
    }
    for (auto& Edge : _Scene.Edges)
    {
        Edge.bScrubHighlighted = false;
        Edge.LiveFlashAlpha = FMath::Max(0.0f, Edge.LiveFlashAlpha - InDeltaTime);
        if (InPreviousStateIndex >= 0 && InCurrentStateIndex >= 0 &&
            InPreviousStateIndex != InCurrentStateIndex &&
            Edge.SourceId == GetStateId(InPreviousStateIndex) &&
            Edge.TargetId == GetStateId(InCurrentStateIndex))
        {
            Edge.LiveFlashAlpha = 1.0f;
        }
    }
}

auto FCkSmRuntimeGraphModel::EstimateStateSize(const FCkSmDebugger_StateInfo& InState,
                                               const bool bInExpandTasks,
                                               const int32 InNameDepth) -> FVector2D
{
    const auto Title = SCkDebug_NameLabel::Get_ShortName(InState.StateName, InNameDepth);
    auto Width = FMath::Max(140.0f, Title.Len() * 7.5f + 40.0f);
    auto Height = 62.0f; // title plus live/visited dwell row
    if (bInExpandTasks)
    {
        for (const auto& Task : InState.Tasks)
        {
            const auto TaskName = SCkDebug_NameLabel::Get_ShortName(Task.ClassName, InNameDepth);
            Width = FMath::Max(Width, TaskName.Len() * 6.5f + 64.0f);
        }
        if (NOT InState.Tasks.IsEmpty())
        {
            Height += 3.0f + InState.Tasks.Num() * 18.0f;
        }
    }
    if (InState.HasEntryBreakpoint || InState.HasExitBreakpoint || InState.IsBreakpointHit)
    {
        Height += 18.0f;
    }
    return FVector2D{Width, Height};
}

auto FCkSmRuntimeGraphModel::ComputeStructureHash(const FCkSmDebugger_SmInfo& InInfo,
                                                  const bool bInExpandTasks,
                                                  const int32 InNameDepth,
                                                  const int32 InSpacingX,
                                                  const int32 InSpacingY,
                                                  const bool bInUndirected) -> uint32
{
    auto Hash = ::GetTypeHash(static_cast<FCk_Handle>(InInfo.Handle));
    Hash = HashCombine(Hash, GetTypeHash(bInExpandTasks));
    Hash = HashCombine(Hash, GetTypeHash(InNameDepth));
    Hash = HashCombine(Hash, GetTypeHash(InSpacingX));
    Hash = HashCombine(Hash, GetTypeHash(InSpacingY));
    Hash = HashCombine(Hash, GetTypeHash(bInUndirected));
    Hash = HashCombine(Hash, GetTypeHash(InInfo.States.Num()));
    Hash = HashCombine(Hash, GetTypeHash(InInfo.Transitions.Num()));

    for (const auto& State : InInfo.States)
    {
        Hash = HashCombine(Hash, GetTypeHash(State.StateName));
        Hash = HashCombine(Hash, GetTypeHash(State.StateClass.Get()));
        Hash = HashCombine(Hash, GetTypeHash(State.ScriptClass.Get()));
        Hash = HashCombine(Hash, GetTypeHash(State.RequestedScriptClass.Get()));
        Hash = HashCombine(Hash, GetTypeHash(State.IsSubSmNode));
        Hash = HashCombine(Hash, GetTypeHash(State.SubSmParentStateName));
        Hash = HashCombine(Hash, GetTypeHash(State.SubSmParentStateIndex));
        Hash = HashCombine(Hash, GetTypeHash(State.HasSubStateMachine));
        Hash = HashCombine(Hash, GetTypeHash(State.IsHistoricalSubSm));
        Hash = HashCombine(Hash, GetTypeHash(State.IsCompoundNode));
        Hash = HashCombine(Hash, GetTypeHash(State.CompoundNodeWidth));
        Hash = HashCombine(Hash, GetTypeHash(State.CompoundNodeHeight));
        Hash = HashCombine(Hash, GetTypeHash(State.CompoundNodeParentStateIndex));
        Hash = HashCombine(Hash, GetTypeHash(State.NodePosition));
        Hash = HashCombine(Hash, GetTypeHash(State.NodeSize));
        Hash = HashCombine(Hash, GetTypeHash(State.Tasks.Num()));
        for (const auto& Task : State.Tasks)
        {
            Hash = HashCombine(Hash, GetTypeHash(Task.ClassName));
            Hash = HashCombine(Hash, GetTypeHash(Task.Mode));
            Hash = HashCombine(Hash, GetTypeHash(Task.HasSubStateMachine));
            Hash = HashCombine(Hash, GetTypeHash(Task.SubSmInitialStateClass.Get()));
        }
    }

    for (const auto& Transition : InInfo.Transitions)
    {
        Hash = HashCombine(Hash, GetTypeHash(Transition.SourceStateIndex));
        Hash = HashCombine(Hash, GetTypeHash(Transition.TargetStateIndex));
        Hash = HashCombine(Hash, GetTypeHash(Transition.Order));
        Hash = HashCombine(Hash, GetTypeHash(Transition.SourceStateName));
        Hash = HashCombine(Hash, GetTypeHash(Transition.TargetStateName));
        Hash = HashCombine(Hash, GetTypeHash(Transition.SourceStateClass.Get()));
        Hash = HashCombine(Hash, GetTypeHash(Transition.TargetStateClass.Get()));
        Hash = HashCombine(Hash, GetTypeHash(Transition.IsSubSmTransition));
        Hash = HashCombine(Hash, GetTypeHash(Transition.IsSubSmConnector));
        Hash = HashCombine(Hash, GetTypeHash(Transition.Conditions.Num()));
        for (const auto& Condition : Transition.Conditions)
        {
            Hash = HashCombine(Hash, GetTypeHash(Condition.ClassName));
            Hash = HashCombine(Hash, GetTypeHash(Condition.Mode));
        }
        for (const auto& Waypoint : Transition.RouteWaypoints)
        {
            Hash = HashCombine(Hash, GetTypeHash(Waypoint));
        }
    }
    return Hash;
}

auto FCkSmRuntimeGraphModel::UpdateRuntimeState(const FCkSmDebugger_SmInfo& InInfo) -> void
{
    for (auto& Node : _Scene.Nodes)
    {
        if (Node.Kind == ECkSmRuntimeGraphNodeKind::State &&
            InInfo.States.IsValidIndex(Node.StateIndex))
        {
            const auto& State = InInfo.States[Node.StateIndex];
            if (Node.State)
            {
                *Node.State = State;
            }
            else
            {
                Node.State = MakeShared<FCkSmDebugger_StateInfo>(State);
            }
            Node.bCurrent = State.IsCurrentState;
            Node.bParentActive = NOT State.IsSubSmNode
                                 || (InInfo.States.IsValidIndex(State.SubSmParentStateIndex)
                                     && InInfo.States[State.SubSmParentStateIndex].IsCurrentState);
            Node.bBreakpoint = State.HasEntryBreakpoint || State.HasExitBreakpoint ||
                               State.IsBreakpointHit;
            Node.bHasOverride = CkSmRuntimeGraph_HasOverride(State);
            Node.bFullyEventDriven = CkSmRuntimeGraph_IsFullyEventDriven(InInfo, Node.StateIndex);
        }
        else if (Node.Kind == ECkSmRuntimeGraphNodeKind::Compound)
        {
            Node.bCurrent = InInfo.States.ContainsByPredicate(
                [&Node](const FCkSmDebugger_StateInfo& InState)
                {
                    return InState.IsSubSmNode &&
                           InState.SubSmParentStateIndex == Node.StateIndex &&
                           InState.IsCurrentState;
                });
        }
        else if (Node.Kind == ECkSmRuntimeGraphNodeKind::Transition &&
                 InInfo.Transitions.IsValidIndex(Node.TransitionIndex))
        {
            const auto& Transition = InInfo.Transitions[Node.TransitionIndex];
            if (Node.Transition)
            {
                *Node.Transition = Transition;
            }
            else
            {
                Node.Transition = MakeShared<FCkSmDebugger_TransitionInfo>(Transition);
            }
            Node.Label = FString::Printf(TEXT("%d/%d"),
                                         Transition.SatisfiedCount,
                                         Transition.TotalCount);
            Node.Accent = Transition.HasBreakpoint
                              ? CkStyle::Err()
                              : (Transition.AreAllConditionsSatisfied ? CkStyle::Ok()
                                                                      : CkStyle::TextDim());
            Node.bBreakpoint = Transition.HasBreakpoint;
        }
    }
}

auto FCkSmRuntimeGraphModel::Rebuild(const FCkSmDebugger_SmInfo& InRawInfo,
                                     const bool bInExpandTasks,
                                     const int32 InNameDepth,
                                     const int32 InSpacingX,
                                     const int32 InSpacingY,
                                     const bool bInUndirected) -> void
{
    // This is the same persistence rule as UCkSmDebugGraph: live child state
    // entities disappear on parent exit, but their last observed topology stays
    // visible until the selected state machine changes.
    if (InRawInfo.Handle != _CachedSubSmOwner)
    {
        _CachedSubSmData.Reset();
        _CachedSubSmOwner = InRawInfo.Handle;
    }

    auto InInfo = InRawInfo;
    auto ParentsWithLiveSubSm = TSet<int32>{};
    for (const auto& State : InInfo.States)
    {
        if (State.IsSubSmNode)
        {
            ParentsWithLiveSubSm.Add(State.SubSmParentStateIndex);
        }
    }
    for (const auto ParentIndex : ParentsWithLiveSubSm)
    {
        auto& Cached = _CachedSubSmData.FindOrAdd(ParentIndex);
        Cached.States.Reset();
        Cached.Transitions.Reset();
        if (InInfo.States.IsValidIndex(ParentIndex))
        {
            for (const auto& Task : InInfo.States[ParentIndex].Tasks)
            {
                if (Task.HasSubStateMachine)
                {
                    Cached.Label = Task.ClassName;
                    break;
                }
            }
        }
        auto GlobalToLocal = TMap<int32, int32>{};
        for (auto Index = 0; Index < InInfo.States.Num(); ++Index)
        {
            if (InInfo.States[Index].IsSubSmNode &&
                InInfo.States[Index].SubSmParentStateIndex == ParentIndex)
            {
                GlobalToLocal.Add(Index, Cached.States.Num());
                Cached.States.Add(InInfo.States[Index]);
            }
        }
        for (const auto& Transition : InInfo.Transitions)
        {
            const auto* LocalSource = GlobalToLocal.Find(Transition.SourceStateIndex);
            const auto* LocalTarget = GlobalToLocal.Find(Transition.TargetStateIndex);
            if (Transition.IsSubSmTransition && LocalSource && LocalTarget)
            {
                auto Copy = Transition;
                Copy.SourceStateIndex = *LocalSource;
                Copy.TargetStateIndex = *LocalTarget;
                Cached.Transitions.Add(MoveTemp(Copy));
            }
        }
    }
    for (auto ParentIndex = 0; ParentIndex < InRawInfo.States.Num(); ++ParentIndex)
    {
        if (InRawInfo.States[ParentIndex].IsSubSmNode || ParentsWithLiveSubSm.Contains(ParentIndex))
        {
            continue;
        }
        const auto* Cached = _CachedSubSmData.Find(ParentIndex);
        if (NOT Cached || Cached->States.IsEmpty())
        {
            continue;
        }
        const auto Offset = InInfo.States.Num();
        for (const auto& CachedState : Cached->States)
        {
            auto Copy = CachedState;
            Copy.IsCurrentState = false;
            Copy.IsSubSmNode = true;
            Copy.IsHistoricalSubSm = true;
            Copy.SubSmParentStateIndex = ParentIndex;
            Copy.SubSmParentStateName = InRawInfo.States[ParentIndex].StateName;
            InInfo.States.Add(MoveTemp(Copy));
        }
        for (const auto& CachedTransition : Cached->Transitions)
        {
            auto Copy = CachedTransition;
            Copy.SourceStateIndex += Offset;
            Copy.TargetStateIndex += Offset;
            Copy.IsSubSmTransition = true;
            Copy.AreAllConditionsSatisfied = false;
            InInfo.Transitions.Add(MoveTemp(Copy));
        }
    }

    auto NewScene = FCkSmRuntimeGraphScene{};
    NewScene.Nodes.Reserve(InInfo.States.Num() * 2 + 1);

    const auto EstimateVisualStateSize = [&](const int32 InStateIndex)
    {
        auto Size = EstimateStateSize(InInfo.States[InStateIndex], bInExpandTasks, InNameDepth);
        if (CkSmRuntimeGraph_HasOverride(InInfo.States[InStateIndex]))
        {
            Size.Y += 18.0f;
        }
        if (CkSmRuntimeGraph_IsFullyEventDriven(InInfo, InStateIndex))
        {
            Size.Y += 18.0f;
        }
        return Size;
    };

    auto LayoutNodes = TArray<FCkDebugGraphLayoutNode>{};
    auto LayoutEdges = TArray<FCkDebugGraphLayoutEdge>{};
    for (auto Index = 0; Index < InInfo.States.Num(); ++Index)
    {
        const auto Size = EstimateVisualStateSize(Index);
        LayoutNodes.Add({Index, FMath::RoundToInt32(Size.X), FMath::RoundToInt32(Size.Y)});
    }
    for (const auto& Transition : InInfo.Transitions)
    {
        if (InInfo.States.IsValidIndex(Transition.SourceStateIndex) &&
            InInfo.States.IsValidIndex(Transition.TargetStateIndex))
        {
            LayoutEdges.Add({Transition.SourceStateIndex, Transition.TargetStateIndex});
        }
    }
    const auto Layout = FCkDebugGraphLayout::ComputeLayout(
        LayoutNodes,
        LayoutEdges,
        {InSpacingX, InSpacingY, 4, NOT bInUndirected, InInfo.CurrentStateIndex});

    for (auto StateIndex = 0; StateIndex < InInfo.States.Num(); ++StateIndex)
    {
        const auto& State = InInfo.States[StateIndex];
        auto Node = FCkSmRuntimeGraphNode{};
        Node.Id = GetStateId(StateIndex);
        Node.Kind = ECkSmRuntimeGraphNodeKind::State;
        Node.StateIndex = StateIndex;
        Node.Label = SCkDebug_NameLabel::Get_ShortName(State.StateName, InNameDepth);
        const auto* LayoutPosition = Layout.Positions.Find(StateIndex);
        Node.Position = State.NodePosition.IsNearlyZero() && LayoutPosition
                            ? FVector2D{static_cast<float>(LayoutPosition->X),
                                        static_cast<float>(LayoutPosition->Y)}
                            : (State.NodePosition.IsNearlyZero() ? GetFallbackPosition(StateIndex)
                                                                 : State.NodePosition);
        Node.Size = State.NodeSize.IsNearlyZero() ? EstimateVisualStateSize(StateIndex)
                                                  : State.NodeSize;
        Node.Accent = CkSmDebugger::ComputeStateColor(State.StateName);
        Node.bCurrent = State.IsCurrentState;
        Node.BorderGlowAlpha = State.IsCurrentState ? 1.0f : 0.0f;
        Node.CellGlowAlpha = State.IsCurrentState || State.HasBeenVisited ? 1.0f : 0.0f;
        Node.bParentActive = NOT State.IsSubSmNode
                             || (InInfo.States.IsValidIndex(State.SubSmParentStateIndex)
                                 && InInfo.States[State.SubSmParentStateIndex].IsCurrentState);
        Node.bBreakpoint = State.HasEntryBreakpoint || State.HasExitBreakpoint ||
                           State.IsBreakpointHit;
        Node.bHasOverride = CkSmRuntimeGraph_HasOverride(State);
        Node.bFullyEventDriven = CkSmRuntimeGraph_IsFullyEventDriven(InInfo, StateIndex);
        Node.bExpandTasks = bInExpandTasks;
        Node.State = MakeShared<FCkSmDebugger_StateInfo>(State);
        NewScene.Nodes.Add(MoveTemp(Node));
    }

    // Runtime counterpart of the editor graph's compound phases: every sub-SM gets a
    // local layout, compounds are placed shallowest-first, and nested owners use the
    // already-finalized position of their parent. This stays entirely value-only.
    auto ChildrenByParent = TMap<int32, TArray<int32>>{};
    for (auto StateIndex = 0; StateIndex < InInfo.States.Num(); ++StateIndex)
    {
        const auto& State = InInfo.States[StateIndex];
        if (State.IsSubSmNode && State.SubSmParentStateIndex >= 0)
        {
            ChildrenByParent.FindOrAdd(State.SubSmParentStateIndex).Add(StateIndex);
        }
    }
    auto ParentIndices = TArray<int32>{};
    ChildrenByParent.GenerateKeyArray(ParentIndices);
    const auto GetDepth = [&InInfo](const int32 InParentIndex)
    {
        auto Depth = 0;
        auto Index = InParentIndex;
        while (InInfo.States.IsValidIndex(Index) && InInfo.States[Index].IsSubSmNode && Depth < 8)
        {
            ++Depth;
            Index = InInfo.States[Index].SubSmParentStateIndex;
        }
        return Depth;
    };
    ParentIndices.Sort([&GetDepth](const int32 InA, const int32 InB)
    {
        const auto DepthA = GetDepth(InA);
        const auto DepthB = GetDepth(InB);
        return DepthA == DepthB ? InA < InB : DepthA < DepthB;
    });

    auto ParentMaxBottom = 0.0f;
    for (const auto& Node : NewScene.Nodes)
    {
        if (Node.Kind == ECkSmRuntimeGraphNodeKind::State && Node.State && NOT Node.State->IsSubSmNode)
        {
            ParentMaxBottom = FMath::Max(ParentMaxBottom, Node.Position.Y + Node.Size.Y);
        }
    }
    constexpr auto CompoundPaddingX = 20.0f;
    constexpr auto CompoundPaddingY = 20.0f;
    constexpr auto CompoundHeaderHeight = 28.0f;
    struct FCkSmRuntimePlacedCompound
    {
        float Left = 0.0f;
        float Right = 0.0f;
        float Bottom = 0.0f;
    };
    auto PlacedCompounds = TArray<FCkSmRuntimePlacedCompound>{};
    for (const auto ParentIndex : ParentIndices)
    {
        const auto* Children = ChildrenByParent.Find(ParentIndex);
        if (Children == nullptr)
        {
            continue;
        }
        auto LocalIndexByGlobal = TMap<int32, int32>{};
        auto LocalNodes = TArray<FCkDebugGraphLayoutNode>{};
        for (auto LocalIndex = 0; LocalIndex < Children->Num(); ++LocalIndex)
        {
            const auto ChildIndex = (*Children)[LocalIndex];
            const auto* Child = NewScene.Nodes.FindByPredicate([ChildIndex](const FCkSmRuntimeGraphNode& Node)
            {
                return Node.Id == FCkSmRuntimeGraphModel::GetStateId(ChildIndex);
            });
            if (Child == nullptr)
            {
                continue;
            }
            LocalIndexByGlobal.Add(ChildIndex, LocalIndex);
            LocalNodes.Add({LocalIndex, FMath::RoundToInt32(Child->Size.X), FMath::RoundToInt32(Child->Size.Y)});
        }
        auto LocalEdges = TArray<FCkDebugGraphLayoutEdge>{};
        for (const auto& Transition : InInfo.Transitions)
        {
            const auto* Source = LocalIndexByGlobal.Find(Transition.SourceStateIndex);
            const auto* Target = LocalIndexByGlobal.Find(Transition.TargetStateIndex);
            if (Transition.IsSubSmTransition && Source && Target)
            {
                LocalEdges.Add({*Source, *Target});
            }
        }
        const auto LocalLayout = FCkDebugGraphLayout::ComputeLayout(
            LocalNodes, LocalEdges,
            {FMath::Max(60, InSpacingX / 2), FMath::Max(28, InSpacingY / 3), 4, true, 0});
        auto Min = FVector2D{TNumericLimits<float>::Max(), TNumericLimits<float>::Max()};
        auto Max = FVector2D{TNumericLimits<float>::Lowest(), TNumericLimits<float>::Lowest()};
        for (const auto ChildIndex : *Children)
        {
            auto* Child = NewScene.Nodes.FindByPredicate(
                [ChildIndex](const FCkSmRuntimeGraphNode& Node)
                {
                    return Node.Id == FCkSmRuntimeGraphModel::GetStateId(ChildIndex);
                });
            if (NOT Child)
            {
                continue;
            }
            if (const auto* LocalIndex = LocalIndexByGlobal.Find(ChildIndex))
            {
                if (const auto* Position = LocalLayout.Positions.Find(*LocalIndex))
                {
                    Child->Position = FVector2D{static_cast<float>(Position->X), static_cast<float>(Position->Y)};
                }
            }
            Min.X = FMath::Min(Min.X, Child->Position.X);
            Min.Y = FMath::Min(Min.Y, Child->Position.Y);
            Max.X = FMath::Max(Max.X, Child->Position.X + Child->Size.X);
            Max.Y = FMath::Max(Max.Y, Child->Position.Y + Child->Size.Y);
        }
        if (Min.X == TNumericLimits<float>::Max())
        {
            continue;
        }
        const auto* Owner = NewScene.Nodes.FindByPredicate([ParentIndex](const FCkSmRuntimeGraphNode& Node)
        {
            return Node.Id == FCkSmRuntimeGraphModel::GetStateId(ParentIndex);
        });
        auto CompoundSize = FVector2D{FMath::Max(160.0f, Max.X - Min.X + 2.0f * CompoundPaddingX),
                                      FMath::Max(120.0f, Max.Y - Min.Y + CompoundHeaderHeight + 2.0f * CompoundPaddingY)};
        auto CompoundPosition = FVector2D{Owner ? Owner->Position.X : 0.0f,
                                          FMath::Max(ParentMaxBottom + InSpacingY,
                                                     Owner ? Owner->Position.Y + Owner->Size.Y + InSpacingY : 0.0f)};
        for (const auto& Placed : PlacedCompounds)
        {
            const auto bOverlapsX = CompoundPosition.X < Placed.Right &&
                                    CompoundPosition.X + CompoundSize.X > Placed.Left;
            if (bOverlapsX)
            {
                CompoundPosition.Y = FMath::Max(CompoundPosition.Y, Placed.Bottom + InSpacingY);
            }
        }
        for (const auto ChildIndex : *Children)
        {
            if (auto* Child = NewScene.Nodes.FindByPredicate([ChildIndex](const FCkSmRuntimeGraphNode& Node)
                { return Node.Id == FCkSmRuntimeGraphModel::GetStateId(ChildIndex); }))
            {
                Child->Position += CompoundPosition + FVector2D{CompoundPaddingX - Min.X,
                                                                  CompoundHeaderHeight + CompoundPaddingY - Min.Y};
            }
        }
        PlacedCompounds.Add({static_cast<float>(CompoundPosition.X),
                              static_cast<float>(CompoundPosition.X + CompoundSize.X),
                              static_cast<float>(CompoundPosition.Y + CompoundSize.Y)});
        auto Compound = FCkSmRuntimeGraphNode{};
        Compound.Id = GetCompoundId(ParentIndex);
        Compound.Kind = ECkSmRuntimeGraphNodeKind::Compound;
        Compound.StateIndex = ParentIndex;
        Compound.Label = InInfo.States.IsValidIndex(ParentIndex) ? InInfo.States[ParentIndex].StateName
                                                               : TEXT("Sub State Machine");
        Compound.Position = CompoundPosition;
        Compound.Size = CompoundSize;
        Compound.Accent = FLinearColor(0.30f, 0.45f, 0.75f, 0.35f);
        Compound.bCurrent = Children->ContainsByPredicate(
            [&InInfo](const int32 ChildIndex)
            {
                return InInfo.States.IsValidIndex(ChildIndex) &&
                       InInfo.States[ChildIndex].IsCurrentState;
            });
        NewScene.Nodes.Add(MoveTemp(Compound));
    }

    if (NOT InInfo.States.IsEmpty())
    {
        auto Entry = FCkSmRuntimeGraphNode{};
        Entry.Id = GetEntryId();
        Entry.Kind = ECkSmRuntimeGraphNodeKind::Entry;
        Entry.Label = TEXT("Entry  ▶");
        Entry.Position = FVector2D{-180.0f, 120.0f};
        Entry.Size = FVector2D{80.0f, 32.0f};
        NewScene.Nodes.Add(MoveTemp(Entry));
        auto EntryEdge = FCkSmRuntimeGraphEdge{};
        EntryEdge.SourceId = GetEntryId();
        EntryEdge.TargetId = GetStateId(0);
        EntryEdge.Color = FLinearColor::White;
        EntryEdge.bDirected = true;
        NewScene.Edges.Add(MoveTemp(EntryEdge));
    }

    for (auto TransitionIndex = 0; TransitionIndex < InInfo.Transitions.Num(); ++TransitionIndex)
    {
        const auto& Transition = InInfo.Transitions[TransitionIndex];
        if (NOT InInfo.States.IsValidIndex(Transition.SourceStateIndex) ||
            NOT InInfo.States.IsValidIndex(Transition.TargetStateIndex))
        {
            continue;
        }

        auto Edge = FCkSmRuntimeGraphEdge{};
        Edge.SourceId = GetStateId(Transition.SourceStateIndex);
        Edge.TargetId = GetStateId(Transition.TargetStateIndex);
        Edge.TransitionId = GetTransitionId(TransitionIndex);
        Edge.Color = CkStyle::TextDim();
        Edge.Thickness = 1.5f;
        Edge.bSelfLoop = Transition.SourceStateIndex == Transition.TargetStateIndex;
        Edge.RoutePoints = Transition.RouteWaypoints;
        for (const auto& Other : InInfo.Transitions)
        {
            if (&Other != &Transition && Other.SourceStateIndex == Transition.TargetStateIndex &&
                Other.TargetStateIndex == Transition.SourceStateIndex)
            {
                Edge.bReverse = true;
                break;
            }
        }
        const auto* SourceNode = NewScene.Nodes.FindByPredicate(
            [&Transition](const FCkSmRuntimeGraphNode& InNode)
            {
                return InNode.Id == FCkSmRuntimeGraphModel::GetStateId(Transition.SourceStateIndex);
            });
        const auto* TargetNode = NewScene.Nodes.FindByPredicate(
            [&Transition](const FCkSmRuntimeGraphNode& InNode)
            {
                return InNode.Id == FCkSmRuntimeGraphModel::GetStateId(Transition.TargetStateIndex);
            });
        if (SourceNode && TargetNode)
        {
            if (Edge.bSelfLoop && Edge.RoutePoints.IsEmpty())
            {
                const auto Right = SourceNode->Position.X + SourceNode->Size.X;
                const auto Top = SourceNode->Position.Y;
                Edge.RoutePoints = {FVector2D{Right + 42.0f, Top + SourceNode->Size.Y * 0.75f},
                                    FVector2D{Right + 42.0f, Top - 34.0f},
                                    FVector2D{SourceNode->Position.X + SourceNode->Size.X * 0.55f,
                                              Top - 34.0f}};
            }
            else if (Edge.bReverse && Edge.RoutePoints.IsEmpty())
            {
                const auto SourceCenter = SourceNode->Position + SourceNode->Size * 0.5f;
                const auto TargetCenter = TargetNode->Position + TargetNode->Size * 0.5f;
                auto Direction = TargetCenter - SourceCenter;
                const auto Length = Direction.Size();
                if (Length > KINDA_SMALL_NUMBER)
                {
                    Direction /= Length;
                    const auto Perpendicular = FVector2D{-Direction.Y, Direction.X};
                    Edge.RoutePoints.Add((SourceCenter + TargetCenter) * 0.5f +
                                         Perpendicular * 20.0f);
                }
            }
            auto Badge = FCkSmRuntimeGraphNode{};
            Badge.Id = GetTransitionId(TransitionIndex);
            Badge.Kind = ECkSmRuntimeGraphNodeKind::Transition;
            Badge.TransitionIndex = TransitionIndex;
            Badge.Label =
                FString::Printf(TEXT("%d/%d"), Transition.SatisfiedCount, Transition.TotalCount);
            Badge.Size = FVector2D{16.0f, 16.0f};
            const auto DefaultCenter = (SourceNode->Position + SourceNode->Size * 0.5f +
                                        TargetNode->Position + TargetNode->Size * 0.5f) *
                                       0.5f;
            auto BadgeCenter = DefaultCenter;
            if (Edge.bSelfLoop && Edge.RoutePoints.Num() >= 2)
            {
                BadgeCenter = Edge.RoutePoints[1];
            }
            else if (Edge.bReverse && NOT Edge.RoutePoints.IsEmpty())
            {
                BadgeCenter = Edge.RoutePoints[0];
            }
            Badge.Position = BadgeCenter - Badge.Size * 0.5f;
            Badge.Accent = Transition.HasBreakpoint
                               ? CkStyle::Err()
                               : (Transition.AreAllConditionsSatisfied ? CkStyle::Ok()
                                                                       : CkStyle::TextDim());
            Badge.bBreakpoint = Transition.HasBreakpoint;
            Badge.Transition = MakeShared<FCkSmDebugger_TransitionInfo>(Transition);
            NewScene.Nodes.Add(MoveTemp(Badge));
        }
        NewScene.Edges.Add(MoveTemp(Edge));
    }
    _Scene = MoveTemp(NewScene);
}
