#include "CkGoapDebugGraph.h"

#include "CkGoapDebugNode_Action.h"
#include "CkGoapDebugNode_Goal.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"

// ====================================================================================================================
// Layout constants — chosen to match the mockup F v2 spacing.
// ====================================================================================================================

namespace ck_goap_graph
{
    constexpr float NodeWidth     = 180.0f;   // minimum card width; cards grow to fit content
    constexpr float NodeMaxWidth  = 420.0f;   // sanity cap — pathological tags fall back to ellipsis
    constexpr float NodeHeight    = 110.0f;   // layout-stacking fallback when Slate isn't initialized
    constexpr float HorzGap       = 70.0f;
    constexpr float VertGap       = 14.0f;
    constexpr float NodeMinX      = 40.0f;
    constexpr float NodeMinY      = 40.0f;
    constexpr float GoalGap       = 90.0f;    // extra horizontal gap before goal anchor
    constexpr float GoalNodeWidth = 200.0f;
}

// ====================================================================================================================
// Helpers
// ====================================================================================================================

namespace
{
    // Flattens a PlannerInfo subtree into a single ordered catalog of pointers
    // into the live snapshot. Atomic Actions appear once (under their owning
    // Planner's ChildActions); composite Actions appear once (they show up as
    // ChildActions[i] of their parent — we don't double-add when we recurse
    // into ChildPlanners[i].ChildActions). Recursion is dedup'd by handle.
    //
    // Suffixed `_Graph` to keep this safe from Adaptive-Unity anon-namespace
    // collisions with same-named helpers in sibling .cpp files.
    auto Flatten_Catalog_Graph(
        const FCkGoapDebugger_PlannerInfo& InPlanner,
        TArray<const FCkGoapDebugger_ActionInfo*>& OutCatalog,
        TSet<FCk_Handle_Goap_Action>& InOutVisited) -> void
    {
        for (const auto& Child : InPlanner.ChildActions)
        {
            if (NOT ck::IsValid(Child.Handle)) { continue; }
            if (InOutVisited.Contains(Child.Handle)) { continue; }
            InOutVisited.Add(Child.Handle);
            OutCatalog.Add(&Child);
        }
        for (const auto& SubPlanner : InPlanner.ChildPlanners)
        {
            Flatten_Catalog_Graph(SubPlanner, OutCatalog, InOutVisited);
        }
    }

    // Find an ActionInfo* in a flattened catalog by handle.
    auto FindByHandle_Graph(
        const TArray<const FCkGoapDebugger_ActionInfo*>& InCatalog,
        const FCk_Handle_Goap_Action& InHandle)
        -> const FCkGoapDebugger_ActionInfo*
    {
        if (ck::Is_NOT_Valid(InHandle)) { return nullptr; }
        for (const auto* A : InCatalog)
        {
            if (A != nullptr && A->Handle == InHandle) { return A; }
        }
        return nullptr;
    }

    // Resolves which Action's _Plan should drive in-plan tinting + plan-step
    // numbering. Prefer the user-selected Action (its plan is what the
    // primary pane is showing in the plan-strip). If unselected (or selection
    // has no plan handles of its own), fall back to the Planner's own plan
    // (PlanHandles + PlanClassNames on FCkGoapDebugger_PlannerInfo).
    //
    // Returns a pair (plan-step-by-classname, in-plan flag): when bool is false
    // the caller treats no node as in-plan.
    auto BuildPlanStepMap_Graph(
        const FCkGoapDebugger_PlannerInfo& InPlanner,
        const TArray<const FCkGoapDebugger_ActionInfo*>& InCatalog,
        const FCk_Handle_Goap_Action& InSelectedActionHandle)
        -> TMap<FString, int32>
    {
        auto StepByName = TMap<FString, int32>{};

        const auto* Selected = FindByHandle_Graph(InCatalog, InSelectedActionHandle);
        if (Selected != nullptr && Selected->PlanClassNames.Num() > 0)
        {
            auto Step = 1;
            for (const auto& Name : Selected->PlanClassNames)
            { StepByName.Add(Name, Step++); }
            return StepByName;
        }

        auto Step = 1;
        for (const auto& Name : InPlanner.PlanClassNames)
        { StepByName.Add(Name, Step++); }
        return StepByName;
    }

