#include "CkGoapDebugGraph.h"
#include "CkGoapDebugNode_Action.h"
#include "CkGoapDebugNode_Goal.h"
#include "CkGoapDebugGraphSchema.h"

#include "CkDebuggerCommon/Graph/CkDebugGraphLayout.h"

// ====================================================================================================================

auto
	UCkGoapDebugGraph::
	UpdateFromGoapInfo(
		const FCkGoapDebugger_GoapInfo& InInfo)
	-> void
{
	const auto NewHash = ComputeTopologyHash(InInfo);

	if (NewHash != _TopologyHash)
	{
		_TopologyHash = NewHash;
		SetSuppressNotifications(true);
		RebuildTopology(InInfo);
		SetSuppressNotifications(false);
		NotifyGraphChanged();
	}
	else
	{
		const auto PlanChanged = UpdateRuntimeState(InInfo);
		if (PlanChanged)
		{
			NotifyGraphChanged();
		}
	}
}

// ====================================================================================================================

auto
	UCkGoapDebugGraph::
	RebuildTopology(
		const FCkGoapDebugger_GoapInfo& InInfo)
	-> void
{
	Nodes.Empty();
	_ActionNodes.Empty();
	_GoalNode = nullptr;

	// Build effect→provider map
	TMap<FString, TArray<int32>> EffectProviders;
	for (auto Index = 0; Index < InInfo.Actions.Num(); ++Index)
	{
		for (const auto& [Key, Value] : InInfo.Actions[Index].Effects)
		{
			EffectProviders.FindOrAdd(Key.ToString()).Add(Index);
		}
	}

	// Create action nodes
	for (auto Index = 0; Index < InInfo.Actions.Num(); ++Index)
	{
		auto* ActionNode = NewObject<UCkGoapDebugNode_Action>(this);
		ActionNode->PopulateFromActionInfo(InInfo.Actions[Index], Index);
		ActionNode->Set_DisplayName(ComputeDisplayName(InInfo.Actions[Index].ClassName, NameDepth));
		ActionNode->CreateNewGuid();
		ActionNode->AllocateDefaultPins();

		const auto PlanIdx = InInfo.PlanActionNames.IndexOfByKey(InInfo.Actions[Index].ClassName);
		ActionNode->UpdatePlanState(PlanIdx != INDEX_NONE, PlanIdx);

		AddNode(ActionNode, false, false);
		_ActionNodes.Add(ActionNode);
	}

	// Create edges: effect→precondition connections
	for (auto TargetIdx = 0; TargetIdx < InInfo.Actions.Num(); ++TargetIdx)
	{
		auto* TargetNode = _ActionNodes[TargetIdx].Get();
		auto* InputPin = TargetNode->FindPin(TEXT("In"), EGPD_Input);
		if (InputPin == nullptr) { continue; }

		for (const auto& [PreKey, PreValue] : InInfo.Actions[TargetIdx].Preconditions)
		{
			const auto KeyStr = PreKey.ToString();
			const auto* Providers = EffectProviders.Find(KeyStr);
			if (Providers == nullptr) { continue; }

			for (const auto SourceIdx : *Providers)
			{
				if (SourceIdx == TargetIdx) { continue; }
				auto* SourceNode = _ActionNodes[SourceIdx].Get();
				auto* OutputPin = SourceNode->FindPin(TEXT("Out"), EGPD_Output);
				if (OutputPin == nullptr) { continue; }

				OutputPin->MakeLinkTo(InputPin);
			}
		}
	}

	// Create goal node for the active goal
	for (const auto& Goal : InInfo.Goals)
	{
		if (Goal.IsActiveGoal)
		{
			_GoalNode = NewObject<UCkGoapDebugNode_Goal>(this);
			_GoalNode->PopulateFromGoalInfo(Goal);
			_GoalNode->Set_DisplayName(ComputeDisplayName(Goal.ClassName, NameDepth));
			_GoalNode->CreateNewGuid();
			_GoalNode->AllocateDefaultPins();
			AddNode(_GoalNode, false, false);

			// Connect last plan action to goal
			if (InInfo.PlanActionNames.Num() > 0)
			{
				const auto& LastActionName = InInfo.PlanActionNames.Last();
				for (auto Idx = 0; Idx < _ActionNodes.Num(); ++Idx)
				{
					auto* Node = _ActionNodes[Idx].Get();
					if (Node->Get_ActionName() == LastActionName)
					{
						auto* OutPin = Node->FindPin(TEXT("Out"), EGPD_Output);
						auto* GoalInPin = _GoalNode->FindPin(TEXT("In"), EGPD_Input);
						if (OutPin && GoalInPin) { OutPin->MakeLinkTo(GoalInPin); }
						break;
					}
				}
			}
			break;
		}
	}

	PerformLayout();
}

