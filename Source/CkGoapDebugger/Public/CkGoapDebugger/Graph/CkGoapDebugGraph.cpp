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
		UpdateRuntimeState(InInfo);
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
	-> void
{
	for (auto Idx = 0; Idx < _ActionNodes.Num(); ++Idx)
	{
		auto* Node = _ActionNodes[Idx].Get();
		const auto PlanIdx = InInfo.PlanActionNames.IndexOfByKey(Node->Get_ActionName());
		Node->UpdatePlanState(PlanIdx != INDEX_NONE, PlanIdx);
	}

	if (_GoalNode != nullptr)
	{
		for (const auto& Goal : InInfo.Goals)
		{
			if (Goal.ClassName == _GoalNode->Get_GoalName())
			{
				_GoalNode->Set_IsActiveGoal(Goal.IsActiveGoal);
				break;
			}
		}
	}
}

// ====================================================================================================================

auto
	UCkGoapDebugGraph::
	PerformLayout()
	-> void
{
	// Build layout nodes + edges for the shared Sugiyama layout
	auto LayoutNodes = TArray<FCkDebugGraphLayoutNode>{};
	for (auto Idx = 0; Idx < _ActionNodes.Num(); ++Idx)
	{
		LayoutNodes.Add({Idx});
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

	auto LayoutParams = FCkDebugGraphLayoutParams{};
	LayoutParams.SpacingX = SpacingX;
	LayoutParams.SpacingY = SpacingY;
	LayoutParams.CrossingReductionPasses = 4;
	LayoutParams.IsDirectedBFS = true;
	LayoutParams.InitialNodeIndex = 0;

	auto Result = FCkDebugGraphLayout::ComputeLayout(LayoutNodes, LayoutEdges, LayoutParams);

	// Apply positions from layout result
	auto MaxX = 0;
	for (auto Idx = 0; Idx < _ActionNodes.Num(); ++Idx)
	{
		auto* Node = _ActionNodes[Idx].Get();
		if (const auto* Pos = Result.Positions.Find(Idx))
		{
			Node->NodePosX = Pos->X;
			Node->NodePosY = Pos->Y;
			MaxX = FMath::Max(MaxX, Pos->X);
		}
	}

	// Position goal node to the right of all action nodes
	if (_GoalNode != nullptr)
	{
		_GoalNode->NodePosX = MaxX + SpacingX;
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

	return Hash;
}

// ====================================================================================================================