    auto Compute_TagLeaf_GraphLayout(const FGameplayTag& InTag) -> FString
    {
        const auto Full = InTag.ToString();
        int32 Idx = INDEX_NONE;
        if (Full.FindLastChar(TEXT('.'), Idx) && Idx != INDEX_NONE)
        { return Full.Mid(Idx + 1); }
        return Full;
    }

    // Measures the action card's desired (width, height) so the layered layout
    // can space columns/rows without overlap and the Slate widget can size the
    // card to its content (no key-name cutoff). Fonts MUST stay in sync with
    // SGraphNode_GoapAction's header/composite/body rows.
    auto Measure_NodeSize_Graph(
        const FCkGoapDebugger_ActionInfo& InAction,
        int32 InNameDepth) -> FVector2D
    {
        using namespace ck_goap_graph;

        if (NOT FSlateApplication::IsInitialized())
        { return FVector2D{NodeWidth, NodeHeight}; }

        const auto FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

        const auto Font_Header    = FCoreStyle::GetDefaultFontStyle("Bold", 10);
        const auto Font_Cost      = FCoreStyle::GetDefaultFontStyle("Bold", 9);
        const auto Font_Composite = FCoreStyle::GetDefaultFontStyle("Regular", 8);
        const auto Font_Row       = FCoreStyle::GetDefaultFontStyle("Regular", 8);

        constexpr auto DotWidth      = 7.0f + 4.0f;   // MakeDot + its padding slot
        // Precondition rows lead with "[exp square] -> [cur square]":
        // 7 + 2 + ~10 (arrow at font 7) + 2 + 7 + 4 trailing padding.
        constexpr auto PreRowPrefix  = 32.0f;
        constexpr auto ColumnGap     = 12.0f;         // breathing room between pre/eff columns
        constexpr auto CardChrome    = 2.0f * 2.0f    // outer border thickness
                                     + 2.0f * 8.0f    // inner Padding_Medium (horizontal)
                                     + 6.0f;          // slack so ellipsis never triggers at the cap
        constexpr auto RowVertPad    = 2.0f;          // 1.0f top + bottom per condition row

        const auto MeasureWidth = [&FontMeasure](const FString& InText, const FSlateFontInfo& InFont) -> float
        { return static_cast<float>(FontMeasure->Measure(InText, InFont).X); };
        const auto MeasureHeight = [&FontMeasure](const FString& InText, const FSlateFontInfo& InFont) -> float
        { return static_cast<float>(FontMeasure->Measure(InText, InFont).Y); };

        const auto HeaderName = SCkDebug_NameLabel::Get_ShortName(InAction.ClassName, InNameDepth);
        const auto CostText   = FString::Printf(TEXT("$%.0f"), InAction.Cost);
        const auto HeaderWidth  = MeasureWidth(HeaderName, Font_Header) + 4.0f + MeasureWidth(CostText, Font_Cost);
        const auto HeaderHeight = MeasureHeight(HeaderName, Font_Header);

        auto CompositeWidth  = 0.0f;
        auto CompositeHeight = 0.0f;
        if (InAction.ChildActionHandles.Num() > 0)
        {
            const auto CompositeText = FString::Printf(TEXT("\x203A %s"), *Compute_TagLeaf_GraphLayout(InAction.ActionTag));
            CompositeWidth  = MeasureWidth(CompositeText, Font_Composite) + 2.0f * 4.0f;        // bar's Padding_Small
            CompositeHeight = MeasureHeight(CompositeText, Font_Composite) + 2.0f * 1.0f + 2.0f; // bar pad + slot Padding_XSmall
        }

        auto MaxPreWidth = 0.0f;
        auto RowHeight   = 0.0f;
        for (const auto& Pre : InAction.Preconditions)
        {
            const auto Leaf = Compute_TagLeaf_GraphLayout(Pre.Key);
            MaxPreWidth = FMath::Max(MaxPreWidth, PreRowPrefix + MeasureWidth(Leaf, Font_Row));
            RowHeight   = FMath::Max(RowHeight, MeasureHeight(Leaf, Font_Row) + RowVertPad);
        }

        auto MaxEffWidth = 0.0f;
        for (const auto& Eff : InAction.Effects)
        {
            const auto Leaf = Compute_TagLeaf_GraphLayout(Eff.Key);
            MaxEffWidth = FMath::Max(MaxEffWidth, MeasureWidth(Leaf, Font_Row) + DotWidth);
            RowHeight   = FMath::Max(RowHeight, MeasureHeight(Leaf, Font_Row) + RowVertPad);
        }

        const auto BodyWidth = MaxPreWidth + ((MaxPreWidth > 0.0f && MaxEffWidth > 0.0f) ? ColumnGap : 0.0f) + MaxEffWidth;

        const auto Width = FMath::Clamp(
            FMath::Max3(HeaderWidth, CompositeWidth, BodyWidth) + CardChrome,
            NodeWidth, NodeMaxWidth);

        const auto BodyRows = static_cast<float>(FMath::Max(InAction.Preconditions.Num(), InAction.Effects.Num()));
        const auto Height =
            4.0f                                            // outer border (top + bottom)
            + 2.0f * 4.0f                                   // inner Padding_Small (vertical)
            + HeaderHeight
            + CompositeHeight
            + (BodyRows > 0.0f ? 4.0f + BodyRows * RowHeight : 0.0f);

        return FVector2D{Width, FMath::Max(Height, 60.0f)};
    }
}