// ====================================================================================================================

auto
	UCkGoapDebugGraph::
	UpdateRuntimeState(
		const FCkGoapDebugger_GoapInfo& InInfo)
	-> bool
{
	auto Changed = false;

	for (auto Idx = 0; Idx < _ActionNodes.Num(); ++Idx)
	{
		auto* Node = _ActionNodes[Idx].Get();
		const auto PlanIdx = InInfo.PlanActionNames.IndexOfByKey(Node->Get_ActionName());
		const auto NewInPlan = PlanIdx != INDEX_NONE;

		if (Node->Get_InPlan() != NewInPlan || Node->Get_PlanStepIndex() != PlanIdx)
		{
			Changed = true;
		}

		Node->UpdatePlanState(NewInPlan, PlanIdx);
	}

	if (_GoalNode != nullptr)
	{
		for (const auto& Goal : InInfo.Goals)
		{
			if (Goal.ClassName == _GoalNode->Get_GoalName())
			{
				if (_GoalNode->Get_IsActiveGoal() != Goal.IsActiveGoal) { Changed = true; }
				_GoalNode->Set_IsActiveGoal(Goal.IsActiveGoal);
				break;
			}
		}
	}

	return Changed;
}

// ====================================================================================================================

auto
	UCkGoapDebugGraph::
	PerformLayout()
	-> void
{
	// Estimate a node's width from the longest string it will render:
	// its display name, each precondition tag, each effect tag. ~7.5px/char
	// at the default font, plus 48px for dot/badge/padding.
	const auto EstimateWidth = [](const UCkGoapDebugNode_Action* InNode) -> int32
	{
		if (InNode == nullptr) { return 0; }
		auto MaxChars = InNode->Get_DisplayName().Len();
		for (const auto& [Key, Value] : InNode->Get_Preconditions())
		{
			MaxChars = FMath::Max(MaxChars, Key.ToString().Len());
		}
		for (const auto& [Key, Value] : InNode->Get_Effects())
		{
			MaxChars = FMath::Max(MaxChars, Key.ToString().Len());
		}
		return static_cast<int32>(MaxChars * 7.5f) + 48;
	};

	// Build layout nodes + edges for the shared Sugiyama layout
	auto LayoutNodes = TArray<FCkDebugGraphLayoutNode>{};
	for (auto Idx = 0; Idx < _ActionNodes.Num(); ++Idx)
	{
		auto Layout = FCkDebugGraphLayoutNode{};
		Layout.Index = Idx;
		Layout.EstimatedWidth = EstimateWidth(_ActionNodes[Idx].Get());
		LayoutNodes.Add(Layout);
	}

	// Build effect→provider map for edge construction
	TMap<FString, TArray<int32>> EffectProviders;
	for (auto Idx = 0; Idx < _ActionNodes.Num(); ++Idx)
	{
		for (const auto& [Key, Value] : _ActionNodes[Idx].Get()->Get_Effects())
		{
			EffectProviders.FindOrAdd(Key.ToString()).Add(Idx);
		}
	}

	auto LayoutEdges = TArray<FCkDebugGraphLayoutEdge>{};
	for (auto TargetIdx = 0; TargetIdx < _ActionNodes.Num(); ++TargetIdx)
	{
		for (const auto& [PreKey, PreVal] : _ActionNodes[TargetIdx].Get()->Get_Preconditions())
		{
			const auto* Providers = EffectProviders.Find(PreKey.ToString());
			if (Providers == nullptr) { continue; }
			for (const auto SourceIdx : *Providers)
			{
				if (SourceIdx != TargetIdx)
				{
					LayoutEdges.Add({SourceIdx, TargetIdx});
				}
			}
		}
	}

	// SpacingX is the minimum inter-column gap. Per-node EstimatedWidth above
	// ensures each column is at least as wide as its widest member, so wide
	// nodes never overflow into the next layer.
	auto LayoutParams = FCkDebugGraphLayoutParams{};
	LayoutParams.SpacingX = SpacingX;
	LayoutParams.SpacingY = SpacingY;
	LayoutParams.CrossingReductionPasses = 4;
	LayoutParams.IsDirectedBFS = true;
	LayoutParams.InitialNodeIndex = 0;

	auto Result = FCkDebugGraphLayout::ComputeLayout(LayoutNodes, LayoutEdges, LayoutParams);

	// Apply positions from layout result. Track the rightmost column's origin
	// AND the width of the widest node in that column so the goal can be
	// placed just past them.
	auto MaxX = 0;
	auto MaxXNodeWidth = 0;
	for (auto Idx = 0; Idx < _ActionNodes.Num(); ++Idx)
	{
		auto* Node = _ActionNodes[Idx].Get();
		if (const auto* Pos = Result.Positions.Find(Idx))
		{
			Node->NodePosX = Pos->X;
			Node->NodePosY = Pos->Y;
			if (Pos->X > MaxX)
			{
				MaxX = Pos->X;
				MaxXNodeWidth = LayoutNodes[Idx].EstimatedWidth;
			}
			else if (Pos->X == MaxX)
			{
				MaxXNodeWidth = FMath::Max(MaxXNodeWidth, LayoutNodes[Idx].EstimatedWidth);
			}
		}
	}

	// Position goal node to the right of all action nodes, past the widest
	// node in the final column plus one SpacingX gutter.
	if (_GoalNode != nullptr)
	{
		_GoalNode->NodePosX = MaxX + MaxXNodeWidth + SpacingX;
		_GoalNode->NodePosY = SpacingY;
	}

}

