#include "CkSmDebugGraph.h"

#include "CkSmDebugNode_Entry.h"
#include "CkSmDebugNode_State.h"
#include "CkSmDebugNode_Transition.h"
#include "CkSmDebugGraphSchema.h"
#include "CkCore/Macros/CkMacros.h"

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
    _TransitionData = InSmInfo.Transitions;

    // ================================================================================================================
    // Sugiyama layered graph drawing — full pipeline
    // Phase 1: Layer assignment (BFS)
    // Phase 2: Dummy vertex insertion (for edges spanning multiple ranks)
    // Phase 3: Barycenter crossing reduction (with dummies for proper long-edge handling)
    // Phase 4: Coordinate assignment
    // ================================================================================================================

    auto StateCount = InSmInfo.States.Num();

    // ----------------------------------------------------------------------------------------------------------------
    // Phase 1: Layer assignment via BFS
    // ----------------------------------------------------------------------------------------------------------------

    auto Ranks = TArray<int32>{};
    Ranks.SetNum(StateCount);
    for (auto& R : Ranks) { R = -1; }

    auto InitialIdx = -1;
    for (auto i = 0; i < StateCount; ++i)
    {
        if (InSmInfo.States[i].StateClass == InSmInfo.InitialStateClass)
        { InitialIdx = i; break; }
    }
    if (InitialIdx < 0 && StateCount > 0) { InitialIdx = 0; }

    if (InitialIdx >= 0)
    {
        auto Queue = TArray<int32>{};
        Queue.Add(InitialIdx);
        Ranks[InitialIdx] = 0;

        for (auto Head = 0; Head < Queue.Num(); ++Head)
        {
            auto Current = Queue[Head];
            for (auto& Trans : InSmInfo.Transitions)
            {
                auto Neighbor = -1;

                if (LayoutParams.bUndirectedBFS)
                {
                    if (Trans.SourceStateIndex == Current) { Neighbor = Trans.TargetStateIndex; }
                    else if (Trans.TargetStateIndex == Current) { Neighbor = Trans.SourceStateIndex; }
                }
                else
                {
                    if (Trans.SourceStateIndex == Current) { Neighbor = Trans.TargetStateIndex; }
                }

                if (Neighbor >= 0 && Neighbor < Ranks.Num() && Ranks[Neighbor] < 0)
                {
                    Ranks[Neighbor] = Ranks[Current] + 1;
                    Queue.Add(Neighbor);
                }
            }
        }
    }

    for (auto& R : Ranks) { if (R < 0) { R = 0; } }

    auto MaxRank = 0;
    for (auto R : Ranks) { MaxRank = FMath::Max(MaxRank, R); }

    // ----------------------------------------------------------------------------------------------------------------
    // Phase 2: Dummy vertex insertion
    // For edges spanning multiple ranks, insert virtual nodes at each intermediate rank.
    // This is critical for crossing reduction — without dummies, the barycenter heuristic
    // cannot detect crossings caused by long-distance edges passing through intermediate ranks.
    // ----------------------------------------------------------------------------------------------------------------

    // Layout node: OriginalIndex >= 0 means real state, -1 means dummy
    struct FLayoutNode { int32 OriginalIndex; int32 Rank; };

    auto LayoutNodes = TArray<FLayoutNode>{};
    LayoutNodes.Reserve(StateCount * 2);

    for (auto i = 0; i < StateCount; ++i)
    { LayoutNodes.Add({ i, Ranks[i] }); }

    // Layout edges (through dummies for multi-rank spans)
    struct FLayoutEdge { int32 From; int32 To; };
    auto LayoutEdges = TArray<FLayoutEdge>{};

    for (auto& Trans : InSmInfo.Transitions)
    {
        auto S = Trans.SourceStateIndex;
        auto T = Trans.TargetStateIndex;
        if (S < 0 || S >= StateCount || T < 0 || T >= StateCount || S == T) { continue; }

        auto RankS = Ranks[S];
        auto RankT = Ranks[T];
        if (RankS == RankT) { continue; }

        // Orient from lower rank to higher rank
        auto From = (RankS < RankT) ? S : T;
        auto To   = (RankS < RankT) ? T : S;
        auto FromRank = FMath::Min(RankS, RankT);
        auto ToRank   = FMath::Max(RankS, RankT);

        if (ToRank - FromRank == 1)
        {
            // Adjacent ranks — direct edge, no dummy needed
            LayoutEdges.Add({ From, To });
        }
        else
        {
            // Multi-rank span — insert dummy at each intermediate rank
            auto Prev = From;
            for (auto R = FromRank + 1; R < ToRank; ++R)
            {
                auto DummyIdx = LayoutNodes.Num();
                LayoutNodes.Add({ -1, R });
                LayoutEdges.Add({ Prev, DummyIdx });
                Prev = DummyIdx;
            }
            LayoutEdges.Add({ Prev, To });
        }
    }

    auto TotalLayoutNodes = LayoutNodes.Num();

    // Recompute MaxRank (dummies don't change it, but be safe)
    for (auto& N : LayoutNodes) { MaxRank = FMath::Max(MaxRank, N.Rank); }

    // ----------------------------------------------------------------------------------------------------------------
    // Phase 3: Barycenter crossing reduction
    // Operates on all nodes (real + dummy). Dummies at intermediate ranks let the
    // heuristic properly route long-distance edges and avoid crossings.
    // ----------------------------------------------------------------------------------------------------------------

    auto RankLayers = TArray<TArray<int32>>{};
    RankLayers.SetNum(MaxRank + 1);
    for (auto i = 0; i < TotalLayoutNodes; ++i)
    { RankLayers[LayoutNodes[i].Rank].Add(i); }

    // Undirected adjacency over layout edges
    auto Adj = TArray<TArray<int32>>{};
    Adj.SetNum(TotalLayoutNodes);
    for (auto& E : LayoutEdges)
    {
        Adj[E.From].AddUnique(E.To);
        Adj[E.To].AddUnique(E.From);
    }

    // Also add same-rank adjacency for states connected by same-rank edges
    for (auto& Trans : InSmInfo.Transitions)
    {
        auto S = Trans.SourceStateIndex;
        auto T = Trans.TargetStateIndex;
        if (S >= 0 && S < StateCount && T >= 0 && T < StateCount && Ranks[S] == Ranks[T])
        {
            Adj[S].AddUnique(T);
            Adj[T].AddUnique(S);
        }
    }

    auto Slots = TArray<float>{};
    Slots.SetNum(TotalLayoutNodes);
    for (auto R = 0; R <= MaxRank; ++R)
    {
        for (auto S = 0; S < RankLayers[R].Num(); ++S)
        { Slots[RankLayers[R][S]] = static_cast<float>(S); }
    }

    // Alternating sweeps
    for (auto Iter = 0; Iter < LayoutParams.CrossingReductionPasses; ++Iter)
    {
        auto bLTR  = (Iter % 2 == 0);
        auto Start = bLTR ? 1 : MaxRank - 1;
        auto End   = bLTR ? MaxRank + 1 : -1;
        auto Step  = bLTR ? 1 : -1;

        for (auto R = Start; R != End; R += Step)
        {
            auto& Layer = RankLayers[R];
            auto Weights = TArray<TPair<float, int32>>{};

            for (auto Idx : Layer)
            {
                auto Sum = 0.0f;
                auto Cnt = 0;
                for (auto Nbr : Adj[Idx])
                {
                    if (LayoutNodes[Nbr].Rank != R)
                    {
                        Sum += Slots[Nbr];
                        ++Cnt;
                    }
                }
                Weights.Add({ Cnt > 0 ? Sum / Cnt : Slots[Idx], Idx });
            }

            Weights.Sort([](const auto& A, const auto& B) { return A.Key < B.Key; });

            for (auto S = 0; S < Weights.Num(); ++S)
            {
                Layer[S] = Weights[S].Value;
                Slots[Weights[S].Value] = static_cast<float>(S);
            }
        }
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Phase 4: Coordinate assignment — extract real node positions from final ordering
    // Dummies are discarded; real nodes keep their rank-order slot.
    // ----------------------------------------------------------------------------------------------------------------

    auto PosX = TArray<int32>{};
    auto PosY = TArray<int32>{};
    PosX.SetNum(StateCount);
    PosY.SetNum(StateCount);

    for (auto R = 0; R <= MaxRank; ++R)
    {
        auto& Layer = RankLayers[R];
        auto Count = Layer.Num();
        auto TotalHeight = (Count - 1) * LayoutParams.SpacingY;

        for (auto S = 0; S < Count; ++S)
        {
            auto Idx = Layer[S];
            if (LayoutNodes[Idx].OriginalIndex >= 0)
            {
                auto Orig = LayoutNodes[Idx].OriginalIndex;
                PosX[Orig] = R * LayoutParams.SpacingX;
                PosY[Orig] = S * LayoutParams.SpacingY - TotalHeight / 2;
            }
        }
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Create state nodes
    // ----------------------------------------------------------------------------------------------------------------

    auto StateNodes = TArray<UCkSmDebugNode_State*>{};
    StateNodes.Reserve(StateCount);

    for (auto i = 0; i < StateCount; ++i)
    {
        auto& State = InSmInfo.States[i];

        auto StateNode = NewObject<UCkSmDebugNode_State>(this);
        StateNode->PopulateFromStateInfo(State, i);
        StateNode->NodePosX = PosX[i];
        StateNode->NodePosY = PosY[i];
        StateNode->AllocateDefaultPins();

        StateNode->SetFlags(RF_Transactional);
        AddNode(StateNode, /*bFromUI=*/ false, /*bSelectNewNode=*/ false);
        StateNodes.Add(StateNode);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Create transition nodes wired in series: SourceState.Out → Transition.In → Transition.Out → TargetState.In
    // ----------------------------------------------------------------------------------------------------------------

    for (auto i = 0; i < InSmInfo.Transitions.Num(); ++i)
    {
        auto& Transition = InSmInfo.Transitions[i];
        auto SourceIdx = Transition.SourceStateIndex;
        auto TargetIdx = Transition.TargetStateIndex;

        if (SourceIdx < 0 || SourceIdx >= StateNodes.Num()) { continue; }
        if (TargetIdx < 0 || TargetIdx >= StateNodes.Num()) { continue; }

        auto TransitionNode = NewObject<UCkSmDebugNode_Transition>(this);
        TransitionNode->PopulateFromTransitionInfo(Transition, i);

        // Position at midpoint — PerformSecondPassLayout will refine
        TransitionNode->NodePosX = (StateNodes[SourceIdx]->NodePosX + StateNodes[TargetIdx]->NodePosX) / 2;
        TransitionNode->NodePosY = (StateNodes[SourceIdx]->NodePosY + StateNodes[TargetIdx]->NodePosY) / 2;
        TransitionNode->AllocateDefaultPins();

        TransitionNode->SetFlags(RF_Transactional);
        AddNode(TransitionNode, /*bFromUI=*/ false, /*bSelectNewNode=*/ false);

        // Wire: SourceState.Out → Transition.In
        auto SourceOutputPin = StateNodes[SourceIdx]->Pins.IsValidIndex(1) ? StateNodes[SourceIdx]->Pins[1] : nullptr;
        auto TransInputPin = TransitionNode->Pins.IsValidIndex(0) ? TransitionNode->Pins[0] : nullptr;
        if (SourceOutputPin && TransInputPin)
        { SourceOutputPin->MakeLinkTo(TransInputPin); }

        // Wire: Transition.Out → TargetState.In
        auto TransOutputPin = TransitionNode->Pins.IsValidIndex(1) ? TransitionNode->Pins[1] : nullptr;
        auto TargetInputPin = StateNodes[TargetIdx]->Pins.IsValidIndex(0) ? StateNodes[TargetIdx]->Pins[0] : nullptr;
        if (TransOutputPin && TargetInputPin)
        { TransOutputPin->MakeLinkTo(TargetInputPin); }
    }

    _TopologyHash = ComputeTopologyHash(InSmInfo);

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
    auto NewHash = ComputeTopologyHash(InSmInfo);
    if (NewHash != _TopologyHash)
    {
        RebuildFromSmInfo(InSmInfo);
        return;
    }

    _TransitionData = InSmInfo.Transitions;

    for (auto Node : Nodes)
    {
        if (auto StateNode = Cast<UCkSmDebugNode_State>(Node))
        {
            auto Idx = StateNode->Get_StateIndex();
            if (Idx >= 0 && Idx < InSmInfo.States.Num())
            { StateNode->UpdateRuntimeData(InSmInfo.States[Idx]); }
        }
        else if (auto TransitionNode = Cast<UCkSmDebugNode_Transition>(Node))
        {
            auto Idx = TransitionNode->Get_TransitionIndex();
            if (Idx >= 0 && Idx < InSmInfo.Transitions.Num())
            { TransitionNode->UpdateRuntimeData(InSmInfo.Transitions[Idx]); }
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