// ====================================================================================================================

auto
    UCkGoapDebugGraph::
    ForceClear()
    -> void
{
    _GoalNode = nullptr;
    _TreeEdgePins_Graph.Reset();
    _LiveWorldState.Reset();
    Nodes.Empty();
}

// ====================================================================================================================

auto
    UCkGoapDebugGraph::
    Is_TreeEdge(
        UEdGraphPin* InOutputPin,
        UEdGraphPin* InInputPin) const
    -> bool
{
    if (NOT InOutputPin || NOT InInputPin) { return false; }
    return _TreeEdgePins_Graph.Contains(TPair<UEdGraphPin*, UEdGraphPin*>{InOutputPin, InInputPin});
}

// ====================================================================================================================

auto
    UCkGoapDebugGraph::
    ComputeTopologyHash(
        const FCkGoapDebugger_PlannerInfo& InPlanner)
    -> uint32
{
    // Capture identity of every catalog node + its pin shape (precondition
    // keys + effect keys) + the Planner's resolved goal. Intentionally
    // excludes mutable per-tick fields (PlanStatus, plan handles, selection)
    // so the fast-path UpdateRuntimeState handles those without a full
    // rebuild.
    auto Hash = uint32{0};
    Hash = HashCombine(Hash, ::GetTypeHash(static_cast<FCk_Handle>(InPlanner.PlannerHandle)));

    // Flatten the catalog so re-parenting / sub-planner promotions /
    // ChildActions mutations show up as topology changes.
    auto Catalog  = TArray<const FCkGoapDebugger_ActionInfo*>{};
    auto Visited  = TSet<FCk_Handle_Goap_Action>{};
    Flatten_Catalog_Graph(InPlanner, Catalog, Visited);

    Hash = HashCombine(Hash, ::GetTypeHash(Catalog.Num()));

    for (const auto* ActionPtr : Catalog)
    {
        if (ActionPtr == nullptr) { continue; }
        const auto& Action = *ActionPtr;

        Hash = HashCombine(Hash, ::GetTypeHash(static_cast<FCk_Handle>(Action.Handle)));
        Hash = HashCombine(Hash, GetTypeHash(Action.ClassName));
        Hash = HashCombine(Hash, ::GetTypeHash(Action.Preconditions.Num()));
        Hash = HashCombine(Hash, ::GetTypeHash(Action.Effects.Num()));

        for (const auto& P : Action.Preconditions)
        {
            Hash = HashCombine(Hash, GetTypeHash(P.Key));
            Hash = HashCombine(Hash, ::GetTypeHash(P.Value ? 1 : 0));
        }
        for (const auto& E : Action.Effects)
        {
            Hash = HashCombine(Hash, GetTypeHash(E.Key));
            Hash = HashCombine(Hash, ::GetTypeHash(E.Value ? 1 : 0));
        }

        // Tree structure participates in topology. Re-parenting an Action,
        // or adding/removing a child, must trigger a full graph rebuild so
        // the tree edges and per-pin connections track the new shape.
        Hash = HashCombine(Hash, ::GetTypeHash(static_cast<FCk_Handle>(Action.ParentActionHandle)));
        Hash = HashCombine(Hash, ::GetTypeHash(Action.ChildActionHandles.Num()));
        for (const auto& Child : Action.ChildActionHandles)
        { Hash = HashCombine(Hash, ::GetTypeHash(static_cast<FCk_Handle>(Child))); }
    }

    // Planner-level goal — its resolved condition set drives the goal node's
    // pin shape, so it must enter the topology hash.
    Hash = HashCombine(Hash, ::GetTypeHash(InPlanner.GoalResolved.Num()));
    for (const auto& G : InPlanner.GoalResolved)
    {
        Hash = HashCombine(Hash, GetTypeHash(G.Key));
        Hash = HashCombine(Hash, ::GetTypeHash(G.Value ? 1 : 0));
    }

    return Hash;
}