// ====================================================================================================================

auto
	UCkGoapDebugGraph::
	FindActionNode(int32 InIndex) const
	-> UCkGoapDebugNode_Action*
{
	return _ActionNodes.IsValidIndex(InIndex) ? _ActionNodes[InIndex].Get() : nullptr;
}

// ====================================================================================================================

auto
	UCkGoapDebugGraph::
	ComputeDisplayName(const FString& InClassName, int32 InDepth)
	-> FString
{
	auto Name = InClassName;
	if (Name.EndsWith(TEXT("_C"))) { Name = Name.LeftChop(2); }

	TArray<FString> Segments;
	Name.ParseIntoArray(Segments, TEXT("_"), true);

	if (InDepth <= 0 || InDepth >= Segments.Num())
	{
		return FString::Join(Segments, TEXT("."));
	}

	auto Result = FString{};
	for (auto i = Segments.Num() - InDepth; i < Segments.Num(); ++i)
	{
		if (Result.Len() > 0) { Result += TEXT("."); }
		Result += Segments[i];
	}
	return Result;
}

// ====================================================================================================================

auto
	UCkGoapDebugGraph::
	ComputeTopologyHash(
		const FCkGoapDebugger_GoapInfo& InInfo) const
	-> uint32
{
	auto Hash = uint32{0};
	Hash = HashCombine(Hash, GetTypeHash(NameDepth));
	Hash = HashCombine(Hash, GetTypeHash(SpacingX));
	Hash = HashCombine(Hash, GetTypeHash(SpacingY));
	Hash = HashCombine(Hash, GetTypeHash(InInfo.Actions.Num()));
	Hash = HashCombine(Hash, GetTypeHash(InInfo.Goals.Num()));

	for (const auto& Action : InInfo.Actions)
	{
		Hash = HashCombine(Hash, GetTypeHash(Action.ClassName));
		Hash = HashCombine(Hash, GetTypeHash(Action.Preconditions.Num()));
		Hash = HashCombine(Hash, GetTypeHash(Action.Effects.Num()));
	}

	// Include the active goal's identity so a goal swap rebuilds the topology
	// (the goal node is rebuilt only in RebuildTopology, not UpdateRuntimeState,
	// so we must treat the active goal as topology).
	for (const auto& Goal : InInfo.Goals)
	{
		if (Goal.IsActiveGoal)
		{
			Hash = HashCombine(Hash, GetTypeHash(Goal.ClassName));
			break;
		}
	}

	// Include the plan contents as topology. The SGraphNode widget reads
	// InPlan / PlanStepIndex into static SBox sizes at construction time
	// (see SGraphNode_GoapAction::UpdateGraphNode) — mutating the UObject's
	// _InPlan bit alone doesn't re-render those. Force a full rebuild when
	// the plan composition shifts so fresh widgets pick up the new state.
	Hash = HashCombine(Hash, GetTypeHash(InInfo.PlanActionNames.Num()));
	for (const auto& Name : InInfo.PlanActionNames)
	{
		Hash = HashCombine(Hash, GetTypeHash(Name));
	}

	return Hash;
}

// ====================================================================================================================
