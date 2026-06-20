#include "CkSmDebugGraph.h"

#include "CkSmDebugNode_Compound.h"
#include "CkSmDebugNode_Entry.h"
#include "CkSmDebugNode_State.h"
#include "CkSmDebugNode_Transition.h"
#include "CkSmDebugGraphSchema.h"
#include "CkSmDebugger/CkSmDebuggerStyle.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkDebuggerCommon/Graph/CkDebugGraphLayout.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugGraph::
    Get_MaxNameDepth() const
    -> int32
{
    auto MaxSegments = 1;
    for (const auto& NodePtr : Nodes)
    {
        const auto* StateNode = Cast<UCkSmDebugNode_State>(NodePtr.Get());
        if (ck::Is_NOT_Valid(StateNode)) { continue; }

        auto Name = StateNode->Get_StateName();
        if (Name.EndsWith(TEXT("_C"))) { Name = Name.LeftChop(2); }

        TArray<FString> Segments;
        Name.ParseIntoArray(Segments, TEXT("_"), true);
        MaxSegments = FMath::Max(MaxSegments, Segments.Num());
    }
    return MaxSegments;
}

// --------------------------------------------------------------------------------------------------------------------
// Real data
// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugGraph::
    RebuildFromSmInfo(
        const FCkSmDebugger_SmInfo& InSmInfo)
    -> void
{
    SetSuppressNotifications(true);

    Nodes.Empty();

    // Clear sub-SM cache when switching to a different SM so stale topology doesn't leak across
    if (InSmInfo.Handle != _CachedSubSmOwner)
    {
        _CachedSubSmData.Reset();
        _CachedSubSmOwner = InSmInfo.Handle;
    }

    // ================================================================================================================
    // Pre-pass: Inject cached sub-SM data for parent states whose sub-SM is not currently running.
    // This keeps compound blocks always visible regardless of sub-SM lifecycle.
    // ================================================================================================================

    auto SmInfo = InSmInfo;  // mutable copy — we may append cached sub-SM states

    // Identify which parent states have live sub-SM data
    auto ParentsWithLiveSubSm = TSet<int32>{};
    for (auto& State : SmInfo.States)
    {
        if (State.IsSubSmNode)
        { ParentsWithLiveSubSm.Add(State.SubSmParentStateIndex); }
    }

    // Cache live sub-SM data for future use
    for (auto ParentIdx : ParentsWithLiveSubSm)
    {
        auto& Cached = _CachedSubSmData.FindOrAdd(ParentIdx);
        Cached.States.Empty();
        Cached.Transitions.Empty();

        // Determine label
        if (ParentIdx >= 0 && ParentIdx < SmInfo.States.Num())
        {
            for (auto& Task : SmInfo.States[ParentIdx].Tasks)
            {
                if (Task.HasSubStateMachine)
                { Cached.Label = Task.ClassName; break; }
            }
        }

        // Collect sub-SM states with LOCAL indices (0-based)
        auto GlobalToLocal = TMap<int32, int32>{};
        for (auto i = 0; i < SmInfo.States.Num(); ++i)
        {
            if (SmInfo.States[i].IsSubSmNode && SmInfo.States[i].SubSmParentStateIndex == ParentIdx)
            {
                auto LocalIdx = Cached.States.Num();
                GlobalToLocal.Add(i, LocalIdx);

                auto StateCopy = SmInfo.States[i];
                Cached.States.Add(MoveTemp(StateCopy));
            }
        }

        // Collect transitions with local indices
        for (auto& Trans : SmInfo.Transitions)
        {
            if (NOT Trans.IsSubSmTransition) { continue; }
            auto* LocalSrc = GlobalToLocal.Find(Trans.SourceStateIndex);
            auto* LocalDst = GlobalToLocal.Find(Trans.TargetStateIndex);
            if (LocalSrc && LocalDst)
            {
                auto TransCopy = Trans;
                TransCopy.SourceStateIndex = *LocalSrc;
                TransCopy.TargetStateIndex = *LocalDst;
                Cached.Transitions.Add(MoveTemp(TransCopy));
            }
        }
    }

    // Inject cached sub-SM data for parent states whose sub-SM is NOT currently running.
    // Don't gate on HasSubStateMachine — that flag may not be set when the parent state
    // is inactive (task entities don't exist). The cache is the source of truth.
    for (auto i = 0; i < InSmInfo.States.Num(); ++i)
    {
        if (InSmInfo.States[i].IsSubSmNode) { continue; }
        if (ParentsWithLiveSubSm.Contains(i)) { continue; }

        auto* Cached = _CachedSubSmData.Find(i);
        if (NOT Cached || Cached->States.Num() == 0) { continue; }

        auto IndexOffset = SmInfo.States.Num();

        for (auto& CachedState : Cached->States)
        {
            auto InjectedState = CachedState;
            InjectedState.IsCurrentState = false;
            // Preserve cached HasBeenVisited / DwellTimeSeconds — they hold the
            // last-known values from when the sub-SM was live, which is exactly
            // what the "Was active for…" popup wants to show.
            InjectedState.IsSubSmNode = true;
            InjectedState.SubSmParentStateIndex = i;
            InjectedState.SubSmParentStateName = SmInfo.States[i].StateName;
            SmInfo.States.Add(MoveTemp(InjectedState));
        }

        for (auto& CachedTrans : Cached->Transitions)
        {
            auto InjectedTrans = CachedTrans;
            InjectedTrans.SourceStateIndex += IndexOffset;
            InjectedTrans.TargetStateIndex += IndexOffset;
            InjectedTrans.IsSubSmTransition = true;
            InjectedTrans.AreAllConditionsSatisfied = false;
            SmInfo.Transitions.Add(MoveTemp(InjectedTrans));
        }
    }

    _TransitionData = SmInfo.Transitions;

    // ================================================================================================================
    // Compound-aware Sugiyama layered graph drawing
    // Phase 0: Partition states into parent vs sub-SM groups, layout sub-SM groups internally
    // Phase 1: Layer assignment (BFS) over parent-level nodes + compound blocks
    // Phase 2: Dummy vertex insertion for parent-level edges
    // Phase 3: Barycenter crossing reduction
    // Phase 4: Coordinate assignment with variable node heights
    // Phase 5: Create graph nodes, wires, and compound containers
    // ================================================================================================================

    auto StateCount = SmInfo.States.Num();

    // ================================================================================================================
    // Phase 0: Partition & internal sub-SM layout
    // ================================================================================================================

    auto ParentStateIndices = TArray<int32>{};
    auto SubSmGroups = TMap<int32, TArray<int32>>{};

    for (auto i = 0; i < StateCount; ++i)
    {
        if (SmInfo.States[i].IsSubSmNode)
        {
            auto ParentIdx = SmInfo.States[i].SubSmParentStateIndex;
            SubSmGroups.FindOrAdd(ParentIdx).Add(i);
        }
        else
        {
            ParentStateIndices.Add(i);
        }
    }

    // Internal layout data for each compound block
    struct FCompoundBlock
    {
        int32 ParentStateIndex;
        FString Label;
        TArray<int32> SubSmStateIndices;
        float Width;
        float Height;
        TMap<int32, FVector2D> InternalOffsets;
        int32 LayoutPosX = 0;
        int32 LayoutPosY = 0;
    };
    auto CompoundBlocks = TArray<FCompoundBlock>{};

    constexpr auto CompoundPadding = 20.0f;
    constexpr auto CompoundHeaderHeight = 28.0f;
    constexpr auto InternalSpacingX = 250;
    constexpr auto InternalSpacingY = 80;

    for (auto& Pair : SubSmGroups)
    {
        auto ParentIdx = Pair.Key;
        auto& SubIndices = Pair.Value;

        if (SubIndices.Num() == 0) { continue; }

        // Build local index mapping: global index → local index (0-based within this sub-SM)
        auto GlobalToLocal = TMap<int32, int32>{};
        for (auto LocalIdx = 0; LocalIdx < SubIndices.Num(); ++LocalIdx)
        { GlobalToLocal.Add(SubIndices[LocalIdx], LocalIdx); }

        auto LocalCount = SubIndices.Num();

        // Collect internal transitions (both endpoints in this sub-SM group)
        struct FLocalTrans { int32 LocalSrc; int32 LocalDst; };
        auto LocalTransitions = TArray<FLocalTrans>{};

        for (auto& Trans : SmInfo.Transitions)
        {
            if (NOT Trans.IsSubSmTransition) { continue; }
            auto* LocalSrc = GlobalToLocal.Find(Trans.SourceStateIndex);
            auto* LocalDst = GlobalToLocal.Find(Trans.TargetStateIndex);
            if (LocalSrc && LocalDst)
            { LocalTransitions.Add({ *LocalSrc, *LocalDst }); }
        }

        // Mini Sugiyama: BFS layer assignment
        auto LocalRanks = TArray<int32>{};
        LocalRanks.SetNum(LocalCount);
        for (auto& R : LocalRanks) { R = -1; }

        // Start from index 0 (first sub-SM state, typically the initial state)
        LocalRanks[0] = 0;
        auto Queue = TArray<int32>{ 0 };
        for (auto Head = 0; Head < Queue.Num(); ++Head)
        {
            auto Cur = Queue[Head];
            for (auto& LT : LocalTransitions)
            {
                auto Neighbor = -1;
                if (LT.LocalSrc == Cur) { Neighbor = LT.LocalDst; }
                else if (LT.LocalDst == Cur) { Neighbor = LT.LocalSrc; }

                if (Neighbor >= 0 && Neighbor < LocalCount && LocalRanks[Neighbor] < 0)
                {
                    LocalRanks[Neighbor] = LocalRanks[Cur] + 1;
                    Queue.Add(Neighbor);
                }
            }
        }
        for (auto& R : LocalRanks) { if (R < 0) { R = 0; } }

        auto LocalMaxRank = 0;
        for (auto R : LocalRanks) { LocalMaxRank = FMath::Max(LocalMaxRank, R); }

        // Mini Sugiyama: coordinate assignment (skip full dummy/crossing for internal layouts)
        auto NodesPerRank = TArray<int32>{};
        NodesPerRank.SetNumZeroed(LocalMaxRank + 1);
        for (auto R : LocalRanks) { ++NodesPerRank[R]; }

        auto SlotCounter = TArray<int32>{};
        SlotCounter.SetNumZeroed(LocalMaxRank + 1);

        auto InternalOffsets = TMap<int32, FVector2D>{};
        auto MaxX = 0.0f;
        auto MaxY = 0.0f;

        for (auto LocalIdx = 0; LocalIdx < LocalCount; ++LocalIdx)
        {
            auto R = LocalRanks[LocalIdx];
            auto Slot = SlotCounter[R]++;
            auto Count = NodesPerRank[R];

            auto X = static_cast<float>(R * InternalSpacingX) + CompoundPadding;
            auto TotalSlotHeight = (Count - 1) * InternalSpacingY;
            auto Y = static_cast<float>(Slot * InternalSpacingY) - TotalSlotHeight / 2.0f
                + CompoundHeaderHeight + CompoundPadding;

            auto GlobalIdx = SubIndices[LocalIdx];
            InternalOffsets.Add(GlobalIdx, FVector2D(X, Y));

            MaxX = FMath::Max(MaxX, X + 140.0f);  // approximate node width
            MaxY = FMath::Max(MaxY, Y + 40.0f);   // approximate node height
        }

        // Compute compound block dimensions
        auto BlockWidth = MaxX + CompoundPadding;
        auto BlockHeight = MaxY + CompoundPadding;

        // Determine label (parent task name that owns the sub-SM)
        auto Label = FString{};
        if (ParentIdx >= 0 && ParentIdx < StateCount)
        {
            for (auto& Task : SmInfo.States[ParentIdx].Tasks)
            {
                if (Task.HasSubStateMachine)
                {
                    Label = Task.ClassName;
                    break;
                }
            }
            if (Label.IsEmpty())
            { Label = SmInfo.States[ParentIdx].StateName; }
        }

        CompoundBlocks.Add({
            ParentIdx,
            Label,
            SubIndices,
            BlockWidth,
            BlockHeight,
            MoveTemp(InternalOffsets)
        });
    }

    // Sort compound blocks by parent state index for deterministic placement order.
    // Without this, TMap iteration order changes when sub-SMs activate/deactivate,
    // causing compound blocks to jump positions.
    CompoundBlocks.Sort([](const FCompoundBlock& A, const FCompoundBlock& B)
    { return A.ParentStateIndex < B.ParentStateIndex; });

    // ================================================================================================================
    // Phases 1-4: Sugiyama layout via shared utility (parent-level states only)
    // ================================================================================================================

    auto ParentCount = ParentStateIndices.Num();
    auto CompoundCount = CompoundBlocks.Num();

    auto ParentGlobalToLayout = TMap<int32, int32>{};
    for (auto i = 0; i < ParentCount; ++i)
    { ParentGlobalToLayout.Add(ParentStateIndices[i], i); }

    // Build layout nodes (using layout indices 0..N-1). Feed each node's
    // estimated width to the shared layout so compound blocks (sub-SMs) and
    // wide state names don't overlap into the next column.
    auto LayoutNodesInput = TArray<FCkDebugGraphLayoutNode>{};
    for (auto i = 0; i < ParentCount; ++i)
    {
        auto Layout = FCkDebugGraphLayoutNode{};
        Layout.Index = i;

        // Default width: max(140, stateName.Len() * 7.5 + 40) — matches the
        // approximate node-width constant used elsewhere in this file.
        auto Width = 140;
        const auto ParentIdx = ParentStateIndices[i];
        if (ParentIdx >= 0 && ParentIdx < StateCount)
        {
            const auto NameLen = SmInfo.States[ParentIdx].StateName.Len();
            Width = FMath::Max(Width, static_cast<int32>(NameLen * 7.5f) + 40);
        }

        // If this parent state owns a compound block (sub-SM), use the pre-
        // computed block width instead — it's already the visual footprint.
        for (const auto& Block : CompoundBlocks)
        {
            if (Block.ParentStateIndex == ParentIdx)
            {
                Width = FMath::Max(Width, static_cast<int32>(Block.Width));
                break;
            }
        }

        Layout.EstimatedWidth = Width;
        LayoutNodesInput.Add(Layout);
    }

    // Build layout edges from parent-level transitions
    auto LayoutEdgesInput = TArray<FCkDebugGraphLayoutEdge>{};
    for (auto& Trans : SmInfo.Transitions)
    {
        if (Trans.IsSubSmTransition) { continue; }

        auto* LayoutSrc = ParentGlobalToLayout.Find(Trans.SourceStateIndex);
        auto* LayoutDst = ParentGlobalToLayout.Find(Trans.TargetStateIndex);
        if (NOT LayoutSrc || NOT LayoutDst) { continue; }
        if (*LayoutSrc == *LayoutDst) { continue; }

        LayoutEdgesInput.Add({ *LayoutSrc, *LayoutDst });
    }

    // Find initial state for BFS root
    auto InitialLayoutIdx = static_cast<int32>(INDEX_NONE);
    for (auto i = 0; i < ParentCount; ++i)
    {
        if (SmInfo.States[ParentStateIndices[i]].StateClass == SmInfo.InitialStateClass)
        { InitialLayoutIdx = i; break; }
    }
    if (InitialLayoutIdx == INDEX_NONE && ParentCount > 0) { InitialLayoutIdx = 0; }

    auto SharedLayoutParams = FCkDebugGraphLayoutParams{};
    SharedLayoutParams.SpacingX = LayoutParams.SpacingX;
    SharedLayoutParams.SpacingY = LayoutParams.SpacingY;
    SharedLayoutParams.CrossingReductionPasses = LayoutParams.CrossingReductionPasses;
    SharedLayoutParams.IsDirectedBFS = NOT LayoutParams.UndirectedBFS;
    SharedLayoutParams.InitialNodeIndex = InitialLayoutIdx;

    auto LayoutResult = FCkDebugGraphLayout::ComputeLayout(LayoutNodesInput, LayoutEdgesInput, SharedLayoutParams);

    auto ParentPosX = TArray<int32>{};
    auto ParentPosY = TArray<int32>{};
    ParentPosX.SetNum(ParentCount);
    ParentPosY.SetNum(ParentCount);

    for (auto i = 0; i < ParentCount; ++i)
    {
        auto* Pos = LayoutResult.Positions.Find(i);
        if (Pos)
        {
            ParentPosX[i] = Pos->X;
            ParentPosY[i] = Pos->Y;
        }
    }

    // ================================================================================================================
    // Phase 5: Compute final state positions
    // Parent states get their Sugiyama positions.
    // Compound blocks are placed to the right of their owning parent state.
    // Sub-SM states are offset within their compound block.
    // ================================================================================================================

    // Build final PosX/PosY for all states
    auto PosX = TArray<int32>{};
    auto PosY = TArray<int32>{};
    PosX.SetNum(StateCount);
    PosY.SetNum(StateCount);

    for (auto i = 0; i < ParentCount; ++i)
    {
        auto GlobalIdx = ParentStateIndices[i];
        PosX[GlobalIdx] = ParentPosX[i];
        PosY[GlobalIdx] = ParentPosY[i];
    }

    // Place compound blocks below the entire parent graph, X-aligned with their
    // parent state. Each block starts at the same Y row. If two blocks overlap
    // horizontally, the later one is pushed down to a new row.
    if (CompoundCount > 0)
    {
        auto ParentMaxY = INT_MIN;
        for (auto i = 0; i < ParentCount; ++i)
        { ParentMaxY = FMath::Max(ParentMaxY, ParentPosY[i]); }

        constexpr auto EstimatedNodeHeight = 50;
        auto BaseCompoundY = ParentMaxY + EstimatedNodeHeight + LayoutParams.SpacingY;

        // Each placed block occupies an X range. Track them to detect horizontal overlap.
        struct FPlacedBlock { int32 X; int32 Width; int32 Y; int32 Height; };
        auto PlacedBlocks = TArray<FPlacedBlock>{};

        for (auto CompIdx = 0; CompIdx < CompoundCount; ++CompIdx)
        {
            auto& Block = CompoundBlocks[CompIdx];
            auto* ParentLayoutIdx = ParentGlobalToLayout.Find(Block.ParentStateIndex);
            if (NOT ParentLayoutIdx) { continue; }

            auto BlockX = ParentPosX[*ParentLayoutIdx];
            auto BlockW = static_cast<int32>(Block.Width);
            auto BlockH = static_cast<int32>(Block.Height);

            // Find the lowest Y where this block doesn't overlap any already-placed block
            auto BlockY = BaseCompoundY;
            for (auto& Placed : PlacedBlocks)
            {
                auto HorizontalOverlap = BlockX < (Placed.X + Placed.Width + LayoutParams.SpacingX)
                    && (BlockX + BlockW) > (Placed.X - LayoutParams.SpacingX);

                if (HorizontalOverlap)
                { BlockY = FMath::Max(BlockY, Placed.Y + Placed.Height + LayoutParams.SpacingY); }
            }

            PlacedBlocks.Add({ BlockX, BlockW, BlockY, BlockH });

            Block.LayoutPosX = BlockX;
            Block.LayoutPosY = BlockY;

            for (auto GlobalIdx : Block.SubSmStateIndices)
            {
                auto* Offset = Block.InternalOffsets.Find(GlobalIdx);
                if (Offset)
                {
                    PosX[GlobalIdx] = BlockX + static_cast<int32>(Offset->X);
                    PosY[GlobalIdx] = BlockY + static_cast<int32>(Offset->Y);
                }
            }
        }
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Create state nodes (all states — parent and sub-SM)
    // ----------------------------------------------------------------------------------------------------------------

    auto StateNodes = TArray<UCkSmDebugNode_State*>{};
    StateNodes.SetNum(StateCount);

    for (auto i = 0; i < StateCount; ++i)
    {
        auto& State = SmInfo.States[i];

        auto StateNode = NewObject<UCkSmDebugNode_State>(this);
        StateNode->PopulateFromStateInfo(State, i);
        StateNode->NodePosX = PosX[i];
        StateNode->NodePosY = PosY[i];
        StateNode->AllocateDefaultPins();

        StateNode->SetFlags(RF_Transactional);
        AddNode(StateNode, /*bFromUI=*/ false, /*bSelectNewNode=*/ false);
        StateNodes[i] = StateNode;
    }

    // Set parent-active state on sub-SM nodes (for fade effect)
    for (auto& Block : CompoundBlocks)
    {
        auto ParentIsActive = Block.ParentStateIndex >= 0
            && Block.ParentStateIndex < StateCount
            && SmInfo.States[Block.ParentStateIndex].IsCurrentState;

        for (auto SubIdx : Block.SubSmStateIndices)
        {
            if (SubIdx >= 0 && SubIdx < StateCount && StateNodes[SubIdx])
            { StateNodes[SubIdx]->Set_IsParentStateActive(ParentIsActive); }
        }
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Create compound block nodes
    // ----------------------------------------------------------------------------------------------------------------

    for (auto CompIdx = 0; CompIdx < CompoundCount; ++CompIdx)
    {
        auto& Block = CompoundBlocks[CompIdx];

        auto CompoundNode = NewObject<UCkSmDebugNode_Compound>(this);
        CompoundNode->Populate(
            Block.ParentStateIndex,
            Block.Label,
            Block.Width,
            Block.Height,
            Block.SubSmStateIndices);

        CompoundNode->NodePosX = Block.LayoutPosX;
        CompoundNode->NodePosY = Block.LayoutPosY;
        CompoundNode->AllocateDefaultPins();

        // Check if any sub-SM state is the current state
        auto HasActive = false;
        for (auto GlobalIdx : Block.SubSmStateIndices)
        {
            if (GlobalIdx >= 0 && GlobalIdx < StateCount && SmInfo.States[GlobalIdx].IsCurrentState)
            { HasActive = true; break; }
        }
        CompoundNode->Set_HasActiveSubState(HasActive);

        auto ParentIsActive = Block.ParentStateIndex >= 0
            && Block.ParentStateIndex < StateCount
            && SmInfo.States[Block.ParentStateIndex].IsCurrentState;
        CompoundNode->Set_IsParentStateActive(ParentIsActive);

        CompoundNode->SetFlags(RF_Transactional);
        AddNode(CompoundNode, /*bFromUI=*/ false, /*bSelectNewNode=*/ false);

        // Wire: ParentState.Out → CompoundBlock.In (draws an arrow like any other transition)
        if (Block.ParentStateIndex >= 0 && Block.ParentStateIndex < StateNodes.Num())
        {
            auto* ParentState = StateNodes[Block.ParentStateIndex];
            if (ParentState)
            {
                auto ParentOutputPin = ParentState->Pins.IsValidIndex(1) ? ParentState->Pins[1] : nullptr;
                auto CompoundInputPin = CompoundNode->Pins.IsValidIndex(0) ? CompoundNode->Pins[0] : nullptr;
                if (ParentOutputPin && CompoundInputPin)
                { ParentOutputPin->MakeLinkTo(CompoundInputPin); }
            }
        }
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Create transition nodes wired in series: SourceState.Out → Transition.In → Transition.Out → TargetState.In
    // ----------------------------------------------------------------------------------------------------------------

    for (auto i = 0; i < SmInfo.Transitions.Num(); ++i)
    {
        auto& Transition = SmInfo.Transitions[i];
        auto SourceIdx = Transition.SourceStateIndex;
        auto TargetIdx = Transition.TargetStateIndex;

        if (SourceIdx < 0 || SourceIdx >= StateNodes.Num()) { continue; }
        if (TargetIdx < 0 || TargetIdx >= StateNodes.Num()) { continue; }
        if (NOT StateNodes[SourceIdx] || NOT StateNodes[TargetIdx]) { continue; }

        auto TransitionNode = NewObject<UCkSmDebugNode_Transition>(this);
        TransitionNode->PopulateFromTransitionInfo(Transition, i);

        TransitionNode->NodePosX = (StateNodes[SourceIdx]->NodePosX + StateNodes[TargetIdx]->NodePosX) / 2;
        TransitionNode->NodePosY = (StateNodes[SourceIdx]->NodePosY + StateNodes[TargetIdx]->NodePosY) / 2;
        TransitionNode->AllocateDefaultPins();

        TransitionNode->SetFlags(RF_Transactional);
        AddNode(TransitionNode, /*bFromUI=*/ false, /*bSelectNewNode=*/ false);

        auto SourceOutputPin = StateNodes[SourceIdx]->Pins.IsValidIndex(1) ? StateNodes[SourceIdx]->Pins[1] : nullptr;
        auto TransInputPin = TransitionNode->Pins.IsValidIndex(0) ? TransitionNode->Pins[0] : nullptr;
        if (SourceOutputPin && TransInputPin)
        { SourceOutputPin->MakeLinkTo(TransInputPin); }

        auto TransOutputPin = TransitionNode->Pins.IsValidIndex(1) ? TransitionNode->Pins[1] : nullptr;
        auto TargetInputPin = StateNodes[TargetIdx]->Pins.IsValidIndex(0) ? StateNodes[TargetIdx]->Pins[0] : nullptr;
        if (TransOutputPin && TargetInputPin)
        { TransOutputPin->MakeLinkTo(TargetInputPin); }
    }

    // Compute "fully event-driven" per state (all outgoing transition conditions
    // are EventDriven AND no task ticks). Tasks-only flag is computed inside the
    // state node during PopulateFromStateInfo; this pass folds in transition data.
    for (auto StateIdx = 0; StateIdx < StateNodes.Num(); ++StateIdx)
    {
        auto* Node = StateNodes[StateIdx];
        if (ck::Is_NOT_Valid(Node)) { continue; }

        if (NOT Node->Get_HasCompleteData())
        {
            Node->Set_IsFullyEventDriven(false);
            continue;
        }

        auto AllOutgoingEventDriven = true;
        for (const auto& Trans : SmInfo.Transitions)
        {
            if (Trans.SourceStateIndex != StateIdx) { continue; }
            for (const auto& Cond : Trans.Conditions)
            {
                if (Cond.Mode != ECk_SmConditionMode::EventDriven)
                { AllOutgoingEventDriven = false; break; }
            }
            if (NOT AllOutgoingEventDriven) { break; }
        }

        Node->Set_IsFullyEventDriven(NOT Node->Get_HasAnyTickingTask() && AllOutgoingEventDriven);
    }

    _TopologyHash = ComputeTopologyHash(SmInfo);

    SetSuppressNotifications(false);
    NotifyGraphChanged();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugGraph::
    UpdateFromSmInfo(
        const FCkSmDebugger_SmInfo& InSmInfo)
    -> void
{
    // Build augmented SmInfo with cached sub-SM data (same as RebuildFromSmInfo)
    auto SmInfo = InSmInfo;

    auto ParentsWithLiveSubSm = TSet<int32>{};
    for (auto& State : SmInfo.States)
    {
        if (State.IsSubSmNode)
        { ParentsWithLiveSubSm.Add(State.SubSmParentStateIndex); }
    }

    for (auto i = 0; i < InSmInfo.States.Num(); ++i)
    {
        if (InSmInfo.States[i].IsSubSmNode) { continue; }
        if (ParentsWithLiveSubSm.Contains(i)) { continue; }

        auto* Cached = _CachedSubSmData.Find(i);
        if (NOT Cached || Cached->States.Num() == 0) { continue; }

        auto IndexOffset = SmInfo.States.Num();
        for (auto& CachedState : Cached->States)
        {
            auto InjectedState = CachedState;
            InjectedState.IsCurrentState = false;
            // Preserve cached HasBeenVisited / DwellTimeSeconds — they hold the
            // last-known values from when the sub-SM was live, which is exactly
            // what the "Was active for…" popup wants to show.
            InjectedState.IsSubSmNode = true;
            InjectedState.SubSmParentStateIndex = i;
            InjectedState.SubSmParentStateName = SmInfo.States[i].StateName;
            SmInfo.States.Add(MoveTemp(InjectedState));
        }
        for (auto& CachedTrans : Cached->Transitions)
        {
            auto InjectedTrans = CachedTrans;
            InjectedTrans.SourceStateIndex += IndexOffset;
            InjectedTrans.TargetStateIndex += IndexOffset;
            InjectedTrans.IsSubSmTransition = true;
            InjectedTrans.AreAllConditionsSatisfied = false;
            SmInfo.Transitions.Add(MoveTemp(InjectedTrans));
        }
    }

    auto NewHash = ComputeTopologyHash(SmInfo);
    if (NewHash != _TopologyHash)
    {
        RebuildFromSmInfo(InSmInfo);
        return;
    }

    _TransitionData = SmInfo.Transitions;

    for (auto Node : Nodes)
    {
        if (auto StateNode = Cast<UCkSmDebugNode_State>(Node))
        {
            auto Idx = StateNode->Get_StateIndex();
            if (Idx >= 0 && Idx < SmInfo.States.Num())
            {
                StateNode->UpdateRuntimeData(SmInfo.States[Idx]);

                if (NOT StateNode->Get_HasCompleteData())
                {
                    StateNode->Set_IsFullyEventDriven(false);
                }
                else
                {
                    auto AllOutgoingEventDriven = true;
                    for (const auto& Trans : SmInfo.Transitions)
                    {
                        if (Trans.SourceStateIndex != Idx) { continue; }
                        for (const auto& Cond : Trans.Conditions)
                        {
                            if (Cond.Mode != ECk_SmConditionMode::EventDriven)
                            { AllOutgoingEventDriven = false; break; }
                        }
                        if (NOT AllOutgoingEventDriven) { break; }
                    }
                    StateNode->Set_IsFullyEventDriven(NOT StateNode->Get_HasAnyTickingTask() && AllOutgoingEventDriven);
                }
            }
        }
        else if (auto TransitionNode = Cast<UCkSmDebugNode_Transition>(Node))
        {
            auto Idx = TransitionNode->Get_TransitionIndex();
            if (Idx >= 0 && Idx < SmInfo.Transitions.Num())
            { TransitionNode->UpdateRuntimeData(SmInfo.Transitions[Idx]); }
        }
        else if (auto CompoundNode = Cast<UCkSmDebugNode_Compound>(Node))
        {
            auto HasActive = false;
            for (auto SubIdx : CompoundNode->Get_SubSmStateIndices())
            {
                if (SubIdx >= 0 && SubIdx < SmInfo.States.Num() && SmInfo.States[SubIdx].IsCurrentState)
                { HasActive = true; break; }
            }
            CompoundNode->Set_HasActiveSubState(HasActive);

            auto ParentIdx = CompoundNode->Get_ParentStateIndex();
            auto ParentIsActive = ParentIdx >= 0
                && ParentIdx < SmInfo.States.Num()
                && SmInfo.States[ParentIdx].IsCurrentState;

            CompoundNode->Set_IsParentStateActive(ParentIsActive);

            // Update parent-active state on sub-SM nodes (for fade effect)

            for (auto SubIdx : CompoundNode->Get_SubSmStateIndices())
            {
                if (auto* SubNode = FindStateNode(SubIdx))
                { SubNode->Set_IsParentStateActive(ParentIsActive); }
            }
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugGraph::
    FindStateNode(
        int32 InStateIndex) const
    -> UCkSmDebugNode_State*
{
    for (auto Node : Nodes)
    {
        if (auto* StateNode = Cast<UCkSmDebugNode_State>(Node))
        {
            if (StateNode->Get_StateIndex() == InStateIndex)
            { return StateNode; }
        }
    }
    return nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugGraph::
    FindTransitionNode(
        int32 InTransitionIndex) const
    -> UCkSmDebugNode_Transition*
{
    for (auto Node : Nodes)
    {
        if (auto* TransNode = Cast<UCkSmDebugNode_Transition>(Node))
        {
            if (TransNode->Get_TransitionIndex() == InTransitionIndex)
            { return TransNode; }
        }
    }
    return nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugGraph::
    FindCompoundNode(
        int32 InParentStateIndex) const
    -> UCkSmDebugNode_Compound*
{
    for (auto Node : Nodes)
    {
        if (auto* CompNode = Cast<UCkSmDebugNode_Compound>(Node))
        {
            if (CompNode->Get_ParentStateIndex() == InParentStateIndex)
            { return CompNode; }
        }
    }
    return nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugGraph::
    FindTransitionBetween(
        int32 InSourceIdx,
        int32 InTargetIdx) const
    -> const FCkSmDebugger_TransitionInfo*
{
    for (auto& Trans : _TransitionData)
    {
        if (Trans.SourceStateIndex == InSourceIdx && Trans.TargetStateIndex == InTargetIdx)
        { return &Trans; }
    }
    return nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugGraph::
    ComputeTopologyHash(
        const FCkSmDebugger_SmInfo& InSmInfo) const
    -> uint32
{
    auto Hash = HashCombine(
        GetTypeHash(InSmInfo.States.Num()),
        GetTypeHash(InSmInfo.Transitions.Num()));

    for (auto& State : InSmInfo.States)
    { Hash = HashCombine(Hash, GetTypeHash(State.StateName)); }

    for (auto& Transition : InSmInfo.Transitions)
    {
        Hash = HashCombine(Hash, GetTypeHash(Transition.SourceStateIndex));
        Hash = HashCombine(Hash, GetTypeHash(Transition.TargetStateIndex));
    }

    return Hash;
}

// --------------------------------------------------------------------------------------------------------------------
// Scrub + Live highlight
// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugGraph::
    ApplyScrubHighlight(
        int32 InActiveStateIdx,
        int32 InExitedStateIdx)
    -> void
{
    for (auto Node : Nodes)
    {
        if (auto* StateNode = Cast<UCkSmDebugNode_State>(Node))
        {
            auto Idx = StateNode->Get_StateIndex();
            StateNode->Set_IsScrubActiveState(Idx == InActiveStateIdx);
            StateNode->Set_IsScrubExitedState(Idx == InExitedStateIdx);
            // Scrub mode shows the scrubbed-active state via its own (green)
            // OnPaint glow — clear the live current-state glow so a stale live
            // highlight doesn't bleed blue onto a node while scrubbing.
            StateNode->Set_BorderGlowAlpha(0.0f);
            StateNode->Set_CellGlowAlpha(0.0f);
            StateNode->Set_PreviousGlowAlpha(0.0f);
            // Keep the entry latch synced to live current-state during scrub so
            // returning to live mode doesn't fire a spurious pulse, and clear any
            // in-flight pulse.
            StateNode->Set_EntryPulseAlpha(0.0f);
            StateNode->Set_WasCurrentState(StateNode->Get_IsCurrentState());
        }
        else if (auto* TransNode = Cast<UCkSmDebugNode_Transition>(Node))
        {
            auto IsHighlighted =
                TransNode->Get_TransitionIndex() >= 0
                && InExitedStateIdx >= 0
                && InActiveStateIdx >= 0;

            if (IsHighlighted)
            {
                auto FoundTrans = FindTransitionBetween(InExitedStateIdx, InActiveStateIdx);
                IsHighlighted = FoundTrans
                    && FoundTrans->SourceStateIndex == InExitedStateIdx
                    && FoundTrans->TargetStateIndex == InActiveStateIdx
                    && TransNode->Get_TransitionIndex() == (FoundTrans - _TransitionData.GetData());
            }

            TransNode->Set_IsScrubHighlighted(IsHighlighted);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugGraph::
    ClearScrubHighlight()
    -> void
{
    for (auto Node : Nodes)
    {
        if (auto* StateNode = Cast<UCkSmDebugNode_State>(Node))
        {
            StateNode->Set_IsScrubActiveState(false);
            StateNode->Set_IsScrubExitedState(false);
            StateNode->Set_IsPreviousState(false);
        }
        else if (auto* TransNode = Cast<UCkSmDebugNode_Transition>(Node))
        {
            TransNode->Set_IsScrubHighlighted(false);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugGraph::
    TickLiveFlash(
        float InDeltaTime,
        int32 InPrevStateIdx,
        int32 InCurrentStateIdx,
        const TSet<FString>& InPreviousStateNames)
    -> void
{
    // On state change: find the transition that fired and flash it
    if (InPrevStateIdx >= 0
        && InCurrentStateIdx >= 0
        && InPrevStateIdx != InCurrentStateIdx)
    {
        for (auto i = 0; i < _TransitionData.Num(); ++i)
        {
            if (_TransitionData[i].SourceStateIndex == InPrevStateIdx
                && _TransitionData[i].TargetStateIndex == InCurrentStateIdx)
            {
                if (auto* TransNode = FindTransitionNode(i))
                { TransNode->Set_LiveFlashAlpha(1.0f); }
                break;
            }
        }
    }

    // Mark previous states by name. The set carries one entry per hierarchy
    // level (outer SM + each sub-SM) — the most recent FromStateName at that
    // level. Exclude nodes that are themselves currently active so a recent
    // self-loop doesn't light up the current state.
    //
    // Drive the live highlight glows in the same pass (read every frame by the
    // pill + OnPaint, no widget rebuild). Channels animate asymmetrically:
    //   - Becoming current: fade the whole cell IN — border grey -> blue AND
    //     cell dim -> full.
    //   - Once left: fade ONLY the border back out (blue -> grey); the cell
    //     HOLDS its brightness, so leaving a state animates the outline, not the
    //     whole node. (Unvisited nodes stay dim until first reached.)
    //   - Grey previous-state glow fades in/out BOTH ways as the node enters /
    //     leaves the previous-state set (instead of snapping).
    const auto FadeStep = FCkSmDebuggerStyle::Sm_HighlightFadeDuration > 0.0f
        ? InDeltaTime / FCkSmDebuggerStyle::Sm_HighlightFadeDuration : 1.0f;
    for (auto Node : Nodes)
    {
        if (auto* StateNode = Cast<UCkSmDebugNode_State>(Node))
        {
            const auto bIsPrevious =
                InPreviousStateNames.Contains(StateNode->Get_StateName())
                && NOT StateNode->Get_IsCurrentState();
            StateNode->Set_IsPreviousState(bIsPrevious);

            if (StateNode->Get_IsCurrentState())
            {
                StateNode->Set_BorderGlowAlpha(FMath::Min(1.0f, StateNode->Get_BorderGlowAlpha() + FadeStep));
                StateNode->Set_CellGlowAlpha(FMath::Min(1.0f, StateNode->Get_CellGlowAlpha() + FadeStep));
            }
            else
            {
                const auto Border = StateNode->Get_BorderGlowAlpha();
                if (Border > 0.0f)
                { StateNode->Set_BorderGlowAlpha(FMath::Max(0.0f, Border - FadeStep)); }
            }

            const auto PrevTarget = bIsPrevious ? 1.0f : 0.0f;
            const auto Prev = StateNode->Get_PreviousGlowAlpha();
            if (Prev < PrevTarget)
            { StateNode->Set_PreviousGlowAlpha(FMath::Min(PrevTarget, Prev + FadeStep)); }
            else if (Prev > PrevTarget)
            { StateNode->Set_PreviousGlowAlpha(FMath::Max(PrevTarget, Prev - FadeStep)); }

            // One-shot entry pulse: fire on the false -> true current edge (covers
            // sub-SM states too, since they are state nodes), then decay. Drives
            // the entry overshoot — a brief brightening of the border colour.
            const auto bCurrentNow = StateNode->Get_IsCurrentState();
            if (bCurrentNow && NOT StateNode->Get_WasCurrentState())
            { StateNode->Set_EntryPulseAlpha(1.0f); }
            StateNode->Set_WasCurrentState(bCurrentNow);

            const auto Pulse = StateNode->Get_EntryPulseAlpha();
            if (Pulse > 0.0f)
            {
                const auto PulseStep = FCkSmDebuggerStyle::Sm_EntryPulseDuration > 0.0f
                    ? InDeltaTime / FCkSmDebuggerStyle::Sm_EntryPulseDuration : 1.0f;
                StateNode->Set_EntryPulseAlpha(FMath::Max(0.0f, Pulse - PulseStep));
            }
        }
    }

    // Decay all flash alphas
    constexpr auto FlashDuration = 0.5f;
    for (auto Node : Nodes)
    {
        if (auto* TransNode = Cast<UCkSmDebugNode_Transition>(Node))
        {
            auto Alpha = TransNode->Get_LiveFlashAlpha();
            if (Alpha > 0.0f)
            {
                Alpha -= InDeltaTime / FlashDuration;
                TransNode->Set_LiveFlashAlpha(FMath::Max(0.0f, Alpha));
            }
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Mockup
// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugGraph::
    BuildMockup()
    -> void
{
    Nodes.Empty();
    _TransitionData.Empty();

    // ----------------------------------------------------------------------------------------------------------------
    // State definitions
    // ----------------------------------------------------------------------------------------------------------------

    struct FMockState
    {
        FString      Name;
        bool         IsActive;
        FLinearColor Color;
        float        DwellTime;
    };

    auto States = TArray<FMockState>
    {
        { TEXT("Idle"),          false, FLinearColor(0.30f, 0.60f, 0.30f),  0.0f  },  // 0
        { TEXT("Walking"),       false, FLinearColor(0.30f, 0.50f, 0.80f),  0.0f  },  // 1
        { TEXT("IdleJump"),      false, FLinearColor(0.80f, 0.40f, 0.30f),  0.0f  },  // 2
        { TEXT("IdleLanding"),   true,  FLinearColor(0.85f, 0.55f, 0.25f), 14.44f },  // 3 — active
        { TEXT("CrouchIdle"),    false, FLinearColor(0.50f, 0.30f, 0.70f),  0.0f  },  // 4
        { TEXT("IdleToCrouch"),  false, FLinearColor(0.70f, 0.60f, 0.30f),  0.0f  },  // 5
        { TEXT("CrouchToIdle"),  false, FLinearColor(0.30f, 0.70f, 0.60f),  0.0f  },  // 6
    };

    struct FMockTrans { int32 Src; int32 Dst; };

    auto Transitions = TArray<FMockTrans>
    {
        { 0, 1 }, { 1, 0 },   // Idle ↔ Walking
        { 0, 2 }, { 2, 0 },   // Idle ↔ IdleJump
        { 3, 0 },             // IdleLanding → Idle
        { 0, 5 }, { 5, 0 },   // Idle ↔ IdleToCrouch
        { 0, 6 }, { 6, 0 },   // Idle ↔ CrouchToIdle
        { 5, 4 }, { 4, 5 },   // IdleToCrouch ↔ CrouchIdle
        { 4, 6 }, { 6, 4 },   // CrouchIdle ↔ CrouchToIdle
    };

    // ----------------------------------------------------------------------------------------------------------------
    // BFS-ranked layout — Entry on the left, Idle next, then outward layers
    // ----------------------------------------------------------------------------------------------------------------

    auto StateCount = States.Num();
    auto InitialIdx = 0;  // Idle

    auto Ranks = TArray<int32>{};
    Ranks.SetNum(StateCount);
    for (auto& R : Ranks) { R = -1; }
    Ranks[InitialIdx] = 0;

    auto Queue = TArray<int32>{ InitialIdx };
    for (auto Head = 0; Head < Queue.Num(); ++Head)
    {
        auto Current = Queue[Head];
        for (auto& T : Transitions)
        {
            auto Neighbor = -1;
            if (T.Src == Current && T.Dst != Current) { Neighbor = T.Dst; }
            else if (T.Dst == Current && T.Src != Current) { Neighbor = T.Src; }

            if (Neighbor >= 0 && Neighbor < StateCount && Ranks[Neighbor] < 0)
            {
                Ranks[Neighbor] = Ranks[Current] + 1;
                Queue.Add(Neighbor);
            }
        }
    }

    for (auto& R : Ranks) { if (R < 0) { R = 0; } }

    auto MaxRank = 0;
    for (auto R : Ranks) { MaxRank = FMath::Max(MaxRank, R); }

    auto NodesPerRank = TArray<int32>{};
    NodesPerRank.SetNumZeroed(MaxRank + 1);
    for (auto R : Ranks) { ++NodesPerRank[R]; }

    constexpr auto SpacingX = 250;
    constexpr auto SpacingY = 100;

    auto SlotCounter = TArray<int32>{};
    SlotCounter.SetNumZeroed(MaxRank + 1);

    auto StatePositions = TArray<FIntPoint>{};
    StatePositions.SetNum(StateCount);

    for (auto i = 0; i < StateCount; ++i)
    {
        auto R = Ranks[i];
        auto Slot = SlotCounter[R]++;
        auto Count = NodesPerRank[R];

        auto X = (R + 1) * SpacingX;
        auto TotalHeight = (Count - 1) * SpacingY;
        auto Y = Slot * SpacingY - TotalHeight / 2;

        StatePositions[i] = FIntPoint(X, Y);
    }

    auto EntryPos = FIntPoint(0, StatePositions[InitialIdx].Y);

    // ----------------------------------------------------------------------------------------------------------------
    // Create entry node
    // ----------------------------------------------------------------------------------------------------------------

    auto* EntryNode = NewObject<UCkSmDebugNode_Entry>(this);
    EntryNode->NodePosX = EntryPos.X;
    EntryNode->NodePosY = EntryPos.Y;
    EntryNode->AllocateDefaultPins();
    EntryNode->SetFlags(RF_Transactional);
    AddNode(EntryNode, /*bFromUI=*/ false, /*bSelectNewNode=*/ false);

    // ----------------------------------------------------------------------------------------------------------------
    // Create state nodes
    // ----------------------------------------------------------------------------------------------------------------

    auto StateNodes = TArray<UCkSmDebugNode_State*>{};
    StateNodes.Reserve(StateCount);

    for (auto i = 0; i < StateCount; ++i)
    {
        auto& S = States[i];

        auto* StateNode = NewObject<UCkSmDebugNode_State>(this);
        StateNode->InitMockup(S.Name, S.IsActive, S.Color, i, S.DwellTime);
        StateNode->NodePosX = StatePositions[i].X;
        StateNode->NodePosY = StatePositions[i].Y;
        StateNode->AllocateDefaultPins();
        StateNode->SetFlags(RF_Transactional);
        AddNode(StateNode, /*bFromUI=*/ false, /*bSelectNewNode=*/ false);
        StateNodes.Add(StateNode);
    }

    // Mockup breakpoints — demonstrate the chosen styles (State=23, Transition=5)
    StateNodes[0]->ToggleEntryBreakpoint();   // Idle — entry BP
    StateNodes[1]->ToggleExitBreakpoint();    // Walking — exit BP
    StateNodes[4]->ToggleEntryBreakpoint();   // CrouchIdle — both BPs
    StateNodes[4]->ToggleExitBreakpoint();

    // Mockup highlight states — demonstrate glow style
    // State 3 (IdleLanding) is already IsActive=true → orange body tint (current state).
    StateNodes[1]->Set_IsScrubActiveState(true);   // Walking — green glow (scrub active)
    StateNodes[2]->Set_IsPreviousState(true);       // IdleJump — grey glow (previous state)

    // ----------------------------------------------------------------------------------------------------------------
    // Entry → Idle (direct)
    // ----------------------------------------------------------------------------------------------------------------

    {
        auto* EntryOut = (EntryNode->Pins.Num() > 0) ? EntryNode->Pins[0] : nullptr;
        auto* IdleIn   = (StateNodes[0]->Pins.Num() > 0) ? StateNodes[0]->Pins[0] : nullptr;
        if (EntryOut && IdleIn)
        { EntryOut->MakeLinkTo(IdleIn); }
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Create transition nodes
    // ----------------------------------------------------------------------------------------------------------------

    for (auto i = 0; i < Transitions.Num(); ++i)
    {
        auto& T = Transitions[i];

        auto* TransNode = NewObject<UCkSmDebugNode_Transition>(this);
        TransNode->InitMockup(States[T.Src].Name, States[T.Dst].Name, i, T.Src, T.Dst);

        TransNode->NodePosX = (StateNodes[T.Src]->NodePosX + StateNodes[T.Dst]->NodePosX) / 2;
        TransNode->NodePosY = (StateNodes[T.Src]->NodePosY + StateNodes[T.Dst]->NodePosY) / 2;
        TransNode->AllocateDefaultPins();
        TransNode->SetFlags(RF_Transactional);
        AddNode(TransNode, /*bFromUI=*/ false, /*bSelectNewNode=*/ false);

        auto* SourceOut = (StateNodes[T.Src]->Pins.Num() > 1)
            ? StateNodes[T.Src]->Pins[1] : nullptr;
        auto* TransIn = (TransNode->Pins.Num() > 0)
            ? TransNode->Pins[0] : nullptr;
        if (SourceOut && TransIn)
        { SourceOut->MakeLinkTo(TransIn); }

        auto* TransOut  = (TransNode->Pins.Num() > 1)
            ? TransNode->Pins[1] : nullptr;
        auto* TargetIn  = (StateNodes[T.Dst]->Pins.Num() > 0)
            ? StateNodes[T.Dst]->Pins[0] : nullptr;
        if (TransOut && TargetIn)
        { TransOut->MakeLinkTo(TargetIn); }
    }

    // Set a distinct hash so exiting test mode triggers a rebuild
    _TopologyHash = 0xDEADBEEF;
}

// --------------------------------------------------------------------------------------------------------------------