// ====================================================================================================================

auto
    UCkGoapDebugGraph::
    UpdateRuntimeState(
        const FCkGoapDebugger_PlannerInfo& InPlanner,
        const FCk_Handle_Goap_Action& InSelectedActionHandle)
    -> bool
{
    // In-plan tinting + plan-step numbering reflect the currently-selected
    // Action's _Plan (or the Planner's own plan when no specific Action is
    // selected).
    //
    // CRITICAL: this is NOT the same as the active chain. In the unified
    // model the active chain only contains composite/root Actions (the
    // tree depth), while _Plan is the flat ordered list of child Action
    // classes the planner chose to satisfy its goal. Atomic operators that
    // satisfy the goal directly (e.g. all four MakeTea steps) live in
    // _Plan but never appear in the active chain.
    auto Catalog = TArray<const FCkGoapDebugger_ActionInfo*>{};
    auto Visited = TSet<FCk_Handle_Goap_Action>{};
    Flatten_Catalog_Graph(InPlanner, Catalog, Visited);

    const auto PlanStepByClassName = BuildPlanStepMap_Graph(InPlanner, Catalog, InSelectedActionHandle);

    auto Changed = false;

    // Refresh the live WS view the action cards' satisfaction dots read from.
    // A value flip counts as a runtime-state change so the pane knows the next
    // paint will differ (the TAttribute-bound dots pick it up without any
    // NotifyGraphChanged).
    auto NewWorldState = TMap<FGameplayTag, bool>{};
    NewWorldState.Reserve(InPlanner.WorldState.Num());
    for (const auto& Entry : InPlanner.WorldState)
    { NewWorldState.Add(Entry.Key, Entry.Value); }

    if (NOT _LiveWorldState.OrderIndependentCompareEqual(NewWorldState))
    {
        _LiveWorldState = MoveTemp(NewWorldState);
        Changed = true;
    }

    for (UEdGraphNode* Node : Nodes)
    {
        auto* ActionNode = Cast<UCkGoapDebugNode_Action>(Node);
        if (ActionNode == nullptr) { continue; }

        const auto& Handle = ActionNode->Get_ActionHandle();

        const auto* CatalogEntry = FindByHandle_Graph(Catalog, Handle);
        if (CatalogEntry == nullptr) { continue; }

        auto NewStep = 0;
        if (auto* StepPtr = PlanStepByClassName.Find(CatalogEntry->ClassName))
        { NewStep = *StepPtr; }
        const auto NewInPlan = NewStep > 0;

        const auto NewSelected = ck::IsValid(InSelectedActionHandle) && (Handle == InSelectedActionHandle);

        const auto NewFailureBlocked =
            CatalogEntry->PlanStatus == ECk_GoapPlanStatus::PlanFailed
            && (CatalogEntry->Role == ECkGoapDebugger_ActionRole::Leaf
                || CatalogEntry->Role == ECkGoapDebugger_ActionRole::Mid);

        // Cost can change at runtime via Request_SetChildActionCost without a
        // topology rebuild — refresh it here so the live-bound card text updates.
        const auto NewCost = CatalogEntry->Cost;

        if (ActionNode->Get_IsInPlan()         != NewInPlan
         || ActionNode->Get_PlanStepIndex()    != NewStep
         || ActionNode->Get_IsSelected()       != NewSelected
         || ActionNode->Get_IsFailureBlocked() != NewFailureBlocked
         || NOT FMath::IsNearlyEqual(ActionNode->Get_Snapshot().Cost, NewCost))
        {
            ActionNode->Set_IsInPlan(NewInPlan);
            ActionNode->Set_PlanStepIndex(NewStep);
            ActionNode->Set_IsSelected(NewSelected);
            ActionNode->Set_IsFailureBlocked(NewFailureBlocked);
            ActionNode->Set_Cost(NewCost);
            Changed = true;
        }
    }

    return Changed;
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
    Get_MaxNameDepth() const
    -> int32
{
    auto Max = 1;
    for (UEdGraphNode* Node : Nodes)
    {
        auto* ActionNode = Cast<UCkGoapDebugNode_Action>(Node);
        if (NOT ActionNode) { continue; }

        auto Name = ActionNode->Get_Snapshot().ClassName;
        if (Name.StartsWith(TEXT("Default__"))) { Name = Name.RightChop(9); }
        if (Name.EndsWith(TEXT("_C")))          { Name = Name.LeftChop(2); }

        TArray<FString> Segs;
        Name.ParseIntoArray(Segs, TEXT("_"), true);
        if (Segs.Num() > Max) { Max = Segs.Num(); }
    }
    return Max;
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
        const FCkGoapDebugger_PlannerInfo& InPlanner,
        const FCk_Handle_Goap_Action& InSelectedActionHandle)
    -> void
{
    using namespace ck_goap_graph;

    SetSuppressNotifications(true);

    _GoalNode = nullptr;
    _TreeEdgePins_Graph.Reset();
    Nodes.Empty();

    _LiveWorldState.Reset();
    _LiveWorldState.Reserve(InPlanner.WorldState.Num());
    for (const auto& Entry : InPlanner.WorldState)
    { _LiveWorldState.Add(Entry.Key, Entry.Value); }

    // Flatten the recursive PlannerInfo tree into an ordered catalog of
    // ActionInfo pointers — this replaces the legacy ActionSetInfo::Catalog
    // flat array. Walk order is parent-first so layout reproduces the same
    // visual ordering the legacy path produced.
    auto CatalogPtrs = TArray<const FCkGoapDebugger_ActionInfo*>{};
    auto VisitedFlatten = TSet<FCk_Handle_Goap_Action>{};
    Flatten_Catalog_Graph(InPlanner, CatalogPtrs, VisitedFlatten);
    const auto ActionCount = CatalogPtrs.Num();

    // ----------------------------------------------------------------------------------------------------------------
    // Phase 1: Create one node per Catalog entry. Defer pin allocation until
    //          after we've populated the snapshot so AllocateDefaultPins can
    //          read the precondition/effect arrays.
    // ----------------------------------------------------------------------------------------------------------------

    auto ActionNodes = TArray<UCkGoapDebugNode_Action*>{};
    ActionNodes.Reserve(ActionCount);

    // Per-node measured (width, height) — drives both the card's WidthOverride
    // and the layered layout's column/row spacing so wide cards never overlap.
    auto NodeSizes = TArray<FVector2D>{};
    NodeSizes.Reserve(ActionCount);

    // Build effect-key → producing-action-index map for fast wiring later.
    auto EffectKeyToProducer = TMap<FGameplayTag, TArray<int32>>{};

    // Plan-step numbering + in-plan tinting are driven by the currently-
    // selected Action's _Plan (or the Planner's own plan when no specific
    // Action is selected). See UpdateRuntimeState's matching comment for
    // the rationale — _Plan is NOT the active chain.
    const auto PlanStepByClassName =
        BuildPlanStepMap_Graph(InPlanner, CatalogPtrs, InSelectedActionHandle);

    for (auto i = 0; i < ActionCount; ++i)
    {
        const auto& Action = *CatalogPtrs[i];

        auto* Node = NewObject<UCkGoapDebugNode_Action>(this);
        Node->PopulateFromActionInfo(Action);
        Node->AllocateDefaultPins();
        Node->SetFlags(RF_Transactional);

        const auto MeasuredSize = Measure_NodeSize_Graph(Action, NameDepth);
        Node->Set_NodeWidth(static_cast<float>(MeasuredSize.X));
        NodeSizes.Add(MeasuredSize);

        // IsInPlan + PlanStepIndex come from the selected (or root) Action's
        // _Plan, NOT the active chain. See UpdateRuntimeState's matching
        // block for the rationale.
        auto Step = 0;
        if (auto* StepPtr = PlanStepByClassName.Find(Action.ClassName))
        { Step = *StepPtr; }
        Node->Set_IsInPlan(Step > 0);
        Node->Set_PlanStepIndex(Step);

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
        const auto& Action = *CatalogPtrs[InIndex];
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

    // Column X offsets accumulate per-layer max width (cards auto-size, so a
    // layer is as wide as its widest card); rows stack with actual measured
    // heights so tall cards (many conditions) don't overlap their neighbours.
    auto LayerWidths = TArray<float>{};
    LayerWidths.Init(NodeWidth, MaxLayer + 1);
    auto LayerHeights = TArray<float>{};
    LayerHeights.Init(0.0f, MaxLayer + 1);

    for (auto i = 0; i < ActionCount; ++i)
    {
        const auto L = Layers[i];
        LayerWidths[L]   = FMath::Max(LayerWidths[L], static_cast<float>(NodeSizes[i].X));
        LayerHeights[L] += static_cast<float>(NodeSizes[i].Y) + VertGap;
    }
    for (auto& H : LayerHeights) { H = FMath::Max(H - VertGap, 0.0f); }   // no trailing gap

    auto LayerX = TArray<float>{};
    LayerX.Init(NodeMinX, MaxLayer + 2);   // +1 extra entry = goal-anchor column
    for (auto L = 1; L <= MaxLayer + 1; ++L)
    { LayerX[L] = LayerX[L - 1] + LayerWidths[FMath::Min(L - 1, MaxLayer)] + HorzGap; }

    auto LayerYCursor = TArray<float>{};
    LayerYCursor.SetNumZeroed(MaxLayer + 1);

    for (auto i = 0; i < ActionCount; ++i)
    {
        const auto L = Layers[i];

        // Vertically centre the layer's nodes around 0 so layers visually balance.
        const auto Y = NodeMinY + LayerYCursor[L] - LayerHeights[L] / 2.0f;
        LayerYCursor[L] += static_cast<float>(NodeSizes[i].Y) + VertGap;

        ActionNodes[i]->NodePosX = static_cast<int32>(LayerX[L]);
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
        const auto& Action = *CatalogPtrs[i];
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
    // Phase 4b: Tree edges (U11.7-D). For each Action that owns direct children
    //           in the tree (composite Actions / Planner roots), add an edge
    //           from this node's hidden TreeOut pin to each child's TreeIn pin.
    //           Record the (Out, In) pin pair in _TreeEdgePins_Graph so the
    //           connection policy can identify and style these wires as dashed,
    //           coexisting with the solid dependency edges from Phase 4.
    // ----------------------------------------------------------------------------------------------------------------

    auto FindActionIndex = [&](const FCk_Handle_Goap_Action& InH) -> int32
    {
        if (ck::Is_NOT_Valid(InH)) { return INDEX_NONE; }
        for (auto Idx = 0; Idx < ActionCount; ++Idx)
        {
            if (CatalogPtrs[Idx] != nullptr && CatalogPtrs[Idx]->Handle == InH) { return Idx; }
        }
        return INDEX_NONE;
    };

    for (auto i = 0; i < ActionCount; ++i)
    {
        const auto& Action = *CatalogPtrs[i];
        if (Action.ChildActionHandles.Num() == 0) { continue; }

        auto* ParentOutPin = ActionNodes[i]->Get_TreeOutPin();
        if (NOT ParentOutPin) { continue; }

        for (const auto& ChildHandle : Action.ChildActionHandles)
        {
            const auto ChildIdx = FindActionIndex(ChildHandle);
            if (ChildIdx == INDEX_NONE) { continue; }   // child not in catalog (shouldn't happen)

            auto* ChildInPin = ActionNodes[ChildIdx]->Get_TreeInPin();
            if (NOT ChildInPin) { continue; }

            ParentOutPin->MakeLinkTo(ChildInPin);
            _TreeEdgePins_Graph.Add(TPair<UEdGraphPin*, UEdGraphPin*>{ParentOutPin, ChildInPin});
        }
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Phase 5: Goal anchor. The Planner's own resolved goal is what we render —
    //          if the selected Action is dual-role and has its own resolved
    //          goal we prefer that (the selected Action is showing what THAT
    //          Action's sub-planner is solving for). Place beyond the last
    //          action layer.
    // ----------------------------------------------------------------------------------------------------------------

    const TArray<FCkGoapDebugger_Condition>* GoalConditions = nullptr;
    auto GoalOwnerName = FString{};

    if (ck::IsValid(InSelectedActionHandle))
    {
        if (const auto* SelectedAction = FindByHandle_Graph(CatalogPtrs, InSelectedActionHandle))
        {
            if (SelectedAction->Goal.Num() > 0)
            {
                GoalConditions = &SelectedAction->Goal;
                GoalOwnerName = SelectedAction->ClassName;
            }
        }
    }

    if (GoalConditions == nullptr && InPlanner.GoalResolved.Num() > 0)
    {
        GoalConditions = &InPlanner.GoalResolved;
        GoalOwnerName = InPlanner.DisplayName;
    }

    if (GoalConditions != nullptr)
    {
        auto* GoalNode = NewObject<UCkGoapDebugNode_Goal>(this);
        GoalNode->PopulateFromGoal(GoalOwnerName, *GoalConditions);
        GoalNode->AllocateDefaultPins();
        GoalNode->SetFlags(RF_Transactional);

        const auto GoalLayer = (MaxLayer >= 0) ? (MaxLayer + 1) : 0;
        GoalNode->NodePosX = static_cast<int32>(LayerX[GoalLayer] + GoalGap);
        GoalNode->NodePosY = static_cast<int32>(NodeMinY);

        AddNode(GoalNode, /*bFromUI=*/ false, /*bSelectNewNode=*/ false);
        _GoalNode = GoalNode;

        // Wire effect-producers into matching goal-condition pins.
        for (const auto& Cond : *GoalConditions)
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
